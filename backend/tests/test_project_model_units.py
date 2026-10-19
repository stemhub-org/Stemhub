"""FL Studio raw values <-> project model units (docs/project-model.md, "Units").

Every conversion must be exactly invertible over its whole raw range, so the
model never needs to store FL's raw values: to_raw(from_raw(r)) == r.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any, Iterator

import pytest

from stemhub.project_model import generate_json_schema, units
from stemhub.project_model.units import LinearUnit

ALL_LINEAR_UNITS = [
    pytest.param(units.CHANNEL_VOLUME, id="channel volume"),
    pytest.param(units.CHANNEL_PAN, id="channel pan"),
    pytest.param(units.INSERT_VOLUME, id="insert volume"),
    pytest.param(units.INSERT_PAN, id="insert pan"),
    pytest.param(units.INSERT_STEREO_SEPARATION, id="insert stereo separation"),
    pytest.param(units.NOTE_VELOCITY, id="note velocity"),
    pytest.param(units.NOTE_PAN, id="note pan"),
    pytest.param(units.SLOT_DRY_WET, id="effect slot dry/wet"),
    pytest.param(units.SEND_LEVEL, id="send level"),
    pytest.param(units.TEMPO, id="tempo"),
]


@pytest.mark.parametrize("unit", ALL_LINEAR_UNITS)
def test_every_raw_value_survives_a_round_trip(unit: LinearUnit) -> None:
    for raw in range(unit.raw_min, unit.raw_max + 1):
        assert unit.to_raw(unit.from_raw(raw)) == raw


@pytest.mark.parametrize("unit", ALL_LINEAR_UNITS)
def test_model_values_are_monotonic_and_stay_in_the_model_range(unit: LinearUnit) -> None:
    values = [unit.from_raw(raw) for raw in range(unit.raw_min, unit.raw_max + 1)]

    assert all(isinstance(value, float) for value in values)
    assert min(values) == unit.model_min
    assert max(values) == unit.model_max
    assert values == sorted(values) or values == sorted(values, reverse=True)


@pytest.mark.parametrize(
    ("unit", "raw", "expected"),
    [
        (units.CHANNEL_VOLUME, 10000, 0.78125),  # FL's default channel volume (78%)
        (units.CHANNEL_VOLUME, 12800, 1.0),
        (units.CHANNEL_PAN, 0, -1.0),
        (units.CHANNEL_PAN, 6400, 0.0),
        (units.CHANNEL_PAN, 12800, 1.0),
        (units.INSERT_VOLUME, 12800, 1.0),  # 100%, 0 dB
        (units.INSERT_VOLUME, 16000, 1.25),  # 125%, the fader's top
        (units.INSERT_PAN, -6400, -1.0),
        (units.INSERT_PAN, 6400, 1.0),
        (units.INSERT_STEREO_SEPARATION, -64, 1.0),  # 100% separated
        (units.INSERT_STEREO_SEPARATION, 64, -1.0),  # 100% merged (mono)
        (units.NOTE_VELOCITY, 100, 0.78125),  # FL's default velocity
        (units.NOTE_VELOCITY, 128, 1.0),
        (units.NOTE_PAN, 64, 0.0),
        (units.NOTE_PAN, 0, -1.0),
        (units.SLOT_DRY_WET, 12800, 1.0),
        (units.SEND_LEVEL, 3200, 0.25),
        (units.TEMPO, 140000, 140.0),
        (units.TEMPO, 128500, 128.5),
    ],
)
def test_known_fl_values_map_to_the_documented_model_values(unit: LinearUnit, raw: int, expected: float) -> None:
    assert unit.from_raw(raw) == expected
    assert unit.to_raw(expected) == raw


def test_to_raw_rounds_values_between_two_raw_steps_to_the_nearest() -> None:
    assert units.NOTE_VELOCITY.to_raw(0.5 + 0.4 / 128) == 64
    assert units.NOTE_VELOCITY.to_raw(0.5 + 0.6 / 128) == 65
    assert units.NOTE_VELOCITY.quantize(0.5 + 0.4 / 128) == 0.5


def test_to_raw_accepts_integers_like_json_numbers() -> None:
    assert units.CHANNEL_VOLUME.to_raw(1) == 12800
    assert units.INSERT_PAN.to_raw(0) == 0


@pytest.mark.parametrize("unit", ALL_LINEAR_UNITS)
def test_from_raw_refuses_raw_values_outside_the_fl_range(unit: LinearUnit) -> None:
    with pytest.raises(ValueError):
        unit.from_raw(unit.raw_min - 1)
    with pytest.raises(ValueError):
        unit.from_raw(unit.raw_max + 1)


@pytest.mark.parametrize("bad_raw", [True, 1.0, "1", None])
def test_from_raw_refuses_anything_but_an_integer(bad_raw) -> None:
    with pytest.raises(ValueError):
        units.NOTE_VELOCITY.from_raw(bad_raw)


@pytest.mark.parametrize("bad_value", [1.1, -0.01, math.nan, math.inf, True, "1", None])
def test_to_raw_refuses_values_outside_the_unit_or_not_numbers(bad_value) -> None:
    with pytest.raises(ValueError):
        units.CHANNEL_VOLUME.to_raw(bad_value)


@pytest.mark.parametrize("unit", ALL_LINEAR_UNITS)
@pytest.mark.parametrize(
    "huge",
    [
        pytest.param(1e308, id="1e308"),
        pytest.param(-1e308, id="-1e308"),
        pytest.param(sys.float_info.max, id="largest float"),
        pytest.param(-sys.float_info.max, id="lowest float"),
        pytest.param(10**400, id="10**400"),
        pytest.param(-(10**400), id="-10**400"),
    ],
)
def test_to_raw_refuses_huge_values_without_overflowing(unit: LinearUnit, huge: float) -> None:
    with pytest.raises(ValueError):
        unit.to_raw(huge)


@pytest.mark.parametrize("unit", ALL_LINEAR_UNITS)
def test_to_raw_takes_values_up_to_half_a_raw_step_outside_the_model_range(unit: LinearUnit) -> None:
    half_step = 0.5 / abs(unit.raw_per_unit)

    assert unit.to_raw(unit.model_min - half_step * 0.9) in (unit.raw_min, unit.raw_max)
    assert unit.to_raw(unit.model_max + half_step * 0.9) in (unit.raw_min, unit.raw_max)
    with pytest.raises(ValueError):
        unit.to_raw(unit.model_min - half_step * 1.1)
    with pytest.raises(ValueError):
        unit.to_raw(unit.model_max + half_step * 1.1)


def test_to_raw_rounds_exactly_half_a_step_outside_the_range_up() -> None:
    half_step = 0.5 / 128  # exact in binary, so the halves are exact too

    assert units.NOTE_VELOCITY.to_raw(0.0 - half_step) == 0  # rounds up to the bottom
    with pytest.raises(ValueError):
        units.NOTE_VELOCITY.to_raw(1.0 + half_step)  # rounds up past the top


def test_channel_volume_stops_at_100_percent_while_insert_volume_goes_to_125() -> None:
    with pytest.raises(ValueError):
        units.CHANNEL_VOLUME.to_raw(1.25)
    assert units.INSERT_VOLUME.to_raw(1.25) == 16000


def test_the_schema_faders_span_fl_studio_volume_ranges() -> None:
    definitions = generate_json_schema()["$defs"]

    for definition, unit in (("InstrumentVolume", units.CHANNEL_VOLUME), ("InsertVolume", units.INSERT_VOLUME)):
        fader = definitions[definition]["properties"]["fader"]
        assert (fader["minimum"], fader["maximum"]) == (unit.model_min, unit.model_max), definition


def test_tempo_to_raw_refuses_tempos_fl_studio_cannot_store() -> None:
    with pytest.raises(ValueError):
        units.TEMPO.to_raw(9.999)
    with pytest.raises(ValueError):
        units.TEMPO.to_raw(600.0)


def test_fine_pitch_round_trips_every_raw_value_in_steps_of_ten_cents() -> None:
    for raw in range(0, 241):
        cents = units.fine_pitch_cents_from_raw(raw)
        assert isinstance(cents, int)
        assert cents % 10 == 0
        assert units.fine_pitch_cents_to_raw(cents) == raw

    assert units.fine_pitch_cents_from_raw(0) == -1200
    assert units.fine_pitch_cents_from_raw(120) == 0
    assert units.fine_pitch_cents_from_raw(240) == 1200


@pytest.mark.parametrize("bad_cents", [15, -1210, 1210, 10.0, True])
def test_fine_pitch_to_raw_refuses_values_fl_studio_cannot_store(bad_cents) -> None:
    with pytest.raises(ValueError):
        units.fine_pitch_cents_to_raw(bad_cents)


@pytest.mark.parametrize("bad_raw", [-1, 241, True, 120.0])
def test_fine_pitch_from_raw_refuses_raw_values_outside_the_fl_range(bad_raw) -> None:
    with pytest.raises(ValueError):
        units.fine_pitch_cents_from_raw(bad_raw)


def test_pitch_is_fl_studio_key_number_unchanged() -> None:
    for raw in range(0, 132):
        assert units.pitch_from_raw(raw) == raw
        assert units.pitch_to_raw(units.pitch_from_raw(raw)) == raw


@pytest.mark.parametrize("bad_pitch", [-1, 132, True, 60.0])
def test_pitch_refuses_values_outside_fl_studio_range(bad_pitch) -> None:
    with pytest.raises(ValueError):
        units.pitch_to_raw(bad_pitch)
    with pytest.raises(ValueError):
        units.pitch_from_raw(bad_pitch)


def test_a_linear_unit_needs_a_non_empty_raw_range_and_a_non_zero_scale() -> None:
    with pytest.raises(ValueError):
        LinearUnit(name="broken", raw_min=10, raw_max=0, raw_zero=0, raw_per_unit=1)
    with pytest.raises(ValueError):
        LinearUnit(name="broken", raw_min=0, raw_max=10, raw_zero=0, raw_per_unit=0)


# ── The example documents hold values FL Studio can store ──

EXAMPLES_DIR = Path(__file__).parent / "fixtures" / "project_model" / "examples"


def _fl_values(document: dict[str, Any]) -> Iterator[tuple[LinearUnit, float]]:
    """(unit, model value) for every FL-backed number of a document."""
    if document["tempo_bpm"] is not None:
        yield units.TEMPO, document["tempo_bpm"]
    for instrument in document["instruments"]:
        yield units.CHANNEL_VOLUME, instrument["volume"]["fader"]
        yield units.CHANNEL_PAN, instrument["pan"]
    for pattern in document["patterns"]:
        for note in pattern["notes"]:
            yield units.NOTE_VELOCITY, note["velocity"]
            yield units.NOTE_PAN, note["pan"]
    for insert in document["mixer"]["inserts"]:
        yield units.INSERT_VOLUME, insert["volume"]["fader"]
        yield units.INSERT_PAN, insert["pan"]
        yield units.INSERT_STEREO_SEPARATION, insert["stereo_separation"]
        for send in insert["sends"]:
            yield units.SEND_LEVEL, send["level"]
        for slot in insert["effect_slots"]:
            yield units.SLOT_DRY_WET, slot["dry_wet"]


@pytest.mark.parametrize("name", sorted(path.name for path in EXAMPLES_DIR.glob("*.json")))
def test_example_values_are_exactly_what_fl_studio_stores(name: str) -> None:
    document = json.loads((EXAMPLES_DIR / name).read_text(encoding="utf-8"))

    for unit, value in _fl_values(document):
        assert unit.quantize(value) == value, (unit.name, value)
    for pattern in document["patterns"]:
        for note in pattern["notes"]:
            cents = note["fine_pitch_cents"]
            assert units.fine_pitch_cents_from_raw(units.fine_pitch_cents_to_raw(cents)) == cents
            assert units.pitch_to_raw(note["pitch"]) == note["pitch"]
