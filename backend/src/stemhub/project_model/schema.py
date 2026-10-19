"""Project model 0.1.0: the DAW-neutral JSON of a project's musical content.

The format is described in docs/project-model.md. These models are its single
source of truth: the committed JSON Schema (schema/project-model-0.1.0.schema.json)
is generated from them with ``python -m stemhub.project_model schema``.

Validation has two passes. These models check each object on its own: types,
ranges, ID shapes, privacy by construction (there is no field for comments,
URLs or absolute paths) and the caps. ``references.check_references`` then
checks how objects refer to one another. Core objects forbid unknown fields
and take integers strictly (1.0 and true are not integers); number fields
accept integers. Anything a DAW stores that the core objects don't model goes
in ``extensions``: free-form JSON under a namespace (``fl_studio``), capped in
depth and size.

Error reports are bounded by the schema's shape, not by the body's size: a
list reports its first invalid item only and an object its first
MAX_UNKNOWN_FIELDS_REPORTED unknown fields only. None of this changes which
documents are valid, nor the generated JSON Schema.
"""
import json
from functools import cache
from pathlib import Path
from typing import Annotated, Any, Callable, Literal, Optional, TypeVar, Union

from pydantic import (
    AfterValidator,
    BaseModel,
    BeforeValidator,
    ConfigDict,
    Discriminator,
    FailFast,
    Field,
    Tag,
    WithJsonSchema,
    model_validator,
)
from pydantic.json_schema import GenerateJsonSchema
from pydantic_core import PydanticCustomError

SCHEMA_NAME = "stemhub.project-model"
SCHEMA_VERSION = "0.1.0"
SCHEMA_ID = f"urn:stemhub:project-model:{SCHEMA_VERSION}"
JSON_SCHEMA_DIALECT = "https://json-schema.org/draft/2020-12/schema"
JSON_SCHEMA_PATH = Path(__file__).parent / "schema" / f"project-model-{SCHEMA_VERSION}.schema.json"

# ── Caps ──

MAX_NAME_LENGTH = 255
MAX_PATH_LENGTH = 255  # the manifest's limit (docs/content-addressed-storage.md)
MAX_NOTES = 100_000  # in the whole model
MAX_CLIPS = 20_000  # in the whole model
MAX_PATTERNS = 999
MAX_INSERTS = 127  # FL Studio: Master (0), inserts 1 to 125 and "Current" (126)
MAX_TRACKS = 500  # per arrangement
MAX_EFFECT_SLOTS = 10  # per insert
MAX_INSTRUMENTS = 999
MAX_AUDIO_SOURCES = 999
MAX_AUTOMATION = 999
MAX_ARRANGEMENTS = 999
MAX_MARKERS = 999  # per arrangement
MAX_AUTOMATION_POINTS = 10_000  # per automation
MAX_REPORTS = 1_000  # unsupported[] and warnings[]
MAX_TICKS = 2**31 - 1  # FL Studio stores positions as 32-bit integers
MAX_SIZE_BYTES = 2**53 - 1  # the largest integer every JSON reader keeps exact
MAX_EXTENSIONS_DEPTH = 8  # the extensions object is level 1, a namespace object level 2
MAX_EXTENSIONS_BYTES = 64 * 1024  # per extensions object, as compact UTF-8 JSON
MAX_EXTENSION_NAMESPACES = 16

# Bounded error reports. Each invalid item or unknown field is one error, so
# without these a 10 MiB body could make validation build millions of them:
# a list reports its first invalid item only (``Items``), an object its first
# unknown fields only, and extensions count their namespaces first.
MAX_UNKNOWN_FIELDS_REPORTED = 8

# ── Patterns (regular expressions) ──
#
# Kept to what both Pydantic's regex engine and JSON Schema (ECMA-262) accept:
# no look-around. A path segment is what the StemHub plugin accepts in a
# manifest path (plugin Manifest.cpp isSafePath): no separator, colon or
# control character, not empty, not ending with a dot or a space (so no "."
# or ".." either). Reserved Windows names are checked in code. An external
# sample's file name is one such segment without bidirectional controls
# (U+202A to U+202E, U+2066 to U+2069), which can make a name display as
# another one ("x", U+202E, "gnp.wav" displays as "xvaw.png").


def _segment(excluded: str) -> str:
    """A path segment made of characters outside ``excluded``: not empty, not ending with a dot or a space."""
    return f"[^{excluded}]*[^{excluded}. ]"


_PATH_EXCLUDED = r"/\\:\x00-\x1f\x7f"
_BIDI_CONTROLS = r"\u202a-\u202e\u2066-\u2069"

_NO_NUL = r"^[^\x00]*$"
_SEGMENT = _segment(_PATH_EXCLUDED)
_ASSET_PATH = "^" + _SEGMENT + "(?:/" + _SEGMENT + ")*$"
_FACTORY_PATH = "^%FL[A-Za-z0-9]+%(?:/" + _SEGMENT + ")+$"
_FILE_NAME = "^" + _segment(_PATH_EXCLUDED + _BIDI_CONTROLS) + "$"
_SHA256 = r"^[0-9a-f]{64}$"
_COLOR = r"^#[0-9A-F]{6}$"
_JSON_POINTER = r"^(?:/(?:[^~/\x00]|~[01])*)*$"
_NAMESPACE = r"^[a-z][a-z0-9_]{0,63}$"
_INDEX = r"(?:0|[1-9][0-9]{0,4})"
_INSERT_NUMBER = r"(?:[0-9]|[1-9][0-9]|1[01][0-9]|12[0-6])"  # 0 to 126
_TRACK_NUMBER = r"(?:[1-9]|[1-9][0-9]|[1-4][0-9]{2}|500)"  # 1 to 500

_RESERVED_WINDOWS_NAMES = frozenset(
    {"CON", "PRN", "AUX", "NUL", *(f"COM{n}" for n in range(1, 10)), *(f"LPT{n}" for n in range(1, 10))}
)


def _no_reserved_windows_names(path: str) -> str:
    # Windows resolves these device names anywhere in a path, "NUL.wav" included.
    for segment in path.split("/"):
        if segment.split(".", 1)[0].rstrip().upper() in _RESERVED_WINDOWS_NAMES:
            raise PydanticCustomError("reserved_file_name", "A file or folder name is a reserved Windows device name.")
    return path


def _strict_int_literal(value: Any) -> Any:
    # A Literal of integers takes true for 1 and 4.0 for 4, even in strict mode.
    # Other values (text, null) are left to the Literal, which refuses them.
    if isinstance(value, (bool, float)):
        raise PydanticCustomError("int_type", "Input should be a valid integer")
    return value


_Item = TypeVar("_Item")

# Every list of the model: it reports its first invalid item only.
Items = Annotated[list[_Item], FailFast()]


# ── Scalar types ──

Name = Annotated[str, Field(max_length=MAX_NAME_LENGTH, pattern=_NO_NUL)]
RequiredName = Annotated[str, Field(min_length=1, max_length=MAX_NAME_LENGTH, pattern=_NO_NUL)]
Text = Annotated[str, Field(min_length=1, max_length=500, pattern=_NO_NUL)]
Ticks = Annotated[int, Field(ge=0, le=MAX_TICKS)]
# Fader positions: FL raw / 12800. The ranges are units.CHANNEL_VOLUME's and units.INSERT_VOLUME's.
_FADER_DESCRIPTION = "Fader position: FL raw / 12800, so 1.0 is FL Studio's 100%."
InstrumentFader = Annotated[float, Field(ge=0.0, le=1.0, description=_FADER_DESCRIPTION)]
InsertFader = Annotated[float, Field(ge=0.0, le=1.25, description=_FADER_DESCRIPTION)]
Pan = Annotated[float, Field(ge=-1.0, le=1.0, description="-1 is fully left, 0 centred, +1 fully right.")]
UnitInterval = Annotated[float, Field(ge=0.0, le=1.0)]
Color = Annotated[str, Field(pattern=_COLOR, description="#RRGGBB, upper-case hex.")]
Sha256 = Annotated[str, Field(pattern=_SHA256)]
JsonPointer = Annotated[str, Field(max_length=512, pattern=_JSON_POINTER, description="An RFC 6901 JSON pointer.")]
AssetPath = Annotated[
    str,
    Field(
        min_length=1,
        max_length=MAX_PATH_LENGTH,
        pattern=_ASSET_PATH,
        description="Path in the project folder, as in the version's manifest: relative and '/'-separated.",
    ),
    AfterValidator(_no_reserved_windows_names),
]
FileName = Annotated[
    str,
    Field(
        min_length=1,
        max_length=MAX_PATH_LENGTH,
        pattern=_FILE_NAME,
        description="A file name without its folder: one segment of an asset path, without bidirectional controls.",
    ),
    AfterValidator(_no_reserved_windows_names),
]
FactoryPath = Annotated[
    str,
    Field(
        min_length=1,
        max_length=MAX_PATH_LENGTH,
        pattern=_FACTORY_PATH,
        description="Path under one of FL Studio's %FL...% folder variables, '/'-separated.",
    ),
    AfterValidator(_no_reserved_windows_names),
]

InstrumentId = Annotated[str, Field(pattern=r"^instrument:" + _INDEX + "$")]
AudioSourceId = Annotated[str, Field(pattern=r"^audio:" + _INDEX + "$")]
AutomationId = Annotated[str, Field(pattern=r"^automation:" + _INDEX + "$")]
PatternId = Annotated[str, Field(pattern=r"^pattern:" + _INDEX + "$")]
ArrangementId = Annotated[str, Field(pattern=r"^arrangement:" + _INDEX + "$")]
InsertId = Annotated[str, Field(pattern=r"^insert:" + _INSERT_NUMBER + "$")]
TrackId = Annotated[str, Field(pattern=r"^track:" + _TRACK_NUMBER + "$")]
InsertNumber = Annotated[int, Field(ge=0, le=MAX_INSERTS - 1)]
EffectSlotIndex = Annotated[
    int, Field(ge=0, le=MAX_EFFECT_SLOTS - 1, description="0-based; FL Studio shows it as index + 1.")
]


# ── Extensions ──


def _few_namespaces(value: Any) -> Any:
    # Counted before the namespaces are validated: Pydantic checks a dict's
    # max_length only after validating every entry, one error per bad entry.
    if isinstance(value, dict) and len(value) > MAX_EXTENSION_NAMESPACES:
        raise PydanticCustomError(
            "too_long",
            "extensions hold at most {max_namespaces} namespaces.",
            {"max_namespaces": MAX_EXTENSION_NAMESPACES},
        )
    return value


def _check_extensions(extensions: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    if not extensions:
        return extensions
    if _depth_exceeds(extensions, MAX_EXTENSIONS_DEPTH):
        raise PydanticCustomError(
            "extensions_too_deep", "extensions nest at most {max_depth} levels deep.", {"max_depth": MAX_EXTENSIONS_DEPTH}
        )
    try:
        encoded = json.dumps(extensions, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    except (TypeError, ValueError):
        raise PydanticCustomError("extensions_invalid", "extensions hold JSON values only.") from None
    if len(encoded) > MAX_EXTENSIONS_BYTES:
        raise PydanticCustomError(
            "extensions_too_large", "extensions hold at most {max_bytes} bytes.", {"max_bytes": MAX_EXTENSIONS_BYTES}
        )
    return extensions


def _depth_exceeds(value: Any, max_depth: int) -> bool:
    # Iterative, so a deep value cannot exhaust the stack.
    stack: list[tuple[Any, int]] = [(value, 1)]
    while stack:
        node, depth = stack.pop()
        children = node.values() if isinstance(node, dict) else node if isinstance(node, list) else None
        if children is None:
            continue
        if depth > max_depth:
            return True
        stack.extend((child, depth + 1) for child in children)
    return False


Extensions = Annotated[
    dict[Annotated[str, Field(pattern=_NAMESPACE)], dict[str, Any]],
    Field(max_length=MAX_EXTENSION_NAMESPACES),
    BeforeValidator(_few_namespaces),
    AfterValidator(_check_extensions),
    WithJsonSchema(
        {
            "type": "object",
            "description": (
                "What the core objects don't model, as free-form JSON per namespace (fl_studio). "
                f"At most {MAX_EXTENSIONS_DEPTH} levels deep (this object is level 1) "
                f"and {MAX_EXTENSIONS_BYTES} bytes as compact UTF-8 JSON; those two caps are checked by the backend."
            ),
            "propertyNames": {"pattern": _NAMESPACE},
            "additionalProperties": {"type": "object"},
            "maxProperties": MAX_EXTENSION_NAMESPACES,
        }
    ),
]


def _extensions_field() -> Any:
    return Field(default_factory=dict)


# ── Base and tagged unions ──


class _Strict(BaseModel):
    model_config = ConfigDict(extra="forbid", strict=True, allow_inf_nan=False, frozen=True)

    @model_validator(mode="before")
    @classmethod
    def _report_first_unknown_fields_only(cls, data: Any) -> Any:
        # Every unknown field is an error; past the first few, leave them out of
        # what Pydantic sees. The object is refused all the same.
        if not isinstance(data, dict) or len(data) <= MAX_UNKNOWN_FIELDS_REPORTED:
            return data
        known = _field_keys(cls)
        unknown = [key for key in data if key not in known]
        if len(unknown) <= MAX_UNKNOWN_FIELDS_REPORTED:
            return data
        reported = frozenset(unknown[:MAX_UNKNOWN_FIELDS_REPORTED])
        return {key: value for key, value in data.items() if key in known or key in reported}


@cache
def _field_keys(model: type[BaseModel]) -> frozenset[str]:
    """The keys an object of ``model`` takes: its fields' aliases, or their names."""
    return frozenset(field.alias or name for name, field in model.model_fields.items())


def _tagged(key: str, tags: tuple[str, ...]) -> Discriminator:
    """The Discriminator/Tag pattern of schemas.VersionManifest, with an error that never echoes the tag."""

    def tag_of(value: Any) -> Optional[str]:
        tag = value.get(key) if isinstance(value, dict) else getattr(value, key, None)
        return tag if isinstance(tag, str) else None

    return _discriminator(tag_of, key, tags)


def _discriminator(tag_of: Callable[[Any], Optional[str]], key: str, tags: tuple[str, ...]) -> Discriminator:
    expected = ", ".join(f"'{tag}'" for tag in tags)
    return Discriminator(
        tag_of,
        custom_error_type=f"invalid_{key}",
        custom_error_message=f"Expected an object whose '{key}' is one of: {expected}.",
    )


class UnknownValue(_Strict):
    """A value the reader could not read. The document lists it in unsupported[] too."""

    status: Literal["unknown"]


# ── Document header ──


class Generator(_Strict):
    """What wrote the document."""

    name: str = Field(pattern=r"^[a-z0-9][a-z0-9._-]{0,63}$")
    version: str = Field(pattern=r"^[\x21-\x7e]{1,64}$")
    pyflp: Optional[str] = Field(pattern=r"^[0-9a-f]{7,40}$", description="The PyFLP commit the reader used.")


class Source(_Strict):
    """The project file the model was read from."""

    daw: Literal["fl_studio"]
    daw_version: Optional[str] = Field(
        pattern=r"^[0-9]{1,4}(?:\.[0-9]{1,6}){0,4}$", description="The version that saved the file, e.g. 25.2.4.4960."
    )
    ppq: int = Field(ge=1, le=65535, description="Ticks per quarter note of every *_ticks value.")


class ProjectMetadata(_Strict):
    """The project's own metadata. Comments, URL, licensee, paths and dates are never stored."""

    title: Optional[Name]
    artists: Optional[Name]
    genre: Optional[Name]


class TimeSignature(_Strict):
    numerator: int = Field(ge=1, le=99)
    denominator: Annotated[Literal[1, 2, 4, 8, 16, 32], BeforeValidator(_strict_int_literal)]


# ── Shared parts ──


class _Volume(_Strict):
    # Every volume has both a fader position and a value in dB; what it belongs to sets the fader's range.
    fader: float
    db: Literal[None] = Field(
        description="Volume in dB: always null in 0.1.0, until the fader-to-dB curve is calibrated (M2)."
    )


class InstrumentVolume(_Volume):
    """An instrument's volume (FL Studio: a channel's), up to FL Studio's 100%."""

    fader: InstrumentFader


class InsertVolume(_Volume):
    """A mixer insert's volume, up to FL Studio's 125%."""

    fader: InsertFader


class ProjectSample(_Strict):
    """A sample inside the project folder: one of the version's assets."""

    location: Literal["project"]
    asset_path: AssetPath


class FactorySample(_Strict):
    """A sample shipped with FL Studio, under its %FL...% folder variable."""

    location: Literal["factory"]
    path: FactoryPath


class ExternalSample(_Strict):
    """A sample outside the project folder: only its file name is kept, never its folder."""

    location: Literal["external"]
    file_name: FileName


Sample = Annotated[
    Union[
        Annotated[ProjectSample, Tag("project")],
        Annotated[FactorySample, Tag("factory")],
        Annotated[ExternalSample, Tag("external")],
    ],
    _tagged("location", ("project", "factory", "external")),
]


class KnownInsert(_Strict):
    status: Literal["known"]
    insert_id: InsertId


InsertRef = Annotated[
    Union[Annotated[KnownInsert, Tag("known")], Annotated[UnknownValue, Tag("unknown")]],
    _tagged("status", ("known", "unknown")),
]


class PluginState(_Strict):
    """The plugin's saved state, by hash only: its bytes stay in the project file."""

    sha256: Sha256
    size_bytes: int = Field(ge=0, le=MAX_SIZE_BYTES)


class Plugin(_Strict):
    """A native or third-party instrument or effect inside the project."""

    format: Literal["fl_native", "vst2", "vst3"]
    name: RequiredName
    vendor: Optional[Name]
    plugin_id: Optional[str] = Field(pattern=r"^[\x20-\x7e]{1,128}$")
    state: Optional[PluginState]


# ── Instruments and audio sources ──


class _Instrument(_Strict):
    # Every instrument has both "sample" and "plugin"; its kind says which one may be set.
    id: InstrumentId
    kind: str
    name: Name
    color: Optional[Color]
    enabled: bool
    volume: InstrumentVolume
    pan: Pan
    insert: InsertRef
    sample: Optional[Sample]
    plugin: Optional[Plugin]
    extensions: Extensions = _extensions_field()


class SamplerInstrument(_Instrument):
    """An instrument that plays a sample (FL Studio: a sampler channel). Its sample is null when none is loaded."""

    kind: Literal["sampler"]
    plugin: None


class PluginInstrument(_Instrument):
    """An instrument played by a plugin (FL Studio: a generator channel)."""

    kind: Literal["plugin"]
    sample: None
    plugin: Plugin


Instrument = Annotated[
    Union[Annotated[SamplerInstrument, Tag("sampler")], Annotated[PluginInstrument, Tag("plugin")]],
    _tagged("kind", ("sampler", "plugin")),
]


class AudioSource(_Strict):
    """Audio that audio clips play (FL Studio: an audio clip channel)."""

    id: AudioSourceId
    name: Name
    sample: Optional[Sample]
    insert: InsertRef
    extensions: Extensions = _extensions_field()


# ── Patterns ──


class Note(_Strict):
    instrument_id: InstrumentId
    pitch: int = Field(ge=0, le=131, description="FL Studio's key number, as MIDI: 60 is middle C.")
    start_ticks: Ticks
    length_ticks: Ticks
    velocity: UnitInterval
    pan: Pan
    fine_pitch_cents: int = Field(ge=-1200, le=1200, multiple_of=10)
    extensions: Extensions = _extensions_field()


class Pattern(_Strict):
    id: PatternId
    name: Name
    length_ticks: Ticks
    notes: Items[Note] = Field(max_length=MAX_NOTES)
    extensions: Extensions = _extensions_field()


# ── Arrangements ──


class _Clip(_Strict):
    kind: str
    source_id: str
    start_ticks: Ticks
    length_ticks: Ticks
    offset_start_ticks: Optional[Ticks]
    offset_end_ticks: Optional[Ticks]
    muted: bool
    extensions: Extensions = _extensions_field()


class PatternClip(_Clip):
    kind: Literal["pattern"]
    source_id: PatternId


class AudioClip(_Clip):
    kind: Literal["audio"]
    source_id: AudioSourceId


class AutomationClip(_Clip):
    kind: Literal["automation"]
    source_id: AutomationId


Clip = Annotated[
    Union[
        Annotated[PatternClip, Tag("pattern")],
        Annotated[AudioClip, Tag("audio")],
        Annotated[AutomationClip, Tag("automation")],
    ],
    _tagged("kind", ("pattern", "audio", "automation")),
]


def _id_must_match_number(model: Any, prefix: str) -> None:
    if model.id != f"{prefix}:{model.number}":
        raise PydanticCustomError("id_number_mismatch", f"The id must be '{prefix}:' followed by the number.")


class Track(_Strict):
    """One lane of an arrangement. A track that isn't listed is empty, not removed."""

    id: TrackId
    number: int = Field(ge=1, le=MAX_TRACKS)
    name: Name
    enabled: bool
    clips: Items[Clip] = Field(max_length=MAX_CLIPS)
    extensions: Extensions = _extensions_field()

    @model_validator(mode="after")
    def _id_matches_number(self) -> "Track":
        _id_must_match_number(self, "track")
        return self


class Marker(_Strict):
    position_ticks: Ticks
    name: Name
    extensions: Extensions = _extensions_field()


class Arrangement(_Strict):
    id: ArrangementId
    name: Name
    markers: Items[Marker] = Field(max_length=MAX_MARKERS)
    tracks: Items[Track] = Field(max_length=MAX_TRACKS)
    extensions: Extensions = _extensions_field()


# ── Mixer ──


class Send(_Strict):
    to_insert_id: InsertId
    level: UnitInterval


class EffectSlot(_Strict):
    index: EffectSlotIndex
    enabled: bool
    dry_wet: UnitInterval
    plugin: Plugin
    extensions: Extensions = _extensions_field()


class Insert(_Strict):
    """One mixer insert; insert 0 is the Master insert. An insert that isn't listed has default values."""

    id: InsertId
    number: InsertNumber
    name: Name
    enabled: bool
    volume: InsertVolume
    pan: Pan
    stereo_separation: float = Field(ge=-1.0, le=1.0, description="+1 fully separated, 0 unchanged, -1 merged (mono).")
    sends: Items[Send] = Field(max_length=MAX_INSERTS - 1)
    effect_slots: Items[EffectSlot] = Field(max_length=MAX_EFFECT_SLOTS)
    extensions: Extensions = _extensions_field()

    @model_validator(mode="after")
    def _id_matches_number(self) -> "Insert":
        _id_must_match_number(self, "insert")
        return self


class Mixer(_Strict):
    inserts: Items[Insert] = Field(max_length=MAX_INSERTS)
    extensions: Extensions = _extensions_field()


# ── Automation ──


class TempoTarget(_Strict):
    kind: Literal["tempo"]


class InstrumentTarget(_Strict):
    kind: Literal["instrument"]
    instrument_id: InstrumentId
    parameter: Literal["volume", "pan"]


class InsertTarget(_Strict):
    kind: Literal["insert"]
    insert_id: InsertId
    parameter: Literal["volume", "pan", "stereo_separation"]


class EffectSlotTarget(_Strict):
    kind: Literal["effect_slot"]
    insert_id: InsertId
    slot_index: EffectSlotIndex
    parameter: Literal["enabled", "dry_wet"]


_TARGET_TAGS = ("tempo", "instrument", "insert", "effect_slot", "unknown")


def _target_tag(value: Any) -> Optional[str]:
    # Known targets carry a kind; an unknown one is {"status": "unknown"} like every unknown value.
    if isinstance(value, dict):
        kind, status = value.get("kind"), value.get("status")
    else:
        kind, status = getattr(value, "kind", None), getattr(value, "status", None)
    if isinstance(kind, str):
        return kind
    return "unknown" if status == "unknown" else None


AutomationTarget = Annotated[
    Union[
        Annotated[TempoTarget, Tag("tempo")],
        Annotated[InstrumentTarget, Tag("instrument")],
        Annotated[InsertTarget, Tag("insert")],
        Annotated[EffectSlotTarget, Tag("effect_slot")],
        Annotated[UnknownValue, Tag("unknown")],
    ],
    _discriminator(_target_tag, "kind", _TARGET_TAGS),
]


class AutomationPoint(_Strict):
    position_ticks: Ticks = Field(description="From the start of the automation clip.")
    value: UnitInterval = Field(description="The parameter's normalized value.")
    tension: float = Field(ge=-1.0, le=1.0)


class Automation(_Strict):
    """A parameter's change over time, played by automation clips. A null target is linked to nothing."""

    id: AutomationId
    name: Name
    target: Optional[AutomationTarget]
    points: Items[AutomationPoint] = Field(max_length=MAX_AUTOMATION_POINTS)
    extensions: Extensions = _extensions_field()


# ── Reports ──


class UnsupportedEntry(_Strict):
    """Something the project file holds that the model doesn't: at ``path`` and below."""

    path: JsonPointer
    reason: Text


class ModelWarning(_Strict):
    """Something read with a loss, e.g. text with invalid characters replaced."""

    path: JsonPointer
    message: Text


# ── The document ──


class ProjectModel(_Strict):
    """A project's musical content, DAW-neutral (StemHub project model 0.1.0)."""

    model_config = ConfigDict(serialize_by_alias=True, validate_by_alias=True, validate_by_name=False)

    schema_name: Literal["stemhub.project-model"] = Field(alias="schema")
    schema_version: Literal["0.1.0"]
    generator: Generator
    source: Source
    project: ProjectMetadata
    tempo_bpm: Optional[float] = Field(
        gt=0.0, le=999.0, description="Null only when the project file stores no tempo (listed in unsupported[])."
    )
    time_signature: TimeSignature
    instruments: Items[Instrument] = Field(max_length=MAX_INSTRUMENTS)
    audio_sources: Items[AudioSource] = Field(max_length=MAX_AUDIO_SOURCES)
    patterns: Items[Pattern] = Field(max_length=MAX_PATTERNS)
    arrangements: Items[Arrangement] = Field(min_length=1, max_length=MAX_ARRANGEMENTS)
    current_arrangement_id: ArrangementId
    mixer: Mixer
    automation: Items[Automation] = Field(max_length=MAX_AUTOMATION)
    unsupported: Items[UnsupportedEntry] = Field(max_length=MAX_REPORTS)
    warnings: Items[ModelWarning] = Field(max_length=MAX_REPORTS)
    extensions: Extensions = _extensions_field()

    @model_validator(mode="after")
    def _within_total_caps(self) -> "ProjectModel":
        if sum(len(pattern.notes) for pattern in self.patterns) > MAX_NOTES:
            raise PydanticCustomError(
                "too_many_notes", "A project model holds at most {max_notes} notes.", {"max_notes": MAX_NOTES}
            )
        clips = sum(len(track.clips) for arrangement in self.arrangements for track in arrangement.tracks)
        if clips > MAX_CLIPS:
            raise PydanticCustomError(
                "too_many_clips", "A project model holds at most {max_clips} clips.", {"max_clips": MAX_CLIPS}
            )
        return self


# ── JSON Schema ──


class _ProjectModelJsonSchema(GenerateJsonSchema):
    """Adds the dialect and the schema's identity. The $id is a URN: no web domain is implied."""

    def generate(self, schema: Any, mode: Any = "validation") -> dict[str, Any]:
        generated = super().generate(schema, mode=mode)
        return {"$schema": JSON_SCHEMA_DIALECT, "$id": SCHEMA_ID, **generated}


def generate_json_schema() -> dict[str, Any]:
    return ProjectModel.model_json_schema(schema_generator=_ProjectModelJsonSchema, mode="validation")


def render_json_schema() -> str:
    """The committed file's exact text."""
    return json.dumps(generate_json_schema(), indent=2, ensure_ascii=True) + "\n"
