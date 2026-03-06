import hashlib
import json
import os
import tempfile
from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO
from uuid import UUID

try:
    from google.cloud import storage as _gcs_storage
except ImportError:  # pragma: no cover - exercised via integration/configuration tests
    _gcs_storage = None


class StorageError(Exception):
    pass


class StorageNotFoundError(StorageError):
    pass


class StorageConfigurationError(StorageError):
    pass


@dataclass(frozen=True)
class StoredArtifact:
    path: str
    size_bytes: int
    checksum_sha256: str


class StorageService(ABC):
    @abstractmethod
    def store_version_artifact(
        self,
        *,
        project_id: UUID,
        branch_id: UUID,
        version_id: UUID,
        filename: str,
        source: BinaryIO,
    ) -> StoredArtifact:
        raise NotImplementedError

    @abstractmethod
    def resolve_artifact_path(self, artifact_path: str) -> Path:
        raise NotImplementedError


class GCSStorageService(StorageService):
    _GCS_SCHEME = "gcs://"

    def __init__(
        self,
        *,
        bucket_name: str,
        project: str | None = None,
        credentials_json: str | None = None,
    ) -> None:
        if _gcs_storage is None:
            raise StorageConfigurationError("google-cloud-storage is required for STEMHUB_STORAGE_PROVIDER=gcs")

        if not bucket_name:
            raise StorageConfigurationError("STEMHUB_GCS_BUCKET is required for STEMHUB_STORAGE_PROVIDER=gcs")

        self._bucket_name = bucket_name

        if credentials_json:
            try:
                credentials = json.loads(credentials_json)
                self._client = _gcs_storage.Client.from_service_account_info(
                    credentials,
                    project=project,
                )
            except (TypeError, ValueError, KeyError) as exc:
                raise StorageConfigurationError("Invalid STEMHUB_GCS_CREDENTIALS_JSON payload") from exc
        else:
            self._client = _gcs_storage.Client(project=project)

        self._bucket = self._client.bucket(bucket_name)

    def store_version_artifact(
        self,
        *,
        project_id: UUID,
        branch_id: UUID,
        version_id: UUID,
        filename: str,
        source: BinaryIO,
    ) -> StoredArtifact:
        safe_filename = Path(filename).name or "snapshot.bin"
        artifact_path = self._build_artifact_path(project_id, branch_id, version_id, safe_filename)
        checksum = hashlib.sha256()
        size_bytes = 0

        if hasattr(source, "seek"):
            source.seek(0)

        with tempfile.NamedTemporaryFile(delete=False) as buffer:
            temp_path = Path(buffer.name)
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk:
                    break
                checksum.update(chunk)
                size_bytes += len(chunk)
                buffer.write(chunk)

        try:
            blob = self._bucket.blob(artifact_path)
            blob.upload_from_filename(str(temp_path))
        finally:
            if temp_path.exists():
                temp_path.unlink()

        return StoredArtifact(
            path=self._to_reference_path(artifact_path),
            size_bytes=size_bytes,
            checksum_sha256=checksum.hexdigest(),
        )

    def resolve_artifact_path(self, artifact_path: str) -> Path:
        blob_path = self._to_blob_path(artifact_path)
        blob = self._bucket.blob(blob_path)

        try:
            temp_handle, temp_path = tempfile.mkstemp(prefix="stemhub-artifact-")
            os.close(temp_handle)
            temp_file = Path(temp_path)
            if not blob.exists():
                raise StorageNotFoundError("Artifact file not found")
            blob.download_to_filename(str(temp_file))
            return temp_file
        except StorageNotFoundError:
            raise
        except Exception as exc:
            if "temp_file" in locals() and temp_file.exists():
                temp_file.unlink()
            raise StorageNotFoundError(str(exc)) from exc

    def _build_artifact_path(self, project_id: UUID, branch_id: UUID, version_id: UUID, filename: str) -> str:
        return (
            f"projects/{project_id}/branches/{branch_id}/versions/{version_id}/snapshot/{filename}"
        )

    def _to_reference_path(self, artifact_path: str) -> str:
        return f"{self._GCS_SCHEME}{self._bucket_name}/{artifact_path}"

    def _to_blob_path(self, artifact_path: str) -> str:
        reference_prefix = f"{self._GCS_SCHEME}{self._bucket_name}/"
        if artifact_path.startswith(reference_prefix):
            return artifact_path[len(reference_prefix):]
        return artifact_path


class LocalFilesystemStorageService(StorageService):
    def __init__(self, root: Path) -> None:
        self.root = root.resolve()
        self.root.mkdir(parents=True, exist_ok=True)

    def store_version_artifact(
        self,
        *,
        project_id: UUID,
        branch_id: UUID,
        version_id: UUID,
        filename: str,
        source: BinaryIO,
    ) -> StoredArtifact:
        safe_filename = Path(filename).name or "snapshot.bin"
        relative_path = (
            Path("projects")/ str(project_id)
            / "branches"/ str(branch_id)
            / "versions"/ str(version_id)
            / "snapshot"/ safe_filename
        )
        destination = self.root / relative_path
        destination.parent.mkdir(parents=True, exist_ok=True)

        checksum = hashlib.sha256()
        size_bytes = 0

        if hasattr(source, "seek"):
            source.seek(0)

        with destination.open("wb") as output:
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk:
                    break
                checksum.update(chunk)
                size_bytes += len(chunk)
                output.write(chunk)

        return StoredArtifact(
            path=relative_path.as_posix(),
            size_bytes=size_bytes,
            checksum_sha256=checksum.hexdigest(),
        )

    def resolve_artifact_path(self, artifact_path: str) -> Path:
        candidate = (self.root / artifact_path).resolve()
        try:
            candidate.relative_to(self.root)
        except ValueError as exc:
            raise StorageNotFoundError("Artifact path is outside of the storage root") from exc

        if not candidate.is_file():
            raise StorageNotFoundError("Artifact file not found")

        return candidate


def _default_artifact_root() -> Path:
    backend_root = Path(__file__).resolve().parents[2]
    return backend_root / "data" / "artifacts"


def get_storage_service() -> StorageService:
    provider = os.getenv("STEMHUB_STORAGE_PROVIDER", "localfs").strip().lower()

    if provider == "localfs":
        artifact_root = Path(os.getenv("STEMHUB_ARTIFACTS_ROOT", str(_default_artifact_root())))
        return LocalFilesystemStorageService(artifact_root)

    if provider == "gcs":
        return GCSStorageService(
            bucket_name=os.getenv("STEMHUB_GCS_BUCKET", "").strip(),
            project=os.getenv("STEMHUB_GCS_PROJECT")
            or os.getenv("GOOGLE_CLOUD_PROJECT"),
            credentials_json=os.getenv("STEMHUB_GCS_CREDENTIALS_JSON"),
        )

    raise StorageConfigurationError(f"Unsupported storage provider: {provider}")
