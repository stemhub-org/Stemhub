import io
import uuid
from datetime import datetime, timezone

from fastapi import FastAPI, HTTPException
from fastapi.testclient import TestClient

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Branch, Project, User, Version
from stemhub.routers import files as files_router_module
from stemhub.routers.files import router as files_router
from stemhub.storage import LocalFilesystemStorageService, StorageNotFoundError, get_storage_service


class DummyAsyncSession:
    def __init__(self) -> None:
        self.commit_calls = 0
        self.refreshed_objects: list[object] = []

    async def commit(self) -> None:
        self.commit_calls += 1

    async def refresh(self, instance: object) -> None:
        self.refreshed_objects.append(instance)


def _build_user() -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username="demo",
        password_hash="hashed-password",
        created_at=datetime.now(timezone.utc),
        is_active=True,
    )


def _build_version(*, owner: User | None = None) -> tuple[User, Version]:
    owner = owner or _build_user()
    project = Project(
        id=uuid.uuid4(),
        owner_id=owner.id,
        name="Demo Project",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
    )
    branch = Branch(
        id=uuid.uuid4(),
        project_id=project.id,
        name="main",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
    )
    version = Version(
        id=uuid.uuid4(),
        branch_id=branch.id,
        commit_message="Initial snapshot",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
    )
    branch.project = project
    version.branch = branch
    return owner, version


def _create_test_client(
    *,
    monkeypatch,
    tmp_path,
    storage = None,
    current_user: User | None = None,
    accessible_version: Version | Exception | None = None,
):
    app = FastAPI()
    app.include_router(files_router)

    session = DummyAsyncSession()
    storage_service = storage if storage is not None else LocalFilesystemStorageService(tmp_path)

    async def override_db():
        yield session

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_storage_service] = lambda: storage_service

    if current_user is not None:
        async def override_current_user():
            return current_user

        app.dependency_overrides[get_current_user] = override_current_user

    if accessible_version is not None:
        async def fake_get_version_with_access(*, version_id, current_user, db, owner_only=False):
            del version_id, current_user, db, owner_only
            if isinstance(accessible_version, Exception):
                raise accessible_version
            return accessible_version

        monkeypatch.setattr(files_router_module, "_get_version_with_access", fake_get_version_with_access)

    client = TestClient(app)
    return client, session, storage_service


def test_upload_version_artifact_persists_file_and_metadata(tmp_path, monkeypatch) -> None:
    current_user, version = _build_version()
    client, session, storage = _create_test_client(
        monkeypatch=monkeypatch,
        tmp_path=tmp_path,
        current_user=current_user,
        accessible_version=version,
    )

    response = client.post(
        f"/versions/{version.id}/artifact",
        files={"artifact": ("demo.flp", b"demo snapshot payload", "application/octet-stream")},
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["id"] == str(version.id)
    assert payload["branch_id"] == str(version.branch_id)
    assert payload["artifact_size_bytes"] == len(b"demo snapshot payload")
    assert payload["artifact_checksum"]
    assert payload["source_project_filename"] == "demo.flp"
    assert payload["artifact_path"].endswith("/snapshot/demo.flp")
    assert storage.resolve_artifact_path(payload["artifact_path"]).read_bytes() == b"demo snapshot payload"
    assert session.commit_calls == 1
    assert session.refreshed_objects == [version]


def test_download_version_artifact_streams_stored_file(tmp_path, monkeypatch) -> None:
    current_user, version = _build_version()
    payload = b"restorable snapshot"
    storage = LocalFilesystemStorageService(tmp_path)
    stored = storage.store_version_artifact(
        project_id=version.branch.project.id,
        branch_id=version.branch_id,
        version_id=version.id,
        filename="restored.flp",
        source=io.BytesIO(payload),
    )
    version.artifact_path = stored.path
    version.artifact_size_bytes = stored.size_bytes
    version.artifact_checksum = stored.checksum_sha256
    version.source_project_filename = "restored.flp"

    client, _, _ = _create_test_client(
        monkeypatch=monkeypatch,
        tmp_path=tmp_path,
        current_user=current_user,
        accessible_version=version,
    )

    response = client.get(f"/versions/{version.id}/artifact")

    assert response.status_code == 200
    assert response.content == payload
    assert "filename=\"restored.flp\"" in response.headers["content-disposition"]


def test_download_version_artifact_uses_storage_service_for_non_local_reference(tmp_path, monkeypatch) -> None:
    current_user, version = _build_version()
    payload = b"remote backend snapshot"
    gcs_reference = "gcs://demo-bucket/projects/demo/branches/main/versions/external/snapshot/remote.flp"

    class RemoteStorageService:
        def store_version_artifact(self, **kwargs):
            raise AssertionError("Store should not be called for download test")

        def resolve_artifact_path(self, artifact_path: str):
            assert artifact_path == gcs_reference
            remote_file = tmp_path / "remote.flp"
            remote_file.write_bytes(payload)
            return remote_file

    version.artifact_path = gcs_reference
    version.artifact_size_bytes = len(payload)
    version.artifact_checksum = "a" * 64
    version.source_project_filename = "remote.flp"

    client, _, _ = _create_test_client(
        monkeypatch=monkeypatch,
        tmp_path=tmp_path,
        current_user=current_user,
        accessible_version=version,
        storage=RemoteStorageService(),
    )

    response = client.get(f"/versions/{version.id}/artifact")

    assert response.status_code == 200
    assert response.content == payload
    assert "filename=\"remote.flp\"" in response.headers["content-disposition"]


def test_upload_version_artifact_requires_authentication(tmp_path, monkeypatch) -> None:
    client, _, _ = _create_test_client(
        monkeypatch=monkeypatch,
        tmp_path=tmp_path,
    )

    response = client.post(
        f"/versions/{uuid.uuid4()}/artifact",
        files={"artifact": ("demo.flp", b"demo snapshot payload", "application/octet-stream")},
    )

    assert response.status_code == 401
    assert response.json()["detail"] == "Could not validate credentials"


def test_upload_version_artifact_rejects_second_upload(tmp_path, monkeypatch) -> None:
    current_user, version = _build_version()
    version.artifact_path = "projects/demo/branches/main/versions/existing/snapshot/demo.flp"

    client, session, _ = _create_test_client(
        monkeypatch=monkeypatch,
        tmp_path=tmp_path,
        current_user=current_user,
        accessible_version=version,
    )

    response = client.post(
        f"/versions/{version.id}/artifact",
        files={"artifact": ("demo.flp", b"demo snapshot payload", "application/octet-stream")},
    )

    assert response.status_code == 409
    assert response.json()["detail"] == "Version artifact already exists"
    assert session.commit_calls == 0


def test_download_version_artifact_returns_404_when_file_is_missing(tmp_path, monkeypatch) -> None:
    current_user, version = _build_version()
    version.artifact_path = "projects/missing/branches/main/versions/missing/snapshot/demo.flp"
    version.source_project_filename = "demo.flp"

    client, _, _ = _create_test_client(
        monkeypatch=monkeypatch,
        tmp_path=tmp_path,
        current_user=current_user,
        accessible_version=version,
    )

    response = client.get(f"/versions/{version.id}/artifact")

    assert response.status_code == 404
    assert response.json()["detail"] == "Artifact file not found"


def test_download_version_artifact_maps_storage_errors_to_404(tmp_path, monkeypatch) -> None:
    current_user, version = _build_version()
    version.artifact_path = "gcs://demo-bucket/projects/demo/branches/main/versions/external/snapshot/missing.flp"

    class ErrorStorageService:
        def store_version_artifact(self, **kwargs):
            raise AssertionError("Store should not be called for download test")

        def resolve_artifact_path(self, artifact_path: str):
            del artifact_path
            raise StorageNotFoundError("Remote artifact not found")

    client, _, _ = _create_test_client(
        monkeypatch=monkeypatch,
        tmp_path=tmp_path,
        current_user=current_user,
        accessible_version=version,
        storage=ErrorStorageService(),
    )

    response = client.get(f"/versions/{version.id}/artifact")

    assert response.status_code == 404
    assert response.json()["detail"] == "Remote artifact not found"


def test_upload_version_artifact_returns_404_for_unowned_version(tmp_path, monkeypatch) -> None:
    current_user = _build_user()
    client, _, _ = _create_test_client(
        monkeypatch=monkeypatch,
        tmp_path=tmp_path,
        current_user=current_user,
        accessible_version=HTTPException(status_code=404, detail="Version not found"),
    )

    response = client.post(
        f"/versions/{uuid.uuid4()}/artifact",
        files={"artifact": ("demo.flp", b"demo snapshot payload", "application/octet-stream")},
    )

    assert response.status_code == 404
    assert response.json()["detail"] == "Version not found"
