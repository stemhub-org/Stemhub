import logging
from typing import Optional
from uuid import UUID

from fastapi import APIRouter, Depends, Query
from sqlalchemy import select, desc, func
from sqlalchemy.orm import joinedload
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.ext.asyncio import AsyncSession

from ..database import get_db
from ..models import Project, User, UserFollow, PlatformUpdate
from ..schemas import ExploreProjectResponse, ExploreFeedResponse, ProducerResponse, PlatformUpdateResponse
from ..auth import get_current_user

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/explore", tags=["explore"])

@router.get("/projects", response_model=list[ExploreProjectResponse])
async def get_explore_projects(
    sort_by: str = Query("recent", description="Sort by 'recent' or 'trending'"),
    tags: Optional[str] = Query(None, description="Comma-separated tags to filter by"),
    limit: int = Query(20, ge=1, le=100),
    offset: int = Query(0, ge=0),
    db: AsyncSession = Depends(get_db)
):
    stmt = select(Project).where(Project.is_public == True, Project.is_deleted == False)
    stmt = stmt.options(joinedload(Project.owner))

    if tags:
        tag_list = [t.strip() for t in tags.split(",") if t.strip()]
        if tag_list:
            stmt = stmt.where(Project.tags.contains(func.cast(tag_list, JSONB)))

    if sort_by == "trending":
        stmt = stmt.order_by(desc(Project.like_count), desc(Project.created_at))
    else:
        stmt = stmt.order_by(desc(Project.created_at))

    stmt = stmt.limit(limit).offset(offset)
    result = await db.execute(stmt)
    projects = result.scalars().all()
    
    response_projects = []
    for p in projects:
        response_projects.append(ExploreProjectResponse(
            id=p.id,
            name=p.name,
            description=p.description,
            category=p.category,
            tags=p.tags,
            like_count=p.like_count,
            bpm=None,  # Note: bpm/key are stored on Track instances
            key=None,
            created_at=p.created_at,
            owner={"id": p.owner.id, "username": p.owner.username}
        ))

    return response_projects

@router.get("/feed", response_model=list[ExploreFeedResponse])
async def get_explore_feed(
    limit: int = Query(20, ge=1, le=100),
    offset: int = Query(0, ge=0),
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """Aggregate recent activity (new projects) from producers the current user follows."""
    stmt = (
        select(Project)
        .join(UserFollow, Project.owner_id == UserFollow.followed_id)
        .where(UserFollow.follower_id == current_user.id)
        .where(Project.is_public == True)
        .where(Project.is_deleted == False)
        .options(joinedload(Project.owner))
        .order_by(desc(Project.created_at))
        .limit(limit)
        .offset(offset)
    )
    result = await db.execute(stmt)
    projects = result.scalars().all()
    
    feed = []
    for p in projects:
        feed.append(ExploreFeedResponse(
            id=p.id,
            action_type="new_project",
            project_id=p.id,
            project_name=p.name,
            producer={"id": p.owner.id, "username": p.owner.username},
            created_at=p.created_at
        ))
    
    return feed

@router.get("/producers", response_model=list[ProducerResponse])
async def get_explore_producers(
    limit: int = Query(20, ge=1, le=100),
    offset: int = Query(0, ge=0),
    db: AsyncSession = Depends(get_db)
):
    """List users/producers, sortable by popularity/followers."""
    subq = select(
        UserFollow.followed_id, 
        func.count(UserFollow.follower_id).label("follower_count")
    ).group_by(UserFollow.followed_id).subquery()
    
    stmt = (
        select(User, subq.c.follower_count)
        .outerjoin(subq, User.id == subq.c.followed_id)
        .where(User.is_active == True)
        .order_by(desc(func.coalesce(subq.c.follower_count, 0)))
        .limit(limit)
        .offset(offset)
    )
    
    result = await db.execute(stmt)
    rows = result.all()
    
    producers = []
    for user, f_count in rows:
        producers.append(ProducerResponse(
            id=user.id,
            username=user.username,
            avatar_url=user.avatar_url,
            bio=user.bio,
            follower_count=f_count or 0,
            genres=user.genres
        ))
    return producers

@router.get("/changelog", response_model=list[PlatformUpdateResponse])
async def get_explore_changelog(
    limit: int = Query(10, ge=1, le=50),
    offset: int = Query(0, ge=0),
    db: AsyncSession = Depends(get_db)
):
    """Get recent platform updates/changelog."""
    stmt = select(PlatformUpdate).order_by(desc(PlatformUpdate.created_at)).limit(limit).offset(offset)
    result = await db.execute(stmt)
    return result.scalars().all()
