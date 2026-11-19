"""The space each project a user owns takes in storage (Settings, #288).

Summed from the content-addressed ``blob`` table (docs/content-addressed-storage.md):
blobs are deduplicated per project, so a file used by many versions is
counted once and the sum is the space actually stored. Preview mixdowns are
not in the table and are not counted.
"""
from __future__ import annotations

import uuid

from sqlalchemy import Select, func, select
from sqlalchemy.ext.asyncio import AsyncSession

from .models import Blob, Project, User
from .schemas import ProjectStorageUsage, StorageUsageResponse


def storage_usage_query(user_id: uuid.UUID) -> Select:
    """One row per project the user owns: id, name, bytes stored, file count.

    Owned projects only: the owner holds the storage, not collaborators.
    LEFT JOIN so a project without any file is listed with 0.
    """
    size = func.coalesce(func.sum(Blob.size_bytes), 0)
    return (
        select(Project.id, Project.name, size, func.count(Blob.sha256))
        .outerjoin(Blob, Blob.project_id == Project.id)
        .where(Project.owner_id == user_id, Project.is_deleted.is_(False))
        .group_by(Project.id, Project.name)
        .order_by(size.desc(), Project.name)
    )


async def storage_usage(db: AsyncSession, user: User) -> StorageUsageResponse:
    rows = (await db.execute(storage_usage_query(user.id))).all()
    projects = [
        ProjectStorageUsage(id=project_id, name=name, size_bytes=size_bytes, file_count=file_count)
        for project_id, name, size_bytes, file_count in rows
    ]
    return StorageUsageResponse(total_bytes=sum(p.size_bytes for p in projects), projects=projects)
