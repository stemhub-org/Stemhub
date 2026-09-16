"""Shared project-access helpers for routers.

Two variants matter:

- ``get_project_with_write_access``: for endpoints that mutate project data
  (blob uploads, version creates). Owner or collaborator with role in
  {Admin, Editor}. Returns 404 on miss to avoid confirming project existence
  to non-members.

- ``get_project_with_read_access``: for endpoints that read project data.
  Public projects are readable by any authenticated user. Otherwise: owner
  or any collaborator. Also 404 on miss.

Both use 404 (privacy-hiding) rather than 403. This diverges from
``projects.py::_get_project_with_access`` which returns 403; that helper
predates this module and is left in place for now.
"""
from uuid import UUID

from fastapi import HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.models import Collaborator, Project, User


_WRITE_ROLES = ("Admin", "Editor")


async def get_project_with_write_access(
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
            Collaborator.role.in_(_WRITE_ROLES),
        )
    )
    if collab.scalars().first() is None:
        raise HTTPException(status_code=404, detail="Project not found")
    return project


async def get_project_with_read_access(
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
