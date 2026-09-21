from datetime import datetime, timezone
from typing import List
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Branch, Project, User
from stemhub.routers._project_access import (
    get_project_with_owner_access,
    get_project_with_read_access,
    get_project_with_write_access,
)
from stemhub.schemas import BranchCreate, BranchResponse, BranchUpdate

router = APIRouter(tags=["branches"])


async def _get_owned_branch(
    *, branch_id: UUID, current_user: User, db: AsyncSession
) -> Branch:
    result = await db.execute(
        select(Branch).join(Project).where(
            Branch.id == branch_id,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    branch = result.scalars().first()
    if branch is None:
        raise HTTPException(status_code=404, detail="Branch not found")
    await get_project_with_owner_access(
        project_id=branch.project_id, current_user=current_user, db=db
    )
    return branch


@router.post("/projects/{project_id}/branches/", response_model=BranchResponse, status_code=status.HTTP_201_CREATED)
async def create_branch(
    project_id: UUID,
    branch_in: BranchCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Create a new branch for a specific project."""
    project = await get_project_with_write_access(
        project_id=project_id, current_user=current_user, db=db
    )
    db_branch = Branch(**branch_in.model_dump(), project_id=project.id)
    db.add(db_branch)
    await db.commit()
    await db.refresh(db_branch)
    return db_branch


@router.get("/projects/{project_id}/branches/", response_model=List[BranchResponse])
async def list_branches(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """List all branches for a specific project."""
    project = await get_project_with_read_access(
        project_id=project_id, current_user=current_user, db=db
    )
    result = await db.execute(
        select(Branch).where(Branch.project_id == project.id, Branch.is_deleted == False)
    )
    return result.scalars().all()


@router.get("/branches/{branch_id}", response_model=BranchResponse)
async def get_branch(
    branch_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Get a specific branch by ID."""
    result = await db.execute(
        select(Branch).join(Project).where(
            Branch.id == branch_id,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    branch = result.scalars().first()
    if branch is None:
        raise HTTPException(status_code=404, detail="Branch not found")
    await get_project_with_read_access(
        project_id=branch.project_id, current_user=current_user, db=db
    )
    return branch


@router.put("/branches/{branch_id}", response_model=BranchResponse)
async def update_branch(
    branch_id: UUID,
    branch_in: BranchUpdate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Update a branch."""
    branch = await _get_owned_branch(branch_id=branch_id, current_user=current_user, db=db)

    for field, value in branch_in.model_dump(exclude_unset=True).items():
        setattr(branch, field, value)

    await db.commit()
    await db.refresh(branch)
    return branch


@router.delete("/branches/{branch_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_branch(
    branch_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Delete a branch."""
    branch = await _get_owned_branch(branch_id=branch_id, current_user=current_user, db=db)
    branch.is_deleted = True
    branch.deleted_at = datetime.now(timezone.utc)
    await db.commit()
