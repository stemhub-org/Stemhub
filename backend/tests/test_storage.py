import hashlib
import io
import types
from uuid import UUID

import pytest

from stemhub.storage import (
    GCSStorageService,
    LocalFilesystemStorageService,
    StorageConfigurationError,
    StorageNotFoundError,
    get_storage_service,
)


def test_local_filesystem_storage_persists_artifact_with_expected_metadata(tmp_path) -> None:
    storage = LocalFilesystemStorageService(tmp_path)
    payload = b"demo snapshot payload"

    stored = storage.store_version_artifact(
        project_id="11111111-1111-1111-1111-111111111111",
        branch_id="22222222-2222-2222-2222-222222222222",
        version_id="33333333-3333-3333-3333-333333333333",
        filename="../unsafe-demo.flp",
        source=io.BytesIO(payload),
    )

    expected_relative_path = (
        "projects/11111111-1111-1111-1111-111111111111/"
        "branches/22222222-2222-2222-2222-222222222222/"
        "versions/33333333-3333-3333-3333-333333333333/"
        "snapshot/unsafe-demo.flp"
    )

    assert stored.path == expected_relative_path
    assert stored.size_bytes == len(payload)
    assert stored.checksum_sha256 == hashlib.sha256(payload).hexdigest()
    assert storage.resolve_artifact_path(stored.path).read_bytes() == payload


def test_local_filesystem_storage_rejects_path_traversal(tmp_path) -> None:
    storage = LocalFilesystemStorageService(tmp_path)

    with pytest.raises(StorageNotFoundError):
        storage.resolve_artifact_path("../outside.flp")


def test_get_storage_service_defaults_to_localfs(tmp_path) -> None:
    storage = get_storage_service()
    assert isinstance(storage, LocalFilesystemStorageService)
    assert storage.root.name == "artifacts"


def test_get_storage_service_localfs_uses_configured_root(tmp_path, monkeypatch) -> None:
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "localfs")
    monkeypatch.setenv("STEMHUB_ARTIFACTS_ROOT", str(tmp_path / "custom-artifacts"))

    storage = get_storage_service()

    assert isinstance(storage, LocalFilesystemStorageService)
    assert storage.root == (tmp_path / "custom-artifacts").resolve()


def test_get_storage_service_rejects_unknown_provider(monkeypatch) -> None:
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "unknown")

    with pytest.raises(StorageConfigurationError, match="Unsupported storage provider"):
        get_storage_service()


def test_get_storage_service_raises_when_gcs_bucket_missing(monkeypatch) -> None:
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "gcs")
    monkeypatch.setenv("STEMHUB_GCS_BUCKET", "")

    with pytest.raises(StorageConfigurationError, match="STEMHUB_GCS_BUCKET"):
        get_storage_service()


def test_gcs_storage_service_rejects_invalid_credentials_json(monkeypatch) -> None:
    monkeypatch.setattr("stemhub.storage._gcs_storage", types.SimpleNamespace(Client=object()), raising=False)
    with pytest.raises(StorageConfigurationError, match="Invalid STEMHUB_GCS_CREDENTIALS_JSON"):
        GCSStorageService(
            bucket_name="demo-bucket",
            credentials_json="{invalid-json",
        )


def test_gcs_storage_service_roundtrip(monkeypatch) -> None:
    uploaded_blobs: dict[str, bytes] = {}

    class FakeBlob:
        def __init__(self, path: str) -> None:
            self.path = path

        def upload_from_filename(self, source_path: str) -> None:
            with open(source_path, "rb") as file:
                uploaded_blobs[self.path] = file.read()

        def exists(self) -> bool:
            return self.path in uploaded_blobs

        def download_to_filename(self, destination_path: str) -> None:
            content = uploaded_blobs.get(self.path)
            if content is None:
                raise FileNotFoundError(f"Missing blob: {self.path}")
            with open(destination_path, "wb") as out:
                out.write(content)

    class FakeBucket:
        def __init__(self, name: str) -> None:
            self.name = name

        def blob(self, path: str) -> FakeBlob:
            return FakeBlob(path)

    class PathReader:
        @staticmethod
        def read(path: str) -> bytes:
            with open(path, "rb") as file:
                return file.read()

    class FakeClient:
        def __init__(self, *args, **kwargs) -> None:
            del args, kwargs

        @classmethod
        def from_service_account_info(cls, credentials, project=None):
            return cls(project=project, credentials=credentials)

        def bucket(self, bucket_name: str) -> FakeBucket:
            return FakeBucket(bucket_name)

    fake_storage_module = types.SimpleNamespace(Client=FakeClient)
    monkeypatch.setattr("stemhub.storage._gcs_storage", fake_storage_module, raising=False)
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "gcs")
    monkeypatch.setenv("STEMHUB_GCS_BUCKET", "test-bucket")

    storage = get_storage_service()
    assert isinstance(storage, GCSStorageService)

    payload = b"demo gcs snapshot"
    artifact = storage.store_version_artifact(
        project_id=UUID("11111111-1111-1111-1111-111111111111"),
        branch_id=UUID("22222222-2222-2222-2222-222222222222"),
        version_id=UUID("33333333-3333-3333-3333-333333333333"),
        filename="demo.flp",
        source=io.BytesIO(payload),
    )

    assert artifact.path.startswith("gcs://test-bucket/projects/11111111-1111-1111-1111-111111111111/")
    assert artifact.size_bytes == len(payload)

    downloaded = storage.resolve_artifact_path(artifact.path)
    assert downloaded.read_bytes() == payload
    downloaded.unlink()
