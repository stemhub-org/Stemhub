"""FL Studio project files (.flp) as raw events: a byte-preserving reader and writer.

An FLP file is a 22-byte header followed by events::

    'FLhd'  u32 6  i16 format  u16 channel_count  u16 ppq
    'FLdt'  u32 data_len  events (data_len bytes)

Each event is one id byte and a payload. Ids 0-63 carry 1 byte, 64-127 2 bytes,
128-191 4 bytes, and 192-255 a LEB128 length then that many bytes (text and
data). FL Studio has one exception so far, in ``PAYLOAD_SIZE_RULES``.

``parse`` keeps every event as ``(id, payload)`` and ``serialize`` writes them
back: ``serialize(parse(data)) == data`` for every file ``parse`` accepts. Values
are not judged here (a PPQ of 0 still reads); anything that breaks the structure
raises ``FlpFormatError``, whose message holds offsets and ids only, never
payload text. ``check_structure`` tells when a file that parses was likely read
out of step. Later layers decode events and patch payloads on these tokens.
"""
from __future__ import annotations

import dataclasses
import difflib
import functools
import re
import string
import struct
from dataclasses import dataclass
from typing import FrozenSet, Iterable, Iterator, List, Mapping, Optional, Tuple

Version = Tuple[int, ...]

EVENT_0XAC = 0xAC  # 3 bytes, meaning unknown; FL 25.2.3+ writes it just before FL_STUDIO_TEXT_ID
FL_STUDIO_TEXT_ID = 192  # "FL Studio <version>.<build>", only ever seen right after EVENT_0XAC
FL_VERSION_ID = 199  # ASCII, e.g. "25.2.4.4960"
LICENSEE_ID = 200  # the registered name, scrambled: personal data
DATA_PATH_ID = 202  # the project's data folder, an absolute path
TEXT_EVENT_IDS = frozenset((*range(192, 208), 231, 239, 241))
UNICODE_TEXT_SINCE: Version = (11, 5)  # text events are UTF-16LE from FL 11.5 on

MAX_LENGTH_PREFIX_BYTES = 5  # a u32 in LEB128
_HEADER = struct.Struct("<4sIhHH4sI")
_HEADER_CHUNK_SIZE = 6
_MAX_U32 = 0xFFFFFFFF

# How FL writes each event's payload: (first id, last id, payload size in bytes or
# None for a LEB128 length prefix). The last row that applies wins, so an exception
# is one more row.
PAYLOAD_SIZE_RULES: Tuple[Tuple[int, int, Optional[int]], ...] = (
    (0, 63, 1),
    (64, 127, 2),
    (128, 191, 4),
    (192, 255, None),
    # 01 01 00, just before the "FL Studio <version>" text event 192. Seen in every
    # FL 25.2.3 and 25.2.4 file and in no earlier one (12.9 to 25.2.0); neither PyFLP
    # nor FLPEdit names it, so the rule holds whatever the version. check_structure()
    # warns when the text event doesn't follow, i.e. when this rule misreads a file.
    (EVENT_0XAC, EVENT_0XAC, 3),
)


class FlpFormatError(ValueError):
    """The bytes are not a well-formed FLP file, or an event cannot be written as given."""


@dataclass(frozen=True)
class FlpHeader:
    format: int  # 0 for a project; signed in the file
    channel_count: int
    ppq: int

    def __post_init__(self) -> None:
        for name, low, high in (("format", -0x8000, 0x7FFF), ("channel_count", 0, 0xFFFF), ("ppq", 0, 0xFFFF)):
            value = getattr(self, name)
            if not _is_int(value):
                raise TypeError(f"header {name} must be an int")
            if not low <= value <= high:
                raise FlpFormatError(f"header {name} {value} is outside {low}..{high}")


@dataclass(frozen=True)
class Event:
    """One event: its id and payload bytes, without the length prefix.

    ``length_width`` keeps a LEB128 length prefix that FL wrote longer than
    needed: the prefix is written at least that wide (wider if the payload
    needs it). None, the usual case, means the shortest prefix.
    """

    id: int
    payload: bytes
    length_width: Optional[int] = None

    def __post_init__(self) -> None:
        if not _is_int(self.id):
            raise TypeError("event id must be an int")
        if not 0 <= self.id <= 0xFF:
            raise FlpFormatError(f"event id {self.id} is outside 0..255")
        if not isinstance(self.payload, bytes):
            raise TypeError("event payload must be bytes")
        if self.length_width is None:
            return
        if not _is_int(self.length_width):
            raise TypeError("event length_width must be an int or None")
        if not 1 <= self.length_width <= MAX_LENGTH_PREFIX_BYTES:
            raise FlpFormatError(f"event {self.id}: length prefix width must be 1..{MAX_LENGTH_PREFIX_BYTES}")
        if self.length_width <= _shortest_length_width(len(self.payload)):
            object.__setattr__(self, "length_width", None)  # normalized at construction

    @property
    def int_value(self) -> int:
        """The payload as a little-endian unsigned integer (for fixed-size events)."""
        return int.from_bytes(self.payload, "little")


@dataclass(frozen=True)
class FlpFile:
    header: FlpHeader
    events: Tuple[Event, ...]

    def __post_init__(self) -> None:
        if not isinstance(self.header, FlpHeader):
            raise TypeError("header must be an FlpHeader")
        events = tuple(self.events)
        if not all(isinstance(event, Event) for event in events):
            raise TypeError("events must be Event objects")
        object.__setattr__(self, "events", events)  # any sequence in, a tuple kept


@dataclass(frozen=True)
class EventChange:
    """One difference found by ``diff_events``: indexes into each file's events."""

    kind: str  # "replace", "delete" (only in a) or "insert" (only in b)
    a_indexes: Tuple[int, ...]
    b_indexes: Tuple[int, ...]


# --- sizes and versions --------------------------------------------------------------------

_PAYLOAD_SIZES: Tuple[Optional[int], ...] = tuple(
    next(size for first, last, size in reversed(PAYLOAD_SIZE_RULES) if first <= event_id <= last)
    for event_id in range(0x100)
)


def payload_size(event_id: int) -> Optional[int]:
    """The payload size of an event in bytes; None: a LEB128 length prefix, then the payload."""
    if not 0 <= event_id <= 0xFF:
        raise FlpFormatError(f"event id {event_id} is outside 0..255")
    return _PAYLOAD_SIZES[event_id]


def fl_version(file: FlpFile) -> Optional[Version]:
    """The FL Studio version that saved the file, from its first version event (199)."""
    for event in file.events:
        if event.id == FL_VERSION_ID:
            return _version_from(event.payload)
    return None


def _version_from(payload: bytes) -> Optional[Version]:
    parts = payload.decode("ascii", "replace").rstrip("\x00").split(".")
    if not all(part.isascii() and part.isdigit() and len(part) <= 9 for part in parts):
        return None
    return tuple(int(part) for part in parts)


# --- reading and writing -------------------------------------------------------------------


def parse(data: bytes, *, max_events: Optional[int] = None) -> FlpFile:
    """Read an FLP file into its header and raw events. Raises FlpFormatError.

    ``max_events`` caps the number of events read (the largest file seen holds
    about 20 000): a file with more raises FlpFormatError. None means no cap.
    """
    if not isinstance(data, (bytes, bytearray, memoryview)):
        raise TypeError("parse() takes the file's bytes")
    if max_events is not None and not _is_int(max_events):
        raise TypeError("max_events must be an int or None")
    if max_events is not None and max_events < 0:
        raise ValueError("max_events must be 0 or more")
    data = bytes(data)
    if len(data) < _HEADER.size:
        raise FlpFormatError(f"file is {len(data)} bytes, too short for the {_HEADER.size}-byte header")
    magic, chunk_size, file_format, channel_count, ppq, data_magic, data_len = _HEADER.unpack_from(data)
    if magic != b"FLhd":
        raise FlpFormatError("missing 'FLhd' at offset 0")
    if chunk_size != _HEADER_CHUNK_SIZE:
        raise FlpFormatError(f"header chunk is {chunk_size} bytes, expected {_HEADER_CHUNK_SIZE}")
    if data_magic != b"FLdt":
        raise FlpFormatError("missing 'FLdt' at offset 14")
    if data_len != len(data) - _HEADER.size:
        raise FlpFormatError(f"data chunk declares {data_len} bytes but {len(data) - _HEADER.size} follow")
    return FlpFile(FlpHeader(file_format, channel_count, ppq), _read_events(data, _HEADER.size, max_events))


def _read_events(data: bytes, position: int, max_events: Optional[int]) -> Tuple[Event, ...]:
    events: List[Event] = []
    end = len(data)
    while position < end:
        if max_events is not None and len(events) >= max_events:
            raise FlpFormatError(f"more than {max_events} events: the event at offset {position} is past the cap")
        offset, event_id = position, data[position]
        position += 1
        size = _PAYLOAD_SIZES[event_id]
        width = None
        if size is None:
            size, width, position = _read_length(data, position, event_id, offset)
        if size > end - position:
            raise FlpFormatError(
                f"event {event_id} at offset {offset} needs {size} bytes, {end - position} left"
            )
        payload = data[position : position + size]
        position += size
        events.append(Event(event_id, payload, width))
    return tuple(events)


def _read_length(data: bytes, position: int, event_id: int, offset: int) -> Tuple[int, Optional[int], int]:
    """A LEB128 length: (value, width if longer than needed else None, next position)."""
    value = 0
    for index in range(MAX_LENGTH_PREFIX_BYTES):
        if position >= len(data):
            raise FlpFormatError(f"event {event_id} at offset {offset}: length prefix runs past the end")
        byte = data[position]
        position += 1
        value |= (byte & 0x7F) << (7 * index)
        if not byte & 0x80:
            width = index + 1
            return value, (None if width == _shortest_length_width(value) else width), position
    raise FlpFormatError(
        f"event {event_id} at offset {offset}: length prefix is longer than {MAX_LENGTH_PREFIX_BYTES} bytes"
    )


def serialize(file: FlpFile) -> bytes:
    """Write the file back. Refuses an event FL would read differently (FlpFormatError)."""
    parts = []
    for index, (event, size) in enumerate(_sized(file.events)):
        parts.append(bytes((event.id,)))
        if size is None:
            parts.append(_encode_length(len(event.payload), event.length_width or 0))
        elif event.length_width is not None:
            raise FlpFormatError(f"event {event.id} (#{index}) is fixed-size: it has no length prefix")
        elif len(event.payload) != size:
            raise FlpFormatError(
                f"event {event.id} (#{index}) has a {len(event.payload)}-byte payload, FL reads {size}"
            )
        parts.append(event.payload)
    body = b"".join(parts)
    if len(body) > _MAX_U32:
        raise FlpFormatError(f"data chunk of {len(body)} bytes does not fit in 32 bits")
    header = file.header
    return _HEADER.pack(
        b"FLhd", _HEADER_CHUNK_SIZE, header.format, header.channel_count, header.ppq, b"FLdt", len(body)
    ) + body


def event_offsets(file: FlpFile) -> Tuple[int, ...]:
    """The file offset of each event's id byte, as ``serialize`` writes it."""
    offsets = []
    position = _HEADER.size
    for event, size in _sized(file.events):
        offsets.append(position)
        prefix = 0 if size is not None else max(event.length_width or 0, _shortest_length_width(len(event.payload)))
        position += 1 + prefix + len(event.payload)
    return tuple(offsets)


def _sized(events: Iterable[Event]) -> Iterator[Tuple[Event, Optional[int]]]:
    for event in events:
        yield event, _PAYLOAD_SIZES[event.id]


def _shortest_length_width(value: int) -> int:
    return max(1, -(-value.bit_length() // 7))


def _encode_length(value: int, width: int) -> bytes:
    width = max(width, _shortest_length_width(value))
    return bytes(((value >> (7 * index)) & 0x7F) | (0x80 if index < width - 1 else 0) for index in range(width))


# --- text ----------------------------------------------------------------------------------


def decode_text(event: Event, version: Optional[Version] = None) -> str:
    """A text event's string, trailing NULs stripped. Never raises: bad bytes become U+FFFD.

    The version event is ASCII; other text is UTF-16LE from FL 11.5 on and the
    Windows ANSI code page before (read as cp1252). Pass ``fl_version(file)``.
    """
    if event.id == FL_VERSION_ID:
        text = event.payload.decode("ascii", "replace")
    elif version is not None and version[:2] < UNICODE_TEXT_SINCE:
        text = event.payload.decode("cp1252", "replace")
    else:
        text = event.payload.decode("utf-16-le", "replace")
    return text.rstrip("\x00")


# --- checking the structure ---------------------------------------------------------------


def check_structure(file: FlpFile) -> Tuple[str, ...]:
    """Signs that a file which parses was read out of step, as warnings; () when none.

    ``parse`` only knows sizes, so a wrong size rule can still give a file that
    parses. This checks what every FL file seen has: a version event (199), and
    right after each 0xAC the "FL Studio <version>" text event (192) naming that
    version. After a warning, the events that follow can't be trusted. Warnings
    give event ids, indexes and offsets only, never payload text.
    """
    version_text = next((decode_text(event) for event in file.events if event.id == FL_VERSION_ID), None)
    warnings = [] if version_text is not None else [f"no FL version event ({FL_VERSION_ID})"]
    offsets = event_offsets(file)
    version = fl_version(file)
    for index, event in enumerate(file.events):
        if event.id != EVENT_0XAC:
            continue
        following = file.events[index + 1] if index + 1 < len(file.events) else None
        problem = _after_0xac(following, version_text, version)
        if problem:
            warnings.append(f"event {EVENT_0XAC} (#{index} @{offsets[index]}) {problem}")
    return tuple(warnings)


def _after_0xac(following: Optional[Event], version_text: Optional[str], version: Optional[Version]) -> str:
    expected = f"the 'FL Studio <version>' text event {FL_STUDIO_TEXT_ID}"
    if following is None:
        return f"is the last event: {expected} should follow it"
    if following.id != FL_STUDIO_TEXT_ID:
        return f"is followed by event {following.id}, not {expected}"
    text, prefix = decode_text(following, version), "FL Studio " + (version_text or "")
    if text == prefix or text.startswith(prefix if version_text is None else prefix + "."):
        return ""
    return f"is followed by a text event {FL_STUDIO_TEXT_ID} that does not name the FL version (event {FL_VERSION_ID})"


# --- scrubbing personal data ---------------------------------------------------------------

PLACEHOLDER_CHAR = "x"
# The licensee's name without its trailing digits is searched only from this length on:
# a shorter one ("Lee") matches unrelated names and bytes. A licensee ending in digits
# is always searched whole.
MIN_SEARCHED_NAME_CHARS = 5
# The project's own text: kept as it is, but worth reading before the file is shared.
PROJECT_TEXT_EVENTS: Mapping[int, str] = {
    194: "title",
    195: "comments",
    197: "URL",
    198: "comments (RTF)",
    207: "artists",
}
# Data events that are a list of fixed-size numeric items or a fixed numeric struct, as
# PyFLP defines them; their sizes match on every local file and none holds text. A name
# found in one is numbers that happen to spell it, so scrubbing never changes these events.
STRUCTURED_DATA_EVENTS: Mapping[int, str] = {
    209: "channel delay",
    212: "plugin wrapper",
    215: "channel parameters",
    217: "playlist selection",
    218: "channel envelope and LFO",
    219: "channel levels",
    221: "channel polyphony",
    223: "pattern controllers",
    224: "notes",
    225: "mixer parameters",
    227: "remote controller",
    228: "channel tracking",
    229: "channel level adjustments",
    233: "playlist items",
    234: "channel automation",
    235: "insert routing",
    236: "insert flags",
    237: "timestamps",
    238: "track data",
}
REASON_LICENSEE = "licensee blanked"
REASON_USER_FOLDER = "user folder name replaced"
REASON_NAME_IN_TEXT = "text holding the licensee's name blanked"
REASON_NAME_IN_DATA = "licensee's name replaced"
REASON_USER_FOLDER_IN_STRUCTURED = "user folder path matched in structured data, not replaced"
REASON_NAME_IN_STRUCTURED = "licensee's name matched in structured data, not replaced"
NOTE_NAME_NOT_SEARCHED = (
    f"the licensee's name without its digits is shorter than {MIN_SEARCHED_NAME_CHARS} characters, "
    "so it was not searched (it would match unrelated text): names and text may still hold it"
)

_USER_ROOTS = ("Users", "home", "Documents and Settings")
_NOT_PERSONAL = frozenset({"shared", "public", "default", "default user", "all users", "guest"})
_TEXT_ENCODINGS = ("latin-1", "utf-16-le")  # how names sit in data: single-byte text or UTF-16LE


@dataclass(frozen=True)
class ScrubChange:
    """One event ``scrub_report`` changed: its index, its id and why."""

    index: int
    event_id: int
    reasons: Tuple[str, ...]  # REASON_* in the order applied


@dataclass(frozen=True)
class ScrubReview:
    """One event ``scrub_report`` kept as it is, to read before sharing: its index, its id and why.

    ``reasons`` is the kind of project text (a PROJECT_TEXT_EVENTS value), or the
    REASON_*_IN_STRUCTURED matched in a structured data event.
    """

    index: int
    event_id: int
    reasons: Tuple[str, ...]


@dataclass(frozen=True)
class ScrubReport:
    """What ``scrub_report`` did: the scrubbed copy, each change, what to read by hand, and notes."""

    file: FlpFile  # the scrubbed copy
    changes: Tuple[ScrubChange, ...]
    to_review: Tuple[ScrubReview, ...]  # sorted by index
    notes: Tuple[str, ...]  # what scrubbing couldn't do (NOTE_*), never quoting the file


def _utf16_pattern(text: str) -> bytes:
    return b"".join(re.escape(char.encode("ascii")) + b"\x00" for char in text)


# <separator><root><separator(s)><name>, the name running to the next separator or control char.
_USER_FOLDER_8BIT = re.compile(
    rb"[\\/](?:" + b"|".join(re.escape(root.encode("ascii")) for root in _USER_ROOTS) + rb")[\\/]+"
    rb"([^\\/\x00-\x1f\x7f]+)",
    re.IGNORECASE,
)
_USER_FOLDER_UTF16 = re.compile(
    rb"[\\/]\x00(?:" + b"|".join(_utf16_pattern(root) for root in _USER_ROOTS) + rb")(?:[\\/]\x00)+"
    rb"((?:[\x20-\x2e\x30-\x5b\x5d-\x7e\x80-\xff]\x00|[\x00-\xff][\x01-\xff])+)",
    re.IGNORECASE,
)
_ENCODINGS = ((_USER_FOLDER_8BIT, "latin-1"), (_USER_FOLDER_UTF16, "utf-16-le"))


def user_names(file: FlpFile) -> FrozenSet[str]:
    """The user names in the file's absolute user-folder paths (/Users/<name>/, C:\\Users\\<name>\\...).

    Shared folders (Shared, Public, Default...) are not personal and not listed.
    Structured data events (STRUCTURED_DATA_EVENTS) hold numbers, not paths, and
    are left out.
    """
    return frozenset(
        name
        for event in file.events
        if payload_size(event.id) is None and event.id not in STRUCTURED_DATA_EVENTS
        for name in _user_names_in(event.payload)
    )


def _user_names_in(payload: bytes) -> Iterator[str]:
    for pattern, encoding in _ENCODINGS:
        for match in pattern.finditer(payload):
            name = match.group(1).decode(encoding, "replace")
            if name.lower() not in _NOT_PERSONAL:
                yield name


def licensee_names(file: FlpFile) -> FrozenSet[str]:
    """The licensee's name as ``scrub`` searches it, unscrambled from event 200.

    A licensee ending in digits is always listed whole. Every licensee seen is a
    name followed by 6 to 8 digits, so the name without its digits is listed too
    when it has at least MIN_SEARCHED_NAME_CHARS characters; a licensee without
    digits is only that name, under the same rule. An empty licensee (a trial
    version's) gives no name.
    """
    return _licensee_search(file)[0]


def _licensee_search(file: FlpFile) -> Tuple[FrozenSet[str], bool]:
    """The names ``licensee_names`` lists, and whether a name was too short to be listed."""
    version = fl_version(file)
    names, too_short = set(), False
    for event in file.events:
        if event.id != LICENSEE_ID:
            continue
        licensee = _unscramble(decode_text(event, version))
        name = licensee.rstrip(string.digits)
        if name != licensee:
            names.add(licensee)
        if len(name) >= MIN_SEARCHED_NAME_CHARS:
            names.add(name)
        elif name:
            too_short = True
    return frozenset(names), too_short


def _unscramble(text: str) -> str:
    """FL's licensee scrambling undone, as PyFLP does it (credited there to libflp).

    Each character is shifted by its index, one of two ways, and both ways can
    give the same code: the way that gives a letter or digit wins. So another
    character reads as a letter (a space as "k"), and a licensee holding one
    reads differently from how it is written elsewhere. Never raises.
    """
    chars = []
    for index, char in enumerate(text):
        for code in (ord(char) - 26 + index, ord(char) + 49 + index):
            if 0 < code < 0x80 and chr(code).isalnum():
                chars.append(chr(code))
                break
    return "".join(chars)


class _NameSearch:
    """Finds the licensee's names, ignoring case: in decoded text, and in data as
    single-byte text or UTF-16LE."""

    def __init__(self, names: FrozenSet[str]) -> None:
        ordered = sorted(names, key=lambda name: (-len(name), name))  # the longest match wins
        self._folded = tuple(name.casefold() for name in ordered)
        self._patterns = tuple(
            (re.compile(b"|".join(re.escape(name.encode(encoding)) for name in ordered), re.IGNORECASE), encoding)
            for encoding in (_TEXT_ENCODINGS if ordered else ())
        )

    def in_text(self, text: str) -> bool:
        folded = text.casefold()
        return any(name in folded for name in self._folded)

    def in_data(self, payload: bytes) -> bool:
        return any(pattern.search(payload) for pattern, _ in self._patterns)

    def blank_in(self, payload: bytes) -> bytes:
        for pattern, encoding in self._patterns:
            payload = pattern.sub(functools.partial(_placeholder, encoding=encoding), payload)
        return payload


def scrub(file: FlpFile) -> FlpFile:
    """``scrub_report(file).file``: the copy without the personal data ``scrub_report`` knows."""
    return scrub_report(file).file


def scrub_report(file: FlpFile) -> ScrubReport:
    """A copy without the personal data FL adds, with every event id and size unchanged.

    - The licensee (200) becomes zeros, which FL reads as no licensee.
    - Structured data events (STRUCTURED_DATA_EVENTS) are never changed: a
      user-folder path or a licensee's name found in one is listed in
      ``to_review`` with its reason instead.
    - In every other text and data event, the user name of a user-folder path
      becomes as many PLACEHOLDER_CHAR, in the same encoding (single-byte or UTF-16LE).
    - Then the licensee's names (``licensee_names``) are searched, ignoring case:
      a text event holding one becomes zeros (empty text), whole, and in a data
      event each occurrence becomes PLACEHOLDER_CHAR, as single-byte text or
      UTF-16LE. A licensee ending in digits is always searched whole; the name
      without its digits only from MIN_SEARCHED_NAME_CHARS characters on, and
      ``notes`` says when it was too short (NOTE_NAME_NOT_SEARCHED).

    Nothing else is changed: the project's own text (title, artists, comments,
    and channel, pattern, insert and file names) may still name people or
    places. The report lists what changed, and in ``to_review`` the non-empty
    title, artists, comments and URL (PROJECT_TEXT_EVENTS) to read by hand
    before sharing.
    """
    version = fl_version(file)
    names, too_short = _licensee_search(file)
    search = _NameSearch(names)
    events: List[Event] = []
    changes: List[ScrubChange] = []
    to_review: List[ScrubReview] = []
    for index, event in enumerate(file.events):
        scrubbed, reasons, kept = _scrub_event(event, version, search)
        events.append(scrubbed)
        if reasons:
            changes.append(ScrubChange(index, event.id, reasons))
        if kept:
            to_review.append(ScrubReview(index, event.id, kept))
    to_review.extend(
        ScrubReview(index, event.id, (PROJECT_TEXT_EVENTS[event.id],))
        for index, event in enumerate(events)
        if event.id in PROJECT_TEXT_EVENTS and any(event.payload)
    )
    return ScrubReport(
        dataclasses.replace(file, events=tuple(events)),
        tuple(changes),
        tuple(sorted(to_review, key=lambda entry: entry.index)),
        (NOTE_NAME_NOT_SEARCHED,) if too_short else (),
    )


def _scrub_event(
    event: Event, version: Optional[Version], search: _NameSearch
) -> Tuple[Event, Tuple[str, ...], Tuple[str, ...]]:
    """The event scrubbed, why it changed, and why it is kept but listed for review."""
    if event.id == LICENSEE_ID:
        blank = bytes(len(event.payload))
        if event.payload == blank:
            return event, (), ()
        return dataclasses.replace(event, payload=blank), (REASON_LICENSEE,), ()
    if payload_size(event.id) is not None:
        return event, (), ()
    if event.id in STRUCTURED_DATA_EVENTS:
        return event, (), _matches_in_structured(event.payload, search)
    reasons = []
    payload = event.payload
    for pattern, encoding in _ENCODINGS:
        payload = pattern.sub(functools.partial(_blank_name, encoding=encoding), payload)
    if payload != event.payload:
        reasons.append(REASON_USER_FOLDER)
    if event.id in TEXT_EVENT_IDS:
        if search.in_text(decode_text(Event(event.id, payload), version)):
            payload = bytes(len(payload))
            reasons.append(REASON_NAME_IN_TEXT)
    else:
        named = search.blank_in(payload)
        if named != payload:
            payload = named
            reasons.append(REASON_NAME_IN_DATA)
    return (dataclasses.replace(event, payload=payload), tuple(reasons), ()) if reasons else (event, (), ())


def _matches_in_structured(payload: bytes, search: _NameSearch) -> Tuple[str, ...]:
    found = []
    if next(_user_names_in(payload), None) is not None:
        found.append(REASON_USER_FOLDER_IN_STRUCTURED)
    if search.in_data(payload):
        found.append(REASON_NAME_IN_STRUCTURED)
    return tuple(found)


def _blank_name(match: "re.Match[bytes]", encoding: str) -> bytes:
    name = match.group(1)
    if name.decode(encoding, "replace").lower() in _NOT_PERSONAL:
        return match.group(0)
    prefix = match.group(0)[: match.start(1) - match.start(0)]
    return prefix + _placeholder_for(len(name), encoding)


def _placeholder(match: "re.Match[bytes]", encoding: str) -> bytes:
    return _placeholder_for(len(match.group(0)), encoding)


def _placeholder_for(byte_count: int, encoding: str) -> bytes:
    unit = PLACEHOLDER_CHAR.encode(encoding)
    return unit * (byte_count // len(unit))


# --- diffing -------------------------------------------------------------------------------


def diff_events(a: FlpFile, b: FlpFile, ignore: Iterable[int] = ()) -> Tuple[EventChange, ...]:
    """The events that differ between two files, ids in ``ignore`` left out.

    Meant for close files (minimal pairs, successive saves): the common start
    and end are skipped, and difflib aligns the rest.
    """
    skipped = frozenset(ignore)
    a_kept = [index for index, event in enumerate(a.events) if event.id not in skipped]
    b_kept = [index for index, event in enumerate(b.events) if event.id not in skipped]
    a_events = [a.events[index] for index in a_kept]
    b_events = [b.events[index] for index in b_kept]
    start = 0
    while start < min(len(a_events), len(b_events)) and a_events[start] == b_events[start]:
        start += 1
    end = 0
    while end < min(len(a_events), len(b_events)) - start and a_events[-1 - end] == b_events[-1 - end]:
        end += 1
    matcher = difflib.SequenceMatcher(
        None, a_events[start : len(a_events) - end], b_events[start : len(b_events) - end], autojunk=False
    )
    return tuple(
        EventChange(
            kind,
            tuple(a_kept[start + index] for index in range(a_first, a_last)),
            tuple(b_kept[start + index] for index in range(b_first, b_last)),
        )
        for kind, a_first, a_last, b_first, b_last in matcher.get_opcodes()
        if kind != "equal"
    )


def _is_int(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)
