from datetime import datetime, timedelta, timezone
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from sqlalchemy import func, cast, Date

from ..auth import get_current_admin_user
from ..blob_gc import sweep_orphan_blobs
from ..database import get_db
from ..models import Project, User
from ..schemas import UserResponse, DailySignup, AdminStats, UserWithProjects, RecentUser
from ..storage import StorageService, get_storage_service

router = APIRouter(prefix="/api/admin", tags=["admin"])


# ── Endpoints ─────────────────────────────────────────────────────────────────

@router.get("/users", response_model=list[UserWithProjects])
async def list_users(
    _: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
    limit: int = Query(default=50, ge=1, le=200),
    offset: int = Query(default=0, ge=0),
) -> list[UserWithProjects]:
    result = await db.execute(
        select(User).order_by(User.created_at.desc()).limit(limit).offset(offset)
    )
    users = result.scalars().all()

    # Count projects per user
    project_counts_result = await db.execute(
        select(Project.owner_id, func.count(Project.id).label("cnt"))
        .where(Project.is_deleted == False)
        .group_by(Project.owner_id)
    )
    counts = {row.owner_id: row.cnt for row in project_counts_result.all()}

    return [
        UserWithProjects(**UserResponse.model_validate(u).model_dump(), project_count=counts.get(u.id, 0))
        for u in users
    ]


@router.get("/stats", response_model=AdminStats)
async def get_stats(
    _: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
) -> AdminStats:
    total_users = (await db.execute(select(func.count()).select_from(User))).scalar() or 0
    active_users = (await db.execute(select(func.count()).select_from(User).where(User.is_active == True))).scalar() or 0
    admin_users = (await db.execute(select(func.count()).select_from(User).where(User.is_admin == True))).scalar() or 0
    total_projects = (await db.execute(select(func.count()).select_from(Project).where(Project.is_deleted == False))).scalar() or 0
    public_projects = (await db.execute(select(func.count()).select_from(Project).where(Project.is_deleted == False, Project.is_public == True))).scalar() or 0
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
    signup_map = {str(row.day): row.cnt for row in rows.all()}

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


@router.post("/blobs/gc")
async def run_blob_gc(
    _: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
    grace_hours: int = Query(24, ge=0, le=720),
    batch_size: int = Query(500, ge=1, le=5000),
):
    """Run one pass of the content-addressed blob GC sweep.

    Deletes blobs where ref_count == 0 and older than the grace window.
    See docs/content-addressed-storage.md. Intended to be invoked by an
    external scheduler; also usable ad-hoc from an admin session.
    """
    result = await sweep_orphan_blobs(
        db=db,
        storage=storage,
        grace_hours=grace_hours,
        batch_size=batch_size,
    )
    return {
        "scanned": result.scanned,
        "deleted": result.deleted,
        "failed": result.failed,
        "grace_hours": grace_hours,
        "batch_size": batch_size,
    }
