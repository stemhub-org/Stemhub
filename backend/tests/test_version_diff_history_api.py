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
    parent_version_id=None,
    commit_message="Compare me",
) -> Version:
    manifest_json = {
        "manifest_version": 1,
        "project_file": {
            "sha256": uuid.uuid4().hex + uuid.uuid4().hex,
            "size_bytes": 1024,
            "filename": source_project_filename or "demo.flp",
        },
        "tracks": [],
    }
    return Version(
        id=uuid.uuid4(),
        branch_id=branch_id,
        parent_version_id=parent_version_id,
        commit_message=commit_message,
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        source_daw=source_daw,
        source_project_filename=source_project_filename,
        manifest_json=manifest_json,
        manifest_version=1,
    )


def _create_test_client(
    *,
    monkeypatch,
    current_user: User,
    versions: list[Version],
    load_snapshot_side_effect,
):
    """load_snapshot_side_effect receives a Version and returns a
    MixerProjectSnapshot, or raises HTTPException/RuntimeError to test error paths."""
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
        return SimpleNamespace(id=branch_id, name="main", project_id=project_id)

    async def fake_list_branch_versions_for_history(*, branch_id, db):
        del db
        return [version for version in versions if version.branch_id == branch_id]

    async def fake_load_snapshot_for_compare(*, version, project_id, db, storage):
        del project_id, db, storage
        result = load_snapshot_side_effect(version)
        if isinstance(result, Exception):
            raise result
        return result

    monkeypatch.setattr(versions_router_module, "_get_branch_with_access", fake_get_branch_with_access)
    monkeypatch.setattr(
        versions_router_module,
        "_list_branch_versions_for_history",
        fake_list_branch_versions_for_history,
    )
    monkeypatch.setattr(
        versions_router_module,
        "_load_snapshot_for_compare",
        fake_load_snapshot_for_compare,
    )

    return TestClient(app)


def test_list_version_diff_history_returns_compared_and_initial_entries(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, commit_message="Oldest")
    newest = _build_version(branch_id=branch_id, parent_version_id=oldest.id, commit_message="Newest")

    snapshots = {
        oldest.id: MixerProjectSnapshot(
            inserts=(MixerInsertSnapshot(iid=0, name="Master", enabled=True, volume=12800, pan=0, slots=()),)
        ),
        newest.id: MixerProjectSnapshot(
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

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=[newest, oldest],
        load_snapshot_side_effect=lambda version: snapshots[version.id],
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
    oldest = _build_version(branch_id=branch_id, commit_message="Oldest")
    newest = _build_version(
        branch_id=branch_id,
        parent_version_id=oldest.id,
        source_daw="Ableton Live",
        source_project_filename="demo.als",
        commit_message="Newest",
    )

    def refuse(version):
        raise AssertionError(f"Snapshots should not be loaded for {version.id}")

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=[newest, oldest],
        load_snapshot_side_effect=refuse,
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert payload[0]["status"] == "unsupported"
    assert payload[0]["status_message"] == "Automatic mixer diff is only available for FL Studio versions."


def test_list_version_diff_history_uses_parent_version_before_previous_version(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, commit_message="Oldest")
    middle = _build_version(branch_id=branch_id, commit_message="Middle")
    newest = _build_version(
        branch_id=branch_id,
        parent_version_id=oldest.id,
        commit_message="Newest",
    )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=[newest, middle, oldest],
        load_snapshot_side_effect=lambda _version: MixerProjectSnapshot(inserts=()),
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert payload[0]["compared_to_version_id"] == str(oldest.id)


def test_list_version_diff_history_returns_parser_errors_as_unsupported(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, commit_message="Oldest")
    newest = _build_version(branch_id=branch_id, parent_version_id=oldest.id, commit_message="Newest")

    def raise_snapshot(version):
        del version
        raise HTTPException(
            status_code=422,
            detail="Snapshot archive does not contain an FL Studio project file.",
        )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=[newest, oldest],
        load_snapshot_side_effect=raise_snapshot,
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert payload[0]["status"] == "unsupported"
    assert payload[0]["status_message"] == "Snapshot archive does not contain an FL Studio project file."


def test_list_version_diff_history_returns_runtime_snapshot_errors_as_unsupported(monkeypatch) -> None:
    branch_id = uuid.uuid4()
    current_user = _build_user()
    oldest = _build_version(branch_id=branch_id, commit_message="Oldest")
    newest = _build_version(branch_id=branch_id, parent_version_id=oldest.id, commit_message="Newest")

    def raise_runtime(version):
        del version
        raise HTTPException(
            status_code=422,
            detail="PyFLP_v2 is not installed.",
        )

    client = _create_test_client(
        monkeypatch=monkeypatch,
        current_user=current_user,
        versions=[newest, oldest],
        load_snapshot_side_effect=raise_runtime,
    )

    response = client.get(f"/branches/{branch_id}/versions/diff-history")

    assert response.status_code == 200
    payload = response.json()
    assert payload[0]["status"] == "unsupported"
    assert payload[0]["status_message"].startswith("PyFLP_v2 is not installed.")
