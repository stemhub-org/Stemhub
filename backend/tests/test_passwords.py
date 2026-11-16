"""Password rules: at least 8 characters, at most 72 bytes (bcrypt's limit).

They apply when a password is chosen (sign-up, password change), not when
one is checked at login, so older accounts with shorter passwords can still
sign in.
"""
from __future__ import annotations

from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from pydantic import ValidationError

from stemhub.auth import router as auth_router
from stemhub.database import get_db
from stemhub.schemas import LoginRequest, PasswordChangeRequest, UserCreate
from stemhub.security import get_password_hash, verify_password

TOO_SHORT = "seven77"
SHORTEST = "eight888"
LONGEST = "x" * 72
# 36 two-byte characters: 36 characters but 72 bytes; one more is 74 bytes.
LONGEST_ACCENTED = "é" * 36
TOO_LONG = ["x" * 73, "é" * 37]


def _sign_up(password: str) -> UserCreate:
    return UserCreate(email="a@example.com", username="demo", password=password, accept_terms=True)


def _change(password: str) -> PasswordChangeRequest:
    return PasswordChangeRequest(current_password="anything", new_password=password)


@pytest.mark.parametrize("password", [SHORTEST, LONGEST, LONGEST_ACCENTED, "a passphrase with spaces"])
def test_valid_passwords_are_accepted(password: str) -> None:
    assert _sign_up(password).password == password
    assert _change(password).new_password == password


@pytest.mark.parametrize("password", ["", TOO_SHORT, *TOO_LONG])
def test_invalid_passwords_are_refused(password: str) -> None:
    with pytest.raises(ValidationError):
        _sign_up(password)
    with pytest.raises(ValidationError):
        _change(password)


def test_passwords_are_not_trimmed() -> None:
    assert _sign_up("  spaced  ").password == "  spaced  "


def test_the_longest_password_can_be_hashed() -> None:
    for password in [LONGEST, LONGEST_ACCENTED]:
        assert verify_password(password, get_password_hash(password))


def test_login_does_not_apply_the_rules() -> None:
    assert LoginRequest(email="a@example.com", password="short").password == "short"
    assert LoginRequest(email="a@example.com", password="x" * 100).password == "x" * 100


def test_a_too_long_password_at_sign_up_gets_422_not_500() -> None:
    session = MagicMock()
    session.execute = AsyncMock()
    app = FastAPI()
    app.include_router(auth_router)

    async def override_db():
        yield session

    app.dependency_overrides[get_db] = override_db
    response = TestClient(app).post(
        "/auth/register",
        json={"email": "a@example.com", "username": "demo", "password": "x" * 100, "accept_terms": True},
    )
    assert response.status_code == 422
    session.execute.assert_not_awaited()


def test_a_too_long_password_at_login_is_simply_wrong() -> None:
    assert verify_password("x" * 100, get_password_hash(LONGEST)) is False
