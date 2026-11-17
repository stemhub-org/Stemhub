from __future__ import annotations

import json
from pathlib import Path
from types import SimpleNamespace

import pyflp
import pytest

from flp_helpers import REFERENCE, MULTI_CHANNEL, CORRUPTED_DIR, fl2025_file
from test_parser_regressions import _fl_24_2_project
from stemhub.project_model.fl_reader import read_flp, model_from_project, ProjectReadError, _Reader
from stemhub.project_model import validate_document
from stemhub.fl_mixer import mixer_from_project_model, diff_fl_mixers

GOLDENS = Path(__file__).parent / "fixtures" / "project_model" / "goldens"


@pytest.mark.parametrize("name,path", [("reference", REFERENCE), ("multi-channel", MULTI_CHANNEL)])
def test_fixture_golden(name, path):
    document = read_flp(path).model_dump(mode="json")
    assert validate_document(document).valid
    assert document == json.loads((GOLDENS / f"{name}.json").read_text())


@pytest.mark.parametrize("name,data", [("fl-24-2", _fl_24_2_project()), ("fl-25-2", fl2025_file())])
def test_modern_fl_golden(name, data, tmp_path):
    path = tmp_path / f"{name}.flp"
    path.write_bytes(data)
    document = read_flp(path).model_dump(mode="json")
    assert validate_document(document).valid
    assert document == json.loads((GOLDENS / f"{name}.json").read_text())


@pytest.mark.parametrize("path", sorted(CORRUPTED_DIR.glob("*.flp")))
def test_corrupt_files_are_safe_errors(path):
    with pytest.raises(ProjectReadError, match="structurally invalid"):
        read_flp(path)


def test_reference_values_are_grounded_in_fl_units():
    model = read_flp(REFERENCE)
    instruments = {i.name: i for i in model.instruments}
    assert instruments["Zero Volume"].volume.fader == 0
    assert instruments["Full Volume"].volume.fader == 1
    assert instruments["100% L"].pan == -1
    assert instruments["100% R"].pan == 1
    assert instruments["Disabled"].enabled is False
    assert model.tempo_bpm == 69.42
    assert model.source.ppq == 96
    notes = next(p.notes for p in model.patterns if p.notes)
    assert notes[0].pitch == 60
    assert notes[0].velocity == 100 / 128
    assert notes[0].fine_pitch_cents == 0
    slots = next(i.effect_slots for i in model.mixer.inserts if i.name == "Plugin Test")
    assert [s.plugin.name for s in slots] == [
        "Fruity Balance",
        "Fruity Fast Dist",
        "Fruity Send",
        "Fruity Soft Clipper",
        "Fruity Stereo Enhancer",
        "Soundgoodizer",
        "OTT",
    ]
    assert {s.dry_wet for s in slots} == {1.0}
    bypassed = next(i for i in model.mixer.inserts if i.name == "Bypassed")
    assert bypassed.effect_slots[0].index == 0  # raw index includes flag bits
    assert bypassed.effect_slots[0].dry_wet == 0


def test_broken_section_does_not_discard_other_sections():
    project = pyflp.parse(REFERENCE)

    class BrokenMixer:
        def __iter__(self):
            raise ValueError("private /Users/someone/sample.wav")

    proxy = SimpleNamespace(
        channels=project.channels,
        patterns=project.patterns,
        arrangements=project.arrangements,
        mixer=BrokenMixer(),
        tempo=project.tempo,
        ppq=project.ppq,
        version=project.version,
    )
    model = model_from_project(proxy)
    assert len(model.instruments) == 14
    assert model.patterns
    assert model.tempo_bpm == 69.42
    assert any(u.path == "/mixer" for u in model.unsupported)
    assert "private" not in model.model_dump_json()
    assert not mixer_from_project_model(model.model_dump(mode="json")).mixer_supported


def test_corrupt_typed_event_keeps_unrelated_music(tmp_path):
    from stemhub.project_model.flp_events import parse, serialize, Event
    from dataclasses import replace

    raw = parse(REFERENCE.read_bytes())
    # A valid outer FLP with an invalid automation payload. Its typed
    # constructor raises, so the fallback must retain unrelated music.
    events = tuple(Event(e.id, b"\x00", e.length_width) if e.id == 234 else e for e in raw.events)
    path = tmp_path / "bad-automation.flp"
    path.write_bytes(serialize(replace(raw, events=events)))
    model = read_flp(path)
    assert model.tempo_bpm == 69.42
    assert len(model.instruments) == 14
    assert any(u.path.startswith(("/instruments", "/automation")) for u in model.unsupported)


@pytest.mark.parametrize(
    "value,expected",
    [
        ("Samples/Kick.wav", {"location": "project", "asset_path": "Samples/Kick.wav"}),
        (
            "%FLStudioFactoryData%\\Packs\\Kick.wav",
            {"location": "factory", "path": "%FLStudioFactoryData%/Packs/Kick.wav"},
        ),
        ("C:\\Users\\Alice\\Kick.wav", {"location": "external", "file_name": "Kick.wav"}),
        ("/Users/Alice/Kick.wav", {"location": "external", "file_name": "Kick.wav"}),
    ],
)
def test_samples_never_store_absolute_paths(value, expected):
    from pyflp.channel import ChannelID

    events = SimpleNamespace(ids={ChannelID.SamplePath}, first=lambda _: SimpleNamespace(value=value))
    assert _Reader().sample(SimpleNamespace(events=events)).model_dump() == expected


def test_sparse_default_inserts_do_not_look_deleted():
    document = read_flp(MULTI_CHANNEL).model_dump(mode="json")
    # Compare the same empty mixer represented densely and sparsely.
    document["mixer"]["inserts"] = [
        dict(
            id="insert:1",
            number=1,
            name="",
            enabled=True,
            volume={"fader": 1.0, "db": None},
            pan=0.0,
            stereo_separation=0.0,
            sends=[],
            effect_slots=[],
        )
    ]
    dense = mixer_from_project_model(document)
    document["mixer"]["inserts"] = []
    sparse = mixer_from_project_model(document)
    assert diff_fl_mixers(dense, sparse).summary.total_changes == 0


def test_unreadable_mixer_does_not_report_deletions():
    good = mixer_from_project_model(read_flp(REFERENCE).model_dump(mode="json"), sha256="a")
    from dataclasses import replace

    broken = replace(good, inserts=(), mixer_supported=False, flp_sha256="b")
    result = diff_fl_mixers(good, broken)
    assert [c.type for c in result.changes] == ["project_file_changed"]


def test_real_mixer_change_uses_model_units(tmp_path):
    import struct

    data = _fl_24_2_project()
    before = tmp_path / "before.flp"
    after = tmp_path / "after.flp"
    before.write_bytes(data)
    after.write_bytes(data.replace(struct.pack("<i", 11000), struct.pack("<i", 6400), 1))
    base = mixer_from_project_model(read_flp(before).model_dump(mode="json"))
    target = mixer_from_project_model(read_flp(after).model_dump(mode="json"))
    changes = diff_fl_mixers(base, target).changes
    assert len(changes) == 1
    assert changes[0].type == "insert_volume_changed"
    assert changes[0].before == 11000 / 12800
    assert changes[0].after == 0.5
