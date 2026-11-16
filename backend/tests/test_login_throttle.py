"""Sign-in throttling: an email is locked for 1, 2, 4, 8 then 15 minutes
after its 5th, 6th, 7th... failure (a success starts over), an IP is blocked
after 30 failures in 15 minutes, and while locked the password is not even
checked."""
from __future__ import annotations

import asyncio
import uuid
from datetime import datetime, timedelta, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI, HTTPException
from fastapi.testclient import TestClient
from sqlalchemy.dialects import postgresql

from stemhub import auth
from stemhub.database import get_db
from stemhub.login_throttle import ensure_login_allowed, lockout_for
from stemhub.models import LoginAttempt, User
from stemhub.security import get_password_hash



def _ago(**delta: float) -> datetime:
    return datetime.now(timezone.utc) - timedelta(**delta)
PASSWORD = "a-strong-password"


def _db(*rows: tuple[Any, Any]) -> MagicMock:
    """Answers the throttle's queries in order: (failures, last failure) for
    the email, then (failures, oldest failure) for the IP."""
    def result(row: tuple[Any, Any]) -> MagicMock:
        answer = MagicMock()
        answer.one.return_value = row
        return answer

    db = MagicMock()
    db.execute = AsyncMock(side_effect=[result(row) for row in rows])
    return db


def _check(db: MagicMock, ip: str | None = "203.0.113.7") -> None:
    asyncio.run(ensure_login_allowed(db, "demo@example.com", ip))


def _refused(db: MagicMock, ip: str | None = "203.0.113.7") -> HTTPException:
    with pytest.raises(HTTPException) as refused:
        _check(db, ip)
    assert refused.value.status_code == 429
    return refused.value


# ── Lockout schedule ──

@pytest.mark.parametrize(
    "failures, minutes",
    [(0, 0), (4, 0), (5, 1), (6, 2), (7, 4), (8, 8), (9, 15), (10, 15), (10_000, 15)],
)
def test_lockout_doubles_up_to_15_minutes(failures: int, minutes: int) -> None:
    assert lockout_for(failures) == timedelta(minutes=minutes)


# ── Per email ──

def test_four_failures_are_allowed() -> None:
    _check(_db((4, _ago(seconds=1)), (4, _ago(minutes=1))))


def test_the_fifth_failure_locks_the_email_for_a_minute() -> None:
    refused = _refused(_db((5, _ago(seconds=20))))
    assert 39 <= int(refused.headers["Retry-After"]) <= 40
    assert refused.detail == "Too many failed sign-in attempts. Try again in 40 seconds."


def test_the_lock_ends() -> None:
    _check(_db((5, _ago(minutes=2)), (5, _ago(minutes=3))))


def test_a_longer_lock_is_spoken_in_minutes() -> None:
    refused = _refused(_db((9, _ago(minutes=1))))
    assert refused.detail == "Too many failed sign-in attempts. Try again in 14 minutes."


def test_only_failures_since_the_last_success_count() -> None:
    db = _db((0, None), (0, None))
    _check(db)
    sql = str(db.execute.await_args_list[0].args[0].compile(dialect=postgresql.dialect()))
    assert "login_attempt.succeeded = false" in sql
    assert "max(login_attempt.created_at)" in sql and "login_attempt.succeeded = true" in sql


# ── Per IP ──

def test_thirty_failures_from_one_ip_block_it() -> None:
    refused = _refused(_db((0, None), (30, _ago(minutes=10))))
    assert 299 <= int(refused.headers["Retry-After"]) <= 300


def test_twenty_nine_failures_from_one_ip_are_allowed() -> None:
    _check(_db((0, None), (29, _ago(minutes=10))))


def test_without_an_ip_only_the_email_is_checked() -> None:
    db = _db((0, None))
    _check(db, ip=None)
    assert db.execute.await_count == 1


# ── Sign-in ──

def _user() -> User:
    return User(
        id=uuid.uuid4(), email="demo@example.com", username="demo",
        password_hash=get_password_hash(PASSWORD), created_at=_ago(days=1),
        is_active=True, is_admin=False, is_deleted=False,
    )


def _login(monkeypatch, password: str, *, locked: bool = False) -> tuple[Any, MagicMock]:
    async def throttle(db, email, ip):
        assert email == "demo@example.com" and ip == "testclient"
        if locked:
            raise HTTPException(status_code=429, detail="Too many failed sign-in attempts.")

    monkeypatch.setattr(auth, "ensure_login_allowed", throttle)
    found = MagicMock()
    found.scalars.return_value.first.return_value = _user()
    db = MagicMock()
    db.execute = AsyncMock(return_value=found)
    db.commit = AsyncMock()

    app = FastAPI()
    app.include_router(auth.router)

    async def override_db():
        yield db

    app.dependency_overrides[get_db] = override_db
    response = TestClient(app).post("/auth/login", json={"email": "Demo@example.com", "password": password})
    return response, db


def _attempts(db: MagicMock) -> list[tuple[str, bool]]:
    return [(c.args[0].email, c.args[0].succeeded) for c in db.add.call_args_list if isinstance(c.args[0], LoginAttempt)]


def test_a_locked_email_is_refused_without_checking_the_password(monkeypatch) -> None:
    checked = MagicMock()
    monkeypatch.setattr(auth, "verify_password", checked)
    response, db = _login(monkeypatch, PASSWORD, locked=True)
    assert response.status_code == 429
    checked.assert_not_called()
    db.execute.assert_not_awaited()
    assert _attempts(db) == []


def test_a_failure_is_recorded_and_committed(monkeypatch) -> None:
    response, db = _login(monkeypatch, "wrong-password")
    assert response.status_code == 401
    assert _attempts(db) == [("demo@example.com", False)]
    db.commit.assert_awaited()


def test_a_success_is_recorded(monkeypatch) -> None:
    response, db = _login(monkeypatch, PASSWORD)
    assert response.status_code == 200
    assert _attempts(db) == [("demo@example.com", True)]
