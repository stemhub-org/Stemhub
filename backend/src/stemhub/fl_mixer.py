"""The FL Studio mixer, read from a project file (.flp) with PyFLP, and its diff.

Inserts use FL Studio's numbering (Master = 0). Effect slots keep their 0-based
index; user-facing messages show them as FL Studio does (index + 1).
"""
from __future__ import annotations

import hashlib
import importlib
from dataclasses import dataclass
from typing import Any

from stemhub.dependency_guard import ensure_pyflp_available
from stemhub.storage import StorageService


# PyFLP numbers inserts from -1 (Master); FlMixer uses FL Studio's numbering, where Master is 0.
PYFLP_TO_FL_INSERT_OFFSET = 1
MASTER_INSERT_INDEX = 0


class MixerReadError(RuntimeError):
    """Raised when a version's project file cannot give a valid FL Studio mixer."""


@dataclass(frozen=True)
class FlEffectSlot:
    index: int
    name: str | None
    internal_name: str | None
    enabled: bool | None
    # Raw PyFLP value (slot "mix"); unit normalization is #170's job.
    dry_wet: int | None
    # The third-party or native plugin loaded in the slot.
    plugin_name: str | None


@dataclass(frozen=True)
class FlMixerInsert:
    index: int
    name: str | None
    enabled: bool | None
    volume: int | None
    pan: int | None
    slots: tuple[FlEffectSlot, ...]


@dataclass(frozen=True)
class FlMixer:
    inserts: tuple[FlMixerInsert, ...]
    flp_sha256: str | None = None
    flp_size_bytes: int | None = None
    mixer_supported: bool = True
    parse_error: str | None = None


@dataclass(frozen=True)
class MixerDiffChange:
    type: str
    # None for project-level changes (project_file_changed).
    insert_index: int | None
    insert_name: str | None
    slot_index: int | None
    before: Any
    after: Any
    message: str


@dataclass(frozen=True)
class MixerDiffSummary:
    total_changes: int
    inserts_changed: int
    slots_changed: int
    parameter_changes: int


@dataclass(frozen=True)
class MixerDiffResult:
    summary: MixerDiffSummary
    changes: tuple[MixerDiffChange, ...]


def load_fl_mixer(
    *,
    storage_uri: str,
    storage: StorageService,
) -> FlMixer:
    """Read the mixer of a stored FL Studio project file.

    The manifest's project file is stored as the raw .flp, so it is parsed
    as is. See docs/content-addressed-storage.md.
    """
    if not storage_uri:
        raise MixerReadError("This version has no project file to read.")

    ensure_pyflp_available()
    pyflp = importlib.import_module("pyflp")

    flp_path = storage.resolve_blob_path(storage_uri)
    flp_bytes = flp_path.read_bytes()
    try:
        project = pyflp.parse(flp_path)
    except Exception as exc:  # pragma: no cover - parser internals vary by FLP shape
        return FlMixer(
            inserts=(),
            flp_sha256=hashlib.sha256(flp_bytes).hexdigest(),
            flp_size_bytes=len(flp_bytes),
            mixer_supported=False,
            parse_error=str(exc),
        )

    return parse_fl_mixer(
        project,
        flp_sha256=hashlib.sha256(flp_bytes).hexdigest(),
        flp_size_bytes=len(flp_bytes),
    )


def parse_fl_mixer(
    project: Any,
    *,
    flp_sha256: str | None = None,
    flp_size_bytes: int | None = None,
) -> FlMixer:
    inserts: list[FlMixerInsert] = []

    for insert in getattr(project, "mixer", []):
        iid = _safe_model_attr(insert, "iid")
        if iid is None:
            continue

        slots = _parse_effect_slots(insert)
        inserts.append(
            FlMixerInsert(
                index=int(iid) + PYFLP_TO_FL_INSERT_OFFSET,
                name=_normalize_optional_text(_safe_model_attr(insert, "name")),
                enabled=_coerce_optional_bool(_safe_model_attr(insert, "enabled")),
                volume=_coerce_optional_int(_safe_model_attr(insert, "volume")),
                pan=_coerce_optional_int(_safe_model_attr(insert, "pan")),
                slots=slots,
            )
        )

    inserts.sort(key=lambda item: item.index)
    return FlMixer(
        inserts=tuple(inserts),
        flp_sha256=flp_sha256,
        flp_size_bytes=flp_size_bytes,
        mixer_supported=len(inserts) > 0,
    )


def diff_fl_mixers(
    base_mixer: FlMixer,
    target_mixer: FlMixer,
) -> MixerDiffResult:
    changes: list[MixerDiffChange] = []
    base_inserts = {insert.index: insert for insert in base_mixer.inserts}
    target_inserts = {insert.index: insert for insert in target_mixer.inserts}

    for insert_index in sorted(set(base_inserts) | set(target_inserts)):
        base_insert = base_inserts.get(insert_index)
        target_insert = target_inserts.get(insert_index)

        if base_insert is None and target_insert is not None:
            changes.append(
                MixerDiffChange(
                    type="insert_added",
                    insert_index=insert_index,
                    insert_name=target_insert.name,
                    slot_index=None,
                    before=None,
                    after=_serialize_insert(target_insert),
                    message=f'{_format_insert_label(insert_index, target_insert.name)} added',
                )
            )
            continue

        if base_insert is not None and target_insert is None:
            changes.append(
                MixerDiffChange(
                    type="insert_removed",
                    insert_index=insert_index,
                    insert_name=base_insert.name,
                    slot_index=None,
                    before=_serialize_insert(base_insert),
                    after=None,
                    message=f'{_format_insert_label(insert_index, base_insert.name)} removed',
                )
            )
            continue

        if base_insert is None or target_insert is None:
            continue

        insert_name = target_insert.name or base_insert.name

        if base_insert.name != target_insert.name:
            changes.append(
                MixerDiffChange(
                    type="insert_renamed",
                    insert_index=insert_index,
                    insert_name=insert_name,
                    slot_index=None,
                    before=base_insert.name,
                    after=target_insert.name,
                    message=(
                        f'{_format_insert_label(insert_index, base_insert.name)} renamed: '
                        f'{_format_text_value(base_insert.name)} -> {_format_text_value(target_insert.name)}'
                    ),
                )
            )

        if base_insert.enabled != target_insert.enabled:
            changes.append(
                MixerDiffChange(
                    type="insert_enabled_changed",
                    insert_index=insert_index,
                    insert_name=insert_name,
                    slot_index=None,
                    before=base_insert.enabled,
                    after=target_insert.enabled,
                    message=(
                        f'{_format_insert_label(insert_index, insert_name)} enabled changed: '
                        f'{_format_bool_value(base_insert.enabled)} -> {_format_bool_value(target_insert.enabled)}'
                    ),
                )
            )

        if base_insert.volume != target_insert.volume:
            changes.append(
                MixerDiffChange(
                    type="insert_volume_changed",
                    insert_index=insert_index,
                    insert_name=insert_name,
                    slot_index=None,
                    before=base_insert.volume,
                    after=target_insert.volume,
                    message=(
                        f'{_format_insert_label(insert_index, insert_name)} volume changed: '
                        f'{_format_scalar_value(base_insert.volume)} -> {_format_scalar_value(target_insert.volume)}'
                    ),
                )
            )

        if base_insert.pan != target_insert.pan:
            changes.append(
                MixerDiffChange(
                    type="insert_pan_changed",
                    insert_index=insert_index,
                    insert_name=insert_name,
                    slot_index=None,
                    before=base_insert.pan,
                    after=target_insert.pan,
                    message=(
                        f'{_format_insert_label(insert_index, insert_name)} pan changed: '
                        f'{_format_scalar_value(base_insert.pan)} -> {_format_scalar_value(target_insert.pan)}'
                    ),
                )
            )

        changes.extend(_diff_effect_slots(base_insert, target_insert))

    insert_changes = {change.insert_index for change in changes if change.type.startswith("insert_")}
    slot_changes = {
        (change.insert_index, change.slot_index)
        for change in changes
        if change.type.startswith("slot_") and change.slot_index is not None
    }
    parameter_change_types = {
        "insert_enabled_changed",
        "insert_volume_changed",
        "insert_pan_changed",
        "slot_enabled_changed",
        "slot_dry_wet_changed",
    }

    summary = MixerDiffSummary(
        total_changes=len(changes),
        inserts_changed=len(insert_changes),
        slots_changed=len(slot_changes),
        parameter_changes=sum(1 for change in changes if change.type in parameter_change_types),
    )

    if (
        summary.total_changes == 0
        and (not base_mixer.mixer_supported or not target_mixer.mixer_supported)
        and base_mixer.flp_sha256 is not None
        and target_mixer.flp_sha256 is not None
        and base_mixer.flp_sha256 != target_mixer.flp_sha256
    ):
        changes.append(
            MixerDiffChange(
                type="project_file_changed",
                insert_index=None,
                insert_name=None,
                slot_index=None,
                before={
                    "sha256": base_mixer.flp_sha256,
                    "size_bytes": base_mixer.flp_size_bytes,
                },
                after={
                    "sha256": target_mixer.flp_sha256,
                    "size_bytes": target_mixer.flp_size_bytes,
                },
                message=(
                    "The FL Studio project file changed, but its mixer could not be read, "
                    "so no mixer diff is available."
                ),
            )
        )
        summary = MixerDiffSummary(
            total_changes=1,
            inserts_changed=0,
            slots_changed=0,
            parameter_changes=0,
        )

    return MixerDiffResult(summary=summary, changes=tuple(changes))


def _parse_effect_slots(insert: Any) -> tuple[FlEffectSlot, ...]:
    slots: list[FlEffectSlot] = []

    try:
        for slot in insert:
            name = _normalize_optional_text(_safe_model_attr(slot, "name"))
            internal_name = _normalize_optional_text(_safe_model_attr(slot, "internal_name"))
            plugin_name = _resolve_slot_plugin_name(slot, name=name, internal_name=internal_name)
            if not any((name, internal_name, plugin_name)):
                continue

            slot_index = _safe_model_attr(slot, "index")
            if slot_index is None:
                continue

            slots.append(
                FlEffectSlot(
                    index=int(slot_index),
                    name=name,
                    internal_name=internal_name,
                    enabled=_coerce_optional_bool(_safe_model_attr(slot, "enabled")),
                    dry_wet=_coerce_optional_int(_safe_model_attr(slot, "mix")),
                    plugin_name=plugin_name,
                )
            )
    except Exception:
        return tuple(slots)

    slots.sort(key=lambda item: item.index)
    return tuple(slots)


def _diff_effect_slots(
    base_insert: FlMixerInsert,
    target_insert: FlMixerInsert,
) -> list[MixerDiffChange]:
    changes: list[MixerDiffChange] = []
    base_slots = {slot.index: slot for slot in base_insert.slots}
    target_slots = {slot.index: slot for slot in target_insert.slots}
    insert_name = target_insert.name or base_insert.name

    for slot_index in sorted(set(base_slots) | set(target_slots)):
        base_slot = base_slots.get(slot_index)
        target_slot = target_slots.get(slot_index)

        if base_slot is None and target_slot is not None:
            changes.append(
                MixerDiffChange(
                    type="slot_added",
                    insert_index=target_insert.index,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=None,
                    after=_serialize_slot(target_slot),
                    message=(
                        f'{_format_slot_label(target_insert.index, insert_name, slot_index)} added: '
                        f'{_format_text_value(target_slot.plugin_name or target_slot.name)}'
                    ),
                )
            )
            continue

        if base_slot is not None and target_slot is None:
            changes.append(
                MixerDiffChange(
                    type="slot_removed",
                    insert_index=base_insert.index,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=_serialize_slot(base_slot),
                    after=None,
                    message=(
                        f'{_format_slot_label(base_insert.index, insert_name, slot_index)} removed: '
                        f'{_format_text_value(base_slot.plugin_name or base_slot.name)}'
                    ),
                )
            )
            continue

        if base_slot is None or target_slot is None:
            continue

        if base_slot.plugin_name != target_slot.plugin_name:
            changes.append(
                MixerDiffChange(
                    type="slot_plugin_changed",
                    insert_index=target_insert.index,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=base_slot.plugin_name,
                    after=target_slot.plugin_name,
                    message=(
                        f'{_format_slot_label(target_insert.index, insert_name, slot_index)} plugin changed: '
                        f'{_format_text_value(base_slot.plugin_name)} -> {_format_text_value(target_slot.plugin_name)}'
                    ),
                )
            )

        if base_slot.enabled != target_slot.enabled:
            changes.append(
                MixerDiffChange(
                    type="slot_enabled_changed",
                    insert_index=target_insert.index,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=base_slot.enabled,
                    after=target_slot.enabled,
                    message=(
                        f'{_format_slot_label(target_insert.index, insert_name, slot_index)} enabled changed: '
                        f'{_format_bool_value(base_slot.enabled)} -> {_format_bool_value(target_slot.enabled)}'
                    ),
                )
            )

        if base_slot.dry_wet != target_slot.dry_wet:
            changes.append(
                MixerDiffChange(
                    type="slot_dry_wet_changed",
                    insert_index=target_insert.index,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=base_slot.dry_wet,
                    after=target_slot.dry_wet,
                    message=(
                        f'{_format_slot_label(target_insert.index, insert_name, slot_index)} dry/wet changed: '
                        f'{_format_scalar_value(base_slot.dry_wet)} -> {_format_scalar_value(target_slot.dry_wet)}'
                    ),
                )
            )

    return changes


def _resolve_slot_plugin_name(
    slot: Any,
    *,
    name: str | None,
    internal_name: str | None,
) -> str | None:
    plugin = _safe_model_attr(slot, "plugin")
    plugin_type = type(plugin).__name__ if plugin is not None else None
    plugin_type = _normalize_optional_text(plugin_type)

    if internal_name and internal_name.lower() != "fruity wrapper":
        return internal_name
    if name:
        return name
    if internal_name:
        return internal_name
    return plugin_type


def _normalize_optional_text(value: Any) -> str | None:
    if value is None:
        return None

    text = str(value).strip()
    return text or None


def _safe_model_attr(obj: Any, name: str) -> Any:
    try:
        return getattr(obj, name, None)
    except Exception:
        return None


def _coerce_optional_int(value: Any) -> int | None:
    if value is None:
        return None
    return int(value)


def _coerce_optional_bool(value: Any) -> bool | None:
    if value is None:
        return None
    return bool(value)


def _serialize_insert(insert: FlMixerInsert) -> dict[str, Any]:
    return {
        "index": insert.index,
        "name": insert.name,
        "enabled": insert.enabled,
        "volume": insert.volume,
        "pan": insert.pan,
        "slots": [_serialize_slot(slot) for slot in insert.slots],
    }


def _serialize_slot(slot: FlEffectSlot) -> dict[str, Any]:
    return {
        "index": slot.index,
        "name": slot.name,
        "internal_name": slot.internal_name,
        "enabled": slot.enabled,
        "dry_wet": slot.dry_wet,
        "plugin_name": slot.plugin_name,
    }


def _format_insert_label(insert_index: int, insert_name: str | None) -> str:
    label = "Master" if insert_index == MASTER_INSERT_INDEX else f"Insert {insert_index}"
    if insert_name and insert_name != label:
        return f'{label} "{insert_name}"'
    return label


def _format_slot_label(insert_index: int, insert_name: str | None, slot_index: int) -> str:
    # FL Studio shows effect slots from 1; slot_index is 0-based.
    return f"{_format_insert_label(insert_index, insert_name)} effect slot {slot_index + 1}"


def _format_text_value(value: str | None) -> str:
    if value is None:
        return "None"
    return f'"{value}"'


def _format_bool_value(value: bool | None) -> str:
    if value is None:
        return "None"
    return "on" if value else "off"


def _format_scalar_value(value: int | None) -> str:
    if value is None:
        return "None"
    return str(value)
