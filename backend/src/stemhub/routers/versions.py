from fastapi import APIRouter, Depends, HTTPException, status
from datetime import datetime, timezone
from typing import List
from uuid import UUID

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.database import get_db
from stemhub.models import Collaborator, Version, Branch, Project, User
from stemhub.schemas import VersionCreate, VersionResponse
from stemhub.auth import get_current_user

router = APIRouter(tags=["versions"])


async def _can_access_project(
    *,
    project_id: UUID,
    current_user: User,
    db: AsyncSession,
) -> bool:
    project_result = await db.execute(
        select(Project).where(
            Project.id == project_id,
            Project.is_deleted == False,
        )
    )
    project = project_result.scalars().first()
    if not project:
        return False

    if project.owner_id == current_user.id:
        return True

    collaborator_result = await db.execute(
        select(Collaborator).where(
            Collaborator.project_id == project_id,
            Collaborator.user_id == current_user.id,
        )
    )
    return collaborator_result.scalars().first() is not None

@router.post("/branches/{branch_id}/versions/", response_model=VersionResponse, status_code=status.HTTP_201_CREATED)
async def create_version(
    branch_id: UUID,
    version_in: VersionCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Create a new version (commit) for a specific branch.
    """
    # Verify the branch exists and the user has access to its project
    result = await db.execute(
        select(Branch).join(Project).where(
            Branch.id == branch_id,
            Project.owner_id == current_user.id,
            Branch.is_deleted == False,
            Project.is_deleted == False
        )
    )
    branch = result.scalars().first()
    if not branch:
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")

    db_version = Version(**version_in.model_dump(), branch_id=branch_id, created_by=current_user.id)
    db.add(db_version)
    await db.commit()
    await db.refresh(db_version)
    return db_version

@router.get("/branches/{branch_id}/versions/", response_model=List[VersionResponse])
async def list_versions(
    branch_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    List all versions for a specific branch.
    """
    # Verify branch access
    result = await db.execute(
        select(Branch).join(Project).where(
            Branch.id == branch_id,
            Branch.is_deleted == False,
            Project.is_deleted == False
        )
    )
    branch = result.scalars().first()
    if not branch:
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")
    if not await _can_access_project(project_id=branch.project_id, current_user=current_user, db=db):
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")

    result_versions = await db.execute(select(Version).where(Version.branch_id == branch_id, Version.is_deleted == False))
    return result_versions.scalars().all()

@router.get("/versions/{version_id}", response_model=VersionResponse)
async def get_version(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific version by ID.
    """
    result = await db.execute(
        select(Version, Branch.project_id).join(Branch).join(Project).where(
            Version.id == version_id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False
        )
    )
    row = result.first()
    if not row:
        raise HTTPException(status_code=404, detail="Version not found")
    version, project_id = row
    if not await _can_access_project(project_id=project_id, current_user=current_user, db=db):
        raise HTTPException(status_code=404, detail="Version not found")
    return version

@router.delete("/versions/{version_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_version(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Delete a version.
    """
    result = await db.execute(
        select(Version).join(Branch).join(Project).where(
            Version.id == version_id,
            Project.owner_id == current_user.id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False
        )
    )
    version = result.scalars().first()
    if not version:
        raise HTTPException(status_code=404, detail="Version not found")

    version.is_deleted = True
    version.deleted_at = datetime.now(timezone.utc)
    await db.commit()
