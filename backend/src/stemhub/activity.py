"""Project activity feed (issue #217).

The feed is derived from data the project already stores; there is no event
table. Each activity type comes from the timestamp that records it:

- ``version_saved``: ``Version.created_at`` (deleted versions too: they were saved)
- ``version_deleted``: ``Version.deleted_at``
- ``branch_created`` / ``branch_deleted``: ``Branch.created_at`` / ``Branch.deleted_at``
- ``pull_request_opened``: ``PullRequest.created_at``
- ``pull_request_merged`` / ``pull_request_closed``: ``PullRequest.closed_at``

Items are ordered newest first by ``(occurred_at, id)``. A page ends with an
opaque cursor: the URL-safe base64 of ``"<occurred_at isoformat>|<item id>"``
of its last item, and the next page holds the items strictly after it.
"""
from __future__ import annotations

import base64
import binascii
from dataclasses import dataclass
from datetime import datetime
from typing import Any, Iterable, get_args
from uuid import UUID

from sqlalchemy import and_, or_
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.models import Branch, PullRequest, User, Version
from stemhub.schemas import (
    ActivityActor,
    ActivityBranch,
    ActivityFeedResponse,
    ActivityItem,
    ActivityPullRequest,
    ActivityType,
    ActivityVersion,
)

ACTIVITY_TYPES: tuple[str, ...] = get_args(ActivityType)

_CURSOR_SEPARATOR = "|"


class InvalidCursor(ValueError):
    """The cursor was not produced by encode_cursor."""


@dataclass(frozen=True)
class Cursor:
    occurred_at: datetime
    item_id: str
    type: str
    entity_id: UUID


def encode_cursor(occurred_at: datetime, item_id: str) -> str:
    raw = f"{occurred_at.isoformat()}{_CURSOR_SEPARATOR}{item_id}".encode()
    # Padding stripped so the cursor needs no escaping in a query string.
    return base64.urlsafe_b64encode(raw).decode().rstrip("=")


def decode_cursor(raw: str) -> Cursor:
    try:
        decoded = base64.urlsafe_b64decode(raw + "=" * (-len(raw) % 4)).decode()
    except (binascii.Error, UnicodeDecodeError) as exc:
        raise InvalidCursor("not URL-safe base64 text") from exc

    occurred_at_text, separator, item_id = decoded.partition(_CURSOR_SEPARATOR)
    if not separator:
        raise InvalidCursor("missing separator")
    try:
        occurred_at = datetime.fromisoformat(occurred_at_text)
    except ValueError as exc:
        raise InvalidCursor("bad timestamp") from exc
    if occurred_at.tzinfo is None:
        # Stored timestamps are timezone-aware; a naive one can't be compared.
        raise InvalidCursor("timestamp has no UTC offset")

    activity_type, separator, entity_id_text = item_id.partition(":")
    if not separator or activity_type not in ACTIVITY_TYPES:
        raise InvalidCursor("bad item id")
    try:
        entity_id = UUID(entity_id_text)
    except ValueError as exc:
        raise InvalidCursor("bad item id") from exc

    return Cursor(occurred_at=occurred_at, item_id=item_id, type=activity_type, entity_id=entity_id)


# ── Feed ──

# Column holding the user who did it, for the types that record one.
_ACTOR_COLUMN = {
    "version_saved": "created_by",
    "pull_request_opened": "created_by",
    "pull_request_merged": "closed_by",
    "pull_request_closed": "closed_by",
}


@dataclass(frozen=True)
class _Event:
    type: str
    occurred_at: datetime
    entity: Any

    @property
    def id(self) -> str:
        return f"{self.type}:{self.entity.id}"

    @property
    def actor_id(self) -> UUID | None:
        column = _ACTOR_COLUMN.get(self.type)
        return getattr(self.entity, column) if column else None

    def is_after(self, cursor: Cursor | None) -> bool:
        """Whether the event comes strictly after the cursor, newest first."""
        if cursor is None:
            return True
        return (self.occurred_at, self.id) < (cursor.occurred_at, cursor.item_id)


def _after_cursor(activity_type: str, occurred_at, entity_id, cursor: Cursor):
    """SQL twin of _Event.is_after for the events of one type.

    Item ids of one type compare like their entity ids (Postgres orders
    uuids by their bytes, which is the order of their hex text). Ids of two
    different types compare by the type alone, so at the cursor's timestamp
    a type's events are either all after the cursor or all before it.
    """
    if activity_type == cursor.type:
        return or_(
            occurred_at < cursor.occurred_at,
            and_(occurred_at == cursor.occurred_at, entity_id < cursor.entity_id),
        )
    if f"{activity_type}:" < f"{cursor.type}:":
        return occurred_at <= cursor.occurred_at
    return occurred_at < cursor.occurred_at


async def _query_events(
    db: AsyncSession,
    activity_type: str,
    model,
    occurred_at,
    filters: list,
    *,
    cursor: Cursor | None,
    limit: int,
) -> list[_Event]:
    """The first `limit` events of one type after the cursor, newest first."""
    if cursor is not None:
        filters = [*filters, _after_cursor(activity_type, occurred_at, model.id, cursor)]
    result = await db.execute(
        select(model)
        .where(*filters)
        .order_by(occurred_at.desc(), model.id.desc())
        .limit(limit)
    )
    return [_Event(activity_type, getattr(row, occurred_at.key), row) for row in result.scalars().all()]


async def _load_actors(db: AsyncSession, events: list[_Event]) -> dict[UUID, User]:
    actor_ids = {event.actor_id for event in events if event.actor_id is not None}
    if not actor_ids:
        return {}
    result = await db.execute(select(User).where(User.id.in_(actor_ids)))
    return {user.id: user for user in result.scalars().all()}


def _branch_summary(branch: Branch) -> ActivityBranch:
    return ActivityBranch(id=branch.id, name=branch.name, is_deleted=branch.is_deleted)


def _to_item(event: _Event, *, branches: dict[UUID, Branch], actors: dict[UUID, User]) -> ActivityItem:
    actor = actors.get(event.actor_id) if event.actor_id is not None else None
    entity = event.entity
    details: dict[str, Any] = {}
    if isinstance(entity, Version):
        details["branch"] = _branch_summary(branches[entity.branch_id])
        details["version"] = ActivityVersion(
            id=entity.id,
            message=entity.message,
            source_daw=entity.source_daw,
            source_project_filename=entity.source_project_filename,
        )
    elif isinstance(entity, Branch):
        details["branch"] = _branch_summary(entity)
    elif isinstance(entity, PullRequest):
        details["pull_request"] = ActivityPullRequest(
            id=entity.id,
            title=entity.title,
            status=entity.status,
            source_branch=_branch_summary(branches[entity.source_branch_id]),
            target_branch=_branch_summary(branches[entity.target_branch_id]),
        )
    return ActivityItem(
        id=event.id,
        type=event.type,
        occurred_at=event.occurred_at,
        actor=ActivityActor(id=actor.id, username=actor.username) if actor else None,
        **details,
    )


def _branch_events(branches: dict[UUID, Branch]) -> list[_Event]:
    """Branch events, from the project's branches already in memory: a
    project has few branches, so this saves two queries."""
    events = []
    for branch in branches.values():
        events.append(_Event("branch_created", branch.created_at, branch))
        if branch.is_deleted and branch.deleted_at is not None:
            events.append(_Event("branch_deleted", branch.deleted_at, branch))
    return events


async def build_activity_feed(
    db: AsyncSession,
    *,
    project_id: UUID,
    limit: int,
    cursor: Cursor | None = None,
    types: Iterable[str] | None = None,
) -> ActivityFeedResponse:
    """One page of the project's activity, newest first, after `cursor`,
    keeping only `types` when given.

    Each source is queried for at most `limit + 1` events after the cursor,
    so merging them is enough to know the page and whether more items exist.
    Actors are then loaded in one query for the page only.
    """
    wanted = set(types) if types else set(ACTIVITY_TYPES)

    # Deleted branches included: their versions were saved, and their own
    # creation and deletion are activity.
    branch_result = await db.execute(select(Branch).where(Branch.project_id == project_id))
    branches = {branch.id: branch for branch in branch_result.scalars().all()}

    in_project = Version.branch_id.in_(list(branches))
    # Soft-deleted pull requests are left out entirely.
    live_pull_requests = [PullRequest.project_id == project_id, PullRequest.is_deleted == False]
    sources = {
        "version_saved": (Version, Version.created_at, [in_project]),
        "version_deleted": (
            Version,
            Version.deleted_at,
            [in_project, Version.is_deleted == True, Version.deleted_at.is_not(None)],
        ),
        "pull_request_opened": (PullRequest, PullRequest.created_at, live_pull_requests),
        "pull_request_merged": (
            PullRequest,
            PullRequest.closed_at,
            [*live_pull_requests, PullRequest.status == "MERGED", PullRequest.closed_at.is_not(None)],
        ),
        "pull_request_closed": (
            PullRequest,
            PullRequest.closed_at,
            [*live_pull_requests, PullRequest.status == "CLOSED", PullRequest.closed_at.is_not(None)],
        ),
    }

    events = [
        event for event in _branch_events(branches)
        if event.type in wanted and event.is_after(cursor)
    ]
    for activity_type, (model, occurred_at, filters) in sources.items():
        if activity_type not in wanted or (model is Version and not branches):
            continue
        events += await _query_events(
            db, activity_type, model, occurred_at, filters, cursor=cursor, limit=limit + 1
        )

    events.sort(key=lambda event: (event.occurred_at, event.id), reverse=True)
    page = events[:limit]
    next_cursor = encode_cursor(page[-1].occurred_at, page[-1].id) if len(events) > limit else None

    actors = await _load_actors(db, page)
    return ActivityFeedResponse(
        items=[_to_item(event, branches=branches, actors=actors) for event in page],
        next_cursor=next_cursor,
    )
