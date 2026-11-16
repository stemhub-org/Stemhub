"""Account deletion (GDPR right to erasure), with a 30-day grace period.

Asking to delete an account hides it at once: the user is marked deleted,
their projects are hidden and every session is revoked. Signing in again
(password or Google) within DELETION_GRACE cancels the deletion and brings
them back. After it, `erase_due_accounts` (run hourly with the sign-in
purge) erases the account for good: its projects with their files, and its
personal data. Its versions and pull requests in other people's projects
stay, without an author, so their history is not broken.
"""
from __future__ import annotations

import logging
from datetime import datetime, timedelta, timezone

from sqlalchemy import ColumnElement, delete, or_, select, update
from sqlalchemy.ext.asyncio import AsyncSession

from .models import Blob, Branch, Collaborator, LoginAttempt, Project, PullRequest, User, UserSession, Version
from .storage import StorageService

logger = logging.getLogger(__name__)

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


async def erase_due_accounts(db: AsyncSession, storage: StorageService) -> int:
    """Erase every account whose grace period is over. Returns how many were."""
    due = await db.execute(
        select(User).where(User.is_deleted == True, User.deleted_at <= datetime.now(timezone.utc) - DELETION_GRACE)
    )
    erased = 0
    for user in due.scalars().all():
        if await erase_account(db, storage, user):
            erased += 1
    return erased


async def erase_account(db: AsyncSession, storage: StorageService, user: User) -> bool:
    """Delete the account and everything it owns, files first. If a file
    cannot be deleted, nothing else is and the account is retried at the
    next run, so no file of it is ever left behind untracked."""
    project_ids = (await db.execute(select(Project.id).where(Project.owner_id == user.id))).scalars().all()
    storage_uris = (
        (await db.execute(select(Blob.storage_uri).where(Blob.project_id.in_(project_ids)))).scalars().all()
        if project_ids
        else []
    )
    try:
        for project_id in project_ids:
            storage.delete_project_preview(project_id)
        for storage_uri in storage_uris:
            storage.delete_blob(storage_uri)
    except Exception:
        # Any storage failure (bucket credentials, network, disk) postpones
        # the whole erasure, so no file of the account is left behind.
        logger.exception("Could not delete the files of account %s; retrying at the next run", user.id)
        return False

    if project_ids:
        branch_ids = select(Branch.id).where(Branch.project_id.in_(project_ids))
        await db.execute(delete(PullRequest).where(PullRequest.project_id.in_(project_ids)))
        # Versions point at their parent, so the links go before the rows.
        await db.execute(update(Version).where(Version.branch_id.in_(branch_ids)).values(parent_version_id=None))
        await db.execute(delete(Version).where(Version.branch_id.in_(branch_ids)))
        await db.execute(delete(Branch).where(Branch.project_id.in_(project_ids)))
        await db.execute(delete(Collaborator).where(Collaborator.project_id.in_(project_ids)))
        # Blob rows go with their project (ON DELETE CASCADE).
        await db.execute(delete(Project).where(Project.id.in_(project_ids)))

    # Contributions to other people's projects stay, without an author.
    await db.execute(update(Version).where(Version.created_by == user.id).values(created_by=None))
    await db.execute(update(PullRequest).where(PullRequest.created_by == user.id).values(created_by=None))
    await db.execute(update(PullRequest).where(PullRequest.closed_by == user.id).values(closed_by=None))
    await db.execute(delete(Collaborator).where(Collaborator.user_id == user.id))
    await db.execute(delete(LoginAttempt).where(LoginAttempt.email == user.email))
    # Sessions, identities, terms acceptances, follows and event or
    # challenge sign-ups go with the user (ON DELETE CASCADE).
    await db.execute(delete(User).where(User.id == user.id))
    await db.commit()
    logger.info("Erased account %s and %d projects", user.id, len(project_ids))
    return True
