"""Content-addressed blob endpoints.

See docs/content-addressed-storage.md for the design rationale.
"""
from uuid import UUID

from fastapi import APIRouter, Depends, File, HTTPException, UploadFile, status
from fastapi.responses import FileResponse
from pydantic import BaseModel
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Blob, Collaborator, Project, User
from stemhub.storage import StorageNotFoundError, StorageService, get_storage_service

router = APIRouter(prefix="/projects/{project_id}/blobs", tags=["blobs"])


class CheckMissingRequest(BaseModel):
    sha256s: list[str]


class CheckMissingResponse(BaseModel):
    missing: list[str]


class BlobResponse(BaseModel):
    sha256: str
    size_bytes: int
    mime_type: str | None = None


async def _get_project_with_write_access(
    *, project_id: UUID, current_user: User, db: AsyncSession
) -> Project:
    result = await db.execute(
        select(Project).where(
            Project.id == project_id,
            Project.is_deleted == False,
        )
    )
    project = result.scalar_one_or_none()
    if project is None:
        raise HTTPException(status_code=404, detail="Project not found")

    if project.owner_id == current_user.id:
        return project

    collab = await db.execute(
        select(Collaborator).where(
            Collaborator.project_id == project.id,
            Collaborator.user_id == current_user.id,
            Collaborator.role.in_(["Admin", "Editor"]),
        )
    )
    if collab.scalars().first() is None:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


async def _get_project_with_read_access(
    *, project_id: UUID, current_user: User, db: AsyncSession
) -> Project:
    result = await db.execute(
        select(Project).where(
            Project.id == project_id,
            Project.is_deleted == False,
        )
    )
    project = result.scalar_one_or_none()
    if project is None:
        raise HTTPException(status_code=404, detail="Project not found")

    if project.is_public or project.owner_id == current_user.id:
        return project

    collab = await db.execute(
        select(Collaborator).where(
            Collaborator.project_id == project.id,
            Collaborator.user_id == current_user.id,
        )
    )
    if collab.scalars().first() is None:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


def _validate_sha256(candidate: str) -> str:
    normalized = candidate.strip().lower()
    if len(normalized) != 64 or any(c not in "0123456789abcdef" for c in normalized):
        raise HTTPException(status_code=400, detail=f"Invalid sha256: {candidate!r}")
    return normalized


@router.post("/check-missing", response_model=CheckMissingResponse)
async def check_missing(
    project_id: UUID,
    payload: CheckMissingRequest,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
) -> CheckMissingResponse:
    await _get_project_with_write_access(
        project_id=project_id, current_user=current_user, db=db
    )
    requested = [_validate_sha256(s) for s in payload.sha256s]
    if not requested:
        return CheckMissingResponse(missing=[])

    result = await db.execute(
        select(Blob.sha256).where(
            Blob.project_id == project_id,
            Blob.sha256.in_(requested),
        )
    )
    present = {row[0] for row in result.all()}
    return CheckMissingResponse(missing=[s for s in requested if s not in present])


@router.put("/{sha256}", response_model=BlobResponse)
async def upload_blob(
    project_id: UUID,
    sha256: str,
    file: UploadFile = File(...),
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
) -> BlobResponse:
    expected_sha = _validate_sha256(sha256)
    await _get_project_with_write_access(
        project_id=project_id, current_user=current_user, db=db
    )

    existing = await db.execute(
        select(Blob).where(Blob.project_id == project_id, Blob.sha256 == expected_sha)
    )
    existing_blob = existing.scalar_one_or_none()
    if existing_blob is not None:
        return BlobResponse(
            sha256=existing_blob.sha256,
            size_bytes=existing_blob.size_bytes,
            mime_type=existing_blob.mime_type,
        )

    stored = storage.store_blob(project_id=project_id, source=file.file)
    if stored.checksum_sha256 != expected_sha:
        # Integrity mismatch: client claimed X, bytes hashed to Y.
        storage.delete_blob(stored.path)
        raise HTTPException(
            status_code=400,
            detail=f"sha256 mismatch: expected {expected_sha}, got {stored.checksum_sha256}",
        )

    blob = Blob(
        project_id=project_id,
        sha256=expected_sha,
        size_bytes=stored.size_bytes,
        mime_type=file.content_type,
        storage_uri=stored.path,
        ref_count=0,
    )
    db.add(blob)
    await db.commit()

    return BlobResponse(
        sha256=blob.sha256,
        size_bytes=blob.size_bytes,
        mime_type=blob.mime_type,
    )


@router.get("/{sha256}")
async def download_blob(
    project_id: UUID,
    sha256: str,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    expected_sha = _validate_sha256(sha256)
    await _get_project_with_read_access(
        project_id=project_id, current_user=current_user, db=db
    )

    result = await db.execute(
        select(Blob).where(Blob.project_id == project_id, Blob.sha256 == expected_sha)
    )
    blob = result.scalar_one_or_none()
    if blob is None:
        raise HTTPException(status_code=404, detail="Blob not found")

    try:
        path = storage.resolve_blob_path(blob.storage_uri)
    except StorageNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc))

    return FileResponse(
        path=str(path),
        media_type=blob.mime_type or "application/octet-stream",
        headers={"X-Blob-SHA256": blob.sha256},
    )
