"""Unit tests for the activity feed building blocks (issue #217).

The endpoint itself is covered in test_activity_api.py.
"""
from __future__ import annotations

import base64
import uuid
from datetime import datetime, timezone

import pytest

from stemhub.activity import InvalidCursor, decode_cursor, encode_cursor

NOW = datetime(2026, 11, 8, 12, 30, 15, 123456, tzinfo=timezone.utc)


def _b64(text: str) -> str:
    return base64.urlsafe_b64encode(text.encode()).decode()


# ── Cursor ──


def test_cursor_round_trips_time_and_item_id() -> None:
    version_id = uuid.uuid4()

    cursor = decode_cursor(encode_cursor(NOW, f"version_saved:{version_id}"))

    assert cursor.occurred_at == NOW
    assert cursor.item_id == f"version_saved:{version_id}"
    assert cursor.type == "version_saved"
    assert cursor.entity_id == version_id


def test_cursor_is_url_safe_base64_of_time_and_item_id() -> None:
    item_id = f"pull_request_merged:{uuid.uuid4()}"

    raw = encode_cursor(NOW, item_id)

    assert set(raw) <= set("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_")
    padded = raw + "=" * (-len(raw) % 4)
    assert base64.urlsafe_b64decode(padded).decode() == f"{NOW.isoformat()}|{item_id}"


def test_cursor_accepts_padded_input() -> None:
    item_id = f"branch_created:{uuid.uuid4()}"

    cursor = decode_cursor(_b64(f"{NOW.isoformat()}|{item_id}"))

    assert cursor.item_id == item_id


@pytest.mark.parametrize(
    "raw",
    [
        "",
        "not base64 at all!",
        "a",
        _b64("no separator"),
        _b64(f"yesterday|version_saved:{uuid.uuid4()}"),
        _b64(f"2026-11-08T12:00:00|version_saved:{uuid.uuid4()}"),  # no UTC offset
        _b64(f"{NOW.isoformat()}|"),
        _b64(f"{NOW.isoformat()}|version_saved"),
        _b64(f"{NOW.isoformat()}|commit:{uuid.uuid4()}"),
        _b64(f"{NOW.isoformat()}|version_saved:not-a-uuid"),
        base64.urlsafe_b64encode(b"\xff\xfe|\x00").decode(),
    ],
)
def test_decode_cursor_rejects_malformed_input(raw: str) -> None:
    with pytest.raises(InvalidCursor):
        decode_cursor(raw)
