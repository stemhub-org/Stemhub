from fastapi import APIRouter, Depends, Query
from sqlalchemy import func
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.auth import get_current_admin_user
from stemhub.database import get_db
from stemhub.models import User, Project
from stemhub.schemas import AdminStatsResponse, AdminUserResponse

router = APIRouter(prefix="/api/admin", tags=["admin"])


@router.get("/stats", response_model=AdminStatsResponse)
async def get_admin_stats(
    _current_user: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
):
    """Return global platform statistics (admin only)."""
    total_users = (await db.execute(select(func.count(User.id)))).scalar() or 0
    total_projects = (
        await db.execute(
            select(func.count(Project.id)).where(Project.is_deleted == False)
        )
    ).scalar() or 0

    return AdminStatsResponse(total_users=total_users, total_projects=total_projects)


@router.get("/users", response_model=list[AdminUserResponse])
async def list_users(
    _current_user: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
    limit: int = Query(default=50, ge=1, le=200),
    offset: int = Query(default=0, ge=0),
):
    """Return a paginated list of all users on the platform (admin only)."""
    result = await db.execute(
        select(User).order_by(User.created_at.desc()).limit(limit).offset(offset)
    )
    return result.scalars().all()
