"""Signing up requires accepting the Terms of Service and the Privacy
Policy, and records which versions were accepted (GDPR proof of consent)."""
from __future__ import annotations

from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from pydantic import ValidationError

from stemhub import auth
from stemhub.database import get_db
from stemhub.legal import CURRENT_VERSIONS, PRIVACY_POLICY_VERSION, TERMS_VERSION
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
