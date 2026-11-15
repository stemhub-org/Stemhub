"""Old sign-in records are purged every hour: sessions 30 days after they
expired or were revoked, login attempts after 30 days, rate limit hits
after a day. Accounts past their deletion grace period are erased in the
same round (test_account_deletion.py)."""
from __future__ import annotations

import asyncio
import contextlib
from datetime import timedelta
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from sqlalchemy.dialects import postgresql

from stemhub import auth_purge
from stemhub.auth_purge import PurgeResult, purge_auth_records, purge_auth_records_forever
from stemhub.login_throttle import EMAIL_WINDOW, IP_WINDOW


def _db(*rowcounts: int) -> MagicMock:
    db = MagicMock()
    db.execute = AsyncMock(side_effect=[MagicMock(rowcount=count) for count in rowcounts])
    db.commit = AsyncMock()
    return db


def _sql(statement: Any) -> str:
    return str(statement.compile(dialect=postgresql.dialect()))


def test_each_table_is_purged_with_its_retention() -> None:
    db = _db(2, 3, 4)
    result = asyncio.run(purge_auth_records(db))

    assert result == PurgeResult(sessions=2, login_attempts=3, rate_limit_hits=4)
    sessions, attempts, hits = (_sql(call.args[0]) for call in db.execute.await_args_list)
    assert sessions.startswith("DELETE FROM user_session")
    assert "user_session.expires_at <" in sessions and "user_session.revoked_at <" in sessions
    assert attempts.startswith("DELETE FROM login_attempt WHERE login_attempt.created_at <")
    assert hits.startswith("DELETE FROM rate_limit_hit WHERE rate_limit_hit.created_at <")
    db.commit.assert_awaited_once()


def test_the_retentions_outlast_what_the_checks_read() -> None:
    assert auth_purge.LOGIN_ATTEMPT_RETENTION >= max(EMAIL_WINDOW, IP_WINDOW)
    # The sign-up limit uses a one-hour window.
    assert auth_purge.RATE_LIMIT_HIT_RETENTION >= timedelta(hours=1)


def _run_forever(monkeypatch, outcomes: list[Any], erasures: list[int] | None = None) -> list[float]:
    """Run the loop for len(outcomes) rounds; each outcome is a PurgeResult or an exception."""
    sleeps: list[float] = []

    async def erase_due_accounts(db, storage):
        if erasures is not None:
            erasures.append(len(sleeps))
        return 0

    monkeypatch.setattr(auth_purge, "erase_due_accounts", erase_due_accounts)
    monkeypatch.setattr(auth_purge, "get_storage_service", MagicMock)

    async def purge(db):
        outcome = outcomes[len(sleeps)]
        if isinstance(outcome, Exception):
            raise outcome
        return outcome

    async def sleep(seconds: float) -> None:
        sleeps.append(seconds)
        if len(sleeps) == len(outcomes):
            raise asyncio.CancelledError

    @contextlib.asynccontextmanager
    async def session_factory():
        yield MagicMock()

    monkeypatch.setattr(auth_purge, "purge_auth_records", purge)
    monkeypatch.setattr(auth_purge.asyncio, "sleep", sleep)
    with pytest.raises(asyncio.CancelledError):
        asyncio.run(purge_auth_records_forever(session_factory))
    return sleeps


def test_the_purge_runs_every_hour(monkeypatch) -> None:
    erasures: list[int] = []
    sleeps = _run_forever(monkeypatch, [PurgeResult(1, 0, 0), PurgeResult(0, 0, 0)], erasures)
    assert sleeps == [3600, 3600]
    # Accounts past their deletion grace period are erased each round too.
    assert erasures == [0, 1]


def test_a_failed_purge_is_logged_and_retried(monkeypatch, caplog) -> None:
    sleeps = _run_forever(monkeypatch, [RuntimeError("database down"), PurgeResult(0, 1, 0)])
    assert sleeps == [3600, 3600]
    assert "Purging old sign-in records failed" in caplog.text
