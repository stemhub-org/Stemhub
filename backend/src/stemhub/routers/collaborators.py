from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from sqlalchemy.orm import selectinload
from typing import List
from uuid import UUID

from stemhub.database import get_db
from stemhub.models import Collaborator, Project, User
from stemhub.schemas import CollaboratorCreate, CollaboratorResponse
from stemhub.auth import get_current_user

router = APIRouter(tags=["collaborators"])


async def _get_owned_project(
    *,
    project_id: UUID,
    current_user: User,
    db: AsyncSession,
) -> Project:
    result = await db.execute(
        select(Project).where(
            Project.id == project_id,
            Project.owner_id == current_user.id,
            Project.is_deleted == False,
        )
    )
    project = result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found or you don't have access")
    return project


@router.post("/projects/{project_id}/collaborators/", response_model=CollaboratorResponse, status_code=status.HTTP_201_CREATED)
async def add_collaborator(
    project_id: UUID,
    collab_in: CollaboratorCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Add a collaborator to a project. Only the project owner can add collaborators.
    """
    await _get_owned_project(project_id=project_id, current_user=current_user, db=db)

    # Check the target user exists
    result = await db.execute(select(User).where(User.username == collab_in.username))
    target_user = result.scalars().first()
    if not target_user:
        raise HTTPException(status_code=404, detail="User not found")

    # Cannot add yourself as collaborator
    if target_user.id == current_user.id:
        raise HTTPException(status_code=400, detail="Cannot add yourself as a collaborator")

    # Check if already a collaborator
    existing = await db.execute(
        select(Collaborator).where(
            Collaborator.project_id == project_id,
            Collaborator.user_id == target_user.id,
        )
    )
    if existing.scalars().first():
        raise HTTPException(status_code=409, detail="User is already a collaborator")

    db_collab = Collaborator(
        project_id=project_id,
        user_id=target_user.id,
        role=collab_in.role or "Viewer",
    )
    db.add(db_collab)
    await db.commit()
    await db.refresh(db_collab)

    # Reload with user relationship for the response
    result = await db.execute(
        select(Collaborator)
        .options(selectinload(Collaborator.user))
        .where(
            Collaborator.project_id == project_id,
            Collaborator.user_id == target_user.id,
        )
    )
    return result.scalars().first()


@router.get("/projects/{project_id}/collaborators/", response_model=List[CollaboratorResponse])
async def list_collaborators(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    List all collaborators for a project.
    Accessible by the project owner or any collaborator of the project.
    """
    # Check if user is owner or collaborator
    project_result = await db.execute(
        select(Project).where(Project.id == project_id, Project.is_deleted == False)
    )
    project = project_result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")

    if project.owner_id != current_user.id:
        collab_result = await db.execute(
            select(Collaborator).where(
                Collaborator.project_id == project_id,
                Collaborator.user_id == current_user.id,
            )
        )
        if not collab_result.scalars().first():
            raise HTTPException(status_code=403, detail="Access denied")

    result = await db.execute(
        select(Collaborator)
        .options(selectinload(Collaborator.user))
        .where(Collaborator.project_id == project_id)
    )
    return result.scalars().all()


@router.delete("/projects/{project_id}/collaborators/{user_id}", status_code=status.HTTP_204_NO_CONTENT)
async def remove_collaborator(
    project_id: UUID,
    user_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Remove a collaborator from a project. Only the project owner can remove collaborators.
    """
    await _get_owned_project(project_id=project_id, current_user=current_user, db=db)

    result = await db.execute(
        select(Collaborator).where(
            Collaborator.project_id == project_id,
            Collaborator.user_id == user_id,
        )
    )
    collab = result.scalars().first()
    if not collab:
        raise HTTPException(status_code=404, detail="Collaborator not found")

    await db.delete(collab)
    await db.commit()
