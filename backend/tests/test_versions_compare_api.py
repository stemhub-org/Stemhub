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
    artifact_path: str | None = "projects/demo/snapshot.zip",
    snapshot_manifest: dict | None = None,
) -> Version:
    return Version(
        id=uuid.uuid4(),
        branch_id=branch_id,
        commit_message="Compare me",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        artifact_path=artifact_path,
        source_daw=source_daw,
        source_project_filename=source_project_filename,
        snapshot_manifest=snapshot_manifest or {"flp_relative_path": "demo.flp"},
    )


def _create_test_client(*, monkeypatch, current_user: User, versions: dict[uuid.UUID, Version], load_snapshot_side_effect=None):
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

    async def fake_get_branch_with_access(*, branch_id, current_user, db):
        del current_user, db
        return SimpleNamespace(id=branch_id, project_id=uuid.uuid4())

    async def fake_get_version_for_branch(*, branch_id, version_id, db):
        del db
        version = versions.get(version_id)
        if version is None or version.branch_id != branch_id:
            raise HTTPException(status_code=404, detail="Version not found in branch")
        return version

    monkeypatch.setattr(versions_router_module, "_get_branch_with_access", fake_get_branch_with_access)
    monkeypatch.setattr(versions_router_module, "_get_version_for_branch", fake_get_version_for_branch)

    if load_snapshot_side_effect is not None:
        monkeypatch.setattr(versions_router_module, "load_fl_studio_mixer_snapshot", load_snapshot_side_effect)

    return TestClient(app)


def test_compare_versions_returns_mixer_diff(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id, artifact_path="projects/demo/base.zip")
    target_version = _build_version(branch_id=branch_id, artifact_path="projects/demo/target.zip")

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

    def fake_load_snapshot(*, artifact_path, snapshot_manifest, storage):
        del snapshot_manifest, storage
        if artifact_path == base_version.artifact_path:
            return snapshots[base_version.id]
        return snapshots[target_version.id]

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_snapshot_side_effect=fake_load_snapshot,
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
    base_version = _build_version(branch_id=branch_id, artifact_path="projects/demo/base.zip")
    target_version = _build_version(branch_id=branch_id, artifact_path="projects/demo/target.zip")
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

    def fake_load_snapshot(*, artifact_path, snapshot_manifest, storage):
        del artifact_path, snapshot_manifest, storage
        return snapshot

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_snapshot_side_effect=fake_load_snapshot,
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


def test_compare_versions_requires_artifacts(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id, artifact_path=None)
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
    assert response.json()["detail"] == "Both versions must have snapshot artifacts to be compared"


def test_compare_versions_rejects_non_fl_studio_versions(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    base_version = _build_version(branch_id=branch_id, source_daw="Ableton Live", source_project_filename="demo.als")
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

    def fake_load_snapshot(*, artifact_path, snapshot_manifest, storage):
        del artifact_path, snapshot_manifest, storage
        raise MixerSnapshotError("Snapshot archive does not contain an FL Studio project file.")

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_snapshot_side_effect=fake_load_snapshot,
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

    def fake_load_snapshot(*, artifact_path, snapshot_manifest, storage):
        del artifact_path, snapshot_manifest, storage
        raise RuntimeError(
            "PyFLP_enhanced is not installed. Run: `git submodule update --init --recursive && pip install -e backend/vendor/PyFLP_enhanced`."
        )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions={base_version.id: base_version, target_version.id: target_version},
        load_snapshot_side_effect=fake_load_snapshot,
    )

    response = client.get(
        f"/branches/{branch_id}/versions/compare",
        params={"base_version_id": str(base_version.id), "target_version_id": str(target_version.id)},
    )

    assert response.status_code == 422
    assert response.json()["detail"].startswith("PyFLP_enhanced is not installed.")


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
