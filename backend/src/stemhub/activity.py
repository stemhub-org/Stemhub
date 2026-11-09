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
from typing import get_args
from uuid import UUID

from stemhub.schemas import ActivityType

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
