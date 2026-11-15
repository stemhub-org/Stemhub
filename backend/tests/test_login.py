"""Login failures never reveal whether an email has an account: same 401,
same message, and a password hash is checked either way so the timing
matches. Only someone who knows the password learns the account is
deactivated."""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from sqlalchemy.dialects import postgresql

from stemhub import security
from stemhub.auth import router as auth_router
from stemhub.database import get_db
from stemhub.models import User
from stemhub.security import UNUSABLE_PASSWORD_HASH, get_password_hash

PASSWORD = "a-strong-password"
WRONG = {"status": 401, "detail": "Incorrect email or password"}

pytestmark = pytest.mark.usefixtures("no_login_throttle")


def _user(**fields: Any) -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username="demo",
        password_hash=fields.pop("password_hash", get_password_hash(PASSWORD)),
        created_at=datetime.now(timezone.utc),
        is_active=fields.pop("is_active", True),
        is_admin=False,
        is_deleted=False,
        **fields,
    )


def _session(user: User | None) -> MagicMock:
    result = MagicMock()
    result.scalars.return_value.first.return_value = user
    session = MagicMock()
    session.execute = AsyncMock(return_value=result)
    session.commit = AsyncMock()
    return session


def _client(session: MagicMock) -> TestClient:
    app = FastAPI()
    app.include_router(auth_router)

    async def override_db():
        yield session

    app.dependency_overrides[get_db] = override_db
    return TestClient(app)


def _login(user: User | None, password: str, *, swagger: bool = False) -> tuple[dict[str, Any], MagicMock]:
    session = _session(user)
    client = _client(session)
    if swagger:
        response = client.post("/auth/swagger-login", data={"username": "demo@example.com", "password": password})
    else:
        response = client.post("/auth/login", json={"email": "demo@example.com", "password": password})
    return {"status": response.status_code, "detail": response.json().get("detail")}, session


@pytest.mark.parametrize("swagger", [False, True])
def test_the_right_password_signs_in(swagger: bool) -> None:
    outcome, _ = _login(_user(), PASSWORD, swagger=swagger)
    assert outcome["status"] == 200


@pytest.mark.parametrize("swagger", [False, True])
@pytest.mark.parametrize(
    "user, password",
    [
        (None, PASSWORD),  # no account with this email
        ("active", "wrong-password"),
        ("inactive", "wrong-password"),  # used to answer "deactivated" here
        ("google", PASSWORD),  # account without a password
    ],
)
def test_every_failure_looks_the_same(user: str | None, password: str, swagger: bool) -> None:
    accounts = {
        None: None,
        "active": _user(),
        "inactive": _user(is_active=False),
        "google": _user(password_hash=UNUSABLE_PASSWORD_HASH),
    }
    outcome, _ = _login(accounts[user], password, swagger=swagger)
    assert outcome == WRONG


@pytest.mark.parametrize("swagger", [False, True])
def test_a_deactivated_account_is_named_only_with_the_right_password(swagger: bool) -> None:
    outcome, _ = _login(_user(is_active=False), PASSWORD, swagger=swagger)
    assert outcome == {"status": 403, "detail": "Your account is deactivated."}


def test_deleted_accounts_are_not_looked_up() -> None:
    _, session = _login(None, PASSWORD)
    sql = str(session.execute.await_args.args[0].compile(dialect=postgresql.dialect()))
    assert "users.is_deleted = false" in sql


@pytest.mark.parametrize("password_hash", [None, UNUSABLE_PASSWORD_HASH])
def test_a_hash_is_checked_even_without_a_real_one(monkeypatch, password_hash: str | None) -> None:
    checked: list[str] = []
    original = security._checkpw

    def spy(plain: str, hashed: str) -> bool:
        checked.append(hashed)
        return original(plain, hashed)

    monkeypatch.setattr(security, "_checkpw", spy)
    assert security.verify_password(PASSWORD, password_hash) is False
    assert checked == [security._DUMMY_PASSWORD_HASH]
