"""/auth/me says which external accounts (Google, ...) the user can sign in
with, so Settings shows the real state instead of a fixed "Connected"."""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from unittest.mock import AsyncMock, MagicMock

from fastapi import FastAPI
from fastapi.testclient import TestClient

from stemhub import auth
from stemhub.database import get_db
from stemhub.legal import PRIVACY_POLICY_VERSION, TERMS_VERSION
from stemhub.models import User


def _client(providers: list[str]) -> TestClient:
    acceptances = MagicMock()
    acceptances.all.return_value = [("terms", TERMS_VERSION), ("privacy", PRIVACY_POLICY_VERSION)]
    identities = MagicMock()
    identities.scalars.return_value.all.return_value = providers

    async def execute(statement, *args, **kwargs):
        # Answer each query with its own rows: the identity lookup reads
        # user_identity, the terms lookup reads the legal acceptances.
        return identities if "user_identity" in str(statement) else acceptances

    db = MagicMock()
    db.execute = AsyncMock(side_effect=execute)
    db.commit = AsyncMock()

    async def override_db():
        yield db

    user = User(
        id=uuid.uuid4(), email="a@example.com", username="demo", password_hash="x",
        created_at=datetime.now(timezone.utc), is_active=True, is_admin=False,
    )
    app = FastAPI()
    app.include_router(auth.router)
    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[auth.get_current_user] = lambda: user
    return TestClient(app)


def test_me_lists_the_linked_providers() -> None:
    body = _client(["google"]).get("/auth/me").json()
    assert body["linked_providers"] == ["google"]
    assert body["terms_accepted"] is True


def test_me_without_identity_lists_none() -> None:
    # An account created with an email and a password.
    assert _client([]).get("/auth/me").json()["linked_providers"] == []


def test_accepting_terms_returns_the_linked_providers() -> None:
    response = _client(["google"]).post("/auth/me/terms", json={"accept_terms": True})
    assert response.status_code == 200
    assert response.json()["linked_providers"] == ["google"]
