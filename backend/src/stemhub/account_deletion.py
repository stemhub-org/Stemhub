"""Account deletion (GDPR right to erasure), with a 30-day grace period.

Asking to delete an account hides it at once: the user is marked deleted,
their projects are hidden and every session is revoked. Signing in again
within DELETION_GRACE cancels the deletion. After it, the account and its
data are erased for good.
"""
from __future__ import annotations

from datetime import datetime, timedelta, timezone

from sqlalchemy import update
from sqlalchemy.ext.asyncio import AsyncSession

from .models import Project, User, UserSession

DELETION_GRACE = timedelta(days=30)


async def schedule_deletion(db: AsyncSession, user: User) -> datetime:
    """Hide the account and its projects and sign it out everywhere; the
    caller commits. Returns when the data will be erased."""
    now = datetime.now(timezone.utc)
    user.is_deleted = True
    user.deleted_at = now
    # Stamped with the account's deleted_at, so cancelling restores exactly
    # these projects and not ones the user had deleted before.
    await db.execute(
        update(Project)
        .where(Project.owner_id == user.id, Project.is_deleted == False)
        .values(is_deleted=True, deleted_at=now)
    )
    await db.execute(
        update(UserSession)
        .where(UserSession.user_id == user.id, UserSession.revoked_at.is_(None))
        .values(revoked_at=now)
    )
    return now + DELETION_GRACE
