from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import func, cast, Date
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from typing import List
from uuid import UUID

from stemhub.database import get_db
from stemhub.models import Version, Branch, Project, User, Collaborator
from stemhub.schemas import (
    ActivityStatsResponse,
    DailyActivity,
    TopContributorsResponse,
    ContributorStats,
)
from stemhub.auth import get_current_user

router = APIRouter(tags=["stats"])

ACTIVITY_WEEKS = 26
ACTIVITY_DAYS = ACTIVITY_WEEKS * 7


async def _check_project_access(
    *,
    project_id: UUID,
    current_user: User,
    db: AsyncSession,
) -> Project:
    """Verify the project exists and the user is the owner or a collaborator."""
    result = await db.execute(
        select(Project).where(Project.id == project_id, Project.is_deleted == False)
    )
    project = result.scalars().first()
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

    return project


@router.get("/projects/{project_id}/stats/activity", response_model=ActivityStatsResponse)
async def get_activity_stats(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Get contribution activity for the last 26 weeks.
    Returns daily commit counts, total commits, and total unique contributors.
    """
    await _check_project_access(project_id=project_id, current_user=current_user, db=db)

    since = datetime.now(timezone.utc) - timedelta(days=ACTIVITY_DAYS)

    # Get all branch IDs for this project
    branch_result = await db.execute(
        select(Branch.id).where(
            Branch.project_id == project_id,
            Branch.is_deleted == False,
        )
    )
    branch_ids = [row[0] for row in branch_result.all()]

    if not branch_ids:
        return ActivityStatsResponse(daily_activity=[], total_commits=0, total_contributors=0)

    # Daily activity: count versions per day
    daily_query = (
        select(
            cast(Version.created_at, Date).label("day"),
            func.count(Version.id).label("count"),
        )
        .where(
            Version.branch_id.in_(branch_ids),
            Version.is_deleted == False,
            Version.created_at >= since,
        )
        .group_by(cast(Version.created_at, Date))
        .order_by(cast(Version.created_at, Date))
    )
    daily_result = await db.execute(daily_query)
    daily_rows = {str(row.day): row.count for row in daily_result.all()}

    # Build full list of days (fill gaps with 0)
    daily_activity: List[DailyActivity] = []
    for i in range(ACTIVITY_DAYS):
        day = (since + timedelta(days=i)).date()
        daily_activity.append(DailyActivity(
            date=str(day),
            count=daily_rows.get(str(day), 0),
        ))

    # Total commits (all time for the project)
    total_result = await db.execute(
        select(func.count(Version.id)).where(
            Version.branch_id.in_(branch_ids),
            Version.is_deleted == False,
        )
    )
    total_commits = total_result.scalar() or 0

    # Total unique contributors
    contributors_result = await db.execute(
        select(func.count(func.distinct(Version.created_by))).where(
            Version.branch_id.in_(branch_ids),
            Version.is_deleted == False,
            Version.created_by.isnot(None),
        )
    )
    total_contributors = contributors_result.scalar() or 0

    return ActivityStatsResponse(
        daily_activity=daily_activity,
        total_commits=total_commits,
        total_contributors=total_contributors,
    )


@router.get("/projects/{project_id}/stats/top-contributors", response_model=TopContributorsResponse)
async def get_top_contributors(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Get the top contributors for a project, ranked by number of commits.
    """
    await _check_project_access(project_id=project_id, current_user=current_user, db=db)

    # Get all branch IDs for this project
    branch_result = await db.execute(
        select(Branch.id).where(
            Branch.project_id == project_id,
            Branch.is_deleted == False,
        )
    )
    branch_ids = [row[0] for row in branch_result.all()]

    if not branch_ids:
        return TopContributorsResponse(contributors=[])

    # Count commits per user
    query = (
        select(
            User.id.label("user_id"),
            User.username,
            func.count(Version.id).label("commits"),
        )
        .join(User, Version.created_by == User.id)
        .where(
            Version.branch_id.in_(branch_ids),
            Version.is_deleted == False,
            Version.created_by.isnot(None),
        )
        .group_by(User.id, User.username)
        .order_by(func.count(Version.id).desc())
    )
    result = await db.execute(query)

    contributors = []
    for row in result.all():
        username = row.username
        # Generate initials from username (first 2 chars uppercase)
        initials = username[:2].upper() if len(username) >= 2 else username.upper()
        contributors.append(ContributorStats(
            user_id=row.user_id,
            username=username,
            initials=initials,
            commits=row.commits,
        ))

    return TopContributorsResponse(contributors=contributors)
