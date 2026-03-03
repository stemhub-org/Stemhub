from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from datetime import datetime, timezone
from typing import List
from uuid import UUID

from stemhub.database import get_db
from stemhub.models import Branch, Project, User
from stemhub.schemas import BranchCreate, BranchUpdate, BranchResponse
from stemhub.auth import get_current_user

router = APIRouter(tags=["branches"])

@router.post("/projects/{project_id}/branches/", response_model=BranchResponse, status_code=status.HTTP_201_CREATED)
async def create_branch(
    project_id: UUID,
    branch_in: BranchCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Create a new branch for a specific project.
    """
    # Verify the project exists and belongs to the user
    result = await db.execute(select(Project).where(Project.id == project_id, Project.owner_id == current_user.id, Project.is_deleted == False))
    project = result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found or you don't have access")

    db_branch = Branch(**branch_in.model_dump(), project_id=project_id)
    db.add(db_branch)
    await db.commit()
    await db.refresh(db_branch)
    return db_branch

@router.get("/projects/{project_id}/branches/", response_model=List[BranchResponse])
async def list_branches(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    List all branches for a specific project.
    """
    # Verify project access
    result = await db.execute(select(Project).where(Project.id == project_id, Project.owner_id == current_user.id, Project.is_deleted == False))
    if not result.scalars().first():
        raise HTTPException(status_code=404, detail="Project not found or you don't have access")

    result_branches = await db.execute(select(Branch).where(Branch.project_id == project_id, Branch.is_deleted == False))
    return result_branches.scalars().all()

@router.get("/branches/{branch_id}", response_model=BranchResponse)
async def get_branch(
    branch_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific branch by ID.
    """
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
        raise HTTPException(status_code=404, detail="Branch not found")
    return branch

@router.put("/branches/{branch_id}", response_model=BranchResponse)
async def update_branch(
    branch_id: UUID,
    branch_in: BranchUpdate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Update a branch.
    """
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
        raise HTTPException(status_code=404, detail="Branch not found")

    update_data = branch_in.model_dump(exclude_unset=True)
    for field, value in update_data.items():
        setattr(branch, field, value)

    await db.commit()
    await db.refresh(branch)
    return branch

@router.delete("/branches/{branch_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_branch(
    branch_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Delete a branch.
    """
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
        raise HTTPException(status_code=404, detail="Branch not found")

    branch.is_deleted = True
    branch.deleted_at = datetime.now(timezone.utc)
    await db.commit()
