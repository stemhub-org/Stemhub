"""Emails are case-insensitive: they are stored and looked up in lowercase,
so Foo@x.com and foo@x.com are one account and either signs in."""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from sqlalchemy.dialects import postgresql

from stemhub.auth import router as auth_router
from stemhub.database import get_db
from stemhub.models import User
from stemhub.schemas import EmailChangeRequest, LoginRequest, UserCreate
from stemhub.security import get_password_hash


def _session(*rows: Any) -> MagicMock:
    def lookup(row: Any) -> MagicMock:
        result = MagicMock()
        result.scalars.return_value.first.return_value = row
        result.first.return_value = row
        return result

    session = MagicMock()
    session.execute = AsyncMock(side_effect=[lookup(row) for row in rows])
    session.commit = AsyncMock()
    session.refresh = AsyncMock()
    return session


def _client(session: MagicMock) -> TestClient:
    app = FastAPI()
    app.include_router(auth_router)

    async def override_db():
        yield session

    app.dependency_overrides[get_db] = override_db
    return TestClient(app)


def _bound_email(statement: Any) -> str:
    params = statement.compile(dialect=postgresql.dialect()).params
    return next(value for key, value in params.items() if key.startswith("email"))


def _user(email: str = "demo@example.com") -> User:
    return User(
        id=uuid.uuid4(),
        email=email,
        username="demo",
        password_hash=get_password_hash("a-strong-password"),
        created_at=datetime.now(timezone.utc),
        is_active=True,
        is_admin=False,
        is_deleted=False,
    )


@pytest.mark.parametrize("submitted", ["Demo@Example.com", "  DEMO@EXAMPLE.COM ", "demo@example.com"])
def test_submitted_emails_are_lowercased(submitted: str) -> None:
    assert UserCreate(email=submitted, username="demo", password="a-strong-password").email == "demo@example.com"
    assert LoginRequest(email=submitted, password="x").email == "demo@example.com"
    assert EmailChangeRequest(new_email=submitted).new_email == "demo@example.com"


def test_sign_up_refuses_an_email_that_differs_only_by_case() -> None:
    session = _session(_user())
    response = _client(session).post(
        "/auth/register",
        json={"email": "DEMO@example.com", "username": "other", "password": "a-strong-password"},
    )
    assert response.status_code == 400
    assert _bound_email(session.execute.await_args_list[0].args[0]) == "demo@example.com"


def test_login_works_whatever_the_capitals() -> None:
    session = _session(_user())
    response = _client(session).post(
        "/auth/login", json={"email": "Demo@Example.COM", "password": "a-strong-password"}
    )
    assert response.status_code == 200
    assert _bound_email(session.execute.await_args_list[0].args[0]) == "demo@example.com"


def test_swagger_login_works_whatever_the_capitals() -> None:
    session = _session(_user())
    response = _client(session).post(
        "/auth/swagger-login", data={"username": "Demo@Example.COM", "password": "a-strong-password"}
    )
    assert response.status_code == 200
    assert _bound_email(session.execute.await_args_list[0].args[0]) == "demo@example.com"

