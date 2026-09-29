"""Project model 0.1.0, second validation pass: how objects refer to one another.

``check_references`` takes a model that passed the first pass (the Pydantic
models in ``schema``) and returns every problem it finds, in document order:

- IDs, track numbers, insert numbers, effect slot indexes and sends are unique
  where they must be;
- every reference resolves: a note's instrument, a clip's source, an
  instrument's or audio source's insert, a send's destination, an automation
  target, the current arrangement. An insert that something refers to is
  listed, even when it has default values;
- notes and clips are sorted by (start_ticks, ordinal) and automation points
  by position, so two readers of one file write the same document;
- ordinals (``extensions.fl_studio.ordinal``, the item's place in the project
  file) are non-negative integers, unique per pattern for notes and per
  arrangement for clips (FL Studio keeps one list of clips per arrangement);
- a value the reader could not read (a null tempo, an ``{"status": "unknown"}``
  insert or automation target) has an ``unsupported[]`` entry at its path or
  above it.

Messages never quote IDs or names: they end up in API responses and logs.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Iterator, Optional, Sequence

from .issues import Issue, PathPart, pointer
from .schema import (
    Arrangement,
    Automation,
    AutomationClip,
    AudioClip,
    EffectSlotTarget,
    Insert,
    InsertTarget,
    InstrumentTarget,
    KnownInsert,
    Mixer,
    Pattern,
    PatternClip,
    ProjectModel,
    UnknownValue,
)

EXTENSIONS_NAMESPACE = "fl_studio"
ORDINAL_KEY = "ordinal"

_MISSING = object()
_INVALID = object()


@dataclass(frozen=True)
class _Known:
    """The IDs a document declares, first occurrence of each."""

    instruments: frozenset[str]
    audio_sources: frozenset[str]
    patterns: frozenset[str]
    automation: frozenset[str]
    arrangements: frozenset[str]
    inserts: frozenset[str]
    unsupported_paths: tuple[str, ...]


def check_references(model: ProjectModel) -> list[Issue]:
    """Every reference problem of a model that passed the first validation pass."""
    known = _Known(
        instruments=frozenset(instrument.id for instrument in model.instruments),
        audio_sources=frozenset(source.id for source in model.audio_sources),
        patterns=frozenset(pattern.id for pattern in model.patterns),
        automation=frozenset(automation.id for automation in model.automation),
        arrangements=frozenset(arrangement.id for arrangement in model.arrangements),
        inserts=frozenset(insert.id for insert in model.mixer.inserts),
        unsupported_paths=tuple(entry.path for entry in model.unsupported),
    )
    return [
        *_check_tempo(model, known),
        *_check_instruments(model, known),
        *_check_audio_sources(model, known),
        *_check_patterns(model.patterns, known),
        *_check_arrangements(model.arrangements, known),
        *_check_current_arrangement(model, known),
        *_check_mixer(model.mixer, known),
        *_check_automation(model.automation, known),
    ]


# ── Sections ──


def _check_tempo(model: ProjectModel, known: _Known) -> Iterator[Issue]:
    if model.tempo_bpm is None:
        yield from _needs_unsupported_entry(pointer("tempo_bpm"), known)


def _check_instruments(model: ProjectModel, known: _Known) -> Iterator[Issue]:
    yield from _duplicate_ids(model.instruments, "instruments", "instrument")
    for index, instrument in enumerate(model.instruments):
        yield from _check_insert_ref(instrument.insert, ("instruments", index, "insert"), known)


def _check_audio_sources(model: ProjectModel, known: _Known) -> Iterator[Issue]:
    yield from _duplicate_ids(model.audio_sources, "audio_sources", "audio source")
    for index, source in enumerate(model.audio_sources):
        yield from _check_insert_ref(source.insert, ("audio_sources", index, "insert"), known)


def _check_patterns(patterns: Sequence[Pattern], known: _Known) -> Iterator[Issue]:
    yield from _duplicate_ids(patterns, "patterns", "pattern")
    for pattern_index, pattern in enumerate(patterns):
        base = ("patterns", pattern_index, "notes")
        for note_index, note in enumerate(pattern.notes):
            if note.instrument_id not in known.instruments:
                yield Issue(
                    pointer(*base, note_index, "instrument_id"),
                    "dangling_reference",
                    "The note's instrument is not one of the instruments.",
                )
        yield from _check_ordered_items(
            [(note.start_ticks, note.extensions) for note in pattern.notes],
            base,
            not_sorted="Notes must be sorted by start_ticks, then by ordinal.",
            duplicate="Another note of this pattern has the same ordinal.",
        )


def _check_arrangements(arrangements: Sequence[Arrangement], known: _Known) -> Iterator[Issue]:
    yield from _duplicate_ids(arrangements, "arrangements", "arrangement")
    for index, arrangement in enumerate(arrangements):
        yield from _check_arrangement(arrangement, index, known)


def _check_arrangement(arrangement: Arrangement, index: int, known: _Known) -> Iterator[Issue]:
    seen_numbers: set[int] = set()
    seen_ordinals: set[int] = set()  # clip ordinals are unique across the arrangement's tracks
    for track_index, track in enumerate(arrangement.tracks):
        base = ("arrangements", index, "tracks", track_index)
        if track.number in seen_numbers:
            yield Issue(
                pointer(*base, "number"),
                "duplicate_number",
                "Another track of this arrangement has the same number.",
            )
        seen_numbers.add(track.number)
        for clip_index, clip in enumerate(track.clips):
            yield from _check_clip_source(clip, (*base, "clips", clip_index), known)
        yield from _check_ordered_items(
            [(clip.start_ticks, clip.extensions) for clip in track.clips],
            (*base, "clips"),
            not_sorted="Clips of a track must be sorted by start_ticks, then by ordinal.",
            duplicate="Another clip of this arrangement has the same ordinal.",
            seen_ordinals=seen_ordinals,
        )


def _check_clip_source(clip: Any, path: tuple[PathPart, ...], known: _Known) -> Iterator[Issue]:
    if isinstance(clip, PatternClip):
        sources, listed_in = known.patterns, "one of the patterns"
    elif isinstance(clip, AudioClip):
        sources, listed_in = known.audio_sources, "one of the audio sources"
    elif isinstance(clip, AutomationClip):
        sources, listed_in = known.automation, "in the automation list"
    else:  # pragma: no cover - the first pass only builds the three kinds
        return
    if clip.source_id not in sources:
        yield Issue(pointer(*path, "source_id"), "dangling_reference", f"The clip's source is not {listed_in}.")


def _check_current_arrangement(model: ProjectModel, known: _Known) -> Iterator[Issue]:
    if model.current_arrangement_id not in known.arrangements:
        yield Issue(
            pointer("current_arrangement_id"),
            "dangling_reference",
            "The current arrangement is not one of the arrangements.",
        )


def _check_mixer(mixer: Mixer, known: _Known) -> Iterator[Issue]:
    seen_numbers: set[int] = set()
    for index, insert in enumerate(mixer.inserts):
        if insert.number in seen_numbers:
            yield Issue(
                pointer("mixer", "inserts", index, "number"),
                "duplicate_number",
                "Another insert has the same number.",
            )
        seen_numbers.add(insert.number)
        yield from _check_sends(insert, index, known)
        yield from _check_effect_slots(insert, index)


def _check_sends(insert: Insert, index: int, known: _Known) -> Iterator[Issue]:
    destinations: set[str] = set()
    for send_index, send in enumerate(insert.sends):
        path = pointer("mixer", "inserts", index, "sends", send_index, "to_insert_id")
        if send.to_insert_id == insert.id:
            yield Issue(path, "send_to_self", "An insert cannot send to itself.")
        elif send.to_insert_id in destinations:
            yield Issue(path, "duplicate_send", "The insert already sends to this destination.")
        elif send.to_insert_id not in known.inserts:
            yield Issue(path, "dangling_reference", _UNLISTED_INSERT.format(what="The send's destination"))
        destinations.add(send.to_insert_id)


def _check_effect_slots(insert: Insert, index: int) -> Iterator[Issue]:
    seen: set[int] = set()
    for slot_index, slot in enumerate(insert.effect_slots):
        if slot.index in seen:
            yield Issue(
                pointer("mixer", "inserts", index, "effect_slots", slot_index, "index"),
                "duplicate_index",
                "Another effect slot of this insert has the same index.",
            )
        seen.add(slot.index)


def _check_automation(automation: Sequence[Automation], known: _Known) -> Iterator[Issue]:
    yield from _duplicate_ids(automation, "automation", "automation")
    for index, item in enumerate(automation):
        yield from _check_target(item.target, ("automation", index, "target"), known)
        positions = [point.position_ticks for point in item.points]
        unsorted = next((i for i in range(1, len(positions)) if positions[i] < positions[i - 1]), None)
        if unsorted is not None:
            yield Issue(
                pointer("automation", index, "points", unsorted),
                "not_sorted",
                "Automation points must be sorted by position_ticks.",
            )


def _check_target(target: Any, path: tuple[PathPart, ...], known: _Known) -> Iterator[Issue]:
    if isinstance(target, UnknownValue):
        yield from _needs_unsupported_entry(pointer(*path), known)
    elif isinstance(target, InstrumentTarget) and target.instrument_id not in known.instruments:
        yield Issue(
            pointer(*path, "instrument_id"),
            "dangling_reference",
            "The automation target's instrument is not one of the instruments.",
        )
    elif isinstance(target, (InsertTarget, EffectSlotTarget)) and target.insert_id not in known.inserts:
        yield Issue(
            pointer(*path, "insert_id"),
            "dangling_reference",
            _UNLISTED_INSERT.format(what="The automation target's insert"),
        )


# ── Shared checks ──

_UNLISTED_INSERT = "{what} is not one of the mixer's inserts; an insert something refers to is listed, even with default values."


def _duplicate_ids(items: Sequence[Any], list_name: str, noun: str) -> Iterator[Issue]:
    seen: set[str] = set()
    for index, item in enumerate(items):
        if item.id in seen:
            yield Issue(pointer(list_name, index, "id"), "duplicate_id", f"Another {noun} has the same id.")
        seen.add(item.id)


def _check_insert_ref(ref: Any, path: tuple[PathPart, ...], known: _Known) -> Iterator[Issue]:
    if isinstance(ref, UnknownValue):
        yield from _needs_unsupported_entry(pointer(*path), known)
    elif isinstance(ref, KnownInsert) and ref.insert_id not in known.inserts:
        yield Issue(pointer(*path, "insert_id"), "dangling_reference", _UNLISTED_INSERT.format(what="The insert"))


def _needs_unsupported_entry(path: str, known: _Known) -> Iterator[Issue]:
    if not any(_covers(entry, path) for entry in known.unsupported_paths):
        yield Issue(
            path,
            "missing_unsupported_entry",
            "A value the reader could not read needs an unsupported[] entry at its path or above it.",
        )


def _covers(entry: str, path: str) -> bool:
    # "/instruments" covers "/instruments/0/insert", not "/instruments_extra"; "" covers everything.
    return path == entry or path.startswith(entry + "/")


def _check_ordered_items(
    items: Sequence[tuple[int, dict[str, dict[str, Any]]]],
    base: tuple[PathPart, ...],
    *,
    not_sorted: str,
    duplicate: str,
    seen_ordinals: Optional[set[int]] = None,
) -> Iterator[Issue]:
    """Notes of a pattern or clips of a track: valid unique ordinals, sorted by (start, ordinal).

    Only the first item out of order is reported: one move usually fixes the rest.
    """
    seen = set() if seen_ordinals is None else seen_ordinals
    keys: list[tuple[int, Optional[int]]] = []
    for index, (start, extensions) in enumerate(items):
        ordinal = _ordinal(extensions)
        if ordinal is _INVALID:
            yield Issue(_ordinal_path(base, index), "invalid_ordinal", "An ordinal is a non-negative integer.")
            ordinal = _MISSING
        elif ordinal is not _MISSING and ordinal in seen:
            yield Issue(_ordinal_path(base, index), "duplicate_ordinal", duplicate)
        elif ordinal is not _MISSING:
            seen.add(ordinal)
        keys.append((start, None if ordinal is _MISSING else ordinal))
    unsorted = next((i for i in range(1, len(keys)) if _before(keys[i], keys[i - 1])), None)
    if unsorted is not None:
        yield Issue(pointer(*base, unsorted), "not_sorted", not_sorted)


def _ordinal_path(base: tuple[PathPart, ...], index: int) -> str:
    return pointer(*base, index, "extensions", EXTENSIONS_NAMESPACE, ORDINAL_KEY)


def _before(item: tuple[int, Optional[int]], previous: tuple[int, Optional[int]]) -> bool:
    (start, ordinal), (previous_start, previous_ordinal) = item, previous
    if start != previous_start:
        return start < previous_start
    # Items starting together are ordered by ordinal when both have one.
    return ordinal is not None and previous_ordinal is not None and ordinal < previous_ordinal


def _ordinal(extensions: dict[str, dict[str, Any]]) -> Any:
    namespace = extensions.get(EXTENSIONS_NAMESPACE)
    if namespace is None or ORDINAL_KEY not in namespace:
        return _MISSING
    value = namespace[ORDINAL_KEY]
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        return _INVALID
    return value
