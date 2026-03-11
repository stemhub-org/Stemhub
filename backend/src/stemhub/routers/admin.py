from datetime import datetime, timedelta, timezone
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from sqlalchemy import func, cast, Date

from ..auth import get_current_admin_user
from ..database import get_db
from ..models import Project, User
from ..schemas import UserResponse

router = APIRouter(prefix="/api/admin", tags=["admin"])


# ── Schemas ──────────────────────────────────────────────────────────────────

class DailySignup(BaseModel):
    date: str
    count: int

class AdminStats(BaseModel):
    total_users: int
    total_projects: int
    active_users: int
    admin_users: int
    public_projects: int
    private_projects: int
    signups_last_30_days: list[DailySignup]

class UserWithProjects(UserResponse):
    project_count: int

class RecentUser(BaseModel):
    id: UUID
    username: str
    email: str
    avatar_url: str | None
    created_at: datetime
    is_admin: bool

    class Config:
        from_attributes = True


# ── Endpoints ─────────────────────────────────────────────────────────────────

@router.get("/users", response_model=list[UserWithProjects])
async def list_users(
    _: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
) -> list[UserWithProjects]:
    result = await db.execute(select(User).order_by(User.created_at.desc()))
    users = result.scalars().all()

    # Count projects per user
    project_counts_result = await db.execute(
        select(Project.owner_id, func.count(Project.id).label("cnt"))
        .where(Project.is_deleted == False)
        .group_by(Project.owner_id)
    )
    counts = {row.owner_id: row.cnt for row in project_counts_result}

    return [
        UserWithProjects(**UserResponse.model_validate(u).model_dump(), project_count=counts.get(u.id, 0))
        for u in users
    ]


@router.get("/stats", response_model=AdminStats)
async def get_stats(
    _: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
) -> AdminStats:
    total_users = (await db.execute(select(func.count()).select_from(User))).scalar_one()
    active_users = (await db.execute(select(func.count()).select_from(User).where(User.is_active == True))).scalar_one()
    admin_users = (await db.execute(select(func.count()).select_from(User).where(User.is_admin == True))).scalar_one()
    total_projects = (await db.execute(select(func.count()).select_from(Project).where(Project.is_deleted == False))).scalar_one()
    public_projects = (await db.execute(select(func.count()).select_from(Project).where(Project.is_deleted == False, Project.is_public == True))).scalar_one()
    private_projects = total_projects - public_projects

    # Signups per day over last 30 days
    since = datetime.now(timezone.utc) - timedelta(days=29)
    rows = await db.execute(
        select(
            cast(User.created_at, Date).label("day"),
            func.count(User.id).label("cnt"),
        )
        .where(User.created_at >= since)
        .group_by("day")
        .order_by("day")
    )
    signup_map = {str(row.day): row.cnt for row in rows}

    # Fill all 30 days (including zeros)
    signups_last_30_days = []
    for i in range(29, -1, -1):
        day = (datetime.now(timezone.utc) - timedelta(days=i)).strftime("%Y-%m-%d")
        signups_last_30_days.append(DailySignup(date=day, count=signup_map.get(day, 0)))

    return AdminStats(
        total_users=total_users,
        total_projects=total_projects,
        active_users=active_users,
        admin_users=admin_users,
        public_projects=public_projects,
        private_projects=private_projects,
        signups_last_30_days=signups_last_30_days,
    )


@router.get("/recent-users", response_model=list[RecentUser])
async def get_recent_users(
    _: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
) -> list[RecentUser]:
    result = await db.execute(
        select(User).order_by(User.created_at.desc()).limit(5)
    )
    return result.scalars().all()


@router.patch("/users/{user_id}/toggle-admin", response_model=UserResponse)
async def toggle_admin(
    user_id: UUID,
    current_admin: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
) -> User:
    if user_id == current_admin.id:
        raise HTTPException(status_code=400, detail="Cannot change your own admin status")
    result = await db.execute(select(User).where(User.id == user_id))
    user = result.scalars().first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    user.is_admin = not user.is_admin
    await db.commit()
    await db.refresh(user)
    return user


@router.patch("/users/{user_id}/toggle-active", response_model=UserResponse)
async def toggle_active(
    user_id: UUID,
    current_admin: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
) -> User:
    if user_id == current_admin.id:
        raise HTTPException(status_code=400, detail="Cannot deactivate your own account")
    result = await db.execute(select(User).where(User.id == user_id))
    user = result.scalars().first()
    if not user:
        raise HTTPException(status_code=404, detail="User not found")
    user.is_active = not user.is_active
    await db.commit()
    await db.refresh(user)
    return user
