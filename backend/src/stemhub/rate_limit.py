"""A fixed limit on how often one client may perform an action, stored in
Postgres so it holds across backend instances.

    await enforce_rate_limit(db, action="register", key=ip, limit=10, window=timedelta(hours=1))

counts the hits of (action, key) within the window: at `limit` it raises a
429 with Retry-After, otherwise it records this hit and commits it, so the
hit counts even if the action then fails.
"""
import math
from datetime import datetime, timedelta, timezone

from fastapi import HTTPException, status
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from .models import RateLimitHit


def spoken_duration(seconds: int) -> str:
    if seconds < 60:
        return f"{seconds} second{'s' if seconds != 1 else ''}"
    minutes = math.ceil(seconds / 60)
    return f"{minutes} minute{'s' if minutes != 1 else ''}"


def too_many_requests(message: str, retry_after: timedelta) -> HTTPException:
    """A 429 whose detail is `message` followed by when to try again."""
    seconds = max(1, math.ceil(retry_after.total_seconds()))
    return HTTPException(
        status_code=status.HTTP_429_TOO_MANY_REQUESTS,
        detail=f"{message} Try again in {spoken_duration(seconds)}.",
        headers={"Retry-After": str(seconds)},
    )


async def enforce_rate_limit(
    db: AsyncSession, *, action: str, key: str, limit: int, window: timedelta, message: str
) -> None:
    now = datetime.now(timezone.utc)
    hits, oldest = (
        await db.execute(
            select(func.count(), func.min(RateLimitHit.created_at)).where(
                RateLimitHit.action == action,
                RateLimitHit.key == key,
                RateLimitHit.created_at > now - window,
            )
        )
    ).one()
    if hits >= limit and oldest is not None:
        raise too_many_requests(message, oldest + window - now)
    db.add(RateLimitHit(action=action, key=key))
    await db.commit()
