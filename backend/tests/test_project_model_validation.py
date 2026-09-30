"""validate_json / validate_document: parsing, both passes, issue pointers and the issue cap.

The first pass is tested in test_project_model_schema.py and the second in
test_project_model_references.py; this file covers what joins them.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest

from stemhub.project_model import validate_document, validate_json
from stemhub.project_model.validation import json_pointer, parse_json

EXAMPLES_DIR = Path(__file__).parent / "fixtures" / "project_model" / "examples"


def _example_text(name: str = "basic-beat.json") -> str:
    return (EXAMPLES_DIR / name).read_text(encoding="utf-8")


def _example(name: str = "basic-beat.json") -> dict[str, Any]:
    return json.loads(_example_text(name))


def _codes(result) -> list[tuple[str, str]]:
    return [(issue.path, issue.code) for issue in result.issues]


# ── Parsing ──


def test_validate_json_takes_text_as_well_as_bytes() -> None:
    assert validate_json(_example_text()).valid


@pytest.mark.parametrize(
    "body",
    [
        pytest.param(_example_text().encode("utf-16"), id="UTF-16"),
        pytest.param(b"\xef\xbb\xbf" + _example_text().encode("utf-8"), id="UTF-8 with a byte order mark"),
        pytest.param(b"[" * 100_000, id="nested deeper than the parser's stack"),
        pytest.param(_example_text().replace('"ppq": 96', '"ppq": ' + "9" * 5000).encode(), id="integer of 5000 digits"),
        pytest.param(b"", id="empty"),
    ],
)
def test_bodies_that_are_not_utf8_json_are_one_issue_at_the_root(body: bytes) -> None:
    result = validate_json(body)

    assert _codes(result) == [("", "json_invalid")]
    assert result.model is None


def test_an_object_repeating_a_key_is_refused() -> None:
    body = _example_text().replace('"tempo_bpm": 90.0,', '"tempo_bpm": 90.0, "tempo_bpm": 180.0,')

    assert _codes(validate_json(body)) == [("", "json_invalid")]


def test_parse_json_keeps_equal_keys_in_different_objects() -> None:
    assert parse_json('{"a": {"x": 1}, "b": {"x": 2}}') == {"a": {"x": 1}, "b": {"x": 2}}


def test_an_overflowing_number_is_reported_where_it_is() -> None:
    body = _example_text().replace('"tempo_bpm": 90.0', '"tempo_bpm": 1e400')

    assert _codes(validate_json(body)) == [("/tempo_bpm", "finite_number")]


def test_a_lone_surrogate_in_a_name_is_refused_where_it_is() -> None:
    body = _example_text().replace('"name": "Kick"', '"name": "K\\ud800ick"')

    assert _codes(validate_json(body)) == [("/instruments/0/name", "string_unicode")]


def test_deep_extensions_are_refused_by_the_depth_cap() -> None:
    # 200 levels parse on every Python version; much deeper bodies are json_invalid (tested above).
    body = _example_text().replace('"channel_iid": 0', '"channel_iid": ' + "[" * 200 + "]" * 200)

    assert _codes(validate_json(body)) == [("/instruments/0/extensions", "extensions_too_deep")]


# ── Both passes ──


def test_a_valid_document_gives_its_model_and_no_issue() -> None:
    result = validate_document(_example())

    assert result.valid
    assert result.model is not None
    assert result.model.tempo_bpm == 90.0
    assert result.truncated is False


def test_the_reference_pass_does_not_run_when_the_first_pass_fails() -> None:
    document = _example()
    document["current_arrangement_id"] = "arrangement:9"  # a reference problem
    document["tempo_bpm"] = -1  # a first-pass problem

    assert _codes(validate_document(document)) == [("/tempo_bpm", "greater_than")]


# ── The issue cap ──


def test_max_issues_keeps_the_first_issues_and_says_so() -> None:
    document = _example()
    document["instruments"][0]["pan"] = 5
    document["audio_sources"][0]["name"] = 5
    document["patterns"][0]["name"] = 5

    result = validate_document(document, max_issues=2)

    assert _codes(result) == [
        ("/instruments/0/pan", "less_than_equal"),
        ("/audio_sources/0/name", "string_type"),
    ]
    assert result.truncated is True
    assert validate_document(document, max_issues=3).truncated is False


def test_a_body_full_of_invalid_items_gives_a_bounded_number_of_issues() -> None:
    # Each invalid item, unknown field and extension namespace is an error: without
    # the bounds, a 10 MiB body could make validation build millions of them.
    unknown = {f"field_{index}": index for index in range(2000)}
    document = _example()
    document["instruments"] = [{**document["instruments"][0], **unknown}] * 999
    for pattern in document["patterns"]:
        pattern["notes"] = [{}] * 2000
    document["arrangements"][0]["markers"] = [{}] * 2000
    for insert in document["mixer"]["inserts"]:
        insert["sends"] = [{**unknown}] * 126
    document["project"].update(unknown)
    document["extensions"] = {f"Bad Key {index}": {} for index in range(2000)}

    result = validate_document(document)

    assert not result.valid
    assert len(result.issues) <= 100


def test_max_issues_applies_to_reference_issues_too() -> None:
    document = _example()
    for note in document["patterns"][0]["notes"]:
        note["instrument_id"] = "instrument:9"

    result = validate_document(document, max_issues=3)

    assert len(result.issues) == 3
    assert result.truncated is True
    assert result.model is not None


# ── JSON pointers ──


PLUGIN_INSTRUMENT = {"instruments": [{"kind": "plugin", "plugin": {"name": 5}}]}
KNOWN_INSERT = {"instruments": [{"kind": "sampler", "insert": {"status": "known"}}]}
PROJECT_SAMPLE = {"audio_sources": [{"sample": {"location": "project"}}]}
INSERT_TARGET = {"automation": [{"target": {"kind": "insert"}}]}


@pytest.mark.parametrize(
    ("loc", "document", "expected"),
    [
        pytest.param(("instruments", 0, "plugin", "id"), PLUGIN_INSTRUMENT, "/instruments/0/id", id="tag of a list item"),
        pytest.param(("instruments", 0, "plugin", "name"), PLUGIN_INSTRUMENT, "/instruments/0/name", id="tag equal to a present field"),
        pytest.param(("instruments", 0, "plugin", "plugin", "name"), PLUGIN_INSTRUMENT, "/instruments/0/plugin/name", id="field named like the tag, after it"),
        pytest.param(("instruments", 0, "plugin", "sample"), PLUGIN_INSTRUMENT, "/instruments/0/sample", id="missing field after the tag"),
        pytest.param(("instruments", 0, "sampler", "insert", "known", "insert_id"), KNOWN_INSERT, "/instruments/0/insert/insert_id", id="union inside a union"),
        pytest.param(("audio_sources", 0, "sample", "project", "asset_path"), PROJECT_SAMPLE, "/audio_sources/0/sample/asset_path", id="location tag"),
        pytest.param(("automation", 0, "target", "insert", "parameter"), INSERT_TARGET, "/automation/0/target/parameter", id="tag equal to a union field name"),
        pytest.param(("extensions", "a/b~c", "[key]"), {"extensions": {"a/b~c": {}}}, "/extensions/<namespace>", id="invalid namespace"),
        pytest.param(("patterns", 0, "extensions", "my_ns"), {"patterns": [{"extensions": {"my_ns": []}}]}, "/patterns/0/extensions/<namespace>", id="namespace that is not an object"),
        pytest.param(("tempo_bpm",), {"kind": "tempo_bpm"}, "/tempo_bpm", id="no tag outside a union"),
        pytest.param(("patterns", 0, "notes"), {"patterns": [{"kind": "notes"}]}, "/patterns/0/notes", id="no tag in a list of plain objects"),
        pytest.param(("instruments", 5, "sampler", "id"), {"instruments": []}, "/instruments/5/sampler/id", id="past the end of the document"),
        pytest.param((), [1], "", id="the whole document"),
    ],
)
def test_json_pointer_names_the_value_in_the_document(loc, document, expected: str) -> None:
    assert json_pointer(loc, document) == expected


@pytest.mark.parametrize(
    ("loc", "document", "expected"),
    [
        pytest.param(("project", "SECRETVALUE-xyz"), {"project": {}}, "/project/<unknown field>", id="field of an object"),
        pytest.param(("instruments", 0, "plugin", "SECRETVALUE-xyz"), PLUGIN_INSTRUMENT, "/instruments/0/<unknown field>", id="field of a union member"),
        pytest.param(("instruments", 0, "plugin", "plugin"), {"instruments": [{"kind": "plugin"}]}, "/instruments/0/<unknown field>", id="field named like the tag"),
        pytest.param(("SECRETVALUE-xyz",), {}, "/<unknown field>", id="field of the document"),
    ],
)
def test_json_pointer_hides_the_name_of_an_unknown_field(loc, document, expected: str) -> None:
    assert json_pointer(loc, document, unknown_field=True) == expected


def test_issue_paths_never_echo_the_keys_of_the_document() -> None:
    # Unknown fields and extensions namespaces are keys the submitter chose.
    secret = "SECRETVALUE-xyz"
    document = _example()
    document["project"][secret] = secret
    document["instruments"][0][secret] = secret
    document["extensions"] = {secret: {}}
    document["patterns"][0]["notes"][0]["extensions"] = {"secretvalue_xyz": secret}

    result = validate_document(document)

    assert _codes(result) == [
        ("/project/<unknown field>", "extra_forbidden"),
        ("/instruments/0/<unknown field>", "extra_forbidden"),
        ("/patterns/0/notes/0/extensions/<namespace>", "dict_type"),
        ("/extensions/<namespace>", "string_pattern_mismatch"),
    ]
    reported = json.dumps([issue.as_dict() for issue in result.issues]).lower()
    assert "secretvalue" not in reported
