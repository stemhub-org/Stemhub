"""Throttling of email + password sign-in, against brute force on one
account and credential stuffing across many.

- Per email: after LOCKOUT_THRESHOLD failures, the email is locked for one
  minute, doubling with each further failure up to MAX_LOCKOUT. A
  successful sign-in starts the count again.
- Per IP: more than IP_MAX_FAILURES failures within IP_WINDOW blocks that
  IP until its oldest failure leaves the window.

While locked, the password is not checked at all. Emails without an
account are throttled the same way, so a 429 never reveals whether an
email is registered.
"""
import math
from datetime import datetime, timedelta, timezone

from fastapi import HTTPException, status
from sqlalchemy import and_, func, or_, select
from sqlalchemy.ext.asyncio import AsyncSession

from .models import LoginAttempt

LOCKOUT_THRESHOLD = 5
FIRST_LOCKOUT = timedelta(minutes=1)
MAX_LOCKOUT = timedelta(minutes=15)
# Failures older than this no longer count towards an email's lockout.
EMAIL_WINDOW = timedelta(hours=24)

IP_MAX_FAILURES = 30
IP_WINDOW = timedelta(minutes=15)


def lockout_for(failures: int) -> timedelta:
    """How long an email stays locked after its latest failure."""
    if failures < LOCKOUT_THRESHOLD:
        return timedelta(0)
    # Capped before raising 2 to it, so many failures cannot overflow.
    doublings = min(failures - LOCKOUT_THRESHOLD, 16)
    return min(FIRST_LOCKOUT * 2**doublings, MAX_LOCKOUT)


def _too_many_attempts(retry_after: timedelta) -> HTTPException:
    seconds = max(1, math.ceil(retry_after.total_seconds()))
    return HTTPException(
        status_code=status.HTTP_429_TOO_MANY_REQUESTS,
        detail=f"Too many failed sign-in attempts. Try again in {_spoken(seconds)}.",
        headers={"Retry-After": str(seconds)},
    )


def _spoken(seconds: int) -> str:
    if seconds < 60:
        return f"{seconds} second{'s' if seconds != 1 else ''}"
    minutes = math.ceil(seconds / 60)
    return f"{minutes} minute{'s' if minutes != 1 else ''}"


async def ensure_login_allowed(db: AsyncSession, email: str, ip: str | None) -> None:
    """Raise a 429 when `email` or `ip` is currently locked out."""
    now = datetime.now(timezone.utc)

    last_success = (
        select(func.max(LoginAttempt.created_at))
        .where(LoginAttempt.email == email, LoginAttempt.succeeded == True)
        .scalar_subquery()
    )
    failures, last_failure = (
        await db.execute(
            select(func.count(), func.max(LoginAttempt.created_at)).where(
                LoginAttempt.email == email,
                LoginAttempt.succeeded == False,
                LoginAttempt.created_at > now - EMAIL_WINDOW,
                or_(last_success.is_(None), LoginAttempt.created_at > last_success),
            )
        )
    ).one()
    if last_failure is not None:
        unlock_at = last_failure + lockout_for(failures)
        if unlock_at > now:
            raise _too_many_attempts(unlock_at - now)

    if ip is None:
        return
    ip_failures, oldest = (
        await db.execute(
            select(func.count(), func.min(LoginAttempt.created_at)).where(
                and_(
                    LoginAttempt.ip == ip,
                    LoginAttempt.succeeded == False,
                    LoginAttempt.created_at > now - IP_WINDOW,
                )
            )
        )
    ).one()
    if ip_failures >= IP_MAX_FAILURES and oldest is not None:
        raise _too_many_attempts(oldest + IP_WINDOW - now)


def record_login_attempt(db: AsyncSession, email: str, ip: str | None, *, succeeded: bool) -> None:
    """Add the attempt to the session; the caller commits."""
    db.add(LoginAttempt(email=email, ip=ip, succeeded=succeeded))
