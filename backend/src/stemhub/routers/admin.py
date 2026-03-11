from fastapi import APIRouter, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from sqlalchemy import func

from ..auth import get_current_admin_user
from ..database import get_db
from ..models import Project, User
from ..schemas import UserResponse

router = APIRouter(prefix="/api/admin", tags=["admin"])


class AdminStatsResponse:
    pass


from pydantic import BaseModel


class AdminStats(BaseModel):
    total_users: int
    total_projects: int
    active_users: int
    admin_users: int


@router.get("/users", response_model=list[UserResponse])
async def list_users(
    _: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
) -> list[User]:
    result = await db.execute(select(User).order_by(User.created_at.desc()))
    return result.scalars().all()


@router.get("/stats", response_model=AdminStats)
async def get_stats(
    _: User = Depends(get_current_admin_user),
    db: AsyncSession = Depends(get_db),
) -> AdminStats:
    total_users = (await db.execute(select(func.count()).select_from(User))).scalar_one()
    active_users = (await db.execute(select(func.count()).select_from(User).where(User.is_active == True))).scalar_one()
    admin_users = (await db.execute(select(func.count()).select_from(User).where(User.is_admin == True))).scalar_one()
    total_projects = (await db.execute(select(func.count()).select_from(Project).where(Project.is_deleted == False))).scalar_one()

    return AdminStats(
        total_users=total_users,
        total_projects=total_projects,
        active_users=active_users,
        admin_users=admin_users,
    )
