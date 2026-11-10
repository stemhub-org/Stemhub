"""Command line: regenerate the committed JSON Schema, validate a document, read FLP events.

    python -m stemhub.project_model schema [--output PATH] [--check]
    python -m stemhub.project_model validate FILE
    python -m stemhub.project_model flp dump FILE [--show-private]
    python -m stemhub.project_model flp diff A B [--ignore 237,167] [--show-private]
    python -m stemhub.project_model flp scrub FILE -o OUTPUT [--force]

``schema`` writes the JSON Schema generated from the models (by default to the
committed file); with ``--check`` it only says whether that file is up to date.
``validate`` runs both validation passes on a document. Exit codes: 0 fine,
1 out of date or invalid, 2 the file cannot be read.

``flp`` works on the raw events of an FL Studio project file (``flp_events``):
``dump`` lists them, ``diff`` lists the ones that differ (exit 1 if any) and
``scrub`` writes a copy without the licensee, the user names of user-folder
paths and the licensee's name (``flp_events.scrub_report``: text holding it is
blanked, and in other data only its bytes are replaced; structured data such as
notes and playlist items is never changed). It then lists what to read by hand:
the project's own text (title, artists, comments, URL) and any structured data
event that matched, and notes when the name without its digits was too short to
search. ``scrub`` never writes over its input and replaces an existing output
only with ``--force``. Unless ``--show-private`` is given, output shows every
event scrubbed and never shows the licensee, the data folder path or a
structured data event that matched. Signs that a file was read out of step
(``flp_events.check_structure``) go to stderr as warnings.
"""
from __future__ import annotations

import argparse
import contextlib
import os
import shutil
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import FrozenSet, List, Optional, Sequence, Tuple

from . import flp_events
from .flp_events import Event, FlpFile, FlpFormatError, Version
from .schema import JSON_SCHEMA_PATH, SCHEMA_VERSION, render_json_schema
from .validation import validate_json

REGENERATE_HINT = "Regenerate it with: python -m stemhub.project_model schema"


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = _parser().parse_args(argv)
    if args.command == "schema":
        return _schema(args.output, check=args.check)
    if args.command == "flp":
        return _FLP_COMMANDS[args.flp_command](args)
    return _validate(args.file)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m stemhub.project_model", description="StemHub project model tools."
    )
    commands = parser.add_subparsers(dest="command", required=True)

    schema = commands.add_parser("schema", help="Write the JSON Schema generated from the models.")
    schema.add_argument("--output", type=Path, default=JSON_SCHEMA_PATH, help="Default: the committed file.")
    schema.add_argument("--check", action="store_true", help="Only check that the file is up to date.")

    validate = commands.add_parser("validate", help="Validate a project model document.")
    validate.add_argument("file", type=Path)

    _add_flp_parser(commands)
    return parser


def _add_flp_parser(commands: argparse._SubParsersAction) -> None:
    flp = commands.add_parser("flp", help="Read the raw events of an FL Studio project file.")
    flp_commands = flp.add_subparsers(dest="flp_command", required=True)
    private = {
        "action": "store_true",
        "help": "Show events as stored: the licensee, the data folder path, user names, the licensee's "
        "name, and structured data that matched it.",
    }

    dump = flp_commands.add_parser("dump", help="List every event: index, offset, id, size and value.")
    dump.add_argument("file", type=Path)
    dump.add_argument("--show-private", **private)

    diff = flp_commands.add_parser("diff", help="List the events that differ between two project files.")
    diff.add_argument("a", type=Path)
    diff.add_argument("b", type=Path)
    diff.add_argument(
        "--ignore", type=_event_ids, default=frozenset(), help="Event ids to leave out, comma-separated (e.g. 237,167)."
    )
    diff.add_argument("--show-private", **private)

    scrub = flp_commands.add_parser(
        "scrub",
        help="Write a copy without the licensee, user-folder names and the licensee's name (text holding it "
        "blanked, its bytes replaced in other data; structured data such as notes never changed); list the "
        "title, artists, comments, URL and any structured data that matched, to read by hand.",
    )
    scrub.add_argument("file", type=Path)
    scrub.add_argument("-o", "--output", type=Path, required=True, help="A new file, never the input.")
    scrub.add_argument("--force", action="store_true", help="Replace the output if it exists (never the input).")


def _event_ids(text: str) -> FrozenSet[int]:
    try:
        ids = frozenset(int(part) for part in text.split(",") if part.strip())
    except ValueError:
        raise argparse.ArgumentTypeError("expected event ids such as 237,167") from None
    if not all(0 <= event_id <= 0xFF for event_id in ids):
        raise argparse.ArgumentTypeError("event ids are 0..255")
    return ids


def _schema(output: Path, *, check: bool) -> int:
    expected = render_json_schema()
    if not check:
        output.write_text(expected, encoding="utf-8")
        print(f"Wrote {output}")
        return 0
    try:
        current = output.read_text(encoding="utf-8")
    except OSError:
        current = None
    if current != expected:
        print(f"{output} is missing or out of date. {REGENERATE_HINT}", file=sys.stderr)
        return 1
    print(f"{output} is up to date.")
    return 0


def _validate(file: Path) -> int:
    try:
        body = file.read_bytes()
    except OSError as error:
        print(f"cannot read {file}: {error.strerror or error}", file=sys.stderr)
        return 2
    result = validate_json(body)
    if result.valid:
        print(f"{file}: valid (schema {SCHEMA_VERSION})")
        return 0
    print(f"{file}: {len(result.issues)} problem(s)")
    for issue in result.issues:
        print(f"  {issue.path or '(document)'}  {issue.code}  {issue.message}")
    return 1


# --- flp -----------------------------------------------------------------------------------

_PRIVATE_EVENT_IDS = frozenset({flp_events.LICENSEE_ID, flp_events.DATA_PATH_ID})
_HEX_PREVIEW_BYTES = 32
_TEXT_PREVIEW_CHARS = 120
_WINDOW_BYTES = 8  # bytes shown on each side of the first difference in a long event


def _flp_dump(args: argparse.Namespace) -> int:
    file = _read_flp(args.file)
    if file is None:
        return 2
    print(f"{args.file}: {_summary(file)}")
    _warn_about_structure(args.file, file)
    view = _View.of(file, show_private=args.show_private)
    for index in range(len(file.events)):
        print(view.line(index))
    return 0


def _flp_diff(args: argparse.Namespace) -> int:
    a, b = _read_flp(args.a), _read_flp(args.b)
    if a is None or b is None:
        return 2
    print(f"a: {args.a}: {_summary(a)}")
    print(f"b: {args.b}: {_summary(b)}")
    _warn_about_structure(args.a, a)
    _warn_about_structure(args.b, b)
    header_changes = [
        f"header: {name} {getattr(a.header, name)} -> {getattr(b.header, name)}"
        for name in ("format", "channel_count", "ppq")
        if getattr(a.header, name) != getattr(b.header, name)
    ]
    for line in header_changes:
        print(line)
    changes = flp_events.diff_events(a, b, args.ignore)
    a_view = _View.of(a, show_private=args.show_private)
    b_view = _View.of(b, show_private=args.show_private)
    for change in changes:
        _print_change(change, a_view, b_view)
    ignoring = f" (ignoring {', '.join(map(str, sorted(args.ignore)))})" if args.ignore else ""
    if not changes and not header_changes:
        print(f"No differences{ignoring}.")
        return 0
    print(f"{len(changes)} change(s){ignoring}.")
    return 1


def _print_change(change: flp_events.EventChange, a_view: "_View", b_view: "_View") -> None:
    print(f"--- {change.kind} a{list(change.a_indexes)} b{list(change.b_indexes)}")
    print(
        "\n".join(
            [f"  - {a_view.line(index)}" for index in change.a_indexes]
            + [f"  + {b_view.line(index)}" for index in change.b_indexes]
        )
    )
    for a_index, b_index in zip(change.a_indexes, change.b_indexes):
        for line in _difference_window(a_view, a_index, b_view, b_index):
            print(line)
    # diff_events compares the events as stored: say so when all that differs is hidden.
    if a_view.public(change.a_indexes) == b_view.public(change.b_indexes):
        print("  (differs in private data only: --show-private shows it)")


def _difference_window(a_view: "_View", a_index: int, b_view: "_View", b_index: int) -> List[str]:
    """Where two versions of an event first differ, when their lines can't show it."""
    a_event, b_event = a_view.shown.events[a_index], b_view.shown.events[b_index]
    if a_event.id != b_event.id or a_view.hides(a_index) or b_view.hides(b_index) or a_event.payload == b_event.payload:
        return []
    lines_show_it = a_view.describe(a_index) != b_view.describe(b_index)
    if lines_show_it and not (a_view.is_cut(a_event) or b_view.is_cut(b_event)):
        return []
    a_payload, b_payload = a_event.payload, b_event.payload
    offset = next(
        (index for index, (x, y) in enumerate(zip(a_payload, b_payload)) if x != y), min(len(a_payload), len(b_payload))
    )
    start, stop = max(0, offset - _WINDOW_BYTES), offset + _WINDOW_BYTES + 1
    return [
        f"    first difference at byte {offset} of the payload ({len(a_payload)} and {len(b_payload)} bytes):",
        f"    a [{start}:{min(stop, len(a_payload))}] {a_payload[start:stop].hex(' ')}",
        f"    b [{start}:{min(stop, len(b_payload))}] {b_payload[start:stop].hex(' ')}",
    ]


def _flp_scrub(args: argparse.Namespace) -> int:
    file = _read_flp(args.file)
    if file is None:
        return 2
    if _same_file(args.output, args.file):
        print(f"{args.output} is the input file: choose another output", file=sys.stderr)
        return 2
    _warn_about_structure(args.file, file)
    report = flp_events.scrub_report(file)
    try:
        _write_output(args.output, flp_events.serialize(report.file), replace=args.force)
    except FileExistsError:
        print(f"{args.output} already exists: pass --force to replace it", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"cannot write {args.output}: {error.strerror or error}", file=sys.stderr)
        return 2
    print(f"Wrote {args.output}: {len(report.changes)} event(s) scrubbed, sizes unchanged.")
    for change in report.changes:
        print(f"  #{change.index} id {change.event_id}: {', '.join(change.reasons)}")
    if report.to_review:
        print(
            "Kept as they are; read them before sharing the file "
            "(flp dump shows them; structured data with --show-private):"
        )
        for entry in report.to_review:
            print(f"  #{entry.index} id {entry.event_id} ({', '.join(entry.reasons)})")
    for note in report.notes:
        print(f"Note: {note}")
    return 0


_FLP_COMMANDS = {"dump": _flp_dump, "diff": _flp_diff, "scrub": _flp_scrub}


def _read_flp(path: Path) -> Optional[FlpFile]:
    try:
        return flp_events.parse(path.read_bytes())
    except OSError as error:
        print(f"cannot read {path}: {error.strerror or error}", file=sys.stderr)
    except FlpFormatError as error:
        print(f"{path}: not a readable FL Studio project file: {error}", file=sys.stderr)
    return None


def _warn_about_structure(path: Path, file: FlpFile) -> None:
    for warning in flp_events.check_structure(file):
        print(f"{path}: warning: {warning}", file=sys.stderr)


def _same_file(output: Path, source: Path) -> bool:
    """Whether ``output`` names the input: its path, a link to it, or its name in another case."""
    try:
        return os.path.samefile(output, source)
    except OSError:  # the output doesn't exist yet, so it isn't the input
        return False


def _write_output(path: Path, data: bytes, *, replace: bool) -> None:
    """Write a new file. With ``replace``, an existing file is replaced whole, never written into."""
    try:
        stream = open(path, "xb")  # refuses anything already there, a link included
    except FileExistsError:
        if not replace:
            raise
        _replace_file(path, data)
        return
    try:
        with stream:
            stream.write(data)
    except BaseException:
        path.unlink(missing_ok=True)
        raise


def _replace_file(path: Path, data: bytes) -> None:
    descriptor, temporary = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.", suffix=".tmp")
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
        with contextlib.suppress(OSError):  # keeps the replaced file's permissions when they can be read
            shutil.copymode(path, temporary)
        os.replace(temporary, path)
    except BaseException:
        with contextlib.suppress(OSError):
            os.unlink(temporary)
        raise


def _summary(file: FlpFile) -> str:
    version = flp_events.fl_version(file)
    header = file.header
    return (
        f"FL {'.'.join(map(str, version)) if version else 'version unknown'}, format {header.format}, "
        f"{header.channel_count} channel(s), ppq {header.ppq}, {len(file.events)} events"
    )


@dataclass(frozen=True)
class _View:
    """A file's events as the output shows them: scrubbed, private events hidden, unless --show-private.

    Hidden: the licensee and the data folder path, and the structured data events
    that scrubbing kept although they matched (``to_review``).
    """

    shown: FlpFile
    version: Optional[Version]
    offsets: Tuple[int, ...]
    show_private: bool
    hidden: FrozenSet[int] = frozenset()  # indexes of matched structured data events

    @classmethod
    def of(cls, file: FlpFile, *, show_private: bool) -> "_View":
        version, offsets = flp_events.fl_version(file), flp_events.event_offsets(file)
        if show_private:
            return cls(file, version, offsets, show_private)
        report = flp_events.scrub_report(file)
        hidden = frozenset(
            entry.index for entry in report.to_review if entry.event_id in flp_events.STRUCTURED_DATA_EVENTS
        )
        return cls(report.file, version, offsets, show_private, hidden)

    def hides(self, index: int) -> bool:
        if self.show_private:
            return False
        return self.shown.events[index].id in _PRIVATE_EVENT_IDS or index in self.hidden

    def public(self, indexes: Sequence[int]) -> Tuple[object, ...]:
        """All the output could ever show of these events, however long."""
        return tuple(self._public(index) for index in indexes)

    def _public(self, index: int) -> object:
        event = self.shown.events[index]
        return (event.id, len(event.payload), event.length_width) if self.hides(index) else event

    def line(self, index: int) -> str:
        return f"#{index:<6} @{self.offsets[index]:<9} {self.describe(index)}"

    def describe(self, index: int) -> str:
        event = self.shown.events[index]
        head = f"id {event.id:<3}"
        if flp_events.payload_size(event.id) is not None:
            return f"{head} {len(event.payload)}B  {event.int_value} (0x{event.int_value:x})"
        head = f"{head} len {len(event.payload)}"
        if event.length_width is not None:
            head = f"{head} (length prefix {event.length_width} bytes)"
        if self.hides(index):
            return f"{head}  <private: --show-private shows it>"
        if event.id in flp_events.TEXT_EVENT_IDS:
            text = flp_events.decode_text(event, self.version)
            more = "..." if len(text) > _TEXT_PREVIEW_CHARS else ""
            return f"{head}  {text[:_TEXT_PREVIEW_CHARS]!r}{more}"
        more = " ..." if len(event.payload) > _HEX_PREVIEW_BYTES else ""
        return f"{head}  {event.payload[:_HEX_PREVIEW_BYTES].hex(' ')}{more}"

    def is_cut(self, event: Event) -> bool:
        """Whether ``describe`` shows only the start of the event."""
        if flp_events.payload_size(event.id) is not None:
            return False
        if event.id in flp_events.TEXT_EVENT_IDS:
            return len(flp_events.decode_text(event, self.version)) > _TEXT_PREVIEW_CHARS
        return len(event.payload) > _HEX_PREVIEW_BYTES


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:  # output piped into head and the like
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        sys.exit(1)
