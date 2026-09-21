from __future__ import annotations

import uuid
from datetime import datetime, timezone
from types import SimpleNamespace

from fastapi import FastAPI, HTTPException
from fastapi.testclient import TestClient

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.flp_mixer_snapshot import (
    MixerInsertSnapshot,
    MixerProjectSnapshot,
    MixerSlotSnapshot,
    MixerSnapshotError,
)
from stemhub.models import User, Version
from stemhub.routers import versions as versions_router_module
from stemhub.routers.versions import router as versions_router
from stemhub.storage import get_storage_service


class DummyAsyncSession:
    async def execute(self, statement):
        raise AssertionError(f"Unexpected DB query in compare test: {statement}")


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
) -> Version:
    """Build a CAS Version. ``project_file_sha`` = None means no manifest is set
    (simulates a version with no project-file blob to compare)."""
    manifest_json = None
    if project_file_sha is not None:
        manifest_json = {
            "manifest_version": 1,
            "project_file": {
                "sha256": project_file_sha,
                "size_bytes": 1024,
                "filename": source_project_filename or "demo.flp",
            },
            "tracks": [],
        }
    return Version(
        id=uuid.uuid4(),
        branch_id=branch_id,
        commit_message="Compare me",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        source_daw=source_daw,
        source_project_filename=source_project_filename,
        manifest_json=manifest_json,
        manifest_version=1 if manifest_json else None,
    )


def _create_test_client(
    *,
    monkeypatch,
    current_user: User,
    versions: dict[uuid.UUID, Version],
    load_snapshot_side_effect=None,
):
    """Build a TestClient with fake branch/version lookups and (optionally) a
    faked snapshot loader. ``load_snapshot_side_effect`` receives the Version
    being loaded and returns a MixerProjectSnapshot (or raises)."""
    app = FastAPI()
    app.include_router(versions_router)
    session = DummyAsyncSession()

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

    if load_snapshot_side_effect is not None:
        async def fake_load_snapshot_for_compare(*, version, project_id, db, storage):
            del project_id, db, storage
            return load_snapshot_side_effect(version)

        monkeypatch.setattr(
            versions_router_module,
            "_load_snapshot_for_compare",
            fake_load_snapshot_for_compare,
        )

    return TestClient(app)


def test_compare_versions_returns_mixer_diff(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)

    snapshots = {
        base_version.id: MixerProjectSnapshot(
            inserts=(
                MixerInsertSnapshot(
                    iid=5,
                    name="Drums",
                    enabled=True,
                    volume=12800,
                    pan=0,
                    slots=(
                        MixerSlotSnapshot(
                            index=2,
                            name="Balance",
                            internal_name="Fruity Balance",
                            enabled=True,
                            mix=3200,
                            plugin_key="Fruity Balance",
                        ),
                    ),
                ),
            )
        ),
        target_version.id: MixerProjectSnapshot(
            inserts=(
                MixerInsertSnapshot(
                    iid=5,
                    name="Drums",
                    enabled=True,
                    volume=14120,
                    pan=0,
                    slots=(
                        MixerSlotSnapshot(
                            index=2,
                            name="Soft Clipper",
                            internal_name="Fruity Soft Clipper",
                            enabled=False,
                            mix=6400,
                            plugin_key="Fruity Soft Clipper",
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
        load_snapshot_side_effect=lambda version: snapshots[version.id],
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
        "slot_mix_changed",
    ]


def test_compare_versions_returns_empty_diff_when_snapshots_match(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id)
    target_version = _build_version(branch_id=branch_id)
    snapshot = MixerProjectSnapshot(
        inserts=(
            MixerInsertSnapshot(
                iid=0,
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
        load_snapshot_side_effect=lambda _version: snapshot,
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

    def raise_snapshot_error(version):
        del version
        raise HTTPException(
            status_code=422,
            detail="Snapshot archive does not contain an FL Studio project file.",
        )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_snapshot_side_effect=raise_snapshot_error,
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"] == "Snapshot archive does not contain an FL Studio project file."


def test_compare_versions_returns_runtime_snapshot_errors_as_validation_failures(monkeypatch) -> None:
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
        load_snapshot_side_effect=raise_runtime,
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


def test_compare_versions_returns_422_when_version_has_no_project_file_blob(monkeypatch) -> None:
    """A CAS version with no project_file in manifest cannot be compared —
    replaces the old 'requires_artifacts' test."""
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id, project_file_sha=None)
    target_version = _build_version(branch_id=branch_id)

    def raise_missing(version):
        raise HTTPException(status_code=422, detail="Version has no project-file blob to compare.")

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_snapshot_side_effect=raise_missing,
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"] == "Version has no project-file blob to compare."


# MixerSnapshotError is imported to keep it in scope for tests that patch the
# loader with fixture-style exceptions in the future.
_ = MixerSnapshotError
