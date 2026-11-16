"""Breached passwords are refused at sign-up and password change, using the
HaveIBeenPwned range API: only a 5-character prefix of the SHA-1 is sent."""
from __future__ import annotations

import asyncio
import hashlib
import uuid
from datetime import datetime, timezone
from unittest.mock import AsyncMock, MagicMock

import httpx
import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient

from stemhub import auth, pwned_passwords
from stemhub.database import get_db
from stemhub.models import User
from stemhub.pwned_passwords import BREACHED_PASSWORD_MESSAGE, is_password_pwned
from stemhub.security import get_password_hash

PASSWORD = "a-strong-password"
DIGEST = hashlib.sha1(PASSWORD.encode()).hexdigest().upper()


def _fake_api(monkeypatch, body: str | Exception, status: int = 200) -> list[httpx.Request]:
    seen: list[httpx.Request] = []

    def handler(request: httpx.Request) -> httpx.Response:
        seen.append(request)
        if isinstance(body, Exception):
            raise body
        return httpx.Response(status, text=body)

    real_client = httpx.AsyncClient
    monkeypatch.setattr(
        pwned_passwords.httpx,
        "AsyncClient",
        lambda **kwargs: real_client(transport=httpx.MockTransport(handler), **kwargs),
    )
    return seen


def _pwned(password: str = PASSWORD) -> bool:
    return asyncio.run(is_password_pwned(password))


def test_only_the_first_five_characters_of_the_hash_are_sent(monkeypatch) -> None:
    seen = _fake_api(monkeypatch, "")
    _pwned()
    [request] = seen
    assert str(request.url) == f"https://api.pwnedpasswords.com/range/{DIGEST[:5]}"
    assert request.headers["Add-Padding"] == "true"
    assert PASSWORD not in str(request.url) and DIGEST[5:] not in str(request.url)


def test_a_breached_password_is_found(monkeypatch) -> None:
    _fake_api(monkeypatch, f"0018A45C4D1DEF81644B54AB7F969B88D65:1\r\n{DIGEST[5:]}:3861493\r\n")
    assert _pwned() is True


def test_an_unknown_password_is_not(monkeypatch) -> None:
    _fake_api(monkeypatch, "0018A45C4D1DEF81644B54AB7F969B88D65:1\r\n")
    assert _pwned() is False


def test_padding_entries_do_not_count(monkeypatch) -> None:
    _fake_api(monkeypatch, f"{DIGEST[5:]}:0\r\n")
    assert _pwned() is False


@pytest.mark.parametrize("failure", [httpx.ConnectTimeout("slow"), httpx.ConnectError("offline")])
def test_an_unreachable_api_lets_the_password_through(monkeypatch, failure: Exception) -> None:
    _fake_api(monkeypatch, failure)
    assert _pwned() is False


def test_an_api_error_lets_the_password_through(monkeypatch) -> None:
    _fake_api(monkeypatch, "Service unavailable", status=503)
    assert _pwned() is False


# ── Where it applies ──

def _client(db: MagicMock, user: User | None = None) -> TestClient:
    app = FastAPI()
    app.include_router(auth.router)

    async def override_db():
        yield db

    app.dependency_overrides[get_db] = override_db
    if user is not None:
        app.dependency_overrides[auth.get_current_user] = lambda: user
    return TestClient(app)


def _breached(monkeypatch) -> list[str]:
    checked: list[str] = []

    async def always(password: str) -> bool:
        checked.append(password)
        return True

    monkeypatch.setattr(auth, "is_password_pwned", always)
    return checked


def test_sign_up_refuses_a_breached_password(monkeypatch) -> None:
    checked = _breached(monkeypatch)
    db = MagicMock()
    db.execute = AsyncMock()
    response = _client(db).post(
        "/auth/register", json={"email": "a@example.com", "username": "demo", "password": PASSWORD, "accept_terms": True}
    )
    assert response.status_code == 400
    assert response.json()["detail"] == BREACHED_PASSWORD_MESSAGE
    assert checked == [PASSWORD]
    db.execute.assert_not_awaited()


def test_a_password_change_refuses_a_breached_password(monkeypatch) -> None:
    checked = _breached(monkeypatch)
    user = User(
        id=uuid.uuid4(), email="a@example.com", username="demo", password_hash=get_password_hash(PASSWORD),
        created_at=datetime.now(timezone.utc), is_active=True, is_admin=False,
    )
    db = MagicMock()
    db.execute = AsyncMock()
    db.commit = AsyncMock()
    response = _client(db, user).put(
        "/auth/me/password", json={"current_password": PASSWORD, "new_password": "password123"}
    )
    assert response.status_code == 400
    assert response.json()["detail"] == BREACHED_PASSWORD_MESSAGE
    assert checked == ["password123"]
    db.commit.assert_not_awaited()
