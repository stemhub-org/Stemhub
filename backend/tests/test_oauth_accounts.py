"""Google sign-in: users are found by their Google account id, accounts it
creates have no usable password, and it only accepts emails Google has
verified.

They used to get the password quote(email) + "oauth_dummy", so knowing a
Google user's email was enough to log in as them with /auth/login.
"""
from __future__ import annotations

import urllib.parse
import uuid
from pathlib import Path
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import bcrypt
import jwt
import pytest
from alembic.config import Config
from alembic.script import ScriptDirectory
from fastapi import FastAPI
from fastapi.testclient import TestClient

from stemhub import auth as auth_module
from stemhub.database import get_db
from stemhub.models import User, UserIdentity
from stemhub.security import ALGORITHM, SECRET_KEY, UNUSABLE_PASSWORD_HASH, get_password_hash, verify_password

BACKEND_DIR = Path(__file__).resolve().parents[1]
LOCK_REVISION = "2e0e729975d1"


def _lock_migration():
    config = Config(str(BACKEND_DIR / "alembic.ini"))
    config.set_main_option("script_location", str(BACKEND_DIR / "alembic"))
    return ScriptDirectory.from_config(config).get_revision(LOCK_REVISION).module


def _old_oauth_hash(email: str) -> str:
    return bcrypt.hashpw((urllib.parse.quote(email) + "oauth_dummy").encode(), bcrypt.gensalt(4)).decode()


def test_no_password_matches_the_unusable_hash() -> None:
    for password in ["", "!", "a-password", "demo%40example.comoauth_dummy"]:
        assert verify_password(password, UNUSABLE_PASSWORD_HASH) is False


def test_regular_passwords_still_verify() -> None:
    password_hash = get_password_hash("a-strong-password")
    assert verify_password("a-strong-password", password_hash) is True
    assert verify_password("wrong", password_hash) is False


def test_the_migration_finds_old_google_passwords() -> None:
    migration = _lock_migration()
    assert migration._has_guessable_password("demo@example.com", _old_oauth_hash("demo@example.com"))


def test_the_migration_leaves_chosen_passwords_alone() -> None:
    migration = _lock_migration()
    chosen = bcrypt.hashpw(b"a-strong-password", bcrypt.gensalt(4)).decode()
    assert not migration._has_guessable_password("demo@example.com", chosen)
    assert not migration._has_guessable_password("demo@example.com", UNUSABLE_PASSWORD_HASH)


def test_the_migration_uses_the_current_unusable_hash() -> None:
    assert _lock_migration().UNUSABLE_PASSWORD_HASH == UNUSABLE_PASSWORD_HASH


# ── Google callback ──

class FakeGoogle:
    """Stands in for httpx.AsyncClient during the Google code exchange."""

    def __init__(self, user_info: dict[str, Any]) -> None:
        self.user_info = user_info

    def __call__(self, *args: Any, **kwargs: Any) -> "FakeGoogle":
        return self

    async def __aenter__(self) -> "FakeGoogle":
        return self

    async def __aexit__(self, *exc: Any) -> None:
        return None

    async def post(self, *args: Any, **kwargs: Any) -> MagicMock:
        return MagicMock(json=lambda: {"access_token": "google-token"})

    async def get(self, *args: Any, **kwargs: Any) -> MagicMock:
        return MagicMock(json=lambda: self.user_info)


def _callback(monkeypatch, user_info: dict[str, Any], *rows: Any):
    """Run the Google callback; `rows` answer its database lookups in order
    (Google id, then email, then username availability). None = no match."""
    monkeypatch.setattr(auth_module, "GOOGLE_CLIENT_ID", "client-id")
    monkeypatch.setattr(auth_module, "GOOGLE_CLIENT_SECRET", "client-secret")
    monkeypatch.setattr(auth_module.httpx, "AsyncClient", FakeGoogle(user_info))

    def lookup(row: Any) -> MagicMock:
        result = MagicMock()
        result.scalars.return_value.first.return_value = row
        result.first.return_value = row
        return result

    session = MagicMock()
    session.execute = AsyncMock(side_effect=[lookup(row) for row in rows])
    session.commit = AsyncMock()
    session.refresh = AsyncMock()

    app = FastAPI()
    app.include_router(auth_module.router)

    async def override_db():
        yield session

    app.dependency_overrides[get_db] = override_db
    client = TestClient(app)
    client.cookies.set("oauth_state", "state")
    response = client.get("/auth/callback/google?code=code&state=state", follow_redirects=False)
    return response, session


def _google_user(**overrides: Any) -> dict[str, Any]:
    return {"id": "109876543210", "email": "demo@example.com", "name": "Demo User", "verified_email": True, **overrides}


def _existing_user(email: str = "demo@example.com", **fields: Any) -> User:
    fields.setdefault("is_active", True)
    fields.setdefault("is_deleted", False)
    return User(id=uuid.uuid4(), email=email, username="demo", password_hash="x", **fields)


def _token_email(response) -> str:
    token = response.headers["location"].split("token=")[1]
    return jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])["sub"]


@pytest.mark.parametrize("verified", [False, None, "true"])
def test_google_sign_in_with_an_unverified_email_is_refused(monkeypatch, verified) -> None:
    info = _google_user(verified_email=verified)
    if verified is None:
        del info["verified_email"]
    response, session = _callback(monkeypatch, info, _existing_user())
    assert response.status_code == 400
    assert response.json()["detail"] == "Your Google email address is not verified"
    session.commit.assert_not_awaited()


def test_google_sign_in_without_an_account_id_is_refused(monkeypatch) -> None:
    info = _google_user()
    del info["id"]
    response, _ = _callback(monkeypatch, info)
    assert response.status_code == 400


def test_a_linked_google_account_signs_in_by_its_id_not_its_email(monkeypatch) -> None:
    # The Google account's email changed since it was linked.
    user = _existing_user(email="old@example.com")
    response, session = _callback(monkeypatch, _google_user(email="new@example.com"), user)
    assert response.status_code == 307
    assert _token_email(response) == "old@example.com"
    assert session.execute.await_count == 1
    session.add.assert_not_called()


def test_a_first_google_sign_in_links_the_account_with_the_same_email(monkeypatch) -> None:
    user = _existing_user()
    response, session = _callback(monkeypatch, _google_user(), None, user)
    assert response.status_code == 307
    assert _token_email(response) == "demo@example.com"
    identity = session.add.call_args.args[0]
    assert isinstance(identity, UserIdentity)
    assert (identity.user_id, identity.provider, identity.provider_user_id) == (user.id, "google", "109876543210")
    session.commit.assert_awaited_once()


def test_google_sign_up_creates_an_account_without_a_usable_password(monkeypatch) -> None:
    response, session = _callback(monkeypatch, _google_user(), None, None, None)
    assert response.status_code == 307
    created = session.add.call_args.args[0]
    assert created.email == "demo@example.com"
    assert created.password_hash == UNUSABLE_PASSWORD_HASH
    assert [(i.provider, i.provider_user_id) for i in created.identities] == [("google", "109876543210")]


@pytest.mark.parametrize("state", [{"is_active": False}, {"is_deleted": True}])
@pytest.mark.parametrize("linked", [True, False])
def test_google_sign_in_does_not_bring_back_a_deactivated_or_deleted_account(monkeypatch, state, linked) -> None:
    user = _existing_user(**state)
    rows = (user,) if linked else (None, user)
    response, session = _callback(monkeypatch, _google_user(), *rows)
    assert response.status_code == 403
    assert response.json()["detail"] == "Your account is deactivated."
    assert user.is_active == state.get("is_active", True)
    session.add.assert_not_called()
    session.commit.assert_not_awaited()
