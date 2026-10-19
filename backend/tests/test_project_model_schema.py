"""Project model 0.1.0, first validation pass: each object on its own (docs/project-model.md).

Types, ranges, ID shapes, privacy by construction (no field for comments,
URLs or absolute paths) and the caps. How objects refer to one another is
tested in test_project_model_references.py.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
from typing import Any, Callable

import pytest
from pydantic import ValidationError

from stemhub.project_model import SCHEMA_VERSION, ProjectModel, validate_document, validate_json
from stemhub.project_model.schema import (
    MAX_CLIPS,
    MAX_EXTENSIONS_BYTES,
    MAX_EXTENSIONS_DEPTH,
    MAX_NOTES,
    MAX_UNKNOWN_FIELDS_REPORTED,
)

EXAMPLES_DIR = Path(__file__).parent / "fixtures" / "project_model" / "examples"
EXAMPLE_NAMES = sorted(path.name for path in EXAMPLES_DIR.glob("*.json"))


def _example(name: str = "basic-beat.json") -> dict[str, Any]:
    return json.loads((EXAMPLES_DIR / name).read_text(encoding="utf-8"))


def _errors(document: dict[str, Any]) -> list[dict[str, Any]]:
    with pytest.raises(ValidationError) as caught:
        ProjectModel.model_validate(document)
    return caught.value.errors()


def _mutated(mutate: Callable[[dict[str, Any]], None], name: str = "basic-beat.json") -> dict[str, Any]:
    document = copy.deepcopy(_example(name))
    mutate(document)
    return document


def _note(document: dict[str, Any]) -> dict[str, Any]:
    return document["patterns"][0]["notes"][0]


def _clip(document: dict[str, Any]) -> dict[str, Any]:
    return document["arrangements"][0]["tracks"][0]["clips"][0]


def _instrument(document: dict[str, Any]) -> dict[str, Any]:
    return document["instruments"][0]


def _insert(document: dict[str, Any], index: int = 1) -> dict[str, Any]:
    return document["mixer"]["inserts"][index]


def _external_sample(file_name: str) -> Callable[[dict[str, Any]], None]:
    return lambda document: _instrument(document).update(sample={"location": "external", "file_name": file_name})


EXTERNAL_SAMPLE_FILE_NAME = ("instruments", 0, "sampler", "sample", "external", "file_name")
RIGHT_TO_LEFT_OVERRIDE = chr(0x202E)  # makes "x<RLO>gnp.wav" display as "xvaw.png"


# ── Valid documents ──


def test_there_are_examples_to_check() -> None:
    assert {"minimal.json", "basic-beat.json", "fl2025-no-tempo.json"} <= set(EXAMPLE_NAMES)


@pytest.mark.parametrize("name", EXAMPLE_NAMES)
def test_every_example_is_a_valid_project_model(name: str) -> None:
    model = ProjectModel.model_validate(_example(name))

    assert model.schema_version == SCHEMA_VERSION


@pytest.mark.parametrize("name", EXAMPLE_NAMES)
def test_dumping_a_model_gives_back_the_same_document(name: str) -> None:
    document = _example(name)

    assert ProjectModel.model_validate(document).model_dump(mode="json") == document


def test_extensions_may_be_left_out_and_default_to_empty() -> None:
    def drop_extensions(document: dict[str, Any]) -> None:
        del document["extensions"]
        del _note(document)["extensions"]
        del _clip(document)["extensions"]
        del _instrument(document)["extensions"]

    model = ProjectModel.model_validate(_mutated(drop_extensions))

    assert model.extensions == {}
    assert model.patterns[0].notes[0].extensions == {}


def test_validate_json_parses_and_validates_bytes() -> None:
    body = (EXAMPLES_DIR / "basic-beat.json").read_bytes()

    result = validate_json(body)

    assert result.valid
    assert result.model is not None
    assert result.issues == ()


def test_floats_accept_json_integers() -> None:
    model = ProjectModel.model_validate(_mutated(lambda d: d.update(tempo_bpm=128)))

    assert model.tempo_bpm == 128.0


def test_instrument_faders_go_to_100_percent_and_insert_faders_to_125() -> None:
    def full_faders(document: dict[str, Any]) -> None:
        _instrument(document)["volume"]["fader"] = 1.0
        _insert(document)["volume"]["fader"] = 1.25

    model = ProjectModel.model_validate(_mutated(full_faders))

    assert model.instruments[0].volume.fader == 1.0
    assert model.mixer.inserts[1].volume.fader == 1.25


@pytest.mark.parametrize("file_name", ["Kick 01 (final).wav", "snare.tight.wav", "Café kick.wav", ".hidden.wav"])
def test_external_sample_file_names_may_hold_spaces_dots_and_accents(file_name: str) -> None:
    model = ProjectModel.model_validate(_mutated(_external_sample(file_name)))

    assert model.instruments[0].sample.file_name == file_name


# ── Invalid documents: structure, types and ranges ──

INVALID_DOCUMENTS: list[Any] = [
    pytest.param(lambda d: d.update(schema="other.format"), ("schema",), id="another format"),
    pytest.param(lambda d: d.update(schema_version="0.2.0"), ("schema_version",), id="another schema version"),
    pytest.param(lambda d: d.pop("mixer"), ("mixer",), id="missing mixer"),
    pytest.param(lambda d: d.update(arrangements=[]), ("arrangements",), id="no arrangement"),
    pytest.param(lambda d: d["project"].update(comments="call me"), ("project", "comments"), id="project comments are never stored"),
    pytest.param(lambda d: d["project"].update(url="https://example.com"), ("project", "url"), id="project url is never stored"),
    pytest.param(lambda d: d.update(licensee="Someone"), ("licensee",), id="licensee is never stored"),
    pytest.param(lambda d: d["source"].update(ppq=0), ("source", "ppq"), id="ppq zero"),
    pytest.param(lambda d: d["source"].update(ppq=96.0), ("source", "ppq"), id="ppq as a float"),
    pytest.param(lambda d: d["source"].update(ppq=True), ("source", "ppq"), id="ppq as a boolean"),
    pytest.param(lambda d: d["source"].update(daw="ableton_live"), ("source", "daw"), id="unsupported daw"),
    pytest.param(lambda d: d.update(tempo_bpm=0), ("tempo_bpm",), id="tempo zero"),
    pytest.param(lambda d: d.update(tempo_bpm="fast"), ("tempo_bpm",), id="tempo as text"),
    pytest.param(lambda d: d["time_signature"].update(denominator=3), ("time_signature", "denominator"), id="denominator 3"),
    pytest.param(lambda d: d["time_signature"].update(denominator=True), ("time_signature", "denominator"), id="denominator true"),
    pytest.param(lambda d: d["time_signature"].update(denominator=4.0), ("time_signature", "denominator"), id="denominator 4.0"),
    pytest.param(lambda d: d["time_signature"].update(denominator=1.0), ("time_signature", "denominator"), id="denominator 1.0"),
    pytest.param(lambda d: d["time_signature"].update(numerator=0), ("time_signature", "numerator"), id="numerator 0"),
    pytest.param(lambda d: _instrument(d).update(id="instrument:01"), ("instruments", 0, "sampler", "id"), id="instrument id with a leading zero"),
    pytest.param(lambda d: _instrument(d).update(id="channel:0"), ("instruments", 0, "sampler", "id"), id="instrument id with another prefix"),
    pytest.param(lambda d: _instrument(d).update(kind="layer"), ("instruments", 0), id="unknown instrument kind"),
    pytest.param(lambda d: _instrument(d).update(plugin=d["instruments"][1]["plugin"]), ("instruments", 0, "sampler", "plugin"), id="sampler with a plugin"),
    pytest.param(lambda d: _instrument(d).pop("plugin"), ("instruments", 0, "sampler", "plugin"), id="sampler without its null plugin"),
    pytest.param(lambda d: d["instruments"][1].pop("plugin"), ("instruments", 1, "plugin", "plugin"), id="plugin instrument without its plugin"),
    pytest.param(lambda d: d["instruments"][1].update(plugin=None), ("instruments", 1, "plugin", "plugin"), id="plugin instrument with a null plugin"),
    pytest.param(lambda d: d["instruments"][1].update(sample=_instrument(d)["sample"]), ("instruments", 1, "plugin", "sample"), id="plugin instrument with a sample"),
    pytest.param(lambda d: _instrument(d)["volume"].update(fader=1.5), ("instruments", 0, "sampler", "volume", "fader"), id="fader above 1.25"),
    pytest.param(lambda d: _instrument(d)["volume"].update(fader=1.01), ("instruments", 0, "sampler", "volume", "fader"), id="instrument fader above 1"),
    pytest.param(lambda d: _instrument(d)["volume"].update(fader=-0.1), ("instruments", 0, "sampler", "volume", "fader"), id="negative fader"),
    pytest.param(lambda d: _instrument(d)["volume"].update(db=-6.0), ("instruments", 0, "sampler", "volume", "db"), id="instrument volume in dB before the calibration"),
    pytest.param(lambda d: _insert(d)["volume"].update(fader=1.26), ("mixer", "inserts", 1, "volume", "fader"), id="insert fader above 1.25"),
    pytest.param(lambda d: _insert(d)["volume"].update(db=0.0), ("mixer", "inserts", 1, "volume", "db"), id="insert volume in dB before the calibration"),
    pytest.param(lambda d: _instrument(d).update(pan=1.5), ("instruments", 0, "sampler", "pan"), id="pan above 1"),
    pytest.param(lambda d: _instrument(d).update(enabled=1), ("instruments", 0, "sampler", "enabled"), id="enabled as a number"),
    pytest.param(lambda d: _instrument(d).update(color="#5c656a"), ("instruments", 0, "sampler", "color"), id="lower-case colour"),
    pytest.param(lambda d: _instrument(d).update(name="x" * 256), ("instruments", 0, "sampler", "name"), id="name over 255 characters"),
    pytest.param(lambda d: _instrument(d).update(name="Ki\x00ck"), ("instruments", 0, "sampler", "name"), id="name with a NUL"),
    pytest.param(lambda d: _instrument(d).update(insert={"status": "unknown", "insert_id": "insert:1"}), ("instruments", 0, "sampler", "insert", "unknown", "insert_id"), id="unknown insert with an id"),
    pytest.param(lambda d: _instrument(d).update(insert={"status": "known"}), ("instruments", 0, "sampler", "insert", "known", "insert_id"), id="known insert without an id"),
    pytest.param(lambda d: _instrument(d).update(insert=None), ("instruments", 0, "sampler", "insert"), id="insert as null"),
    pytest.param(lambda d: _instrument(d).update(insert={"status": "known", "insert_id": "insert:127"}), ("instruments", 0, "sampler", "insert", "known", "insert_id"), id="insert 127"),
    pytest.param(lambda d: _instrument(d)["sample"].update(location="absolute"), ("instruments", 0, "sampler", "sample"), id="unknown sample location"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="/Users/someone/kick.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="absolute asset path"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="../kick.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path leaving the project folder"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="Samples/./kick.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path with a dot segment"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="Samples//kick.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path with an empty segment"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="C:/kick.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path with a drive letter"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="Samples\\kick.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path with a backslash"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="Samples/kick.wav "), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path ending with a space"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="Samples/ki\x01ck.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path with a control character"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="Samples/" + "k" * 250), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path over 255 characters"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="Samples/CON.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path with a reserved Windows name"),
    pytest.param(lambda d: _instrument(d)["sample"].update(asset_path="lpt1/kick.wav"), ("instruments", 0, "sampler", "sample", "project", "asset_path"), id="asset path with a reserved Windows folder"),
    pytest.param(lambda d: _instrument(d).update(sample={"location": "factory", "path": "C:/Program Files/Image-Line/kick.wav"}), ("instruments", 0, "sampler", "sample", "factory", "path"), id="factory sample without its FL variable"),
    pytest.param(lambda d: _instrument(d).update(sample={"location": "factory", "path": "%FLStudioFactoryData%/../kick.wav"}), ("instruments", 0, "sampler", "sample", "factory", "path"), id="factory sample leaving its folder"),
    pytest.param(lambda d: _instrument(d).update(sample={"location": "external", "file_name": "Users/someone/kick.wav"}), ("instruments", 0, "sampler", "sample", "external", "file_name"), id="external sample with a folder"),
    pytest.param(lambda d: _instrument(d).update(sample={"location": "external", "file_name": "C:\\kick.wav"}), ("instruments", 0, "sampler", "sample", "external", "file_name"), id="external sample with a Windows folder"),
    pytest.param(_external_sample("."), EXTERNAL_SAMPLE_FILE_NAME, id="external sample named ."),
    pytest.param(_external_sample(".."), EXTERNAL_SAMPLE_FILE_NAME, id="external sample named .."),
    pytest.param(_external_sample(" "), EXTERNAL_SAMPLE_FILE_NAME, id="external sample named with a space only"),
    pytest.param(_external_sample(""), EXTERNAL_SAMPLE_FILE_NAME, id="external sample without a name"),
    pytest.param(_external_sample("C:x"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a drive letter"),
    pytest.param(_external_sample("kick.wav:stream"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a colon"),
    pytest.param(_external_sample("CON"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a reserved Windows name"),
    pytest.param(_external_sample("nul.wav"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a reserved Windows name and an extension"),
    pytest.param(_external_sample("a."), EXTERNAL_SAMPLE_FILE_NAME, id="external sample ending with a dot"),
    pytest.param(_external_sample("kick.wav "), EXTERNAL_SAMPLE_FILE_NAME, id="external sample ending with a space"),
    pytest.param(_external_sample("ki\x01ck.wav"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a control character"),
    pytest.param(_external_sample("x" + RIGHT_TO_LEFT_OVERRIDE + "gnp.wav"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a right-to-left override"),
    pytest.param(_external_sample("kick" + chr(0x2066) + ".wav"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a bidirectional isolate"),
    pytest.param(_external_sample("kick" + chr(0x202A) + ".wav"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a bidirectional embedding"),
    pytest.param(_external_sample("kick" + chr(0x2069) + ".wav"), EXTERNAL_SAMPLE_FILE_NAME, id="external sample with a pop directional isolate"),
    pytest.param(lambda d: d["instruments"][1]["plugin"].update(format="clap"), ("instruments", 1, "plugin", "plugin", "format"), id="unknown plugin format"),
    pytest.param(lambda d: d["instruments"][1]["plugin"].update(name=""), ("instruments", 1, "plugin", "plugin", "name"), id="plugin without a name"),
    pytest.param(lambda d: d["instruments"][1]["plugin"]["state"].update(sha256="ABC"), ("instruments", 1, "plugin", "plugin", "state", "sha256"), id="plugin state hash not hex"),
    pytest.param(lambda d: _note(d).update(pitch=132), ("patterns", 0, "notes", 0, "pitch"), id="pitch above 131"),
    pytest.param(lambda d: _note(d).update(pitch=-1), ("patterns", 0, "notes", 0, "pitch"), id="negative pitch"),
    pytest.param(lambda d: _note(d).update(key=60), ("patterns", 0, "notes", 0, "key"), id="note key instead of pitch"),
    pytest.param(lambda d: _note(d).update(velocity=1.01), ("patterns", 0, "notes", 0, "velocity"), id="velocity above 1"),
    pytest.param(lambda d: _note(d).update(fine_pitch_cents=15), ("patterns", 0, "notes", 0, "fine_pitch_cents"), id="fine pitch not a multiple of 10"),
    pytest.param(lambda d: _note(d).update(fine_pitch_cents=1210), ("patterns", 0, "notes", 0, "fine_pitch_cents"), id="fine pitch above 1200"),
    pytest.param(lambda d: _note(d).update(start_ticks=-1), ("patterns", 0, "notes", 0, "start_ticks"), id="negative ticks"),
    pytest.param(lambda d: _note(d).update(start_ticks=1.0), ("patterns", 0, "notes", 0, "start_ticks"), id="ticks as a float"),
    pytest.param(lambda d: _note(d).update(start_ticks=True), ("patterns", 0, "notes", 0, "start_ticks"), id="ticks as a boolean"),
    pytest.param(lambda d: _note(d).update(start_ticks=2**31), ("patterns", 0, "notes", 0, "start_ticks"), id="ticks beyond int32"),
    pytest.param(lambda d: _clip(d).update(kind="video"), ("arrangements", 0, "tracks", 0, "clips", 0), id="unknown clip kind"),
    pytest.param(lambda d: _clip(d).update(source_id="audio:2"), ("arrangements", 0, "tracks", 0, "clips", 0, "pattern", "source_id"), id="pattern clip playing audio"),
    pytest.param(lambda d: _clip(d).update(kind="automation"), ("arrangements", 0, "tracks", 0, "clips", 0, "automation", "source_id"), id="automation clip playing a pattern"),
    pytest.param(lambda d: _clip(d).update(muted=None), ("arrangements", 0, "tracks", 0, "clips", 0, "pattern", "muted"), id="muted as null"),
    pytest.param(lambda d: d["arrangements"][0]["tracks"][0].update(id="track:0", number=0), ("arrangements", 0, "tracks", 0, "id"), id="track 0"),
    pytest.param(lambda d: d["arrangements"][0]["tracks"][0].update(number=2), ("arrangements", 0, "tracks", 0), id="track id not matching its number"),
    pytest.param(lambda d: _insert(d).update(number=5), ("mixer", "inserts", 1), id="insert id not matching its number"),
    pytest.param(lambda d: _insert(d).update(stereo_separation=-1.5), ("mixer", "inserts", 1, "stereo_separation"), id="stereo separation below -1"),
    pytest.param(lambda d: _insert(d)["sends"][1].update(level=1.5), ("mixer", "inserts", 1, "sends", 1, "level"), id="send level above 1"),
    pytest.param(lambda d: _insert(d, 2)["effect_slots"][0].update(index=10), ("mixer", "inserts", 2, "effect_slots", 0, "index"), id="effect slot 11"),
    pytest.param(lambda d: _insert(d, 2)["effect_slots"][0].update(dry_wet=2.0), ("mixer", "inserts", 2, "effect_slots", 0, "dry_wet"), id="dry/wet above 1"),
    pytest.param(lambda d: _insert(d, 2)["effect_slots"][0].update(mix=1.0), ("mixer", "inserts", 2, "effect_slots", 0, "mix"), id="mix instead of dry/wet"),
    pytest.param(lambda d: d["automation"][0].update(target={"kind": "tempo", "status": "unknown"}), ("automation", 0, "target", "tempo", "status"), id="automation target both known and unknown"),
    pytest.param(lambda d: d["automation"][0].update(target={"kind": "insert", "insert_id": "insert:0", "parameter": "eq"}), ("automation", 0, "target", "insert", "parameter"), id="automation target parameter unknown"),
    pytest.param(lambda d: d["automation"][0]["points"][0].update(value=1.5), ("automation", 0, "points", 0, "value"), id="automation value above 1"),
    pytest.param(lambda d: d["unsupported"][0].update(path="instruments"), ("unsupported", 0, "path"), id="unsupported path not a JSON pointer"),
    pytest.param(lambda d: d["unsupported"][0].update(reason=""), ("unsupported", 0, "reason"), id="unsupported without a reason"),
    pytest.param(lambda d: d.update(extensions={"FL Studio": {}}), ("extensions", "FL Studio", "[key]"), id="extension namespace not snake case"),
    pytest.param(lambda d: d.update(extensions={"fl_studio": []}), ("extensions", "fl_studio"), id="extension namespace not an object"),
]


@pytest.mark.parametrize(("mutate", "loc"), INVALID_DOCUMENTS)
def test_invalid_documents_are_rejected_where_the_problem_is(mutate, loc) -> None:
    errors = _errors(_mutated(mutate))

    assert loc in [error["loc"] for error in errors], errors


def test_every_problem_is_reported_not_only_the_first() -> None:
    def break_twice(document: dict[str, Any]) -> None:
        _note(document)["pitch"] = 200
        _clip(document)["start_ticks"] = -5

    locs = {error["loc"] for error in _errors(_mutated(break_twice))}

    assert ("patterns", 0, "notes", 0, "pitch") in locs
    assert ("arrangements", 0, "tracks", 0, "clips", 0, "pattern", "start_ticks") in locs


def test_nan_and_infinity_are_rejected() -> None:
    body = (EXAMPLES_DIR / "basic-beat.json").read_text(encoding="utf-8").replace('"tempo_bpm": 90.0', '"tempo_bpm": NaN')

    result = validate_json(body.encode())

    assert not result.valid
    assert [issue.path for issue in result.issues] == ["/tempo_bpm"]


# ── Extensions: free-form but capped ──


def _nested(depth: int) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for _ in range(depth):
        value = {"x": value}
    return value


def test_extensions_hold_any_json_up_to_the_depth_cap() -> None:
    # The extensions object counts as level 1 and the namespace object as level 2.
    deepest_allowed = {"fl_studio": _nested(MAX_EXTENSIONS_DEPTH - 2)}

    model = ProjectModel.model_validate(_mutated(lambda d: d.update(extensions=deepest_allowed)))

    assert model.extensions == deepest_allowed


def test_extensions_deeper_than_the_cap_are_rejected() -> None:
    too_deep = {"fl_studio": _nested(MAX_EXTENSIONS_DEPTH - 1)}

    errors = _errors(_mutated(lambda d: d.update(extensions=too_deep)))

    assert [(error["loc"], error["type"]) for error in errors] == [(("extensions",), "extensions_too_deep")]


def test_lists_count_towards_the_extensions_depth() -> None:
    too_deep: Any = []
    for _ in range(MAX_EXTENSIONS_DEPTH - 2):
        too_deep = [too_deep]

    errors = _errors(_mutated(lambda d: _note(d).update(extensions={"fl_studio": {"x": too_deep}})))

    assert [error["type"] for error in errors] == ["extensions_too_deep"]


def test_extensions_larger_than_the_cap_are_rejected() -> None:
    too_big = {"fl_studio": {"blob": "x" * MAX_EXTENSIONS_BYTES}}

    errors = _errors(_mutated(lambda d: _clip(d).update(extensions=too_big)))

    assert [error["type"] for error in errors] == ["extensions_too_large"]


def test_extensions_must_hold_json_values() -> None:
    errors = _errors(_mutated(lambda d: d.update(extensions={"fl_studio": {"when": float("nan")}})))

    assert [error["type"] for error in errors] == ["extensions_invalid"]

    errors = _errors(_mutated(lambda d: d.update(extensions={"fl_studio": {"raw": b"\x00"}})))

    assert [error["type"] for error in errors] == ["extensions_invalid"]


# ── Bounded error reports: a large body cannot make validation build millions of errors ──


EVERY_LIST = [
    pytest.param(lambda d: d, "instruments", ("instruments",), id="instruments"),
    pytest.param(lambda d: d, "audio_sources", ("audio_sources",), id="audio sources"),
    pytest.param(lambda d: d, "patterns", ("patterns",), id="patterns"),
    pytest.param(lambda d: d["patterns"][0], "notes", ("patterns", 0, "notes"), id="notes"),
    pytest.param(lambda d: d, "arrangements", ("arrangements",), id="arrangements"),
    pytest.param(lambda d: d["arrangements"][0], "markers", ("arrangements", 0, "markers"), id="markers"),
    pytest.param(lambda d: d["arrangements"][0], "tracks", ("arrangements", 0, "tracks"), id="tracks"),
    pytest.param(lambda d: d["arrangements"][0]["tracks"][0], "clips", ("arrangements", 0, "tracks", 0, "clips"), id="clips"),
    pytest.param(lambda d: d["mixer"], "inserts", ("mixer", "inserts"), id="inserts"),
    pytest.param(lambda d: _insert(d), "sends", ("mixer", "inserts", 1, "sends"), id="sends"),
    pytest.param(lambda d: _insert(d), "effect_slots", ("mixer", "inserts", 1, "effect_slots"), id="effect slots"),
    pytest.param(lambda d: d, "automation", ("automation",), id="automation"),
    pytest.param(lambda d: d["automation"][0], "points", ("automation", 0, "points"), id="automation points"),
    pytest.param(lambda d: d, "unsupported", ("unsupported",), id="unsupported"),
    pytest.param(lambda d: d, "warnings", ("warnings",), id="warnings"),
]


@pytest.mark.parametrize(("owner", "key", "loc"), EVERY_LIST)
def test_a_list_reports_its_first_invalid_item_only(owner, key: str, loc: tuple[Any, ...]) -> None:
    errors = _errors(_mutated(lambda d: owner(d).update({key: [{}] * 5})))

    assert {error["loc"][: len(loc) + 1] for error in errors} == {(*loc, 0)}


@pytest.mark.parametrize("count", [MAX_UNKNOWN_FIELDS_REPORTED + 1, 1000])
def test_an_object_reports_its_first_unknown_fields_only(count: int) -> None:
    unknown = {f"field_{index}": index for index in range(count)}

    errors = _errors(_mutated(lambda d: d["project"].update(unknown)))

    assert [error["loc"] for error in errors] == [
        ("project", f"field_{index}") for index in range(MAX_UNKNOWN_FIELDS_REPORTED)
    ]
    assert {error["type"] for error in errors} == {"extra_forbidden"}


def test_an_object_of_unknown_fields_only_reports_its_missing_fields_and_first_unknown_ones() -> None:
    unknown = {f"field_{index}": index for index in range(MAX_UNKNOWN_FIELDS_REPORTED + 3)}

    errors = _errors(_mutated(lambda d: d.update(project=unknown)))

    assert sorted(error["type"] for error in errors) == ["extra_forbidden"] * MAX_UNKNOWN_FIELDS_REPORTED + ["missing"] * 3


def test_a_few_unknown_fields_are_all_reported() -> None:
    errors = _errors(_mutated(lambda d: d["project"].update(comments="x", url="y")))

    assert [error["loc"] for error in errors] == [("project", "comments"), ("project", "url")]


def test_extensions_with_too_many_namespaces_are_one_error() -> None:
    namespaces = {f"Bad Key {index}": [] for index in range(1000)}

    errors = _errors(_mutated(lambda d: d.update(extensions=namespaces)))

    assert [(error["loc"], error["type"]) for error in errors] == [(("extensions",), "too_long")]


# ── Caps ──


def test_a_pattern_list_holds_at_most_999_patterns() -> None:
    patterns = [{"id": f"pattern:{index}", "name": "", "length_ticks": 0, "notes": []} for index in range(1000)]

    errors = _errors(_mutated(lambda d: d.update(patterns=patterns)))

    assert [(error["loc"], error["type"]) for error in errors] == [(("patterns",), "too_long")]


def test_the_mixer_holds_at_most_127_inserts() -> None:
    master = _example()["mixer"]["inserts"][0]
    inserts = [dict(master)] * 128

    errors = _errors(_mutated(lambda d: d["mixer"].update(inserts=inserts)))

    assert [(error["loc"], error["type"]) for error in errors] == [(("mixer", "inserts"), "too_long")]


def test_an_arrangement_holds_at_most_500_tracks() -> None:
    tracks = [
        {"id": f"track:{number}", "number": number, "name": "", "enabled": True, "clips": []} for number in range(1, 501)
    ]
    ProjectModel.model_validate(_mutated(lambda d: d["arrangements"][0].update(tracks=tracks)))

    errors = _errors(_mutated(lambda d: d["arrangements"][0].update(tracks=[*tracks, tracks[0]])))

    assert [(error["loc"], error["type"]) for error in errors] == [(("arrangements", 0, "tracks"), "too_long")]


def test_the_model_holds_at_most_100000_notes_across_patterns() -> None:
    note = _example()["patterns"][0]["notes"][0]
    half = MAX_NOTES // 2

    def fill(document: dict[str, Any], extra: int) -> None:
        document["patterns"][0]["notes"] = [note] * half
        document["patterns"][1]["notes"] = [note] * (half + extra)

    ProjectModel.model_validate(_mutated(lambda d: fill(d, 0)))
    errors = _errors(_mutated(lambda d: fill(d, 1)))

    assert [(error["loc"], error["type"]) for error in errors] == [((), "too_many_notes")]


def test_the_model_holds_at_most_20000_clips_across_tracks() -> None:
    clip = _example()["arrangements"][0]["tracks"][0]["clips"][0]
    half = MAX_CLIPS // 2

    def fill(document: dict[str, Any], extra: int) -> None:
        document["arrangements"][0]["tracks"][0]["clips"] = [clip] * half
        document["arrangements"][0]["tracks"][1]["clips"] = [clip] * (half + extra)

    ProjectModel.model_validate(_mutated(lambda d: fill(d, 0)))
    errors = _errors(_mutated(lambda d: fill(d, 1)))

    assert [(error["loc"], error["type"]) for error in errors] == [((), "too_many_clips")]


# ── Error reporting: JSON pointers, no input echoed ──


@pytest.mark.parametrize(
    ("mutate", "path"),
    [
        (lambda d: _instrument(d).update(name=5), "/instruments/0/name"),
        (lambda d: d["instruments"][1]["plugin"].update(name=5), "/instruments/1/plugin/name"),
        (lambda d: d["instruments"][1].update(name=5), "/instruments/1/name"),
        (lambda d: _instrument(d)["sample"].update(asset_path="../x.wav"), "/instruments/0/sample/asset_path"),
        (lambda d: _instrument(d)["insert"].update(insert_id="insert:x"), "/instruments/0/insert/insert_id"),
        (lambda d: d["audio_sources"][0]["sample"].update(asset_path="/x.wav"), "/audio_sources/0/sample/asset_path"),
        (lambda d: d["audio_sources"][0]["insert"].update(insert_id=None), "/audio_sources/0/insert/insert_id"),
        (lambda d: _clip(d).update(start_ticks=-1), "/arrangements/0/tracks/0/clips/0/start_ticks"),
        (lambda d: d["automation"][0]["target"].update(parameter="eq"), "/automation/0/target/parameter"),
        (lambda d: _instrument(d).update(kind="layer"), "/instruments/0"),
        (lambda d: d.update(extensions={"Bad/Key~": {}}), "/extensions/<namespace>"),
        (lambda d: d["project"].update(comments="x"), "/project/<unknown field>"),
        (lambda d: d["mixer"].update(inserts="none"), "/mixer/inserts"),
        (lambda d: d.update(tempo_bpm=None, unsupported=[], patterns="x"), "/patterns"),
    ],
)
def test_issues_point_at_the_json_value_with_a_json_pointer(mutate, path) -> None:
    result = validate_document(_mutated(mutate))

    assert not result.valid
    assert path in [issue.path for issue in result.issues]


def test_issues_never_echo_the_submitted_values() -> None:
    secret = "/Users/alice/Secret project/kick.wav"

    def leak(document: dict[str, Any]) -> None:
        _instrument(document)["sample"]["asset_path"] = secret
        document["audio_sources"][0]["insert"]["status"] = secret
        _clip(document)["kind"] = secret
        document["automation"][0]["target"]["kind"] = secret
        document["time_signature"]["denominator"] = secret

    result = validate_document(_mutated(leak))

    assert not result.valid
    assert all(secret not in issue.message for issue in result.issues)
    assert {issue.code for issue in result.issues} >= {
        "string_pattern_mismatch",
        "invalid_status",
        "invalid_kind",
        "literal_error",
    }


def test_a_document_that_is_not_an_object_is_rejected() -> None:
    result = validate_document([1, 2, 3])

    assert not result.valid
    assert [issue.path for issue in result.issues] == [""]


def test_invalid_json_is_reported_as_one_issue() -> None:
    result = validate_json(b'{"schema": ')

    assert not result.valid
    assert [(issue.path, issue.code) for issue in result.issues] == [("", "json_invalid")]
