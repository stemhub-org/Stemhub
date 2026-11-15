"""Deletes sign-in records that are no longer needed: they would otherwise
grow forever, and emails and IPs are personal data not to be kept longer
than useful (GDPR).

- Sessions: 30 days after they expired or were revoked, kept until then to
  investigate a compromised account.
- Login attempts: after 30 days; throttling only reads the last 24 hours.
- Rate limit hits: after 1 day; the longest window is one hour.

The backend runs `purge_auth_records` every hour (see main.py).
"""
from __future__ import annotations

import asyncio
import logging
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone

from sqlalchemy import delete, or_
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker

from .models import LoginAttempt, RateLimitHit, UserSession

logger = logging.getLogger(__name__)

SESSION_RETENTION = timedelta(days=30)
LOGIN_ATTEMPT_RETENTION = timedelta(days=30)
RATE_LIMIT_HIT_RETENTION = timedelta(days=1)
PURGE_INTERVAL = timedelta(hours=1)


@dataclass(frozen=True)
class PurgeResult:
    sessions: int
    login_attempts: int
    rate_limit_hits: int


async def purge_auth_records(db: AsyncSession) -> PurgeResult:
    now = datetime.now(timezone.utc)
    sessions = await db.execute(
        delete(UserSession).where(
            or_(
                UserSession.expires_at < now - SESSION_RETENTION,
                UserSession.revoked_at < now - SESSION_RETENTION,
            )
        )
    )
    login_attempts = await db.execute(
        delete(LoginAttempt).where(LoginAttempt.created_at < now - LOGIN_ATTEMPT_RETENTION)
    )
    rate_limit_hits = await db.execute(
        delete(RateLimitHit).where(RateLimitHit.created_at < now - RATE_LIMIT_HIT_RETENTION)
    )
    await db.commit()
    return PurgeResult(sessions.rowcount, login_attempts.rowcount, rate_limit_hits.rowcount)


async def purge_auth_records_forever(session_factory: async_sessionmaker[AsyncSession]) -> None:
    """Purge now, then every PURGE_INTERVAL, until cancelled. A failed run is
    logged and retried at the next interval."""
    while True:
        try:
            async with session_factory() as db:
                result = await purge_auth_records(db)
            if result.sessions or result.login_attempts or result.rate_limit_hits:
                logger.info(
                    "Purged %d sessions, %d login attempts and %d rate limit hits",
                    result.sessions, result.login_attempts, result.rate_limit_hits,
                )
        except Exception:
            logger.exception("Purging old sign-in records failed")
        await asyncio.sleep(PURGE_INTERVAL.total_seconds())
