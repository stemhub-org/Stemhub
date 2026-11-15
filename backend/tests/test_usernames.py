"""Usernames are unique regardless of case, whitespace or Unicode form.

The schemas normalize the submitted name, the auth endpoints compare it
case-insensitively against existing users, and the unique index
uq_users_username_lower is the guarantee under concurrent requests.
"""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from pydantic import ValidationError
from sqlalchemy.dialects import postgresql
from sqlalchemy.exc import IntegrityError

from stemhub.auth import get_current_user, router as auth_router
from stemhub.database import get_db
from stemhub.models import User
from stemhub.schemas import UserCreate, UserUpdate


def _build_user(username: str = "demo") -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username=username,
        password_hash="hashed-password",
        avatar_url=None,
        created_at=datetime.now(timezone.utc),
        is_active=True,
        is_admin=False,
    )


def _result(row: Any) -> MagicMock:
    result = MagicMock()
    result.first.return_value = row
    result.scalars.return_value.first.return_value = row
    return result


class FakeSession:
    """Answers each `execute` with the next queued row (None = no match)."""

    def __init__(self, *rows: Any, commit_error: Exception | None = None) -> None:
        self.rows = list(rows)
        self.statements: list[Any] = []
        self.commit = AsyncMock(side_effect=commit_error)
        self.rollback = AsyncMock()
        self.refresh = AsyncMock(side_effect=self._refresh)
        self.added: list[Any] = []

    async def execute(self, statement: Any) -> MagicMock:
        self.statements.append(statement)
        return _result(self.rows.pop(0))

    def add(self, obj: Any) -> None:
        self.added.append(obj)

    @staticmethod
    async def _refresh(obj: Any) -> None:
        obj.id = obj.id or uuid.uuid4()
        obj.created_at = obj.created_at or datetime.now(timezone.utc)
        obj.is_active = True if obj.is_active is None else obj.is_active
        obj.is_admin = bool(obj.is_admin)


def _client(session: FakeSession, current_user: User | None = None) -> TestClient:
    app = FastAPI()
    app.include_router(auth_router)

    async def override_db():
        yield session

    app.dependency_overrides[get_db] = override_db
    if current_user is not None:
        app.dependency_overrides[get_current_user] = lambda: current_user
    return TestClient(app)


def _sql(statement: Any) -> str:
    return str(statement.compile(dialect=postgresql.dialect()))


def _register(client: TestClient, username: str):
    return client.post(
        "/auth/register",
        json={"email": "new@example.com", "username": username, "password": "a-strong-password"},
    )


def _unique_violation(constraint: str) -> IntegrityError:
    return IntegrityError("INSERT", {}, Exception(f'duplicate key value violates unique constraint "{constraint}"'))


# ── Normalization ──

@pytest.mark.parametrize(
    ("submitted", "stored"),
    [
        ("  hubert  ", "hubert"),
        ("Hubert   Touraine", "Hubert Touraine"),
        ("Ｈｕｂｅｒｔ", "Hubert"),  # fullwidth letters
    ],
)
def test_usernames_are_normalized(submitted: str, stored: str) -> None:
    assert UserCreate(email="a@example.com", username=submitted, password="x").username == stored
    assert UserUpdate(username=submitted).username == stored


@pytest.mark.parametrize("username", ["", "   ", "ab", "x" * 33, "bad​name", "bad\x00name"])
def test_invalid_usernames_are_refused(username: str) -> None:
    with pytest.raises(ValidationError):
        UserCreate(email="a@example.com", username=username, password="x")
    with pytest.raises(ValidationError):
        UserUpdate(username=username)


def test_username_is_optional_on_update() -> None:
    assert UserUpdate(bio="hi").username is None


# ── Registration ──

def test_register_compares_usernames_case_insensitively() -> None:
    session = FakeSession(None, None)
    assert _register(_client(session), "Hubert").status_code == 200
    assert "lower(users.username) = lower(" in _sql(session.statements[1])


def test_register_refuses_a_username_that_differs_only_by_case() -> None:
    session = FakeSession(None, uuid.uuid4())
    response = _register(_client(session), "HUBERT")
    assert response.status_code == 400
    assert response.json()["detail"] == "Username already taken"
    assert session.added == []


def test_register_stores_the_normalized_username() -> None:
    session = FakeSession(None, None)
    assert _register(_client(session), "  Hubert   T ").json()["username"] == "Hubert T"


def test_a_concurrent_duplicate_username_gets_400_not_500() -> None:
    session = FakeSession(None, None, commit_error=_unique_violation("uq_users_username_lower"))
    response = _register(_client(session), "hubert")
    assert response.status_code == 400
    assert response.json()["detail"] == "Username already taken"
    session.rollback.assert_awaited_once()


def test_a_concurrent_duplicate_email_gets_400_not_500() -> None:
    session = FakeSession(None, None, commit_error=_unique_violation("ix_users_email"))
    response = _register(_client(session), "hubert")
    assert response.status_code == 400
    assert response.json()["detail"] == "Email already registered"


# ── Profile update ──

def test_update_refuses_another_users_name_in_a_different_case() -> None:
    session = FakeSession(uuid.uuid4())
    response = _client(session, _build_user("demo")).put("/auth/me", json={"username": "HUBERT"})
    assert response.status_code == 400
    assert response.json()["detail"] == "Username already taken"


def test_update_lets_users_change_the_case_of_their_own_name() -> None:
    user = _build_user("demo")
    session = FakeSession(None)
    response = _client(session, user).put("/auth/me", json={"username": "Demo"})
    assert response.status_code == 200
    assert response.json()["username"] == "Demo"
    assert "users.id != " in _sql(session.statements[0])
