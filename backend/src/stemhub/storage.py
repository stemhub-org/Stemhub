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

    @abstractmethod
    def store_project_preview(
        self,
        *,
        project_id: UUID,
        filename: str,
        source: BinaryIO,
    ) -> StoredArtifact:
        raise NotImplementedError

    @abstractmethod
    def has_project_preview(self, project_id: UUID) -> bool:
        raise NotImplementedError

    @abstractmethod
    def resolve_project_preview_path(self, project_id: UUID) -> Path:
        raise NotImplementedError

    @abstractmethod
    def delete_project_preview(self, project_id: UUID) -> bool:
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
        if not bucket_name:
            raise StorageConfigurationError("STEMHUB_GCS_BUCKET is required for STEMHUB_STORAGE_PROVIDER=gcs")

        if _gcs_storage is None:
            raise StorageConfigurationError("google-cloud-storage is required for STEMHUB_STORAGE_PROVIDER=gcs")

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

        if hasattr(source, "seek"):
            source.seek(0)

        blob = self._bucket.blob(artifact_path)
        
        # Stream the file-like object directly to Google Cloud Storage
        # This completely avoids writing the file to the local disk.
        blob.upload_from_file(source)
        
        # Reload to get the size and hash computed by GCS
        blob.reload()

        return StoredArtifact(
            path=self._to_reference_path(artifact_path),
            size_bytes=blob.size,
            checksum_sha256=blob.md5_hash, # We use GCS's native md5_hash instead of calculating SHA256 manually
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

    def store_project_preview(
        self,
        *,
        project_id: UUID,
        filename: str,
        source: BinaryIO,
    ) -> StoredArtifact:
        safe_filename = Path(filename).name or "preview.wav"
        prefix = self._build_project_preview_prefix(project_id)
        artifact_path = f"{prefix}/{safe_filename}"

        for old_blob in self._client.list_blobs(self._bucket_name, prefix=f"{prefix}/"):
            old_blob.delete()

        if hasattr(source, "seek"):
            source.seek(0)

        blob = self._bucket.blob(artifact_path)
        blob.upload_from_file(source)
        blob.reload()

        return StoredArtifact(
            path=self._to_reference_path(artifact_path),
            size_bytes=blob.size,
            checksum_sha256=blob.md5_hash,
        )

    def has_project_preview(self, project_id: UUID) -> bool:
        prefix = self._build_project_preview_prefix(project_id)
        return any(self._client.list_blobs(self._bucket_name, prefix=f"{prefix}/", max_results=1))

    def resolve_project_preview_path(self, project_id: UUID) -> Path:
        prefix = self._build_project_preview_prefix(project_id)
        blobs = list(self._client.list_blobs(self._bucket_name, prefix=f"{prefix}/"))
        if not blobs:
            raise StorageNotFoundError("Project preview not found")

        blobs.sort(key=lambda item: item.updated.timestamp() if item.updated else 0.0, reverse=True)
        blob = blobs[0]

        try:
            suffix = Path(blob.name).suffix or ".wav"
            temp_handle, temp_path = tempfile.mkstemp(prefix="stemhub-project-preview-", suffix=suffix)
            os.close(temp_handle)
            temp_file = Path(temp_path)
            blob.download_to_filename(str(temp_file))
            return temp_file
        except Exception as exc:
            if "temp_file" in locals() and temp_file.exists():
                temp_file.unlink()
            raise StorageNotFoundError(str(exc)) from exc

    def delete_project_preview(self, project_id: UUID) -> bool:
        prefix = self._build_project_preview_prefix(project_id)
        deleted = False
        for blob in self._client.list_blobs(self._bucket_name, prefix=f"{prefix}/"):
            blob.delete()
            deleted = True
        return deleted

    def _build_artifact_path(self, project_id: UUID, branch_id: UUID, version_id: UUID, filename: str) -> str:
        return (
            f"projects/{project_id}/branches/{branch_id}/versions/{version_id}/snapshot/{filename}"
        )

    def _build_project_preview_prefix(self, project_id: UUID) -> str:
        return f"projects/{project_id}/preview"

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

    def store_project_preview(
        self,
        *,
        project_id: UUID,
        filename: str,
        source: BinaryIO,
    ) -> StoredArtifact:
        safe_filename = Path(filename).name or "preview.wav"
        relative_dir = Path("projects") / str(project_id) / "preview"
        preview_dir = self.root / relative_dir
        preview_dir.mkdir(parents=True, exist_ok=True)

        for existing in preview_dir.iterdir():
            if existing.is_file():
                existing.unlink()

        destination = preview_dir / safe_filename
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
            path=(relative_dir / safe_filename).as_posix(),
            size_bytes=size_bytes,
            checksum_sha256=checksum.hexdigest(),
        )

    def has_project_preview(self, project_id: UUID) -> bool:
        preview_dir = self.root / "projects" / str(project_id) / "preview"
        if not preview_dir.is_dir():
            return False
        return any(item.is_file() for item in preview_dir.iterdir())

    def resolve_project_preview_path(self, project_id: UUID) -> Path:
        preview_dir = self.root / "projects" / str(project_id) / "preview"
        if not preview_dir.is_dir():
            raise StorageNotFoundError("Project preview not found")

        files = [item for item in preview_dir.iterdir() if item.is_file()]
        if not files:
            raise StorageNotFoundError("Project preview not found")

        files.sort(key=lambda item: item.stat().st_mtime, reverse=True)
        return files[0]

    def delete_project_preview(self, project_id: UUID) -> bool:
        preview_dir = self.root / "projects" / str(project_id) / "preview"
        if not preview_dir.is_dir():
            return False

        deleted = False
        for item in preview_dir.iterdir():
            if item.is_file():
                item.unlink()
                deleted = True

        try:
            preview_dir.rmdir()
        except OSError:
            pass

        return deleted


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
