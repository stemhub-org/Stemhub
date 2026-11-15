"""Signing up requires accepting the Terms of Service and the Privacy
Policy, and records which versions were accepted (GDPR proof of consent).
Users without the current versions (Google sign-up, older accounts, a new
version) see terms_accepted: false in /auth/me and accept with
POST /auth/me/terms."""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from pydantic import ValidationError

from stemhub import auth
from stemhub.database import get_db
from stemhub.legal import CURRENT_VERSIONS, PRIVACY_POLICY_VERSION, TERMS_VERSION
from stemhub.models import User
from stemhub.schemas import UserCreate
from stemhub.validation_errors import install_validation_error_handler

SIGN_UP = {"email": "a@example.com", "username": "demo", "password": "a-strong-password"}


@pytest.mark.parametrize("accept_terms", [None, False])
def test_sign_up_requires_accepting_the_terms(accept_terms: bool | None) -> None:
    fields: dict[str, Any] = dict(SIGN_UP)
    if accept_terms is not None:
        fields["accept_terms"] = accept_terms
    with pytest.raises(ValidationError, match="You must accept the Terms of Service and the Privacy Policy"):
        UserCreate(**fields)


def test_the_refusal_reads_well_in_the_api() -> None:
    app = FastAPI()
    app.include_router(auth.router)
    install_validation_error_handler(app)
    response = TestClient(app).post("/auth/register", json=SIGN_UP)
    assert response.status_code == 422
    assert response.json()["detail"] == [
        {
            "type": "value_error",
            "loc": ["body", "accept_terms"],
            "msg": "You must accept the Terms of Service and the Privacy Policy",
        }
    ]


def test_both_documents_have_a_current_version() -> None:
    assert CURRENT_VERSIONS == {"terms": TERMS_VERSION, "privacy": PRIVACY_POLICY_VERSION}


def test_sign_up_records_the_accepted_versions() -> None:
    lookup = MagicMock()
    lookup.scalars.return_value.first.return_value = None
    lookup.first.return_value = None
    db = MagicMock()
    db.execute = AsyncMock(return_value=lookup)
    db.commit = AsyncMock()
    db.refresh = AsyncMock()

    app = FastAPI()
    app.include_router(auth.router)

    async def override_db():
        yield db

    app.dependency_overrides[get_db] = override_db
    # The response needs database defaults the fake session does not fill in.
    TestClient(app, raise_server_exceptions=False).post("/auth/register", json={**SIGN_UP, "accept_terms": True})

    created = db.add.call_args_list[0].args[0]
    assert created.email == "a@example.com"
    assert {(a.document, a.version) for a in created.legal_acceptances} == {
        ("terms", TERMS_VERSION),
        ("privacy", PRIVACY_POLICY_VERSION),
    }


# ── Accepting later (Google sign-up, older accounts, new versions) ──

def _user() -> User:
    return User(
        id=uuid.uuid4(), email="a@example.com", username="demo", password_hash="x",
        created_at=datetime.now(timezone.utc), is_active=True, is_admin=False,
    )


def _me_client(accepted: set[tuple[str, str]]) -> tuple[TestClient, MagicMock]:
    rows = MagicMock()
    rows.all.return_value = list(accepted)
    db = MagicMock()
    db.execute = AsyncMock(return_value=rows)
    db.commit = AsyncMock()
    app = FastAPI()
    app.include_router(auth.router)
    install_validation_error_handler(app)

    async def override_db():
        yield db

    user = _user()
    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[auth.get_current_user] = lambda: user
    return TestClient(app), db


CURRENT = {("terms", TERMS_VERSION), ("privacy", PRIVACY_POLICY_VERSION)}


@pytest.mark.parametrize(
    "accepted, expected",
    [
        (set(), False),  # Google sign-up or an account from before
        ({("terms", TERMS_VERSION)}, False),
        ({("terms", "2020-01-01"), ("privacy", "2020-01-01")}, False),  # older versions only
        (CURRENT, True),
        (CURRENT | {("terms", "2020-01-01")}, True),
    ],
)
def test_me_says_whether_the_current_versions_are_accepted(accepted, expected) -> None:
    client, _ = _me_client(accepted)
    body = client.get("/auth/me").json()
    assert body["terms_accepted"] is expected
    assert body["email"] == "a@example.com"


def _added(db: MagicMock) -> set[tuple[str, str]]:
    return {(c.args[0].document, c.args[0].version) for c in db.add.call_args_list}


def test_accepting_records_the_missing_current_versions() -> None:
    client, db = _me_client({("terms", TERMS_VERSION)})
    response = client.post("/auth/me/terms", json={"accept_terms": True})
    assert response.status_code == 200
    assert _added(db) == {("privacy", PRIVACY_POLICY_VERSION)}
    db.commit.assert_awaited_once()


def test_accepting_again_records_nothing() -> None:
    client, db = _me_client(CURRENT)
    assert client.post("/auth/me/terms", json={"accept_terms": True}).status_code == 200
    assert _added(db) == set()


@pytest.mark.parametrize("body", [{}, {"accept_terms": False}])
def test_accepting_must_be_explicit(body: dict[str, Any]) -> None:
    client, db = _me_client(set())
    response = client.post("/auth/me/terms", json=body)
    assert response.status_code == 422
    assert response.json()["detail"][0]["msg"] == "You must accept the Terms of Service and the Privacy Policy"
    db.add.assert_not_called()
