"""Project activity feed endpoint (issue #217).

What happened in a project, newest first: versions saved and deleted,
branches created and deleted, pull requests opened, merged and closed. The
feed is derived from those tables (see stemhub.activity); there is no event
table.

Access control is the same as the stats endpoints: read access, 404 on miss
so a non-member cannot probe whether a project exists.
"""
from uuid import UUID

from fastapi import APIRouter, Depends, Query
from sqlalchemy.ext.asyncio import AsyncSession

from stemhub.activity import build_activity_feed
from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import User
from stemhub.routers._project_access import get_project_with_read_access
from stemhub.schemas import ActivityFeedResponse

router = APIRouter(tags=["activity"])


@router.get("/projects/{project_id}/activity", response_model=ActivityFeedResponse)
async def get_project_activity(
    project_id: UUID,
    limit: int = Query(30, ge=1, le=100),
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """List the project's activity, newest first."""
    await get_project_with_read_access(project_id=project_id, current_user=current_user, db=db)

    return await build_activity_feed(db, project_id=project_id, limit=limit)
