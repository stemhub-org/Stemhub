from uuid import UUID

from fastapi import APIRouter, Depends, File, HTTPException, UploadFile, status
from fastapi.responses import FileResponse
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Branch, Collaborator, Project, User, Version
from stemhub.schemas import VersionResponse
from stemhub.storage import StorageNotFoundError, StorageService, get_storage_service

router = APIRouter(tags=["files"])


async def _get_version_with_access(
    *,
    version_id: UUID,
    current_user: User,
    db: AsyncSession,
    owner_only: bool = False,
) -> Version:
    result = await db.execute(
        select(Version, Branch, Project)
        .join(Branch, Version.branch_id == Branch.id)
        .join(Project, Branch.project_id == Project.id)
        .where(
            Version.id == version_id,
            Project.owner_id == current_user.id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    row = result.first()
    if not row:
        raise HTTPException(status_code=404, detail="Version not found")

    version, branch, project = row

    if owner_only and project.owner_id != current_user.id:
        raise HTTPException(status_code=404, detail="Version not found")

    if not owner_only and project.owner_id != current_user.id:
        collab_result = await db.execute(
            select(Collaborator).where(
                Collaborator.project_id == project.id,
                Collaborator.user_id == current_user.id,
            )
        )
        if not collab_result.scalars().first():
            raise HTTPException(status_code=404, detail="Version not found")

    version.branch = branch
    branch.project = project
    return version


@router.post("/versions/{version_id}/artifact", response_model=VersionResponse, status_code=status.HTTP_200_OK)
async def upload_artifact(
    version_id: UUID,
    artifact: UploadFile = File(...),
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    version = await _get_version_with_access(
        version_id=version_id,
        current_user=current_user,
        db=db,
        owner_only=True,
    )

    if not artifact.filename:
        raise HTTPException(status_code=400, detail="Uploaded artifact must include a filename")

    if version.artifact_path:
        raise HTTPException(status_code=409, detail="Version artifact already exists")

    stored = storage.store_version_artifact(
        project_id=version.branch.project.id,
        branch_id=version.branch_id,
        version_id=version.id,
        filename=artifact.filename,
        source=artifact.file,
    )

    version.artifact_path = stored.path
    version.artifact_size_bytes = stored.size_bytes
    version.artifact_checksum = stored.checksum_sha256
    if not version.source_project_filename:
        version.source_project_filename = artifact.filename

    await db.commit()
    await db.refresh(version)
    await artifact.close()
    return version


@router.get("/versions/{version_id}/artifact")
async def download_artifact(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    version = await _get_version_with_access(
        version_id=version_id,
        current_user=current_user,
        db=db,
    )

    if not version.artifact_path:
        raise HTTPException(status_code=404, detail="Version artifact not found")

    try:
        artifact_path = storage.resolve_artifact_path(version.artifact_path)
    except StorageNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc

    return FileResponse(
        path=artifact_path,
        media_type="application/octet-stream",
        filename=version.source_project_filename or artifact_path.name,
    )
