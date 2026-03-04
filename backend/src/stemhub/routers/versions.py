from fastapi import APIRouter, Depends, HTTPException, status
from datetime import datetime, timezone
from typing import List
from uuid import UUID

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.database import get_db
from stemhub.models import Version, Branch, Project, User
from stemhub.schemas import VersionCreate, VersionUpdate, VersionResponse
from stemhub.auth import get_current_user

router = APIRouter(tags=["versions"])

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

    db_version = Version(**version_in.model_dump(), branch_id=branch_id)
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
            Project.owner_id == current_user.id,
            Branch.is_deleted == False,
            Project.is_deleted == False
        )
    )
    if not result.scalars().first():
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
    return version

@router.put("/versions/{version_id}", response_model=VersionResponse)
async def update_version(
    version_id: UUID,
    version_in: VersionUpdate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Update mutable version metadata.
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

    update_data = version_in.model_dump(exclude_unset=True)
    for field, value in update_data.items():
        setattr(version, field, value)

    await db.commit()
    await db.refresh(version)
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
