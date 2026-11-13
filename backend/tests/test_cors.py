"""CORS headers the web app relies on when it calls the API from another origin."""
from __future__ import annotations

import importlib

from fastapi.testclient import TestClient

from stemhub import logging_config

FRONTEND_ORIGIN = "http://localhost:3000"


def _app(monkeypatch):
    # Importing stemhub.main configures logging for the whole process; keep the
    # test runner's log capture intact. No lifespan runs without a `with` block,
    # so no database is needed.
    monkeypatch.setattr(logging_config, "configure_logging", lambda: None)
    return importlib.import_module("stemhub.main").app


def test_cors_exposes_content_disposition_so_the_web_app_can_name_downloads(monkeypatch) -> None:
    # The preview download keeps the uploaded file's name, read from this header.
    client = TestClient(_app(monkeypatch))

    response = client.get("/health", headers={"Origin": FRONTEND_ORIGIN})

    assert response.status_code == 200
    assert response.headers["access-control-allow-origin"] == FRONTEND_ORIGIN
    exposed = {name.strip().lower() for name in response.headers.get("access-control-expose-headers", "").split(",")}
    assert "content-disposition" in exposed
