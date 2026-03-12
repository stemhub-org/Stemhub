from typing import Optional
from uuid import UUID

from fastapi import APIRouter, Depends, Query, HTTPException, status
from sqlalchemy import select, desc
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.exc import IntegrityError

from ..database import get_db
from ..models import Challenge, Event, ChallengeParticipant, EventAttendee, User
from ..schemas import ChallengeResponse, EventResponse
from ..auth import get_current_user

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

@router.post("/challenges/{challenge_id}/join")
async def join_challenge(
    challenge_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """Join an active challenge."""
    challenge = await db.get(Challenge, challenge_id)
    if not challenge:
        raise HTTPException(status_code=404, detail="Challenge not found")
        
    participant = ChallengeParticipant(user_id=current_user.id, challenge_id=challenge_id)
    db.add(participant)
    try:
        await db.commit()
    except IntegrityError:
        await db.rollback()
        raise HTTPException(status_code=400, detail="Already joined this challenge")
        
    return {"message": "Successfully joined challenge"}

@router.post("/events/{event_id}/register")
async def register_event(
    event_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """Register for an event."""
    event = await db.get(Event, event_id)
    if not event:
        raise HTTPException(status_code=404, detail="Event not found")
        
    attendee = EventAttendee(user_id=current_user.id, event_id=event_id)
    db.add(attendee)
    try:
        await db.commit()
    except IntegrityError:
        await db.rollback()
        raise HTTPException(status_code=400, detail="Already registered for this event")
        
    return {"message": "Successfully registered for event"}
