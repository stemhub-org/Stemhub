"""Shared project-access helpers for routers.

Three variants:

- ``get_project_with_owner_access``: for endpoints that require ownership
  (rename, delete, invite collaborators). Only the owner passes.

- ``get_project_with_write_access``: for endpoints that mutate project data
  (blob uploads, version creates, branch mutations). Owner or collaborator
  with role in {Admin, Editor}.

- ``get_project_with_read_access``: for endpoints that read project data.
  Public projects are readable by any authenticated user. Otherwise: owner
  or any collaborator.

All three return **404** (privacy-hiding) rather than 403, so a non-member
cannot probe whether a given project id exists. This matches
SPECIFICATION.md §9.3 scenario 6.
"""
from uuid import UUID

from fastapi import HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.models import Collaborator, Project, User


_WRITE_ROLES = ("Admin", "Editor")


async def _load_project(project_id: UUID, db: AsyncSession) -> Project:
    result = await db.execute(
        select(Project).where(
            Project.id == project_id,
            Project.is_deleted == False,
        )
    )
    project = result.scalar_one_or_none()
    if project is None:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


async def get_project_with_owner_access(
    *, project_id: UUID, current_user: User, db: AsyncSession
) -> Project:
    project = await _load_project(project_id, db)
    if project.owner_id != current_user.id:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


async def get_project_with_write_access(
    *, project_id: UUID, current_user: User, db: AsyncSession
) -> Project:
    project = await _load_project(project_id, db)
    if project.owner_id == current_user.id:
        return project

    collab = await db.execute(
        select(Collaborator).where(
            Collaborator.project_id == project.id,
            Collaborator.user_id == current_user.id,
            Collaborator.role.in_(_WRITE_ROLES),
        )
    )
    if collab.scalars().first() is None:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


async def get_project_with_read_access(
    *, project_id: UUID, current_user: User, db: AsyncSession
) -> Project:
    project = await _load_project(project_id, db)
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
