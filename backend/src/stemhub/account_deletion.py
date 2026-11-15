"""Account deletion (GDPR right to erasure), with a 30-day grace period.

Asking to delete an account hides it at once: the user is marked deleted,
their projects are hidden and every session is revoked. Signing in again
(password or Google) within DELETION_GRACE cancels the deletion and brings
them back. After it, the account and its data are erased for good.
"""
from __future__ import annotations

from datetime import datetime, timedelta, timezone

from sqlalchemy import ColumnElement, or_, update
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


def is_restorable(user: User) -> bool:
    """Python twin of restorable(), for a user already loaded."""
    return not user.is_deleted or (
        user.deleted_at is not None and user.deleted_at > datetime.now(timezone.utc) - DELETION_GRACE
    )


def restorable() -> ColumnElement[bool]:
    """Users who can still sign in: not deleted, or deleted within the grace period."""
    return or_(User.is_deleted == False, User.deleted_at > datetime.now(timezone.utc) - DELETION_GRACE)


async def restore_account(db: AsyncSession, user: User) -> None:
    """Cancel a scheduled deletion: bring back the account and the projects
    its deletion hid. The caller commits."""
    await db.execute(
        update(Project)
        .where(Project.owner_id == user.id, Project.is_deleted == True, Project.deleted_at == user.deleted_at)
        .values(is_deleted=False, deleted_at=None)
    )
    user.is_deleted = False
    user.deleted_at = None
