"""What the FLP tests share: event ids, synthetic files, the local corpora, assertions.

test_flp_events.py, test_flp_scrub.py and test_flp_cli.py import it as a
top-level module: backend/tests has no __init__.py, so pytest puts the folder
on sys.path when it imports them (its default "prepend" import mode), from
the repository root as from backend/.
"""
from __future__ import annotations

import os
import struct
from pathlib import Path
from typing import Optional

import pytest

from stemhub.project_model.flp_events import FlpFile

FIXTURES_DIR = Path(__file__).resolve().parent / "fixtures" / "parser_corpus" / "assets" / "fl_studio"
REFERENCE = FIXTURES_DIR / "FL 20.8.4.flp"
MULTI_CHANNEL = FIXTURES_DIR / "patterns" / "multi-channel.flp"
CORRUPTED_DIR = FIXTURES_DIR / "corrupted"
VALID_FIXTURES = [pytest.param(REFERENCE, id="FL 20.8.4"), pytest.param(MULTI_CHANNEL, id="multi-channel")]

TEMPO = 156
NEW_IN_25_2_3 = 0xAC
FL_STUDIO_TEXT = 192
TITLE = 194
COMMENTS = 195
SAMPLE_PATH = 196
FL_VERSION = 199
LICENSEE = 200
DATA_PATH = 202
CHANNEL_NAME = 203
INSERT_NAME = 204
ARTISTS = 207
PLUGIN_DATA = 213
NOTES = 224
MIXER_PARAMS = 225
PLAYLIST = 233
TIMESTAMP = 237
TRACK_DATA = 238
TRACK_NAME = 239

HEADER_SIZE = 22
FAKE_USER = "JaneDoe"  # never a real name: these files are synthetic
FAKE_LICENSEE = FAKE_USER + "20240917"  # every licensee seen is a name followed by digits


# --- building synthetic files --------------------------------------------------------------


def flp_data(body: bytes, *, ppq: int = 96, data_len: Optional[int] = None) -> bytes:
    size = len(body) if data_len is None else data_len
    return b"FLhd" + struct.pack("<IhHH", 6, 0, 1, ppq) + b"FLdt" + struct.pack("<I", size) + body


def fixed_event(event_id: int, value: int, size: int) -> bytes:
    return bytes([event_id]) + value.to_bytes(size, "little")


def var_event(event_id: int, payload: bytes) -> bytes:
    length, prefix = len(payload), bytearray()
    while True:
        prefix.append((length & 0x7F) | (0x80 if length > 0x7F else 0))
        length >>= 7
        if not length:
            return bytes([event_id]) + bytes(prefix) + payload


def utf16_text(text: str) -> bytes:
    return (text + "\0").encode("utf-16-le")


def ascii_text(text: str) -> bytes:
    return (text + "\0").encode("ascii")


def scrambled(name: str) -> str:
    """How FL stores a licensee (PyFLP's setter): each character shifted by its index."""
    out = []
    for index, char in enumerate(name):
        first, second = ord(char) + 26 - index, ord(char) - 49 - index
        out.append(chr(first if 0 < first <= 127 else second))
    return "".join(out)


def fl2025_file(tempo_bpm_x1000: int = 120000) -> bytes:
    """What FL 25.2.4 writes first: version, 0xAC (3 bytes), the 'FL Studio' text, tempo."""
    return flp_data(
        var_event(FL_VERSION, ascii_text("25.2.4.4960"))
        + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
        + var_event(FL_STUDIO_TEXT, utf16_text("FL Studio 25.2.4.4960.4960"))
        + fixed_event(TEMPO, tempo_bpm_x1000, 4)
    )


def private_file() -> bytes:
    """A FL 25 file holding a licensee and user folders in text and plugin data."""
    plugin_state = (
        b"\x03\x35path=/Users/" + FAKE_USER.encode() + b"/Library/Audio/x.fxp\x00"
        + b"\x10C:\\Users\\" + FAKE_USER.encode() + b"\\Desktop\\kit\x00"
        + b"json=C:\\\\Users\\\\" + FAKE_USER.encode() + b"\\\\Music\x00"
        + b"/Users/Shared/Loops\x00"
    )
    return flp_data(
        var_event(FL_VERSION, ascii_text("25.2.4.4960"))
        + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
        + var_event(LICENSEE, utf16_text(scrambled(FAKE_LICENSEE)))
        + var_event(DATA_PATH, utf16_text(f"/Users/{FAKE_USER}/Music/Projects/"))
        + var_event(SAMPLE_PATH, utf16_text(f"C:\\Users\\{FAKE_USER}\\Samples\\kick.wav"))
        + var_event(SAMPLE_PATH, utf16_text("/Users/Shared/StemHub/snare.wav"))
        + var_event(PLUGIN_DATA, plugin_state)
        + fixed_event(TEMPO, 140000, 4)
    )


def licensee_file(*events: bytes, licensee: str = FAKE_LICENSEE) -> bytes:
    """A FL 25 file: the version, the scrambled licensee (event #1), then ``events``."""
    return flp_data(
        var_event(FL_VERSION, ascii_text("25.2.4.4960"))
        + var_event(LICENSEE, utf16_text(scrambled(licensee)))
        + b"".join(events)
    )


GOOD_BODY = var_event(FL_VERSION, ascii_text("20.8.4.2576")) + fixed_event(TEMPO, 140000, 4)


# --- assertions ----------------------------------------------------------------------------


def assert_same_structure(before: FlpFile, after: FlpFile) -> None:
    assert after.header == before.header
    assert [(event.id, len(event.payload), event.length_width) for event in after.events] == [
        (event.id, len(event.payload), event.length_width) for event in before.events
    ]


def assert_names_are_gone(names: frozenset, data: bytes) -> None:
    """None of the names is left in the bytes, in any case, as single-byte text or UTF-16LE."""
    lowered = data.lower()
    for name in names:
        assert name.lower().encode("ascii") not in lowered
        assert name.lower().encode("utf-16-le") not in lowered


# --- the local corpora (never committed) ---------------------------------------------------


def extended_corpus() -> list:
    roots = [root for root in os.environ.get("STEMHUB_EXTENDED_CORPUS", "").split(os.pathsep) if root]
    if not roots:
        reason = "STEMHUB_EXTENDED_CORPUS is not set"
        return [pytest.param(None, id="no-extended-corpus", marks=pytest.mark.skip(reason=reason))]
    files = sorted({path for root in roots for path in Path(root).rglob("*.flp")})
    if not files:
        return [pytest.param(None, id="empty-extended-corpus")]
    return [pytest.param(path, id=path.name) for path in files]


EXTENDED_CORPUS = extended_corpus()
