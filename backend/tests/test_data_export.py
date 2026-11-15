"""GET /auth/me/export: everything StemHub holds about the account as a
JSON download (GDPR right of access and portability), never a secret."""
from __future__ import annotations

import asyncio
import json
import uuid
from datetime import datetime, timedelta, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

from fastapi import FastAPI
from fastapi.testclient import TestClient

from stemhub import auth
from stemhub.data_export import export_user_data
from stemhub.database import get_db
from stemhub.models import User
from stemhub.security import UNUSABLE_PASSWORD_HASH, get_password_hash

SECTIONS = {
    "format_version", "exported_at", "account", "linked_accounts", "terms_acceptances", "sessions",
    "sign_in_attempts", "projects", "contributions", "following", "followers", "challenges", "events",
}


def _user(**fields: Any) -> User:
    return User(
        id=uuid.uuid4(), email="jb@example.com", username=fields.pop("username", "jb"),
        password_hash=fields.pop("password_hash", get_password_hash("a-strong-password")),
        bio="Beats at night", created_at=datetime.now(timezone.utc), is_active=True, is_admin=False, **fields,
    )


def _empty_db() -> MagicMock:
    """Answers every query with no rows: an account with nothing else yet."""
    result = MagicMock()
    result.scalars.return_value.all.return_value = []
    result.all.return_value = []
    db = MagicMock()
    db.execute = AsyncMock(return_value=result)
    db.commit = AsyncMock()
    return db


def test_the_export_has_every_section_and_the_profile() -> None:
    data = asyncio.run(export_user_data(_empty_db(), _user()))
    assert set(data) == SECTIONS
    assert data["account"]["email"] == "jb@example.com"
    assert data["account"]["bio"] == "Beats at night"
    assert data["account"]["has_password"] is True
    json.dumps(data)  # Only JSON values.


def test_the_export_holds_no_secret() -> None:
    user = _user()
    text = json.dumps(asyncio.run(export_user_data(_empty_db(), user)))
    assert "password_hash" not in text
    assert user.password_hash not in text


def test_a_google_only_account_says_it_has_no_password() -> None:
    data = asyncio.run(export_user_data(_empty_db(), _user(password_hash=UNUSABLE_PASSWORD_HASH)))
    assert data["account"]["has_password"] is False


def _client(user: User, db: MagicMock) -> TestClient:
    app = FastAPI()
    app.include_router(auth.router)

    async def override_db():
        yield db

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[auth.get_current_user] = lambda: user
    return TestClient(app)


def test_the_export_downloads_as_a_dated_json_file(monkeypatch) -> None:
    limits: list[dict[str, Any]] = []

    async def limiter(db, **kwargs):
        limits.append(kwargs)

    monkeypatch.setattr(auth, "enforce_rate_limit", limiter)
    user = _user(username="Hubert Touraine")
    response = _client(user, _empty_db()).get("/auth/me/export")

    assert response.status_code == 200
    today = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    assert response.headers["content-disposition"] == f'attachment; filename="stemhub-data-Hubert_Touraine-{today}.json"'
    assert response.json()["account"]["username"] == "Hubert Touraine"
    assert limits == [{
        "action": "export", "key": str(user.id), "limit": 5,
        "window": timedelta(hours=1), "message": "Too many data exports.",
    }]
