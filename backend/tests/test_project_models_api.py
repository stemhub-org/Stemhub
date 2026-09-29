"""POST /api/project-models/validate: the backend's input boundary for project models (#170).

Signed-in users only. Bodies are capped (Content-Length and the bytes actually
streamed) and at most two validations run at once (429 otherwise).
200 {valid, schema_version} or 422 {valid: false, errors, truncated}; errors
never echo the submitted values or keys.
"""
from __future__ import annotations

import asyncio
import importlib
import json
import threading
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterator

import httpx
import pytest
from fastapi import FastAPI, HTTPException
from fastapi.testclient import TestClient
from starlette.requests import Request

from stemhub import logging_config
from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import User
from stemhub.routers import project_models as project_models_module
from stemhub.routers.project_models import router as project_models_router

EXAMPLES_DIR = Path(__file__).parent / "fixtures" / "project_model" / "examples"
URL = "/api/project-models/validate"


def _example(name: str = "basic-beat.json") -> dict[str, Any]:
    return json.loads((EXAMPLES_DIR / name).read_text(encoding="utf-8"))


def _build_user() -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username="demo",
        password_hash="hashed-password",
        created_at=datetime.now(timezone.utc),
        is_active=True,
    )


def _app(*, signed_in: bool = True) -> FastAPI:
    app = FastAPI()
    app.include_router(project_models_router)

    async def override_db():
        yield object()

    async def override_current_user():
        return _build_user()

    app.dependency_overrides[get_db] = override_db
    if signed_in:
        app.dependency_overrides[get_current_user] = override_current_user
    return app


def _client(*, signed_in: bool = True) -> TestClient:
    return TestClient(_app(signed_in=signed_in))


def _post_json(client: TestClient, document: Any):
    return client.post(URL, content=json.dumps(document), headers={"Content-Type": "application/json"})


# ── Valid documents ──


@pytest.mark.parametrize("name", ["minimal.json", "basic-beat.json", "fl2025-no-tempo.json"])
def test_a_valid_document_is_accepted(name: str) -> None:
    response = _post_json(_client(), _example(name))

    assert response.status_code == 200
    assert response.json() == {"valid": True, "schema_version": "0.1.0"}


def test_signing_in_is_required() -> None:
    response = _post_json(_client(signed_in=False), _example())

    assert response.status_code == 401


def test_signing_in_is_checked_before_the_body_is_read(monkeypatch) -> None:
    monkeypatch.setattr(project_models_module, "MAX_PROJECT_MODEL_BYTES", 16)

    response = _post_json(_client(signed_in=False), _example())

    assert response.status_code == 401


# ── Invalid documents ──


def test_a_structurally_invalid_document_gets_422_with_located_errors() -> None:
    document = _example()
    document["patterns"][0]["notes"][0]["pitch"] = 200
    document["instruments"][1]["plugin"]["format"] = "clap"

    response = _post_json(_client(), document)

    assert response.status_code == 422
    body = response.json()
    assert body["valid"] is False
    assert body["truncated"] is False
    assert sorted(error["path"] for error in body["errors"]) == [
        "/instruments/1/plugin/format",
        "/patterns/0/notes/0/pitch",
    ]
    for error in body["errors"]:
        assert set(error) == {"path", "code", "message"}


def test_reference_errors_get_422_too() -> None:
    document = _example()
    document["arrangements"][0]["tracks"][0]["clips"][0]["source_id"] = "pattern:9"

    response = _post_json(_client(), document)

    assert response.status_code == 422
    assert response.json()["errors"] == [
        {
            "path": "/arrangements/0/tracks/0/clips/0/source_id",
            "code": "dangling_reference",
            "message": "The clip's source is not one of the patterns.",
        }
    ]


def test_errors_never_echo_the_submitted_values() -> None:
    secret = "C:/Users/alice/Secret project/kick.wav"
    document = _example()
    document["instruments"][0]["sample"]["asset_path"] = secret
    document["instruments"][0]["name"] = secret * 10
    document["arrangements"][0]["tracks"][0]["clips"][0]["kind"] = secret
    document["source"]["daw"] = secret

    response = _post_json(_client(), document)

    assert response.status_code == 422
    assert "alice" not in response.text
    for error in response.json()["errors"]:
        assert "input" not in error and "ctx" not in error


def test_errors_never_echo_the_submitted_keys() -> None:
    secret = "SECRETVALUE-xyz"
    document = _example()
    document["project"][secret] = secret
    document["extensions"] = {secret: {}}
    document["patterns"][0]["notes"][0]["extensions"] = {"secretvalue_xyz": secret}

    response = _post_json(_client(), document)

    assert response.status_code == 422
    assert "secretvalue" not in response.text.lower()
    assert [error["path"] for error in response.json()["errors"]] == [
        "/project/<unknown field>",
        "/patterns/0/notes/0/extensions/<namespace>",
        "/extensions/<namespace>",
    ]


@pytest.mark.parametrize(
    ("section", "field", "value"),
    [("time_signature", "denominator", 4.0), ("time_signature", "denominator", 1.0), ("source", "ppq", 96.0)],
)
def test_integers_written_as_floats_are_refused(section: str, field: str, value: float) -> None:
    document = _example()
    document[section][field] = value

    response = _post_json(_client(), document)

    assert response.status_code == 422
    assert [(error["path"], error["code"]) for error in response.json()["errors"]] == [
        (f"/{section}/{field}", "int_type")
    ]


def test_the_number_of_returned_errors_is_capped(monkeypatch) -> None:
    monkeypatch.setattr(project_models_module, "MAX_REPORTED_ERRORS", 2)
    document = _example()
    document["instruments"][0]["pan"] = 5
    document["audio_sources"][0]["name"] = 5
    document["patterns"][0]["name"] = 5

    response = _post_json(_client(), document)

    assert response.status_code == 422
    body = response.json()
    assert [error["path"] for error in body["errors"]] == ["/instruments/0/pan", "/audio_sources/0/name"]
    assert body["truncated"] is True


def test_at_most_100_errors_are_returned() -> None:
    assert project_models_module.MAX_REPORTED_ERRORS == 100


@pytest.mark.parametrize(
    "body",
    [b'{"schema": ', b"\xff\xfe\x00", b"", b'{"tempo_bpm": NaN}'],
    ids=["truncated", "not utf-8", "empty", "NaN"],
)
def test_a_body_that_is_not_a_valid_json_document_gets_422(body: bytes) -> None:
    response = _client().post(URL, content=body, headers={"Content-Type": "application/json"})

    assert response.status_code == 422
    assert response.json()["valid"] is False


def test_invalid_json_is_one_error_at_the_document_root() -> None:
    response = _client().post(URL, content=b"[1, 2", headers={"Content-Type": "application/json"})

    assert response.status_code == 422
    assert [(error["path"], error["code"]) for error in response.json()["errors"]] == [("", "json_invalid")]


# ── Size cap ──


def test_a_body_declared_larger_than_the_cap_gets_413(monkeypatch) -> None:
    monkeypatch.setattr(project_models_module, "MAX_PROJECT_MODEL_BYTES", 1024)
    body = json.dumps(_example()).encode()
    assert len(body) > 1024

    response = _client().post(URL, content=body, headers={"Content-Type": "application/json"})

    assert response.status_code == 413


def test_a_streamed_body_larger_than_the_cap_gets_413_without_a_content_length(monkeypatch) -> None:
    monkeypatch.setattr(project_models_module, "MAX_PROJECT_MODEL_BYTES", 1024)
    body = json.dumps(_example()).encode()

    def chunks() -> Iterator[bytes]:
        for start in range(0, len(body), 256):
            yield body[start : start + 256]

    response = _client().post(URL, content=chunks(), headers={"Content-Type": "application/json"})

    assert response.status_code == 413


def test_a_streamed_body_within_the_cap_is_validated() -> None:
    body = json.dumps(_example()).encode()

    def chunks() -> Iterator[bytes]:
        for start in range(0, len(body), 4096):
            yield body[start : start + 4096]

    response = _client().post(URL, content=chunks(), headers={"Content-Type": "application/json"})

    assert response.status_code == 200


def test_the_cap_is_10_mib() -> None:
    assert project_models_module.MAX_PROJECT_MODEL_BYTES == 10 * 1024 * 1024


def _request(headers: list[tuple[bytes, bytes]], chunks: list[bytes]) -> Request:
    messages = [{"type": "http.request", "body": chunk, "more_body": True} for chunk in chunks]
    messages.append({"type": "http.request", "body": b"", "more_body": False})

    async def receive() -> dict[str, Any]:
        return messages.pop(0)

    scope = {"type": "http", "method": "POST", "path": URL, "headers": headers, "query_string": b""}
    return Request(scope, receive)


@pytest.mark.parametrize("content_length", [b"abc", b"-1", b"1.5", "\u0661".encode("utf-8")])
def test_an_invalid_content_length_gets_400(content_length: bytes) -> None:
    request = _request([(b"content-length", content_length)], [b"{}"])

    with pytest.raises(HTTPException) as caught:
        asyncio.run(project_models_module.read_capped_body(request, limit=1024))

    assert caught.value.status_code == 400


def test_a_body_longer_than_its_content_length_still_stops_at_the_cap() -> None:
    request = _request([(b"content-length", b"10")], [b"x" * 600, b"x" * 600])

    with pytest.raises(HTTPException) as caught:
        asyncio.run(project_models_module.read_capped_body(request, limit=1024))

    assert caught.value.status_code == 413


def test_a_body_at_the_cap_is_read_whole() -> None:
    request = _request([(b"content-length", b"1024")], [b"x" * 512, b"x" * 512])

    body = asyncio.run(project_models_module.read_capped_body(request, limit=1024))

    assert body == b"x" * 1024


# ── Concurrent validations ──


def test_at_most_2_validations_run_at_once() -> None:
    assert project_models_module.MAX_CONCURRENT_VALIDATIONS == 2
    # The route's own semaphore, not just the constant, holds 2 slots.
    assert project_models_module._validation_slots._value == 2


def test_a_validation_while_every_slot_is_busy_gets_429_at_once(monkeypatch) -> None:
    monkeypatch.setattr(project_models_module, "_validation_slots", asyncio.Semaphore(0))

    def must_not_validate(*args: Any, **kwargs: Any) -> None:
        raise AssertionError("validated while every slot was busy")

    monkeypatch.setattr(project_models_module, "validate_json", must_not_validate)

    # Without the 429 guard the request would wait on the semaphore forever, so
    # it runs in a thread and a timeout turns that into a failure.
    responses: list[Any] = []
    request = threading.Thread(
        target=lambda: responses.append(_post_json(_client(), _example())), daemon=True
    )
    request.start()
    request.join(timeout=10)
    assert not request.is_alive(), "the request waited for a slot instead of answering 429"
    response = responses[0]

    assert response.status_code == 429
    assert response.json() == {"detail": "Too many validations in progress, retry shortly"}
    assert response.headers["retry-after"] == "1"


def test_a_third_validation_while_two_run_gets_429(monkeypatch) -> None:
    monkeypatch.setattr(project_models_module, "_validation_slots", asyncio.Semaphore(2))
    started = threading.Semaphore(0)
    finish = threading.Event()
    validate_json = project_models_module.validate_json

    def slow_validate_json(body: bytes, **kwargs: Any):
        started.release()
        finish.wait(timeout=10)
        return validate_json(body, **kwargs)

    monkeypatch.setattr(project_models_module, "validate_json", slow_validate_json)
    body = json.dumps(_example())
    headers = {"Content-Type": "application/json"}

    def both_started() -> bool:
        return all(started.acquire(timeout=10) for _ in range(2))

    async def three_requests() -> list[int]:
        transport = httpx.ASGITransport(app=_app())
        async with httpx.AsyncClient(transport=transport, base_url="http://testserver") as client:
            running = [asyncio.create_task(client.post(URL, content=body, headers=headers)) for _ in range(2)]
            assert await asyncio.to_thread(both_started)
            third = await client.post(URL, content=body, headers=headers)
            finish.set()
            return [(await request).status_code for request in running] + [third.status_code]

    try:
        assert asyncio.run(three_requests()) == [200, 200, 429]
    finally:
        finish.set()


def test_a_validation_frees_its_slot_when_it_ends(monkeypatch) -> None:
    slots = asyncio.Semaphore(1)
    monkeypatch.setattr(project_models_module, "_validation_slots", slots)
    client = _client()
    invalid = _example()
    invalid["tempo_bpm"] = -1

    assert _post_json(client, _example()).status_code == 200
    assert _post_json(client, invalid).status_code == 422
    assert _post_json(client, _example()).status_code == 200
    assert not slots.locked()


def test_a_validation_that_fails_frees_its_slot(monkeypatch) -> None:
    slots = asyncio.Semaphore(1)
    monkeypatch.setattr(project_models_module, "_validation_slots", slots)

    def crash(*args: Any, **kwargs: Any) -> None:
        raise RuntimeError("validation crashed")

    monkeypatch.setattr(project_models_module, "validate_json", crash)

    with pytest.raises(RuntimeError):
        _post_json(_client(), _example())
    assert not slots.locked()


# ── Registration ──


def test_the_route_is_registered_on_the_app(monkeypatch) -> None:
    monkeypatch.setattr(logging_config, "configure_logging", lambda: None)
    app = importlib.import_module("stemhub.main").app

    # Through OpenAPI: newer FastAPI versions nest included routers in app.routes.
    assert "post" in app.openapi()["paths"][URL]


def test_the_route_is_documented_in_openapi() -> None:
    client = _client()
    operation = client.get("/openapi.json").json()["paths"][URL]["post"]

    assert {"200", "413", "422", "429"} <= set(operation["responses"])

