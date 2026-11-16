"""Everything StemHub holds about a user, as one JSON document (GDPR right
of access and data portability).

Projects the user owns come with their branches, versions, collaborators,
pull requests and the list of their files (hash and size; the files
themselves are downloaded from each project). Projects deleted but still
stored are included, since their data still exists. Nothing secret is
exported: no password hash, no session token.
"""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import Any

from sqlalchemy import or_, select
from sqlalchemy.ext.asyncio import AsyncSession

from .models import (
    Blob,
    Branch,
    Challenge,
    ChallengeParticipant,
    Collaborator,
    Event,
    EventAttendee,
    LegalAcceptance,
    LoginAttempt,
    Project,
    PullRequest,
    User,
    UserFollow,
    UserIdentity,
    UserSession,
    Version,
)
from .security import UNUSABLE_PASSWORD_HASH

FORMAT_VERSION = 1


def _row(obj: Any, *fields: str) -> dict[str, Any]:
    return {field: _plain(getattr(obj, field)) for field in fields}


def _plain(value: Any) -> Any:
    if isinstance(value, datetime):
        return value.isoformat()
    if isinstance(value, uuid.UUID):
        return str(value)
    return value


async def _all(db: AsyncSession, statement) -> list[Any]:
    return list((await db.execute(statement)).scalars().all())


async def _usernames(db: AsyncSession, user_ids: set[uuid.UUID | None]) -> dict[uuid.UUID, str]:
    ids = {user_id for user_id in user_ids if user_id is not None}
    if not ids:
        return {}
    rows = await db.execute(select(User.id, User.username).where(User.id.in_(ids)))
    return dict(rows.all())


async def export_user_data(db: AsyncSession, user: User) -> dict[str, Any]:
    now = datetime.now(timezone.utc)

    identities = await _all(db, select(UserIdentity).where(UserIdentity.user_id == user.id))
    acceptances = await _all(db, select(LegalAcceptance).where(LegalAcceptance.user_id == user.id))
    sessions = await _all(db, select(UserSession).where(UserSession.user_id == user.id).order_by(UserSession.created_at))
    attempts = await _all(db, select(LoginAttempt).where(LoginAttempt.email == user.email).order_by(LoginAttempt.created_at))

    projects = await _all(db, select(Project).where(Project.owner_id == user.id).order_by(Project.created_at))
    project_ids = [project.id for project in projects]
    branches = await _all(db, select(Branch).where(Branch.project_id.in_(project_ids))) if project_ids else []
    branch_ids = [branch.id for branch in branches]
    versions = await _all(db, select(Version).where(Version.branch_id.in_(branch_ids)).order_by(Version.created_at)) if branch_ids else []
    collaborators = await _all(db, select(Collaborator).where(Collaborator.project_id.in_(project_ids))) if project_ids else []
    pull_requests = await _all(db, select(PullRequest).where(PullRequest.project_id.in_(project_ids))) if project_ids else []
    blobs = await _all(db, select(Blob).where(Blob.project_id.in_(project_ids))) if project_ids else []

    # The user's part in other people's projects.
    collaborations = await _all(db, select(Collaborator).where(Collaborator.user_id == user.id))
    other_versions = await _all(
        db,
        select(Version).join(Branch).join(Project)
        .where(Version.created_by == user.id, Project.owner_id != user.id)
        .order_by(Version.created_at),
    )
    other_pull_requests = await _all(
        db,
        select(PullRequest).join(Project)
        .where(or_(PullRequest.created_by == user.id, PullRequest.closed_by == user.id), Project.owner_id != user.id),
    )
    other_branches = {
        branch.id: branch
        for branch in await _all(db, select(Branch).where(Branch.id.in_({v.branch_id for v in other_versions})))
    } if other_versions else {}
    other_project_ids = (
        {c.project_id for c in collaborations}
        | {b.project_id for b in other_branches.values()}
        | {pr.project_id for pr in other_pull_requests}
    )
    other_projects = {
        project.id: project.name
        for project in await _all(db, select(Project).where(Project.id.in_(other_project_ids)))
    } if other_project_ids else {}

    following = await _all(db, select(UserFollow).where(UserFollow.follower_id == user.id))
    followers = await _all(db, select(UserFollow).where(UserFollow.followed_id == user.id))
    challenges = (
        await db.execute(
            select(ChallengeParticipant, Challenge.title)
            .join(Challenge, Challenge.id == ChallengeParticipant.challenge_id)
            .where(ChallengeParticipant.user_id == user.id)
        )
    ).all()
    events = (
        await db.execute(
            select(EventAttendee, Event.title)
            .join(Event, Event.id == EventAttendee.event_id)
            .where(EventAttendee.user_id == user.id)
        )
    ).all()

    names = await _usernames(
        db,
        {c.user_id for c in collaborators}
        | {v.created_by for v in versions}
        | {pr.created_by for pr in pull_requests}
        | {pr.closed_by for pr in pull_requests}
        | {f.followed_id for f in following}
        | {f.follower_id for f in followers},
    )

    def project_document(project: Project) -> dict[str, Any]:
        own_branches = [b for b in branches if b.project_id == project.id]
        own_branch_ids = {b.id for b in own_branches}
        return {
            **_row(project, "id", "name", "description", "category", "tags", "is_public", "like_count",
                   "created_at", "is_deleted", "deleted_at"),
            "branches": [
                {
                    **_row(branch, "id", "name", "created_at", "is_deleted", "deleted_at"),
                    "versions": [
                        {
                            **_row(version, "id", "message", "created_at", "parent_version_id", "source_daw",
                                   "source_project_filename", "is_deleted", "deleted_at", "manifest_json"),
                            "author": names.get(version.created_by),
                        }
                        for version in versions
                        if version.branch_id == branch.id
                    ],
                }
                for branch in own_branches
            ],
            "collaborators": [
                {"username": names.get(c.user_id), **_row(c, "role", "created_at")}
                for c in collaborators
                if c.project_id == project.id
            ],
            "pull_requests": [
                {
                    **_row(pr, "id", "title", "description", "status", "created_at", "closed_at", "is_deleted"),
                    "source_branch_id": _plain(pr.source_branch_id) if pr.source_branch_id in own_branch_ids else None,
                    "target_branch_id": _plain(pr.target_branch_id) if pr.target_branch_id in own_branch_ids else None,
                    "author": names.get(pr.created_by),
                    "closed_by": names.get(pr.closed_by),
                }
                for pr in pull_requests
                if pr.project_id == project.id
            ],
            "files": [_row(blob, "sha256", "size_bytes", "mime_type", "created_at") for blob in blobs if blob.project_id == project.id],
        }

    return {
        "format_version": FORMAT_VERSION,
        "exported_at": now.isoformat(),
        "account": {
            **_row(user, "id", "email", "username", "avatar_url", "bio", "location", "website", "genres",
                   "created_at", "is_active", "is_admin"),
            "has_password": user.password_hash != UNUSABLE_PASSWORD_HASH,
        },
        "linked_accounts": [_row(i, "provider", "provider_user_id", "email", "created_at") for i in identities],
        "terms_acceptances": [_row(a, "document", "version", "accepted_at") for a in acceptances],
        "sessions": [_row(s, "created_at", "expires_at", "revoked_at") for s in sessions],
        "sign_in_attempts": [_row(a, "ip", "succeeded", "created_at") for a in attempts],
        "projects": [project_document(project) for project in projects],
        "contributions": {
            "collaborations": [
                {"project": other_projects.get(c.project_id), **_row(c, "project_id", "role", "created_at")}
                for c in collaborations
            ],
            "versions": [
                {
                    "project": other_projects.get(other_branches[v.branch_id].project_id),
                    "branch": other_branches[v.branch_id].name,
                    **_row(v, "id", "message", "created_at", "source_daw", "source_project_filename", "is_deleted"),
                }
                for v in other_versions
            ],
            "pull_requests": [
                {
                    "project": other_projects.get(pr.project_id),
                    "role": "author" if pr.created_by == user.id else "closed_by",
                    **_row(pr, "id", "title", "description", "status", "created_at", "closed_at"),
                }
                for pr in other_pull_requests
            ],
        },
        "following": [{"username": names.get(f.followed_id), **_row(f, "created_at")} for f in following],
        "followers": [{"username": names.get(f.follower_id), **_row(f, "created_at")} for f in followers],
        "challenges": [{"challenge": title, **_row(p, "joined_at")} for p, title in challenges],
        "events": [{"event": title, **_row(a, "registered_at")} for a, title in events],
    }
