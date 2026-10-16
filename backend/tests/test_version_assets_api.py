"""Tests for GET /versions/{id}/assets.

Reads only from `Version.manifest_json` (spec §7 CAS), in either manifest
version: v1 (legacy) lists assets under `tracks`/`filename`, v2 under
`assets`/`path`. Pre-manifest versions yield an empty list.
"""
from __future__ import annotations

import uuid
from datetime import datetime, timezone

from fastapi import FastAPI, HTTPException
from fastapi.testclient import TestClient

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import User, Version
from stemhub.routers import versions as versions_router_module
from stemhub.routers.versions import router as versions_router


def _build_user() -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username="demo",
        password_hash="hashed-password",
        created_at=datetime.now(timezone.utc),
        is_active=True,
    )


def _build_version(*, manifest_json: dict | None = None) -> Version:
    return Version(
        id=uuid.uuid4(),
        branch_id=uuid.uuid4(),
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        manifest_json=manifest_json,
    )


def _create_test_client(*, monkeypatch, current_user: User, version: Version):
    app = FastAPI()
    app.include_router(versions_router)

    async def override_db():
        yield object()

    async def override_current_user():
        return current_user

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_current_user] = override_current_user

    async def fake_get_version_with_access(*, version_id, current_user, db):
        del current_user, db
        if version_id != version.id:
            raise HTTPException(status_code=404, detail="Version not found")
        return version

    monkeypatch.setattr(versions_router_module, "_get_version_with_access", fake_get_version_with_access)

    return TestClient(app)


def test_list_assets_from_a_v2_manifest(monkeypatch) -> None:
    version = _build_version(
        manifest_json={
            "manifest_version": 2,
            "source_daw": "FL Studio",
            "project_file": {"sha256": "f" * 64, "size_bytes": 99, "path": "Song.flp"},
            "assets": [
                {"sha256": "a" * 64, "size_bytes": 1024, "path": "Samples/Drums/Kick 01.wav"},
                {"sha256": "b" * 64, "size_bytes": 12, "path": "Melody.mid"},
            ],
        }
    )
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{version.id}/assets")

    assert response.status_code == 200
    assert response.json() == [
        {
            "id": f"{'a' * 64}:0",
            "path": "Samples/Drums/Kick 01.wav",
            "name": "Kick 01",
            "file_type": "wav",
            "size_bytes": 1024,
        },
        {
            "id": f"{'b' * 64}:1",
            "path": "Melody.mid",
            "name": "Melody",
            "file_type": "mid",
            "size_bytes": 12,
        },
    ]


def test_list_assets_from_a_legacy_v1_manifest(monkeypatch) -> None:
    version = _build_version(
        manifest_json={
            "manifest_version": 1,
            "tracks": [
                {
                    "sha256": "a" * 64,
                    "name": "Kick",
                    "filename": "kick.wav",
                    "size_bytes": 1024,
                    "bpm": 128,
                    "key": "Cm",
                    "duration_seconds": 12,
                },
                # Malformed entries are skipped rather than crashing the endpoint.
                {"sha256": "not a name"},
            ],
        }
    )
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{version.id}/assets")

    assert response.status_code == 200
    assert response.json() == [
        {
            "id": f"{'a' * 64}:0",
            "path": "kick.wav",
            "name": "Kick",
            "file_type": "wav",
            "size_bytes": 1024,
        }
    ]


def test_list_assets_ids_unique_even_with_duplicate_blob(monkeypatch) -> None:
    # Two assets at different paths may intentionally hold identical audio;
    # they must not collide on `id`.
    shared_sha = "e" * 64
    version = _build_version(
        manifest_json={
            "manifest_version": 2,
            "project_file": {"sha256": "f" * 64, "size_bytes": 1, "path": "Song.flp"},
            "assets": [
                {"sha256": shared_sha, "size_bytes": 1, "path": "Samples/kick.wav"},
                {"sha256": shared_sha, "size_bytes": 1, "path": "Samples/kick (copy).wav"},
            ],
        }
    )
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{version.id}/assets")

    assert response.status_code == 200
    payload = response.json()
    assert len(payload) == 2
    assert payload[0]["id"] != payload[1]["id"]
    assert {asset["path"] for asset in payload} == {"Samples/kick.wav", "Samples/kick (copy).wav"}


def test_list_assets_ids_keep_the_position_in_the_stored_list(monkeypatch) -> None:
    # A skipped malformed entry must not shift the ids of the assets after it.
    version = _build_version(
        manifest_json={
            "manifest_version": 2,
            "project_file": {"sha256": "f" * 64, "size_bytes": 1, "path": "Song.flp"},
            "assets": [
                {"size_bytes": 1, "path": "Samples/no-hash.wav"},
                {"sha256": "a" * 64, "size_bytes": 1024, "path": "Samples/kick.wav"},
                {"sha256": "b" * 64, "size_bytes": 1, "path": ""},
                {"sha256": "c" * 64, "size_bytes": 12, "path": "Melody.mid"},
            ],
        }
    )
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{version.id}/assets")

    assert response.status_code == 200
    assert [asset["id"] for asset in response.json()] == [f"{'a' * 64}:1", f"{'c' * 64}:3"]


def test_list_assets_empty_when_version_has_no_manifest(monkeypatch) -> None:
    version = _build_version(manifest_json=None)
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{version.id}/assets")

    assert response.status_code == 200
    assert response.json() == []


def test_list_assets_404_when_no_access(monkeypatch) -> None:
    version = _build_version()
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{uuid.uuid4()}/assets")

    assert response.status_code == 404


def test_the_tracks_route_is_gone(monkeypatch) -> None:
    version = _build_version(manifest_json={"manifest_version": 2, "assets": []})
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{version.id}/tracks")

    assert response.status_code == 404
