import hashlib
import io
import logging
import types
from pathlib import Path
from uuid import UUID

import pytest

from stemhub import storage as storage_module
from stemhub.storage import (
    GCSStorageService,
    LocalFilesystemStorageService,
    StorageConfigurationError,
    StorageNotFoundError,
    get_storage_service,
)


@pytest.fixture
def fresh_deprecation_warning():
    # The deprecated root variable is warned about once per process; start from scratch.
    storage_module._warn_that_the_legacy_root_env_is_deprecated.cache_clear()
    yield
    storage_module._warn_that_the_legacy_root_env_is_deprecated.cache_clear()


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
    monkeypatch.delenv("STEMHUB_ARTIFACTS_ROOT", raising=False)
    monkeypatch.setenv("STEMHUB_STORAGE_ROOT", str(tmp_path / "storage"))
    storage = get_storage_service()
    assert isinstance(storage, LocalFilesystemStorageService)
    assert storage.root.name == "storage"


def test_get_storage_service_localfs_uses_configured_root(tmp_path, monkeypatch) -> None:
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "localfs")
    monkeypatch.delenv("STEMHUB_ARTIFACTS_ROOT", raising=False)
    monkeypatch.setenv("STEMHUB_STORAGE_ROOT", str(tmp_path / "custom-storage"))

    storage = get_storage_service()

    assert isinstance(storage, LocalFilesystemStorageService)
    assert storage.root == (tmp_path / "custom-storage").resolve()


def test_get_storage_service_falls_back_to_the_deprecated_root_variable(tmp_path, monkeypatch, caplog, fresh_deprecation_warning) -> None:
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "localfs")
    monkeypatch.delenv("STEMHUB_STORAGE_ROOT", raising=False)
    monkeypatch.setenv("STEMHUB_ARTIFACTS_ROOT", str(tmp_path / "legacy-root"))

    with caplog.at_level(logging.WARNING, logger="stemhub.storage"):
        storage = get_storage_service()

    assert isinstance(storage, LocalFilesystemStorageService)
    assert storage.root == (tmp_path / "legacy-root").resolve()
    assert any(
        "STEMHUB_ARTIFACTS_ROOT" in record.getMessage() and "STEMHUB_STORAGE_ROOT" in record.getMessage()
        for record in caplog.records
    )


def test_the_deprecated_root_variable_is_warned_about_once_per_process(tmp_path, monkeypatch, caplog, fresh_deprecation_warning) -> None:
    # get_storage_service runs on every request, so warning each time would flood the logs.
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "localfs")
    monkeypatch.delenv("STEMHUB_STORAGE_ROOT", raising=False)
    monkeypatch.setenv("STEMHUB_ARTIFACTS_ROOT", str(tmp_path / "legacy-root"))

    with caplog.at_level(logging.WARNING, logger="stemhub.storage"):
        first = get_storage_service()
        second = get_storage_service()

    assert first.root == second.root == (tmp_path / "legacy-root").resolve()
    deprecation_warnings = [
        record for record in caplog.records if "STEMHUB_ARTIFACTS_ROOT" in record.getMessage()
    ]
    assert len(deprecation_warnings) == 1


def test_get_storage_service_prefers_the_new_root_variable(tmp_path, monkeypatch) -> None:
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "localfs")
    monkeypatch.setenv("STEMHUB_STORAGE_ROOT", str(tmp_path / "new-root"))
    monkeypatch.setenv("STEMHUB_ARTIFACTS_ROOT", str(tmp_path / "legacy-root"))

    storage = get_storage_service()

    assert storage.root == (tmp_path / "new-root").resolve()


def test_get_storage_service_keeps_the_default_directory_so_existing_blobs_are_found(monkeypatch) -> None:
    # Blobs already stored under the historical default must stay reachable.
    backend_root = Path(__file__).resolve().parents[1]
    monkeypatch.setattr(LocalFilesystemStorageService, "__init__", _record_root_only)
    monkeypatch.setenv("STEMHUB_STORAGE_PROVIDER", "localfs")
    monkeypatch.delenv("STEMHUB_STORAGE_ROOT", raising=False)
    monkeypatch.delenv("STEMHUB_ARTIFACTS_ROOT", raising=False)

    storage = get_storage_service()

    assert storage.root == backend_root / "data" / "artifacts"


def _record_root_only(self, root: Path) -> None:
    # Skips the mkdir so the test never creates the real default directory.
    self.root = root


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
