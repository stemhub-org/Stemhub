"""What StemHub relies on in the pinned PyFLP (backend/vendor/PyFLP_v2, PYFLP_COMMIT).

Each test fails with the parser pinned before the FL Studio 2024/2025 fixes
(54d2b96) and passes with the one that has them, so a submodule move that loses
a fix is caught here. The facts pinned on the reference project are checked
against its raw events first, with StemHub's own tokenizer, so they don't rest
on PyFLP alone. See docs/fl-studio-format.md.
"""
from __future__ import annotations

import importlib
import struct
from pathlib import Path
from typing import Any, Optional

import pytest

from flp_helpers import (
    FL_VERSION,
    INSERT_COUNT,
    INSERT_FLAGS,
    INSERT_INPUT,
    INSERT_NAME,
    INSERT_OUTPUT,
    MIXER_PARAMS,
    PLUGIN_INTERNAL_NAME,
    PLUGIN_NAME,
    REFERENCE,
    SLOT_INDEX,
    TEMPO,
    ascii_text,
    fixed_event,
    fl2025_file,
    flp_data,
    utf16_text,
    var_event,
)
from stemhub.dependency_guard import ensure_pyflp_available
from stemhub.fl_mixer import FlMixer, FlMixerInsert, parse_fl_mixer
from stemhub.project_model.flp_events import FL_STUDIO_TEXT_ID, EVENT_0XAC, decode_text, parse

SLOTS_PER_INSERT = 10
DEFAULT_DRY_WET = 12800

# The effect slots of the reference project's "Plugin Test" insert, in FL Studio's order:
# (slot index, plugin internal name, the name the user sees or None).
PLUGIN_TEST_SLOTS = [
    (0, "Fruity Balance", None),
    (1, "Fruity Fast Dist", None),
    (2, "Fruity Send", None),
    (3, "Fruity Soft Clipper", None),
    (4, "Fruity Stereo Enhancer", None),
    (5, "Soundgoodizer", None),
    (6, "Fruity Wrapper", "OTT (failed to load)"),
]


def _parse_project(path: Path) -> Any:
    ensure_pyflp_available()
    return importlib.import_module("pyflp").parse(path)


def _insert_named(mixer: FlMixer, name: str) -> FlMixerInsert:
    matches = [insert for insert in mixer.inserts if insert.name == name]
    assert len(matches) == 1, f"expected one insert named {name!r}, found {len(matches)}"
    return matches[0]


@pytest.fixture(scope="module")
def reference_mixer() -> FlMixer:
    return parse_fl_mixer(_parse_project(REFERENCE))


# --- the reference project (FL 20.8.4): effect slots ---------------------------------------


def _raw_slots_of_insert(name: str) -> list[tuple[int, Optional[str], Optional[str]]]:
    """The loaded effect slots of an insert, read from the raw events.

    A slot's plugin events come before its slot-index event (98), which ends the slot.
    """
    events = parse(REFERENCE.read_bytes()).events
    start = next(
        position
        for position, event in enumerate(events)
        if event.id == INSERT_NAME and decode_text(event) == name
    )
    slots: list[tuple[int, Optional[str], Optional[str]]] = []
    internal_name: Optional[str] = None
    slot_name: Optional[str] = None
    for event in events[start + 1 :]:
        if event.id == INSERT_OUTPUT:  # the insert's last event
            break
        if event.id == PLUGIN_INTERNAL_NAME:
            internal_name = decode_text(event)
        elif event.id == PLUGIN_NAME:
            slot_name = decode_text(event)
        elif event.id == SLOT_INDEX:
            if internal_name is not None:
                slots.append((int.from_bytes(event.payload, "little"), internal_name, slot_name))
            internal_name = slot_name = None
    return slots


def test_each_effect_slot_holds_its_own_plugin(reference_mixer: FlMixer) -> None:
    # Before the fix a slot reported the next slot's plugin: slot 1 read "Fruity Send"
    # and the last loaded slot was lost.
    assert _raw_slots_of_insert("Plugin Test") == PLUGIN_TEST_SLOTS  # the file itself says so
    insert = _insert_named(reference_mixer, "Plugin Test")

    assert [(slot.index, slot.internal_name, slot.name) for slot in insert.slots] == PLUGIN_TEST_SLOTS
    assert [slot.plugin_name for slot in insert.slots] == [
        "Fruity Balance",
        "Fruity Fast Dist",
        "Fruity Send",
        "Fruity Soft Clipper",
        "Fruity Stereo Enhancer",
        "Soundgoodizer",
        "OTT (failed to load)",
    ]


def test_the_last_loaded_effect_slot_of_an_insert_is_not_lost(reference_mixer: FlMixer) -> None:
    insert = _insert_named(reference_mixer, "Effect slots")

    assert _raw_slots_of_insert("Effect slots") == [
        (0, "Fruity NoteBook 2", "Colored"),
        (1, "Fruity NoteBook 2", "Iconified"),
    ]
    assert [(slot.index, slot.name) for slot in insert.slots] == [(0, "Colored"), (1, "Iconified")]


def test_every_effect_slot_has_enabled_and_dry_wet(reference_mixer: FlMixer) -> None:
    # Before the fix both were None on every slot, so the mixer diff never saw them change.
    slots = [slot for insert in reference_mixer.inserts for slot in insert.slots]

    assert slots, "the reference project has loaded effect slots"
    assert [slot for slot in slots if slot.enabled is None or slot.dry_wet is None] == []
    plugin_test = _insert_named(reference_mixer, "Plugin Test")
    assert {(slot.enabled, slot.dry_wet) for slot in plugin_test.slots} == {(True, DEFAULT_DRY_WET)}
    assert [slot.dry_wet for slot in _insert_named(reference_mixer, "Bypassed").slots] == [0]


# --- FL 25.2.3+: event 172 carries 3 bytes -------------------------------------------------


@pytest.mark.parametrize("tempo_x1000", [128000, 140000, 120000, 90000])
def test_fl_25_2_3_project_keeps_its_tempo(tmp_path: Path, tempo_x1000: int) -> None:
    # A reader taking event 172 as 4 bytes swallows the id of the next event, walks the
    # "FL Studio" text as 1-byte events, then reads the tempo's id as a payload. What
    # follows depends on the tempo's bytes: with 128 or 140 BPM it falls back into step
    # and the file parses without its tempo, as real 25.2.3+ files did; with 120 or 90
    # BPM the parse fails.
    data = fl2025_file(tempo_x1000)
    events = parse(data).events
    assert [event.id for event in events] == [FL_VERSION, EVENT_0XAC, FL_STUDIO_TEXT_ID, TEMPO]
    assert events[1].payload == bytes((1, 1, 0))
    path = tmp_path / "fl-25-2-4.flp"
    path.write_bytes(data)

    project = _parse_project(path)

    assert str(project.version) == "25.2.4.4960"
    assert project.tempo is not None
    assert float(project.tempo) == pytest.approx(tempo_x1000 / 1000)


# --- FL 24.2.99+: a stored insert count and mixer parameters keyed from 448 ----------------

KEY_MASTER_FROM_24_2 = 448
KEY_CURRENT_FROM_24_2 = 949
INSERT_GROUP = 31
SLOT_ENABLED, SLOT_DRY_WET, VOLUME, PAN = 0, 1, 192, 193
INSERT_FLAGS_ENABLED = bytes(4) + struct.pack("<i", 0x0C) + bytes(4)


def _insert_block(name: Optional[str] = None, plugins: Optional[dict[int, str]] = None) -> bytes:
    """One insert as FL 24.2.99+ stores it: each slot's plugin, then its index; output last."""
    block = fixed_event(42, 0, 1)
    if name is not None:
        block += var_event(INSERT_NAME, utf16_text(name))
    block += var_event(INSERT_FLAGS, INSERT_FLAGS_ENABLED)
    for index in range(SLOTS_PER_INSERT):
        if plugins and index in plugins:
            block += var_event(PLUGIN_INTERNAL_NAME, utf16_text(plugins[index]))
        block += fixed_event(SLOT_INDEX, index, 2)
    return (
        block
        + fixed_event(165, 3, 4)
        + fixed_event(166, 1, 4)
        + fixed_event(49, 0, 1)
        + fixed_event(INSERT_INPUT, 0xFFFFFFFF, 4)
        + fixed_event(INSERT_OUTPUT, 0xFFFFFFFF, 4)
    )


def _mixer_param(key: int, slot: int, parameter: int, value: int) -> bytes:
    """A 12-byte item of event 225; the insert's key is ``channel data >> 6``."""
    return struct.pack("<IBBHi", 0, parameter, INSERT_GROUP, (key << 6) | slot, value)


def _fl_24_2_project() -> bytes:
    """Master, "Drums" (an effect in slot 3), "Bass" and the "current" insert."""
    blocks = [
        _insert_block("Master"),
        _insert_block("Drums", {2: "Fruity Limiter"}),
        _insert_block("Bass"),
        _insert_block(),
    ]
    params = b"".join(
        (
            _mixer_param(KEY_MASTER_FROM_24_2, 0, VOLUME, 12000),
            _mixer_param(KEY_MASTER_FROM_24_2 + 1, 0, VOLUME, 11000),
            _mixer_param(KEY_MASTER_FROM_24_2 + 1, 0, PAN, -3200),
            _mixer_param(KEY_MASTER_FROM_24_2 + 1, 2, SLOT_ENABLED, 0),
            _mixer_param(KEY_MASTER_FROM_24_2 + 1, 2, SLOT_DRY_WET, 6400),
            _mixer_param(KEY_MASTER_FROM_24_2 + 2, 0, VOLUME, 10000),
            _mixer_param(KEY_MASTER_FROM_24_2 + 2, 0, PAN, 1600),
            _mixer_param(KEY_CURRENT_FROM_24_2, 0, VOLUME, 9000),
        )
    )
    return flp_data(
        var_event(FL_VERSION, ascii_text("24.2.99.4720"))
        + fixed_event(INSERT_COUNT, len(blocks), 2)
        + b"".join(blocks)
        + var_event(MIXER_PARAMS, params)
    )


@pytest.fixture()
def fl_24_2_mixer(tmp_path: Path) -> FlMixer:
    path = tmp_path / "fl-24-2.flp"
    path.write_bytes(_fl_24_2_project())
    return parse_fl_mixer(_parse_project(path))


def test_fl_24_2_insert_volume_and_pan_are_read(fl_24_2_mixer: FlMixer) -> None:
    # Before the fix the insert of an item was key & 0x7F, which is wrong from key 448 on:
    # the inserts were found, with no volume and no pan.
    assert fl_24_2_mixer.mixer_supported is True
    assert [
        (insert.index, insert.name, insert.volume, insert.pan) for insert in fl_24_2_mixer.inserts
    ] == [
        (0, "Master", 12000, None),
        (1, "Drums", 11000, -3200),
        (2, "Bass", 10000, 1600),
        (3, None, 9000, None),
    ]


def test_fl_24_2_effect_slot_enabled_and_dry_wet_are_read(fl_24_2_mixer: FlMixer) -> None:
    drums = _insert_named(fl_24_2_mixer, "Drums")

    assert [
        (slot.index, slot.plugin_name, slot.enabled, slot.dry_wet) for slot in drums.slots
    ] == [(2, "Fruity Limiter", False, 6400)]
    assert [insert.slots for insert in fl_24_2_mixer.inserts if insert.name != "Drums"] == [(), (), ()]
