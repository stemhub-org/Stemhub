from __future__ import annotations

import uuid
from datetime import datetime, timezone
from types import SimpleNamespace

from fastapi import FastAPI, HTTPException
from fastapi.testclient import TestClient

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.fl_mixer import (
    FlEffectSlot,
    FlMixer,
    FlMixerInsert,
    MixerReadError,
)
from stemhub.models import Blob, User, Version
from stemhub.routers import versions as versions_router_module
from stemhub.routers.versions import router as versions_router
from stemhub.storage import get_storage_service


class DummyAsyncSession:
    async def execute(self, statement):
        raise AssertionError(f"Unexpected DB query in compare test: {statement}")


class _BlobLookupResult:
    def __init__(self, blob: Blob | None) -> None:
        self._blob = blob

    def scalars(self):
        return self

    def first(self):
        return self._blob


class BlobLookupSession:
    """Answers the project-file lookup of the real mixer loader with ``blob`` (or nothing)."""

    def __init__(self, blob: Blob | None) -> None:
        self.blob = blob

    async def execute(self, statement):
        assert "FROM blob" in str(statement), f"Unexpected DB query in compare test: {statement}"
        return _BlobLookupResult(self.blob)


def _build_user() -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username="demo",
        password_hash="hashed-password",
        created_at=datetime.now(timezone.utc),
        is_active=True,
    )


def _build_version(
    *,
    branch_id,
    source_daw: str | None = "FL Studio",
    source_project_filename: str | None = "demo.flp",
    project_file_sha: str | None = "a" * 64,
    project_file_path: str = "demo.flp",
    manifest_version: int = 1,
) -> Version:
    """Build a CAS Version with a v1 or v2 manifest. ``project_file_sha`` = None
    means no manifest is set (simulates a version with no project file to compare)."""
    manifest_json = None
    if project_file_sha is not None:
        if manifest_version == 1:
            manifest_json = {
                "manifest_version": 1,
                "project_file": {"sha256": project_file_sha, "size_bytes": 1024, "filename": project_file_path},
                "tracks": [],
            }
        else:
            manifest_json = {
                "manifest_version": 2,
                "project_file": {"sha256": project_file_sha, "size_bytes": 1024, "path": project_file_path},
                "assets": [],
            }
    return Version(
        id=uuid.uuid4(),
        branch_id=branch_id,
        message="Compare me",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        source_daw=source_daw,
        source_project_filename=source_project_filename,
        manifest_json=manifest_json,
        manifest_version=manifest_version if manifest_json else None,
    )


def _create_test_client(
    *,
    monkeypatch,
    current_user: User,
    versions: dict[uuid.UUID, Version],
    load_mixer_side_effect=None,
    session=None,
):
    """Build a TestClient with fake branch/version lookups and (optionally) a
    faked mixer loader. ``load_mixer_side_effect`` receives the Version
    being loaded and returns an FlMixer (or raises)."""
    app = FastAPI()
    app.include_router(versions_router)
    session = session if session is not None else DummyAsyncSession()

    async def override_db():
        yield session

    async def override_current_user():
        return current_user

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_current_user] = override_current_user
    app.dependency_overrides[get_storage_service] = lambda: object()

    project_id = uuid.uuid4()

    async def fake_get_branch_with_access(*, branch_id, current_user, db):
        del current_user, db
        return SimpleNamespace(id=branch_id, project_id=project_id, name="main")

    async def fake_get_version_for_branch(*, branch_id, version_id, db):
        del db
        version = versions.get(version_id)
        if version is None or version.branch_id != branch_id:
            raise HTTPException(status_code=404, detail="Version not found in branch")
        return version

    monkeypatch.setattr(versions_router_module, "_get_branch_with_access", fake_get_branch_with_access)
    monkeypatch.setattr(versions_router_module, "_get_version_for_branch", fake_get_version_for_branch)

    if load_mixer_side_effect is not None:
        async def fake_load_mixer_for_compare(*, version, project_id, db, storage):
            del project_id, db, storage
            return load_mixer_side_effect(version)

        monkeypatch.setattr(
            versions_router_module,
            "_load_mixer_for_compare",
            fake_load_mixer_for_compare,
        )

    return TestClient(app)


def test_compare_versions_returns_mixer_diff(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)

    mixers = {
        base_version.id: FlMixer(
            inserts=(
                FlMixerInsert(
                    index=5,
                    name="Drums",
                    enabled=True,
                    volume=12800,
                    pan=0,
                    slots=(
                        FlEffectSlot(
                            index=2,
                            name="Balance",
                            internal_name="Fruity Balance",
                            enabled=True,
                            dry_wet=3200,
                            plugin_name="Fruity Balance",
                        ),
                    ),
                ),
            )
        ),
        target_version.id: FlMixer(
            inserts=(
                FlMixerInsert(
                    index=5,
                    name="Drums",
                    enabled=True,
                    volume=14120,
                    pan=0,
                    slots=(
                        FlEffectSlot(
                            index=2,
                            name="Soft Clipper",
                            internal_name="Fruity Soft Clipper",
                            enabled=False,
                            dry_wet=6400,
                            plugin_name="Fruity Soft Clipper",
                        ),
                    ),
                ),
            )
        ),
    }

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_mixer_side_effect=lambda version: mixers[version.id],
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["summary"] == {
        "total_changes": 4,
        "inserts_changed": 1,
        "slots_changed": 1,
        "parameter_changes": 3,
    }
    assert [change["type"] for change in payload["changes"]] == [
        "insert_volume_changed",
        "slot_plugin_changed",
        "slot_enabled_changed",
        "slot_dry_wet_changed",
    ]


def test_compare_versions_returns_empty_diff_when_mixers_match(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)
    mixer = FlMixer(
        inserts=(
            FlMixerInsert(
                index=0,
                name="Master",
                enabled=True,
                volume=12800,
                pan=0,
                slots=(),
            ),
        )
    )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_mixer_side_effect=lambda _version: mixer,
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 200
    assert response.json() == {
        "summary": {
            "total_changes": 0,
            "inserts_changed": 0,
            "slots_changed": 0,
            "parameter_changes": 0,
        },
        "changes": [],
    }


def test_compare_versions_rejects_same_version_ids(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version},
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(base_version.id)},
    )

    assert response.status_code == 400
    assert response.json()["detail"] == "base_version_id and target_version_id must be different"


def test_compare_versions_rejects_non_fl_studio_versions(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(
        branch_id=branch_id, source_daw="Ableton Live", source_project_filename="demo.als"
    )
    target_version = _build_version(branch_id=branch_id)

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"] == "Only FL Studio versions can be compared"


def test_compare_versions_returns_parser_errors_as_validation_failures(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)

    def raise_read_error(version):
        del version
        raise HTTPException(
            status_code=422,
            detail="The FL Studio project file could not be read.",
        )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_mixer_side_effect=raise_read_error,
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"] == "The FL Studio project file could not be read."


def test_compare_versions_returns_runtime_read_errors_as_validation_failures(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)

    def raise_runtime(version):
        del version
        raise HTTPException(
            status_code=422,
            detail="PyFLP_v2 is not installed.",
        )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_mixer_side_effect=raise_runtime,
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"].startswith("PyFLP_v2 is not installed.")


def test_compare_versions_rejects_versions_outside_branch(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    other_branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=other_branch_id)

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 404
    assert response.json()["detail"] == "Version not found in branch"


def test_compare_versions_returns_422_when_version_has_no_project_file(monkeypatch) -> None:
    """A version whose manifest lists no project file cannot be compared; the
    real loader answers before touching the database or storage."""
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id, project_file_sha=None)
    target_version = _build_version(branch_id=branch_id)

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"] == "This version has no project file to compare."


def test_compare_versions_returns_422_when_the_stored_project_file_is_missing(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        session=BlobLookupSession(blob=None),
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    detail = response.json()["detail"]
    assert detail == "The project file of this version is missing from storage."
    assert "blob" not in detail.lower()


def test_compare_versions_surfaces_mixer_read_errors(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)
    stored = Blob(project_id=uuid.uuid4(), sha256="a" * 64, size_bytes=1024, storage_uri="projects/x/blobs/aa/a")

    def raise_read_error(*, storage_uri, storage):
        del storage_uri, storage
        raise MixerReadError("This version has no project file to read.")

    monkeypatch.setattr(versions_router_module, "load_fl_mixer", raise_read_error)
    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        session=BlobLookupSession(blob=stored),
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"] == "This version has no project file to read."


def test_compare_versions_wraps_unexpected_read_failures_without_internal_terms(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)
    stored = Blob(project_id=uuid.uuid4(), sha256="a" * 64, size_bytes=1024, storage_uri="projects/x/blobs/aa/a")

    def raise_unexpected(*, storage_uri, storage):
        del storage_uri, storage
        raise ValueError("bad header")

    monkeypatch.setattr(versions_router_module, "load_fl_mixer", raise_unexpected)
    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        session=BlobLookupSession(blob=stored),
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"] == "Failed to read the FL Studio project file: bad header"


def test_compare_versions_detects_fl_studio_from_the_project_file_path_of_v1_and_v2_manifests(monkeypatch) -> None:
    # No source_daw / source_project_filename recorded: the manifest's project
    # file path is the only hint, read from `filename` (v1) or `path` (v2).
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(
        branch_id=branch_id, source_daw=None, source_project_filename=None, project_file_path="Song.flp"
    )
    target_version = _build_version(
        branch_id=branch_id,
        source_daw=None,
        source_project_filename=None,
        project_file_path="Song.flp",
        manifest_version=2,
    )
    mixer = FlMixer(inserts=())

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_mixer_side_effect=lambda _version: mixer,
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 200
    assert response.json()["changes"] == []
