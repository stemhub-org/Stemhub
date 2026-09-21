from __future__ import annotations

import hashlib
import importlib
import logging
from dataclasses import dataclass
from typing import Any

from stemhub.dependency_guard import ensure_pyflp_available
from stemhub.storage import StorageService

logger = logging.getLogger(__name__)


class MixerSnapshotError(RuntimeError):
    """Raised when a version artifact cannot produce a valid FL Studio mixer snapshot."""


@dataclass(frozen=True)
class MixerSlotSnapshot:
    index: int
    name: str | None
    internal_name: str | None
    enabled: bool | None
    mix: int | None
    plugin_key: str | None


@dataclass(frozen=True)
class MixerInsertSnapshot:
    iid: int
    name: str | None
    enabled: bool | None
    volume: int | None
    pan: int | None
    slots: tuple[MixerSlotSnapshot, ...]


@dataclass(frozen=True)
class MixerProjectSnapshot:
    inserts: tuple[MixerInsertSnapshot, ...]
    flp_sha256: str | None = None
    flp_size_bytes: int | None = None
    mixer_supported: bool = True
    parse_error: str | None = None


@dataclass(frozen=True)
class MixerDiffChange:
    type: str
    insert_iid: int
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


def load_fl_studio_mixer_snapshot(
    *,
    storage_uri: str,
    storage: StorageService,
) -> MixerProjectSnapshot:
    """Load a mixer snapshot from a content-addressed .flp blob.

    The manifest_json.project_file blob is the raw .flp — no bundle/zip
    unpacking needed. See docs/content-addressed-storage.md.
    """
    if not storage_uri:
        raise MixerSnapshotError("Version has no project-file blob to read.")

    ensure_pyflp_available()
    pyflp = importlib.import_module("pyflp")

    flp_path = storage.resolve_blob_path(storage_uri)
    flp_bytes = flp_path.read_bytes()
    try:
        project = pyflp.parse(flp_path)
    except Exception as exc:  # pragma: no cover - parser internals vary by FLP shape
        return MixerProjectSnapshot(
            inserts=(),
            flp_sha256=hashlib.sha256(flp_bytes).hexdigest(),
            flp_size_bytes=len(flp_bytes),
            mixer_supported=False,
            parse_error=str(exc),
        )

    return build_mixer_snapshot(
        project,
        flp_sha256=hashlib.sha256(flp_bytes).hexdigest(),
        flp_size_bytes=len(flp_bytes),
    )


def build_mixer_snapshot(
    project: Any,
    *,
    flp_sha256: str | None = None,
    flp_size_bytes: int | None = None,
) -> MixerProjectSnapshot:
    inserts: list[MixerInsertSnapshot] = []

    for insert in getattr(project, "mixer", []):
        iid = _safe_model_attr(insert, "iid")
        if iid is None or iid == -1:
            continue

        slots = _build_slot_snapshots(insert)
        inserts.append(
            MixerInsertSnapshot(
                iid=int(iid),
                name=_normalize_optional_text(_safe_model_attr(insert, "name")),
                enabled=_coerce_optional_bool(_safe_model_attr(insert, "enabled")),
                volume=_coerce_optional_int(_safe_model_attr(insert, "volume")),
                pan=_coerce_optional_int(_safe_model_attr(insert, "pan")),
                slots=slots,
            )
        )

    inserts.sort(key=lambda item: item.iid)
    return MixerProjectSnapshot(
        inserts=tuple(inserts),
        flp_sha256=flp_sha256,
        flp_size_bytes=flp_size_bytes,
        mixer_supported=len(inserts) > 0,
    )


def diff_mixer_project_snapshots(
    base_snapshot: MixerProjectSnapshot,
    target_snapshot: MixerProjectSnapshot,
) -> MixerDiffResult:
    changes: list[MixerDiffChange] = []
    base_inserts = {insert.iid: insert for insert in base_snapshot.inserts}
    target_inserts = {insert.iid: insert for insert in target_snapshot.inserts}

    for insert_iid in sorted(set(base_inserts) | set(target_inserts)):
        base_insert = base_inserts.get(insert_iid)
        target_insert = target_inserts.get(insert_iid)

        if base_insert is None and target_insert is not None:
            changes.append(
                MixerDiffChange(
                    type="insert_added",
                    insert_iid=insert_iid,
                    insert_name=target_insert.name,
                    slot_index=None,
                    before=None,
                    after=_serialize_insert(target_insert),
                    message=f'{_format_insert_label(insert_iid, target_insert.name)} added',
                )
            )
            continue

        if base_insert is not None and target_insert is None:
            changes.append(
                MixerDiffChange(
                    type="insert_removed",
                    insert_iid=insert_iid,
                    insert_name=base_insert.name,
                    slot_index=None,
                    before=_serialize_insert(base_insert),
                    after=None,
                    message=f'{_format_insert_label(insert_iid, base_insert.name)} removed',
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
                    insert_iid=insert_iid,
                    insert_name=insert_name,
                    slot_index=None,
                    before=base_insert.name,
                    after=target_insert.name,
                    message=(
                        f'{_format_insert_label(insert_iid, base_insert.name)} renamed: '
                        f'{_format_text_value(base_insert.name)} -> {_format_text_value(target_insert.name)}'
                    ),
                )
            )

        if base_insert.enabled != target_insert.enabled:
            changes.append(
                MixerDiffChange(
                    type="insert_enabled_changed",
                    insert_iid=insert_iid,
                    insert_name=insert_name,
                    slot_index=None,
                    before=base_insert.enabled,
                    after=target_insert.enabled,
                    message=(
                        f'{_format_insert_label(insert_iid, insert_name)} enabled changed: '
                        f'{_format_bool_value(base_insert.enabled)} -> {_format_bool_value(target_insert.enabled)}'
                    ),
                )
            )

        if base_insert.volume != target_insert.volume:
            changes.append(
                MixerDiffChange(
                    type="insert_volume_changed",
                    insert_iid=insert_iid,
                    insert_name=insert_name,
                    slot_index=None,
                    before=base_insert.volume,
                    after=target_insert.volume,
                    message=(
                        f'{_format_insert_label(insert_iid, insert_name)} volume changed: '
                        f'{_format_scalar_value(base_insert.volume)} -> {_format_scalar_value(target_insert.volume)}'
                    ),
                )
            )

        if base_insert.pan != target_insert.pan:
            changes.append(
                MixerDiffChange(
                    type="insert_pan_changed",
                    insert_iid=insert_iid,
                    insert_name=insert_name,
                    slot_index=None,
                    before=base_insert.pan,
                    after=target_insert.pan,
                    message=(
                        f'{_format_insert_label(insert_iid, insert_name)} pan changed: '
                        f'{_format_scalar_value(base_insert.pan)} -> {_format_scalar_value(target_insert.pan)}'
                    ),
                )
            )

        changes.extend(_diff_insert_slots(base_insert, target_insert))

    insert_changes = {change.insert_iid for change in changes if change.type.startswith("insert_")}
    slot_changes = {
        (change.insert_iid, change.slot_index)
        for change in changes
        if change.type.startswith("slot_") and change.slot_index is not None
    }
    parameter_change_types = {
        "insert_enabled_changed",
        "insert_volume_changed",
        "insert_pan_changed",
        "slot_enabled_changed",
        "slot_mix_changed",
    }

    summary = MixerDiffSummary(
        total_changes=len(changes),
        inserts_changed=len(insert_changes),
        slots_changed=len(slot_changes),
        parameter_changes=sum(1 for change in changes if change.type in parameter_change_types),
    )

    if (
        summary.total_changes == 0
        and (not base_snapshot.mixer_supported or not target_snapshot.mixer_supported)
        and base_snapshot.flp_sha256 is not None
        and target_snapshot.flp_sha256 is not None
        and base_snapshot.flp_sha256 != target_snapshot.flp_sha256
    ):
        changes.append(
            MixerDiffChange(
                type="project_binary_changed",
                insert_iid=-1,
                insert_name=None,
                slot_index=None,
                before={
                    "flp_sha256": base_snapshot.flp_sha256,
                    "flp_size_bytes": base_snapshot.flp_size_bytes,
                },
                after={
                    "flp_sha256": target_snapshot.flp_sha256,
                    "flp_size_bytes": target_snapshot.flp_size_bytes,
                },
                message=(
                    "FL Studio project binary changed, but mixer semantic extraction is unavailable "
                    "for this snapshot format."
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


def _build_slot_snapshots(insert: Any) -> tuple[MixerSlotSnapshot, ...]:
    slots: list[MixerSlotSnapshot] = []

    try:
        for slot in insert:
            name = _normalize_optional_text(_safe_model_attr(slot, "name"))
            internal_name = _normalize_optional_text(_safe_model_attr(slot, "internal_name"))
            plugin_key = _resolve_slot_plugin_key(slot, name=name, internal_name=internal_name)
            if not any((name, internal_name, plugin_key)):
                continue

            slot_index = _safe_model_attr(slot, "index")
            if slot_index is None:
                continue

            slots.append(
                MixerSlotSnapshot(
                    index=int(slot_index),
                    name=name,
                    internal_name=internal_name,
                    enabled=_coerce_optional_bool(_safe_model_attr(slot, "enabled")),
                    mix=_coerce_optional_int(_safe_model_attr(slot, "mix")),
                    plugin_key=plugin_key,
                )
            )
    except Exception as exc:
        logger.warning(
            "mixer insert slot extraction stopped early: iid=%s slots_collected=%d error=%s",
            _safe_model_attr(insert, "iid"),
            len(slots),
            exc,
        )
        return tuple(slots)

    slots.sort(key=lambda item: item.index)
    return tuple(slots)


def _diff_insert_slots(
    base_insert: MixerInsertSnapshot,
    target_insert: MixerInsertSnapshot,
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
                    insert_iid=target_insert.iid,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=None,
                    after=_serialize_slot(target_slot),
                    message=(
                        f'{_format_slot_label(target_insert.iid, insert_name, slot_index)} added: '
                        f'{_format_text_value(target_slot.plugin_key or target_slot.name)}'
                    ),
                )
            )
            continue

        if base_slot is not None and target_slot is None:
            changes.append(
                MixerDiffChange(
                    type="slot_removed",
                    insert_iid=base_insert.iid,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=_serialize_slot(base_slot),
                    after=None,
                    message=(
                        f'{_format_slot_label(base_insert.iid, insert_name, slot_index)} removed: '
                        f'{_format_text_value(base_slot.plugin_key or base_slot.name)}'
                    ),
                )
            )
            continue

        if base_slot is None or target_slot is None:
            continue

        if base_slot.plugin_key != target_slot.plugin_key:
            changes.append(
                MixerDiffChange(
                    type="slot_plugin_changed",
                    insert_iid=target_insert.iid,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=base_slot.plugin_key,
                    after=target_slot.plugin_key,
                    message=(
                        f'{_format_slot_label(target_insert.iid, insert_name, slot_index)} plugin changed: '
                        f'{_format_text_value(base_slot.plugin_key)} -> {_format_text_value(target_slot.plugin_key)}'
                    ),
                )
            )

        if base_slot.enabled != target_slot.enabled:
            changes.append(
                MixerDiffChange(
                    type="slot_enabled_changed",
                    insert_iid=target_insert.iid,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=base_slot.enabled,
                    after=target_slot.enabled,
                    message=(
                        f'{_format_slot_label(target_insert.iid, insert_name, slot_index)} enabled changed: '
                        f'{_format_bool_value(base_slot.enabled)} -> {_format_bool_value(target_slot.enabled)}'
                    ),
                )
            )

        if base_slot.mix != target_slot.mix:
            changes.append(
                MixerDiffChange(
                    type="slot_mix_changed",
                    insert_iid=target_insert.iid,
                    insert_name=insert_name,
                    slot_index=slot_index,
                    before=base_slot.mix,
                    after=target_slot.mix,
                    message=(
                        f'{_format_slot_label(target_insert.iid, insert_name, slot_index)} mix changed: '
                        f'{_format_scalar_value(base_slot.mix)} -> {_format_scalar_value(target_slot.mix)}'
                    ),
                )
            )

    return changes


def _resolve_slot_plugin_key(
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


def _serialize_insert(insert: MixerInsertSnapshot) -> dict[str, Any]:
    return {
        "iid": insert.iid,
        "name": insert.name,
        "enabled": insert.enabled,
        "volume": insert.volume,
        "pan": insert.pan,
        "slots": [_serialize_slot(slot) for slot in insert.slots],
    }


def _serialize_slot(slot: MixerSlotSnapshot) -> dict[str, Any]:
    return {
        "index": slot.index,
        "name": slot.name,
        "internal_name": slot.internal_name,
        "enabled": slot.enabled,
        "mix": slot.mix,
        "plugin_key": slot.plugin_key,
    }


def _format_insert_label(insert_iid: int, insert_name: str | None) -> str:
    label = f"Insert {insert_iid}"
    if insert_name:
        return f'{label} "{insert_name}"'
    return label


def _format_slot_label(insert_iid: int, insert_name: str | None, slot_index: int) -> str:
    return f"{_format_insert_label(insert_iid, insert_name)} slot {slot_index}"


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


