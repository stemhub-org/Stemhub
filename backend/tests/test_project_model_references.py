"""Project model 0.1.0, second validation pass: how objects refer to one another.

check_references runs on a model that already passed the first pass and
returns every problem as {path, code, message}, path being a JSON pointer.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
from typing import Any, Callable

import pytest

from stemhub.project_model import Issue, ProjectModel, check_references, validate_document

EXAMPLES_DIR = Path(__file__).parent / "fixtures" / "project_model" / "examples"
EXAMPLE_NAMES = sorted(path.name for path in EXAMPLES_DIR.glob("*.json"))


def _example(name: str = "basic-beat.json") -> dict[str, Any]:
    return json.loads((EXAMPLES_DIR / name).read_text(encoding="utf-8"))


def _issues(mutate: Callable[[dict[str, Any]], None], name: str = "basic-beat.json") -> list[tuple[str, str]]:
    document = copy.deepcopy(_example(name))
    mutate(document)
    model = ProjectModel.model_validate(document)  # the first pass must accept it
    return [(issue.path, issue.code) for issue in check_references(model)]


def _notes(document: dict[str, Any], pattern: int = 0) -> list[dict[str, Any]]:
    return document["patterns"][pattern]["notes"]


def _clips(document: dict[str, Any], track: int = 0) -> list[dict[str, Any]]:
    return document["arrangements"][0]["tracks"][track]["clips"]


def _inserts(document: dict[str, Any]) -> list[dict[str, Any]]:
    return document["mixer"]["inserts"]


def _swap(items: list[Any], first: int, second: int) -> None:
    items[first], items[second] = items[second], items[first]


@pytest.mark.parametrize("name", EXAMPLE_NAMES)
def test_examples_have_consistent_references(name: str) -> None:
    assert check_references(ProjectModel.model_validate(_example(name))) == []


def test_issues_are_structured() -> None:
    model = ProjectModel.model_validate(_example())
    broken = model.model_copy(update={"current_arrangement_id": "arrangement:9"})

    issues = check_references(broken)

    assert issues == [
        Issue(
            path="/current_arrangement_id",
            code="dangling_reference",
            message="The current arrangement is not one of the arrangements.",
        )
    ]


# ── Duplicates ──


@pytest.mark.parametrize(
    ("mutate", "expected"),
    [
        pytest.param(
            lambda d: d["instruments"].append(d["instruments"][0]),
            [("/instruments/2/id", "duplicate_id")],
            id="instrument",
        ),
        pytest.param(
            lambda d: d["audio_sources"].append({**d["audio_sources"][0], "insert": {"status": "known", "insert_id": "insert:0"}}),
            [("/audio_sources/1/id", "duplicate_id")],
            id="audio source",
        ),
        pytest.param(
            lambda d: d["patterns"].append(d["patterns"][0]),
            [("/patterns/2/id", "duplicate_id")],
            id="pattern",
        ),
        pytest.param(
            lambda d: d["automation"].append(d["automation"][0]),
            [("/automation/1/id", "duplicate_id")],
            id="automation",
        ),
        pytest.param(
            lambda d: d["arrangements"].append({**d["arrangements"][0], "tracks": []}),
            [("/arrangements/1/id", "duplicate_id")],
            id="arrangement",
        ),
        pytest.param(
            lambda d: d["arrangements"][0]["tracks"][1].update(id="track:1", number=1),
            [("/arrangements/0/tracks/1/number", "duplicate_number")],
            id="track number",
        ),
        pytest.param(
            lambda d: _inserts(d).append(_inserts(d)[2]),
            [("/mixer/inserts/3/number", "duplicate_number")],
            id="insert number",
        ),
        pytest.param(
            lambda d: _inserts(d)[2]["effect_slots"].append(_inserts(d)[2]["effect_slots"][0]),
            [("/mixer/inserts/2/effect_slots/1/index", "duplicate_index")],
            id="effect slot index",
        ),
        pytest.param(
            lambda d: _inserts(d)[1]["sends"].append({"to_insert_id": "insert:2", "level": 0.5}),
            [("/mixer/inserts/1/sends/2/to_insert_id", "duplicate_send")],
            id="send",
        ),
    ],
)
def test_duplicates_are_rejected(mutate, expected) -> None:
    assert _issues(mutate) == expected


def test_the_same_track_numbers_may_appear_in_two_arrangements() -> None:
    def second_arrangement(document: dict[str, Any]) -> None:
        document["arrangements"].append({**document["arrangements"][0], "id": "arrangement:1"})

    assert _issues(second_arrangement) == []


# ── Dangling references ──


@pytest.mark.parametrize(
    ("mutate", "expected"),
    [
        pytest.param(
            lambda d: _notes(d)[0].update(instrument_id="instrument:9"),
            [("/patterns/0/notes/0/instrument_id", "dangling_reference")],
            id="note to a missing instrument",
        ),
        pytest.param(
            lambda d: _clips(d)[0].update(source_id="pattern:9"),
            [("/arrangements/0/tracks/0/clips/0/source_id", "dangling_reference")],
            id="pattern clip to a missing pattern",
        ),
        pytest.param(
            lambda d: _clips(d)[2].update(source_id="audio:9"),
            [("/arrangements/0/tracks/0/clips/2/source_id", "dangling_reference")],
            id="audio clip to a missing audio source",
        ),
        pytest.param(
            lambda d: _clips(d, 1)[1].update(source_id="automation:9"),
            [("/arrangements/0/tracks/1/clips/1/source_id", "dangling_reference")],
            id="automation clip to a missing automation",
        ),
        pytest.param(
            lambda d: d.update(current_arrangement_id="arrangement:1"),
            [("/current_arrangement_id", "dangling_reference")],
            id="current arrangement missing",
        ),
        pytest.param(
            lambda d: d["instruments"][0]["insert"].update(insert_id="insert:60"),
            [("/instruments/0/insert/insert_id", "dangling_reference")],
            id="instrument routed to an unlisted insert",
        ),
        pytest.param(
            lambda d: d["audio_sources"][0]["insert"].update(insert_id="insert:60"),
            [("/audio_sources/0/insert/insert_id", "dangling_reference")],
            id="audio source routed to an unlisted insert",
        ),
        pytest.param(
            lambda d: _inserts(d)[1]["sends"][1].update(to_insert_id="insert:60"),
            [("/mixer/inserts/1/sends/1/to_insert_id", "dangling_reference")],
            id="send to an unlisted insert",
        ),
        pytest.param(
            lambda d: d["automation"][0]["target"].update(insert_id="insert:60"),
            [("/automation/0/target/insert_id", "dangling_reference")],
            id="automation of an unlisted insert",
        ),
        pytest.param(
            lambda d: d["automation"][0].update(
                target={"kind": "instrument", "instrument_id": "instrument:9", "parameter": "volume"}
            ),
            [("/automation/0/target/instrument_id", "dangling_reference")],
            id="automation of a missing instrument",
        ),
        pytest.param(
            lambda d: d["automation"][0].update(
                target={"kind": "effect_slot", "insert_id": "insert:60", "slot_index": 0, "parameter": "dry_wet"}
            ),
            [("/automation/0/target/insert_id", "dangling_reference")],
            id="automation of an effect slot on an unlisted insert",
        ),
    ],
)
def test_dangling_references_are_rejected(mutate, expected) -> None:
    assert _issues(mutate) == expected


def test_notes_play_instruments_only_not_audio_sources() -> None:
    assert _issues(lambda d: _notes(d)[0].update(instrument_id="instrument:2")) == [
        ("/patterns/0/notes/0/instrument_id", "dangling_reference")
    ]


@pytest.mark.parametrize(
    "target",
    [
        None,
        {"kind": "tempo"},
        {"kind": "instrument", "instrument_id": "instrument:1", "parameter": "pan"},
        {"kind": "effect_slot", "insert_id": "insert:2", "slot_index": 0, "parameter": "dry_wet"},
    ],
)
def test_automation_targets_that_resolve_are_accepted(target) -> None:
    assert _issues(lambda d: d["automation"][0].update(target=target)) == []


def test_an_insert_cannot_send_to_itself() -> None:
    assert _issues(lambda d: _inserts(d)[1]["sends"][1].update(to_insert_id="insert:1")) == [
        ("/mixer/inserts/1/sends/1/to_insert_id", "send_to_self")
    ]


# ── Order ──


def test_notes_must_be_sorted_by_start() -> None:
    assert _issues(lambda d: _swap(_notes(d), 1, 2)) == [("/patterns/0/notes/2", "not_sorted")]


def test_notes_starting_together_are_sorted_by_ordinal() -> None:
    def chord(document: dict[str, Any]) -> None:
        notes = _notes(document)
        notes[1]["start_ticks"] = 0
        _swap(notes, 0, 1)

    assert _issues(chord) == [("/patterns/0/notes/1", "not_sorted")]


def test_notes_without_ordinals_starting_together_may_come_in_any_order() -> None:
    def chord_without_ordinals(document: dict[str, Any]) -> None:
        notes = _notes(document)
        notes[1]["start_ticks"] = 0
        for note in notes:
            note["extensions"] = {}
        _swap(notes, 0, 1)

    assert _issues(chord_without_ordinals) == []


def test_clips_must_be_sorted_by_start_on_each_track() -> None:
    assert _issues(lambda d: _swap(_clips(d), 0, 2)) == [("/arrangements/0/tracks/0/clips/1", "not_sorted")]


def test_automation_points_must_be_sorted_by_position() -> None:
    assert _issues(lambda d: d["automation"][0]["points"].reverse()) == [("/automation/0/points/1", "not_sorted")]


# ── Ordinals (extensions.fl_studio.ordinal) ──


def test_two_notes_of_a_pattern_cannot_share_an_ordinal() -> None:
    assert _issues(lambda d: _notes(d)[1]["extensions"]["fl_studio"].update(ordinal=0)) == [
        ("/patterns/0/notes/1/extensions/fl_studio/ordinal", "duplicate_ordinal")
    ]


def test_notes_of_different_patterns_may_share_an_ordinal() -> None:
    assert _issues(lambda d: None) == []  # both patterns number their notes from 0


def test_two_clips_of_an_arrangement_cannot_share_an_ordinal_even_on_different_tracks() -> None:
    assert _issues(lambda d: _clips(d, 1)[0]["extensions"]["fl_studio"].update(ordinal=0)) == [
        ("/arrangements/0/tracks/1/clips/0/extensions/fl_studio/ordinal", "duplicate_ordinal")
    ]


@pytest.mark.parametrize("bad_ordinal", [-1, 1.0, True, "0", None])
def test_ordinals_are_non_negative_integers(bad_ordinal) -> None:
    assert _issues(lambda d: _notes(d)[0]["extensions"]["fl_studio"].update(ordinal=bad_ordinal)) == [
        ("/patterns/0/notes/0/extensions/fl_studio/ordinal", "invalid_ordinal")
    ]


# ── Unknown values need an unsupported[] entry ──


def test_a_missing_tempo_needs_an_unsupported_entry() -> None:
    assert _issues(lambda d: d.update(tempo_bpm=None)) == [("/tempo_bpm", "missing_unsupported_entry")]


def test_a_missing_tempo_with_its_unsupported_entry_is_accepted() -> None:
    def no_tempo(document: dict[str, Any]) -> None:
        document["tempo_bpm"] = None
        document["unsupported"].append({"path": "/tempo_bpm", "reason": "No tempo event."})

    assert _issues(no_tempo) == []


def test_an_unknown_insert_needs_an_unsupported_entry_at_it_or_above_it() -> None:
    def unknown_routing(document: dict[str, Any], entry_path: str | None) -> None:
        document["instruments"][0]["insert"] = {"status": "unknown"}
        document["unsupported"] = []
        if entry_path is not None:
            document["unsupported"].append({"path": entry_path, "reason": "Routing not decoded."})

    assert _issues(lambda d: unknown_routing(d, None)) == [("/instruments/0/insert", "missing_unsupported_entry")]
    assert _issues(lambda d: unknown_routing(d, "/instruments/0/insert")) == []
    assert _issues(lambda d: unknown_routing(d, "/instruments/0")) == []
    # The example's own entry, /instruments, doesn't cover the audio sources.
    assert _issues(lambda d: d["audio_sources"][0].update(insert={"status": "unknown"})) == [
        ("/audio_sources/0/insert", "missing_unsupported_entry")
    ]


def test_an_unsupported_entry_covers_its_path_only_not_siblings_sharing_a_prefix() -> None:
    def unknown_tempo(document: dict[str, Any]) -> None:
        document["tempo_bpm"] = None
        document["unsupported"] = [{"path": "/tempo", "reason": "Not the tempo_bpm field."}]

    assert _issues(unknown_tempo) == [("/tempo_bpm", "missing_unsupported_entry")]


def test_an_unknown_automation_target_needs_an_unsupported_entry() -> None:
    assert _issues(lambda d: d["automation"][0].update(target={"status": "unknown"})) == [
        ("/automation/0/target", "missing_unsupported_entry")
    ]


# ── Through validate_document ──


def test_validate_document_runs_the_reference_pass_after_the_first_pass() -> None:
    document = _example()
    document["current_arrangement_id"] = "arrangement:7"

    result = validate_document(document)

    assert not result.valid
    assert result.model is not None
    assert [(issue.path, issue.code) for issue in result.issues] == [("/current_arrangement_id", "dangling_reference")]


def test_reference_messages_never_echo_ids() -> None:
    document = _example()
    _notes(document)[0]["instrument_id"] = "instrument:4242"

    result = validate_document(document)

    assert all("4242" not in issue.message for issue in result.issues)
