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


def test_local_filesystem_storage_persists_blob_with_content_addressed_path(tmp_path) -> None:
    storage = LocalFilesystemStorageService(tmp_path)
    payload = b"demo blob payload"
    project_id = UUID("11111111-1111-1111-1111-111111111111")

    stored = storage.store_blob(project_id=project_id, source=io.BytesIO(payload))

    expected_sha = hashlib.sha256(payload).hexdigest()
    expected_relative_path = f"projects/{project_id}/blobs/{expected_sha[:2]}/{expected_sha}"

    assert stored.path == expected_relative_path
    assert stored.size_bytes == len(payload)
    assert stored.checksum_sha256 == expected_sha
    assert storage.resolve_blob_path(stored.path).read_bytes() == payload


def test_local_filesystem_storage_rejects_path_traversal(tmp_path) -> None:
    storage = LocalFilesystemStorageService(tmp_path)

    with pytest.raises(StorageNotFoundError):
        storage.resolve_blob_path("../outside.flp")


def test_get_storage_service_defaults_to_localfs(tmp_path, monkeypatch) -> None:
    monkeypatch.delenv("STEMHUB_STORAGE_PROVIDER", raising=False)
    monkeypatch.setenv("STEMHUB_ARTIFACTS_ROOT", str(tmp_path / "artifacts"))
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
            self.size: int | None = None
            self.md5_hash: str | None = None

        def upload_from_filename(self, source_path: str) -> None:
            with open(source_path, "rb") as file:
                uploaded_blobs[self.path] = file.read()

        def upload_from_file(self, source_file, size=None) -> None:
            del size
            uploaded_blobs[self.path] = source_file.read()

        def reload(self) -> None:
            payload = uploaded_blobs[self.path]
            self.size = len(payload)
            self.md5_hash = hashlib.md5(payload).hexdigest()

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

    payload = b"demo gcs blob"
    project_id = UUID("11111111-1111-1111-1111-111111111111")
    stored = storage.store_blob(project_id=project_id, source=io.BytesIO(payload))

    expected_sha = hashlib.sha256(payload).hexdigest()
    assert stored.path.startswith(
        f"gcs://test-bucket/projects/{project_id}/blobs/{expected_sha[:2]}/{expected_sha}"
    )
    assert stored.size_bytes == len(payload)

    downloaded = storage.resolve_blob_path(stored.path)
    assert downloaded.read_bytes() == payload
    downloaded.unlink()
