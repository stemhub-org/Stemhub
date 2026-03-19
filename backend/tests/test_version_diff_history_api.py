from __future__ import annotations

import uuid
from datetime import datetime, timezone
from types import SimpleNamespace

from fastapi import FastAPI
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
        raise AssertionError(f"Unexpected DB query in diff history test: {statement}")


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
    parent_version_id=None,
    commit_message="Compare me",
) -> Version:
    return Version(
        id=uuid.uuid4(),
        branch_id=branch_id,
        parent_version_id=parent_version_id,
        commit_message=commit_message,
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        artifact_path=artifact_path,
        source_daw=source_daw,
        source_project_filename=source_project_filename,
        snapshot_manifest=snapshot_manifest or {"flp_relative_path": "demo.flp"},
    )


def _create_test_client(*, monkeypatch, current_user: User, versions: list[Version], load_snapshot_side_effect):
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
        return SimpleNamespace(id=branch_id, name="main", project_id=uuid.uuid4())

    async def fake_list_branch_versions_for_history(*, branch_id, db):
        del db
        return [version for version in versions if version.branch_id == branch_id]

    monkeypatch.setattr(versions_router_module, "_get_branch_with_access", fake_get_branch_with_access)
    monkeypatch.setattr(versions_router_module, "_list_branch_versions_for_history", fake_list_branch_versions_for_history)
    monkeypatch.setattr(versions_router_module, "load_fl_studio_mixer_snapshot", load_snapshot_side_effect)

    return TestClient(app)


def test_list_version_diff_history_returns_compared_and_initial_entries(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, artifact_path="projects/demo/oldest.zip", commit_message="Oldest")
    newest = _build_version(
        branch_id=branch_id,
        artifact_path="projects/demo/newest.zip",
        parent_version_id=oldest.id,
        commit_message="Newest",
    )
    versions = [newest, oldest]

    snapshots = {
        oldest.artifact_path: MixerProjectSnapshot(
            inserts=(MixerInsertSnapshot(iid=0, name="Master", enabled=True, volume=12800, pan=0, slots=()),)
        ),
        newest.artifact_path: MixerProjectSnapshot(
            inserts=(
                MixerInsertSnapshot(
                    iid=0,
                    name="Master",
                    enabled=True,
                    volume=14000,
                    pan=0,
                    slots=(
                        MixerSlotSnapshot(
                            index=1,
                            name="Soft Clipper",
                            internal_name="Fruity Soft Clipper",
                            enabled=True,
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
        return snapshots[artifact_path]

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=versions,
        load_snapshot_side_effect=fake_load_snapshot,
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert len(payload) == 2
    assert payload[0]["version"]["id"] == str(newest.id)
    assert payload[0]["compared_to_version_id"] == str(oldest.id)
    assert payload[0]["status"] == "compared"
    assert payload[0]["summary"]["total_changes"] == 2
    assert [change["type"] for change in payload[0]["changes"]] == [
        "insert_volume_changed",
        "slot_added",
    ]
    assert payload[1]["version"]["id"] == str(oldest.id)
    assert payload[1]["status"] == "initial"
    assert payload[1]["status_message"] == "Initial snapshot on this branch."


def test_list_version_diff_history_marks_unsupported_versions(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, artifact_path="projects/demo/oldest.zip", commit_message="Oldest")
    newest = _build_version(
        branch_id=branch_id,
        artifact_path="projects/demo/newest.zip",
        parent_version_id=oldest.id,
        source_daw="Ableton Live",
        source_project_filename="demo.als",
        commit_message="Newest",
    )
    versions = [newest, oldest]

    def fake_load_snapshot(*, artifact_path, snapshot_manifest, storage):
        del artifact_path, snapshot_manifest, storage
        raise AssertionError("Snapshots should not be loaded for unsupported history entries")

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=versions,
        load_snapshot_side_effect=fake_load_snapshot,
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert payload[0]["status"] == "unsupported"
    assert payload[0]["status_message"] == "Automatic mixer diff is only available for FL Studio versions."


def test_list_version_diff_history_uses_parent_version_before_previous_version(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, artifact_path="projects/demo/oldest.zip", commit_message="Oldest")
    middle = _build_version(branch_id=branch_id, artifact_path="projects/demo/middle.zip", commit_message="Middle")
    newest = _build_version(
        branch_id=branch_id,
        artifact_path="projects/demo/newest.zip",
        parent_version_id=oldest.id,
        commit_message="Newest",
    )
    versions = [newest, middle, oldest]

    def fake_load_snapshot(*, artifact_path, snapshot_manifest, storage):
        del artifact_path, snapshot_manifest, storage
        return MixerProjectSnapshot(inserts=())

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=versions,
        load_snapshot_side_effect=fake_load_snapshot,
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert payload[0]["compared_to_version_id"] == str(oldest.id)


def test_list_version_diff_history_returns_parser_errors_as_unsupported(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, artifact_path="projects/demo/oldest.zip", commit_message="Oldest")
    newest = _build_version(
        branch_id=branch_id,
        artifact_path="projects/demo/newest.zip",
        parent_version_id=oldest.id,
        commit_message="Newest",
    )
    versions = [newest, oldest]

    def fake_load_snapshot(*, artifact_path, snapshot_manifest, storage):
        del artifact_path, snapshot_manifest, storage
        raise MixerSnapshotError("Snapshot archive does not contain an FL Studio project file.")

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=versions,
        load_snapshot_side_effect=fake_load_snapshot,
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert payload[0]["status"] == "unsupported"
    assert payload[0]["status_message"] == "Snapshot archive does not contain an FL Studio project file."


def test_list_version_diff_history_returns_runtime_snapshot_errors_as_unsupported(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, artifact_path="projects/demo/oldest.zip", commit_message="Oldest")
    newest = _build_version(
        branch_id=branch_id,
        artifact_path="projects/demo/newest.zip",
        parent_version_id=oldest.id,
        commit_message="Newest",
    )
    versions = [newest, oldest]

    def fake_load_snapshot(*, artifact_path, snapshot_manifest, storage):
        del artifact_path, snapshot_manifest, storage
        raise RuntimeError(
            "PyFLP_v2 is not installed. Run: `git submodule update --init --recursive && pip install -e backend/vendor/PyFLP_v2`."
        )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=versions,
        load_snapshot_side_effect=fake_load_snapshot,
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert payload[0]["status"] == "unsupported"
    assert payload[0]["status_message"].startswith("PyFLP_v2 is not installed.")
