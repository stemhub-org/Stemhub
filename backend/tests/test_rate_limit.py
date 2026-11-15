"""Fixed rate limits stored in Postgres, used for sign-up: 10 attempts per
IP per hour, refused attempts included."""
from __future__ import annotations

import asyncio
from datetime import datetime, timedelta, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI, HTTPException
from fastapi.testclient import TestClient
from sqlalchemy.dialects import postgresql

from stemhub import auth, rate_limit
from stemhub.database import get_db
from stemhub.models import RateLimitHit
from stemhub.rate_limit import enforce_rate_limit

WINDOW = timedelta(hours=1)


def _ago(**delta: float) -> datetime:
    return datetime.now(timezone.utc) - timedelta(**delta)


def _db(hits: int, oldest: datetime | None) -> MagicMock:
    answer = MagicMock()
    answer.one.return_value = (hits, oldest)
    db = MagicMock()
    db.execute = AsyncMock(return_value=answer)
    db.commit = AsyncMock()
    return db


def _enforce(db: MagicMock) -> None:
    asyncio.run(
        enforce_rate_limit(db, action="register", key="203.0.113.7", limit=10, window=WINDOW, message="Too many sign-ups.")
    )


def _recorded(db: MagicMock) -> list[tuple[str, str]]:
    return [(c.args[0].action, c.args[0].key) for c in db.add.call_args_list if isinstance(c.args[0], RateLimitHit)]


def test_under_the_limit_the_hit_is_recorded_and_committed() -> None:
    db = _db(9, _ago(minutes=30))
    _enforce(db)
    assert _recorded(db) == [("register", "203.0.113.7")]
    db.commit.assert_awaited_once()


def test_at_the_limit_the_action_is_refused_until_the_oldest_hit_expires() -> None:
    db = _db(10, _ago(minutes=45))
    with pytest.raises(HTTPException) as refused:
        _enforce(db)
    assert refused.value.status_code == 429
    assert 899 <= int(refused.value.headers["Retry-After"]) <= 900
    assert refused.value.detail == "Too many sign-ups. Try again in 15 minutes."
    assert _recorded(db) == []


def test_only_hits_of_this_action_key_and_window_count() -> None:
    db = _db(0, None)
    _enforce(db)
    sql = str(db.execute.await_args.args[0].compile(dialect=postgresql.dialect(), compile_kwargs={"literal_binds": True}))
    assert "rate_limit_hit.action = 'register'" in sql
    assert "rate_limit_hit.key = '203.0.113.7'" in sql
    assert "rate_limit_hit.created_at >" in sql


# ── Sign-up ──

def _register(monkeypatch, *, limited: bool) -> tuple[Any, list[dict[str, Any]]]:
    calls: list[dict[str, Any]] = []

    async def limiter(db, **kwargs):
        calls.append(kwargs)
        if limited:
            raise rate_limit.too_many_requests(kwargs["message"], timedelta(minutes=5))

    monkeypatch.setattr(auth, "enforce_rate_limit", limiter)
    db = MagicMock()
    db.execute = AsyncMock()

    app = FastAPI()
    app.include_router(auth.router)

    async def override_db():
        yield db

    app.dependency_overrides[get_db] = override_db
    # Past the limiter the fake database has nothing to answer; only the
    # limiter's arguments and a refusal matter here.
    response = TestClient(app, raise_server_exceptions=False).post(
        "/auth/register", json={"email": "a@example.com", "username": "demo", "password": "a-strong-password", "accept_terms": True}
    )
    return response, calls


def test_sign_up_is_limited_to_ten_attempts_per_ip_per_hour(monkeypatch) -> None:
    _, [call] = _register(monkeypatch, limited=False)
    assert call == {
        "action": "register",
        "key": "testclient",
        "limit": 10,
        "window": timedelta(hours=1),
        "message": "Too many sign-ups from your network.",
    }


def test_a_limited_sign_up_does_nothing_else(monkeypatch) -> None:
    checked: list[str] = []

    async def pwned(password: str) -> bool:
        checked.append(password)
        return False

    monkeypatch.setattr(auth, "is_password_pwned", pwned)
    response, _ = _register(monkeypatch, limited=True)
    assert response.status_code == 429
    assert response.json()["detail"] == "Too many sign-ups from your network. Try again in 5 minutes."
    assert response.headers["Retry-After"] == "300"
    assert checked == []
