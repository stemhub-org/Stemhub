"""The committed JSON Schema of the project model (docs/project-model.md, "Versioning").

The schema file is generated from the Pydantic models and committed, so other
tools (the StemHub plugin, the web app, third parties) can validate documents
without Python. These tests keep it in step with the models and check that it
accepts and rejects the same documents, where JSON Schema can say so.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
from typing import Any

import pytest
from jsonschema import Draft202012Validator
from pydantic import ValidationError

from stemhub.project_model import ProjectModel
from stemhub.project_model.__main__ import main as cli_main
from stemhub.project_model.schema import (
    JSON_SCHEMA_PATH,
    SCHEMA_ID,
    SCHEMA_VERSION,
    generate_json_schema,
    render_json_schema,
)

EXAMPLES_DIR = Path(__file__).parent / "fixtures" / "project_model" / "examples"
EXAMPLE_NAMES = sorted(path.name for path in EXAMPLES_DIR.glob("*.json"))
REGENERATE = "Regenerate it with: PYTHONPATH=backend/src python -m stemhub.project_model schema"


def _committed_schema() -> dict[str, Any]:
    return json.loads(JSON_SCHEMA_PATH.read_text(encoding="utf-8"))


def _validator() -> Draft202012Validator:
    return Draft202012Validator(_committed_schema())


def _example(name: str = "basic-beat.json") -> dict[str, Any]:
    return json.loads((EXAMPLES_DIR / name).read_text(encoding="utf-8"))


def test_the_committed_schema_is_the_generated_one() -> None:
    assert JSON_SCHEMA_PATH.exists(), f"{JSON_SCHEMA_PATH} is missing. {REGENERATE}"
    assert _committed_schema() == generate_json_schema(), f"{JSON_SCHEMA_PATH.name} is out of date. {REGENERATE}"


def test_the_committed_schema_file_is_named_after_its_version() -> None:
    assert JSON_SCHEMA_PATH.name == f"project-model-{SCHEMA_VERSION}.schema.json"


def test_the_schema_declares_its_dialect_and_identity() -> None:
    schema = _committed_schema()

    assert schema["$schema"] == "https://json-schema.org/draft/2020-12/schema"
    assert schema["$id"] == SCHEMA_ID == f"urn:stemhub:project-model:{SCHEMA_VERSION}"
    Draft202012Validator.check_schema(schema)


def test_the_schema_pins_the_format_and_its_version() -> None:
    properties = _committed_schema()["properties"]

    assert properties["schema"]["const"] == "stemhub.project-model"
    assert properties["schema_version"]["const"] == SCHEMA_VERSION


def test_the_schema_forbids_unknown_fields_on_every_core_object() -> None:
    schema = _committed_schema()
    objects = [schema, *schema["$defs"].values()]

    for definition in objects:
        if definition.get("type") == "object" and "properties" in definition:
            assert definition.get("additionalProperties") is False, definition.get("title")


@pytest.mark.parametrize("name", EXAMPLE_NAMES)
def test_every_example_validates_against_the_committed_schema(name: str) -> None:
    errors = sorted(_validator().iter_errors(_example(name)), key=lambda error: list(error.path))

    assert errors == [], [f"{list(error.path)}: {error.message}" for error in errors]


def test_the_schema_accepts_the_edges_of_the_ranges() -> None:
    document = _example()
    document["instruments"][0]["volume"]["fader"] = 1.0
    document["instruments"][0]["sample"] = {"location": "external", "file_name": "Café kick (final).wav"}
    document["mixer"]["inserts"][1]["volume"]["fader"] = 1.25

    assert _validator().is_valid(document)
    ProjectModel.model_validate(document)


# Invalid documents. The last value says whether JSON Schema can express the
# rule; the others (strict integers, reserved Windows names, cross-field and
# whole-document rules, extensions caps) are Pydantic-only and documented.
def _set(path: list[Any], value: Any):
    def mutate(document: dict[str, Any]) -> None:
        node = document
        for key in path[:-1]:
            node = node[key]
        node[path[-1]] = value

    return mutate


def _delete(path: list[Any]):
    def mutate(document: dict[str, Any]) -> None:
        node = document
        for key in path[:-1]:
            node = node[key]
        del node[path[-1]]

    return mutate


NOTE = ["patterns", 0, "notes", 0]
CLIP = ["arrangements", 0, "tracks", 0, "clips", 0]
SAMPLER = ["instruments", 0]
SEND = ["mixer", "inserts", 1, "sends", 1]

INVALID_DOCUMENTS = [
    pytest.param(_set(["schema_version"], "0.2.0"), True, id="another schema version"),
    pytest.param(_set(["project", "comments"], "call me"), True, id="project comments"),
    pytest.param(_delete(["mixer"]), True, id="missing mixer"),
    pytest.param(_set(["arrangements"], []), True, id="no arrangement"),
    pytest.param(_set(["source", "ppq"], 0), True, id="ppq zero"),
    pytest.param(_set(["source", "ppq"], True), True, id="ppq as a boolean"),
    pytest.param(_set(["tempo_bpm"], 0), True, id="tempo zero"),
    pytest.param(_set(["time_signature", "denominator"], 3), True, id="denominator 3"),
    pytest.param(_set(["time_signature", "denominator"], True), True, id="denominator true"),
    pytest.param(_set(["time_signature", "denominator"], 4.0), False, id="denominator 4.0"),
    pytest.param(_set([*SAMPLER, "id"], "instrument:01"), True, id="instrument id with a leading zero"),
    pytest.param(_set([*SAMPLER, "kind"], "layer"), True, id="unknown instrument kind"),
    pytest.param(_set([*SAMPLER, "volume", "fader"], 1.5), True, id="fader above 1.25"),
    pytest.param(_set([*SAMPLER, "volume", "fader"], 1.01), True, id="instrument fader above 1"),
    pytest.param(_set([*SAMPLER, "volume", "db"], -6.0), True, id="instrument volume in dB"),
    pytest.param(_set(["mixer", "inserts", 1, "volume", "fader"], 1.26), True, id="insert fader above 1.25"),
    pytest.param(_set(["mixer", "inserts", 1, "volume", "db"], 0.0), True, id="insert volume in dB"),
    pytest.param(_set([*SAMPLER, "pan"], -1.5), True, id="pan below -1"),
    pytest.param(_set([*SAMPLER, "enabled"], 1), True, id="enabled as a number"),
    pytest.param(_set([*SAMPLER, "color"], "#5c656a"), True, id="lower-case colour"),
    pytest.param(_set([*SAMPLER, "name"], "x" * 256), True, id="name over 255 characters"),
    pytest.param(_set([*SAMPLER, "name"], "Ki\x00ck"), True, id="name with a NUL"),
    pytest.param(_set([*SAMPLER, "insert"], {"status": "unknown", "insert_id": "insert:1"}), True, id="unknown insert with an id"),
    pytest.param(_set([*SAMPLER, "insert", "insert_id"], "insert:127"), True, id="insert 127"),
    pytest.param(_set([*SAMPLER, "sample", "asset_path"], "/Users/someone/kick.wav"), True, id="absolute asset path"),
    pytest.param(_set([*SAMPLER, "sample", "asset_path"], "../kick.wav"), True, id="asset path leaving the project folder"),
    pytest.param(_set([*SAMPLER, "sample", "asset_path"], "C:/kick.wav"), True, id="asset path with a drive letter"),
    pytest.param(_set([*SAMPLER, "sample", "asset_path"], "Samples\\kick.wav"), True, id="asset path with a backslash"),
    pytest.param(_set([*SAMPLER, "sample", "asset_path"], "Samples/ki\x01ck.wav"), True, id="asset path with a control character"),
    pytest.param(_set([*SAMPLER, "sample", "asset_path"], "Samples/CON.wav"), False, id="asset path with a reserved Windows name"),
    pytest.param(_set([*SAMPLER, "sample"], {"location": "external", "file_name": "a/b.wav"}), True, id="external sample with a folder"),
    pytest.param(_set([*SAMPLER, "sample"], {"location": "external", "file_name": ".."}), True, id="external sample named .."),
    pytest.param(_set([*SAMPLER, "sample"], {"location": "external", "file_name": "C:x"}), True, id="external sample with a drive letter"),
    pytest.param(_set([*SAMPLER, "sample"], {"location": "external", "file_name": "a."}), True, id="external sample ending with a dot"),
    pytest.param(_set([*SAMPLER, "sample"], {"location": "external", "file_name": "x" + chr(0x202E) + "gnp.wav"}), True, id="external sample with a right-to-left override"),
    pytest.param(_set([*SAMPLER, "sample"], {"location": "external", "file_name": "CON"}), False, id="external sample with a reserved Windows name"),
    pytest.param(_set(["instruments", 1, "plugin", "format"], "clap"), True, id="unknown plugin format"),
    pytest.param(_set([*SAMPLER, "plugin"], {"format": "vst3", "name": "Synth", "vendor": None, "plugin_id": None, "state": None}), True, id="sampler with a plugin"),
    pytest.param(_set(["instruments", 1, "sample"], {"location": "external", "file_name": "a.wav"}), True, id="plugin instrument with a sample"),
    pytest.param(_set([*NOTE, "pitch"], 132), True, id="pitch above 131"),
    pytest.param(_set([*NOTE, "velocity"], 1.01), True, id="velocity above 1"),
    pytest.param(_set([*NOTE, "fine_pitch_cents"], 15), True, id="fine pitch not a multiple of 10"),
    pytest.param(_set([*NOTE, "fine_pitch_cents"], -1210), True, id="fine pitch below -1200"),
    pytest.param(_set([*NOTE, "start_ticks"], -1), True, id="negative ticks"),
    pytest.param(_set([*NOTE, "start_ticks"], 1.0), False, id="ticks as a float"),
    pytest.param(_set([*NOTE, "start_ticks"], 2**31), True, id="ticks beyond int32"),
    pytest.param(_set([*CLIP, "kind"], "video"), True, id="unknown clip kind"),
    pytest.param(_set([*CLIP, "source_id"], "audio:2"), True, id="pattern clip playing audio"),
    pytest.param(_set(["arrangements", 0, "tracks", 0, "number"], 2), False, id="track id not matching its number"),
    pytest.param(_set(["mixer", "inserts", 1, "number"], 5), False, id="insert id not matching its number"),
    pytest.param(_set([*SEND, "level"], 1.5), True, id="send level above 1"),
    pytest.param(_set(["mixer", "inserts", 2, "effect_slots", 0, "index"], 10), True, id="effect slot 11"),
    pytest.param(_set(["automation", 0, "target"], {"kind": "tempo", "status": "unknown"}), True, id="target both known and unknown"),
    pytest.param(_set(["unsupported", 0, "path"], "instruments"), True, id="unsupported path not a JSON pointer"),
    pytest.param(_set(["extensions"], {"FL Studio": {}}), True, id="extension namespace not snake case"),
    pytest.param(_set(["extensions"], {"fl_studio": []}), True, id="extension namespace not an object"),
    pytest.param(_set(["extensions"], {"fl_studio": {"blob": "x" * 70_000}}), False, id="extensions over 64 KiB"),
]


@pytest.mark.parametrize(("mutate", "json_schema_can_express_it"), INVALID_DOCUMENTS)
def test_invalid_documents_are_rejected_by_pydantic_and_by_the_schema_where_it_can(
    mutate, json_schema_can_express_it: bool
) -> None:
    document = copy.deepcopy(_example())
    mutate(document)

    with pytest.raises(ValidationError):
        ProjectModel.model_validate(document)
    if json_schema_can_express_it:
        assert not _validator().is_valid(document)


# ── The CLI that regenerates the file ──


def test_the_cli_writes_the_schema_where_asked(tmp_path: Path) -> None:
    output = tmp_path / "schema.json"

    assert cli_main(["schema", "--output", str(output)]) == 0
    assert output.read_text(encoding="utf-8") == render_json_schema()


def test_the_cli_check_passes_on_the_committed_file_and_fails_on_a_stale_one(tmp_path: Path, capsys) -> None:
    assert cli_main(["schema", "--check"]) == 0

    stale = tmp_path / "stale.json"
    stale.write_text("{}\n", encoding="utf-8")
    assert cli_main(["schema", "--check", "--output", str(stale)]) == 1
    assert "out of date" in capsys.readouterr().err

    assert cli_main(["schema", "--check", "--output", str(tmp_path / "missing.json")]) == 1


def test_the_cli_validates_a_document_file(tmp_path: Path, capsys) -> None:
    assert cli_main(["validate", str(EXAMPLES_DIR / "basic-beat.json")]) == 0
    assert "valid" in capsys.readouterr().out

    broken = tmp_path / "broken.json"
    document = _example()
    document["current_arrangement_id"] = "arrangement:5"
    broken.write_text(json.dumps(document), encoding="utf-8")

    assert cli_main(["validate", str(broken)]) == 1
    assert "/current_arrangement_id" in capsys.readouterr().out


def test_the_cli_reports_a_file_it_cannot_read(tmp_path: Path, capsys) -> None:
    assert cli_main(["validate", str(tmp_path / "missing.json")]) == 2
    assert "cannot read" in capsys.readouterr().err
