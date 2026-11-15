"""422 responses keep each error's location, type and message, never the
submitted value (a refused password used to come back in the response)."""
from __future__ import annotations

from fastapi import FastAPI
from fastapi.exceptions import RequestValidationError
from fastapi.testclient import TestClient

from stemhub.auth import router as auth_router
from stemhub.main import app as main_app
from stemhub.validation_errors import install_validation_error_handler

SECRET = "short"


def _client() -> TestClient:
    app = FastAPI()
    app.include_router(auth_router)
    install_validation_error_handler(app)
    return TestClient(app)


def _register(client: TestClient, **body):
    return client.post("/auth/register", json={"email": "a@example.com", "username": "demo", **body})


def test_a_refused_password_is_not_echoed() -> None:
    response = _register(_client(), password=SECRET)
    assert response.status_code == 422
    assert SECRET not in response.text
    assert response.json()["detail"] == [
        {"type": "value_error", "loc": ["body", "password"], "msg": "Password must be at least 8 characters"}
    ]


def test_built_in_errors_keep_their_message() -> None:
    response = _client().post("/auth/register", json={"email": "not-an-email", "username": "demo", "password": "a-strong-password"})
    assert response.status_code == 422
    assert "not-an-email" not in response.text
    [error] = response.json()["detail"]
    assert error["loc"] == ["body", "email"]
    assert set(error) == {"type", "loc", "msg"}


def test_missing_fields_are_still_reported() -> None:
    response = _client().post("/auth/register", json={})
    assert response.status_code == 422
    assert {tuple(error["loc"]) for error in response.json()["detail"]} == {
        ("body", "email"), ("body", "username"), ("body", "password"),
    }


def test_the_app_uses_the_handler() -> None:
    assert RequestValidationError in main_app.exception_handlers
