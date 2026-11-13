"""FL Studio raw values <-> project model units.

The project model stores DAW-neutral units (docs/project-model.md, "Units"):
volume as a fader position where 1.0 is FL Studio's 100%, pan from -1 (left)
to +1 (right), dry/wet, send level and velocity from 0 to 1, pitch as an
integer, fine pitch in cents. FL Studio stores integers. Every conversion here
is linear and exactly invertible over FL Studio's whole raw range:
``to_raw(from_raw(raw)) == raw`` for every raw value, so the model never needs
to keep the raw values. The tests check every raw value.

This module mirrors FL Studio's own storage, so it keeps FL's names
(channel, mixer insert). It imports neither FL Studio code nor PyFLP; the
readers and writers pass the integers in and out. The raw ranges are those
PyFLP documents; the ones marked "to confirm" are checked against the
ground-truth FL Studio 2025 files before the writer uses them.
"""
from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class LinearUnit:
    """A linear map between an integer raw range and a model range.

    ``model = (raw - raw_zero) / raw_per_unit``. A negative ``raw_per_unit``
    flips the direction (FL's stereo separation grows towards merged).
    """

    name: str
    raw_min: int
    raw_max: int
    raw_zero: int
    raw_per_unit: int

    def __post_init__(self) -> None:
        if self.raw_min > self.raw_max:
            raise ValueError(f"{self.name}: raw_min is above raw_max")
        if self.raw_per_unit == 0:
            raise ValueError(f"{self.name}: raw_per_unit must not be 0")

    @property
    def model_min(self) -> float:
        return min(self.from_raw(self.raw_min), self.from_raw(self.raw_max))

    @property
    def model_max(self) -> float:
        return max(self.from_raw(self.raw_min), self.from_raw(self.raw_max))

    def from_raw(self, raw: int) -> float:
        """The model value of an FL Studio raw integer."""
        _require_int(raw, self.name)
        if not self.raw_min <= raw <= self.raw_max:
            raise ValueError(f"{self.name}: raw value must be between {self.raw_min} and {self.raw_max}")
        return (raw - self.raw_zero) / self.raw_per_unit

    def to_raw(self, value: float) -> int:
        """The FL Studio raw integer nearest to a model value (halves round up)."""
        # isfinite() is for floats only: a huge integer cannot be converted to one.
        if isinstance(value, bool) or not isinstance(value, (int, float)) or (
            isinstance(value, float) and not math.isfinite(value)
        ):
            raise ValueError(f"{self.name}: the value must be a finite number")
        # Checked before scaling: a huge value would overflow to infinity, which floor() cannot take.
        half_step = 0.5 / abs(self.raw_per_unit)
        if not self.model_min - half_step <= value <= self.model_max + half_step:
            raise ValueError(self._outside_message())
        raw = math.floor(value * self.raw_per_unit + self.raw_zero + 0.5)
        if not self.raw_min <= raw <= self.raw_max:
            raise ValueError(self._outside_message())
        return raw

    def _outside_message(self) -> str:
        return f"{self.name}: the value is outside what FL Studio can store"

    def quantize(self, value: float) -> float:
        """The model value FL Studio would store for ``value``: the nearest raw step."""
        return self.from_raw(self.to_raw(value))


# Volume, as a fader position: raw / 12800, so 1.0 is FL's 100% (0 dB on an insert).
CHANNEL_VOLUME = LinearUnit("channel volume", raw_min=0, raw_max=12800, raw_zero=0, raw_per_unit=12800)
INSERT_VOLUME = LinearUnit("insert volume", raw_min=0, raw_max=16000, raw_zero=0, raw_per_unit=12800)

# Pan: -1 is 100% left, +1 is 100% right.
CHANNEL_PAN = LinearUnit("channel pan", raw_min=0, raw_max=12800, raw_zero=6400, raw_per_unit=6400)
INSERT_PAN = LinearUnit("insert pan", raw_min=-6400, raw_max=6400, raw_zero=0, raw_per_unit=6400)
NOTE_PAN = LinearUnit("note pan", raw_min=0, raw_max=128, raw_zero=64, raw_per_unit=64)

# Stereo separation: +1 is 100% separated (FL raw -64), -1 is 100% merged, i.e. mono (FL raw 64).
INSERT_STEREO_SEPARATION = LinearUnit(
    "insert stereo separation", raw_min=-64, raw_max=64, raw_zero=0, raw_per_unit=-64
)

# 0 to 1.
NOTE_VELOCITY = LinearUnit("note velocity", raw_min=0, raw_max=128, raw_zero=0, raw_per_unit=128)
# Raw ranges to confirm: PyFLP cannot read these two yet.
SLOT_DRY_WET = LinearUnit("effect slot dry/wet", raw_min=0, raw_max=12800, raw_zero=0, raw_per_unit=12800)
SEND_LEVEL = LinearUnit("send level", raw_min=0, raw_max=12800, raw_zero=0, raw_per_unit=12800)

# Tempo in BPM: FL Studio 11 and later store BPM x 1000, from 10 to 522 BPM.
TEMPO = LinearUnit("tempo", raw_min=10_000, raw_max=522_000, raw_zero=0, raw_per_unit=1000)

# Pitch: FL's note key number, the same numbering as MIDI (60 is middle C, FL's "C5").
PITCH_MIN = 0
PITCH_MAX = 131

# Fine pitch: FL raw 0..240 with 120 as no detune, in steps of 10 cents.
FINE_PITCH_RAW_MIN = 0
FINE_PITCH_RAW_MAX = 240
_FINE_PITCH_RAW_ZERO = 120
_CENTS_PER_FINE_PITCH_STEP = 10


def pitch_from_raw(raw: int) -> int:
    _require_int(raw, "pitch")
    _require_range(raw, PITCH_MIN, PITCH_MAX, "pitch")
    return raw


def pitch_to_raw(pitch: int) -> int:
    return pitch_from_raw(pitch)


def fine_pitch_cents_from_raw(raw: int) -> int:
    _require_int(raw, "fine pitch")
    _require_range(raw, FINE_PITCH_RAW_MIN, FINE_PITCH_RAW_MAX, "fine pitch")
    return (raw - _FINE_PITCH_RAW_ZERO) * _CENTS_PER_FINE_PITCH_STEP


def fine_pitch_cents_to_raw(cents: int) -> int:
    _require_int(cents, "fine pitch")
    if cents % _CENTS_PER_FINE_PITCH_STEP != 0:
        raise ValueError("fine pitch: FL Studio stores fine pitch in steps of 10 cents")
    raw = cents // _CENTS_PER_FINE_PITCH_STEP + _FINE_PITCH_RAW_ZERO
    _require_range(raw, FINE_PITCH_RAW_MIN, FINE_PITCH_RAW_MAX, "fine pitch")
    return raw


def _require_int(value: object, name: str) -> None:
    # bool is an int in Python; neither true nor 1.0 is a raw value.
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"{name}: expected an integer")


def _require_range(value: int, low: int, high: int, name: str) -> None:
    if not low <= value <= high:
        raise ValueError(f"{name}: must be between {low} and {high}")
