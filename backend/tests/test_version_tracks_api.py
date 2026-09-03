"""Tests for GET /versions/{id}/tracks.

Covers the two track-data sources a version can have (manifest_json vs. the
legacy Track table) and the no-data case, per the source selection logic in
stemhub.routers.versions.list_version_tracks.
"""
from __future__ import annotations

import uuid
from datetime import datetime, timezone

from fastapi import FastAPI, HTTPException
from fastapi.testclient import TestClient

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Track, User, Version
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


class _FakeTrackResult:
    def __init__(self, tracks: list[Track]) -> None:
        self._tracks = tracks

    def scalars(self):
        return self

    def all(self):
        return self._tracks


class _TrackTableSession:
    """Only answers the `select(Track).where(...)` query the endpoint issues
    once _get_version_with_access has been monkeypatched away."""

    def __init__(self, tracks: list[Track]) -> None:
        self.tracks = tracks

    async def execute(self, statement):
        del statement
        return _FakeTrackResult(self.tracks)


def _create_test_client(*, monkeypatch, current_user: User, version: Version, session=None):
    app = FastAPI()
    app.include_router(versions_router)

    async def override_db():
        yield session or object()

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


def test_list_tracks_from_manifest(monkeypatch) -> None:
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

    response = client.get(f"/versions/{version.id}/tracks")

    assert response.status_code == 200
    payload = response.json()
    assert len(payload) == 1
    assert payload[0] == {
        "id": f"{'a' * 64}:0",
        "name": "Kick",
        "file_type": "wav",
        "bpm": 128,
        "key": "Cm",
        "duration_seconds": 12,
        "size_bytes": 1024,
        "source": "manifest",
    }


def test_list_tracks_manifest_ids_unique_even_with_duplicate_blob(monkeypatch) -> None:
    # Two distinct named tracks intentionally referencing the same blob
    # (e.g. a duplicated/bounced stem) must not collide on `id`.
    shared_sha = "e" * 64
    version = _build_version(
        manifest_json={
            "manifest_version": 1,
            "tracks": [
                {"sha256": shared_sha, "name": "Kick", "filename": "kick.wav"},
                {"sha256": shared_sha, "name": "Kick (copy)", "filename": "kick.wav"},
            ],
        }
    )
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{version.id}/tracks")

    assert response.status_code == 200
    payload = response.json()
    assert len(payload) == 2
    assert payload[0]["id"] != payload[1]["id"]
    assert {t["name"] for t in payload} == {"Kick", "Kick (copy)"}


def test_list_tracks_from_legacy_table_when_no_manifest(monkeypatch) -> None:
    version = _build_version(manifest_json=None)
    track = Track(
        id=uuid.uuid4(),
        version_id=version.id,
        name="Lead Synth",
        file_type=".wav",
        bpm=140,
        key="Am",
        duration=30,
    )
    session = _TrackTableSession([track])
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version, session=session)

    response = client.get(f"/versions/{version.id}/tracks")

    assert response.status_code == 200
    payload = response.json()
    assert len(payload) == 1
    assert payload[0]["id"] == str(track.id)
    assert payload[0]["name"] == "Lead Synth"
    assert payload[0]["source"] == "legacy"
    assert payload[0]["size_bytes"] is None
    # Normalized to match the manifest path's format (no leading dot).
    assert payload[0]["file_type"] == "wav"


def test_list_tracks_empty_when_no_track_data(monkeypatch) -> None:
    version = _build_version(manifest_json=None)
    session = _TrackTableSession([])
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version, session=session)

    response = client.get(f"/versions/{version.id}/tracks")

    assert response.status_code == 200
    assert response.json() == []


def test_list_tracks_404_when_no_access(monkeypatch) -> None:
    version = _build_version()
    client = _create_test_client(monkeypatch=monkeypatch, current_user=_build_user(), version=version)

    response = client.get(f"/versions/{uuid.uuid4()}/tracks")

    assert response.status_code == 404
