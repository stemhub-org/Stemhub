from typing import Optional

from fastapi import APIRouter, Depends, Query
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession

from ..database import get_db
from ..models import Challenge, Event
from ..schemas import ChallengeResponse, EventResponse

router = APIRouter(prefix="/community", tags=["community"])

@router.get("/challenges", response_model=list[ChallengeResponse])
async def get_community_challenges(
    status: Optional[str] = Query(None, description="Filter by status 'active' or 'ended'"),
    limit: int = Query(20, ge=1, le=100),
    offset: int = Query(0, ge=0),
    db: AsyncSession = Depends(get_db)
):
    from datetime import datetime, timezone
    now = datetime.now(timezone.utc)
    
    stmt = select(Challenge)
    
    if status == "active":
        stmt = stmt.where(Challenge.ends_at > now)
    elif status == "ended":
        stmt = stmt.where(Challenge.ends_at <= now)
        
    stmt = stmt.order_by(desc(Challenge.created_at)).limit(limit).offset(offset)
    
    result = await db.execute(stmt)
    return result.scalars().all()

@router.get("/events", response_model=list[EventResponse])
async def get_community_events(
    limit: int = Query(20, ge=1, le=100),
    offset: int = Query(0, ge=0),
    db: AsyncSession = Depends(get_db)
):
    from datetime import datetime, timezone
    now = datetime.now(timezone.utc)
    
    # Upcoming events
    stmt = select(Event).where(Event.event_date >= now).order_by(Event.event_date).limit(limit).offset(offset)
    
    result = await db.execute(stmt)
    return result.scalars().all()
