import hashlib
import os
from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO
from uuid import UUID


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
    artifact_root = Path(os.getenv("STEMHUB_ARTIFACTS_ROOT", str(_default_artifact_root())))

    if provider == "localfs":
        return LocalFilesystemStorageService(artifact_root)

    raise StorageConfigurationError(f"Unsupported storage provider: {provider}")
