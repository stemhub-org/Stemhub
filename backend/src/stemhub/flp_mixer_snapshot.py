from __future__ import annotations

import importlib
import tempfile
import zipfile
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator

from stemhub.dependency_guard import ensure_pyflp_available
from stemhub.storage import StorageService


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
    artifact_path: str,
    snapshot_manifest: dict[str, Any] | None,
    storage: StorageService,
) -> MixerProjectSnapshot:
    """Load a mixer-only snapshot from a stored version artifact."""
    if not artifact_path:
        raise MixerSnapshotError("Version artifact path is missing.")

    ensure_pyflp_available()
    pyflp = importlib.import_module("pyflp")

    with _resolved_artifact_file(storage, artifact_path) as snapshot_zip_path:
        with tempfile.TemporaryDirectory(prefix="stemhub-flp-snapshot-") as extract_root:
            flp_path = extract_flp_from_snapshot(
                snapshot_zip_path=snapshot_zip_path,
                snapshot_manifest=snapshot_manifest,
                extract_root=Path(extract_root),
            )
            project = pyflp.parse(flp_path)

    return build_mixer_snapshot(project)


def extract_flp_from_snapshot(
    *,
    snapshot_zip_path: Path,
    snapshot_manifest: dict[str, Any] | None,
    extract_root: Path,
) -> Path:
    """Extract the FL Studio project file from a StemHub snapshot zip."""
    try:
        snapshot_zip = zipfile.ZipFile(snapshot_zip_path)
    except zipfile.BadZipFile as exc:
        raise MixerSnapshotError("Version artifact is not a valid snapshot archive.") from exc

    manifest_path = _normalize_archive_member(snapshot_manifest, "flp_relative_path")
    fallback_filename = _normalize_archive_member(snapshot_manifest, "source_project_filename")

    with snapshot_zip:
        candidate_name = None
        for member_name in (manifest_path, fallback_filename):
            if member_name and _zip_entry_exists(snapshot_zip, member_name):
                candidate_name = member_name
                break

        if candidate_name is None:
            candidate_name = _find_first_flp_entry(snapshot_zip)

        if candidate_name is None:
            raise MixerSnapshotError("Snapshot archive does not contain an FL Studio project file.")

        extracted_path = _build_safe_extract_path(extract_root, candidate_name)
        extracted_path.parent.mkdir(parents=True, exist_ok=True)

        with snapshot_zip.open(candidate_name) as source, extracted_path.open("wb") as destination:
            destination.write(source.read())

    return extracted_path


def build_mixer_snapshot(project: Any) -> MixerProjectSnapshot:
    inserts: list[MixerInsertSnapshot] = []

    for insert in getattr(project, "mixer", []):
        iid = getattr(insert, "iid", None)
        if iid is None or iid == -1:
            continue

        slots = _build_slot_snapshots(insert)
        inserts.append(
            MixerInsertSnapshot(
                iid=int(iid),
                name=_normalize_optional_text(getattr(insert, "name", None)),
                enabled=_coerce_optional_bool(getattr(insert, "enabled", None)),
                volume=_coerce_optional_int(getattr(insert, "volume", None)),
                pan=_coerce_optional_int(getattr(insert, "pan", None)),
                slots=slots,
            )
        )

    inserts.sort(key=lambda item: item.iid)
    return MixerProjectSnapshot(inserts=tuple(inserts))


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
    return MixerDiffResult(summary=summary, changes=tuple(changes))


def _build_slot_snapshots(insert: Any) -> tuple[MixerSlotSnapshot, ...]:
    slots: list[MixerSlotSnapshot] = []

    for slot in insert:
        name = _normalize_optional_text(getattr(slot, "name", None))
        internal_name = _normalize_optional_text(getattr(slot, "internal_name", None))
        plugin_key = _resolve_slot_plugin_key(slot, name=name, internal_name=internal_name)
        if not any((name, internal_name, plugin_key)):
            continue

        slot_index = getattr(slot, "index", None)
        if slot_index is None:
            continue

        slots.append(
            MixerSlotSnapshot(
                index=int(slot_index),
                name=name,
                internal_name=internal_name,
                enabled=_coerce_optional_bool(getattr(slot, "enabled", None)),
                mix=_coerce_optional_int(getattr(slot, "mix", None)),
                plugin_key=plugin_key,
            )
        )

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
    plugin = getattr(slot, "plugin", None)
    plugin_type = type(plugin).__name__ if plugin is not None else None
    plugin_type = _normalize_optional_text(plugin_type)

    if internal_name and internal_name.lower() != "fruity wrapper":
        return internal_name
    if name:
        return name
    if internal_name:
        return internal_name
    return plugin_type


def _normalize_archive_member(snapshot_manifest: dict[str, Any] | None, key: str) -> str | None:
    if not snapshot_manifest:
        return None

    value = snapshot_manifest.get(key)
    if not isinstance(value, str):
        return None

    normalized = value.replace("\\", "/").strip("/")
    return normalized or None


def _build_safe_extract_path(extract_root: Path, member_name: str) -> Path:
    extracted_path = (extract_root / member_name).resolve()
    try:
        extracted_path.relative_to(extract_root.resolve())
    except ValueError as exc:
        raise MixerSnapshotError("Snapshot archive contains an invalid FL Studio project path.") from exc
    return extracted_path


def _find_first_flp_entry(snapshot_zip: zipfile.ZipFile) -> str | None:
    for member in snapshot_zip.infolist():
        if member.is_dir():
            continue
        if member.filename.lower().endswith(".flp"):
            return member.filename
    return None


def _zip_entry_exists(snapshot_zip: zipfile.ZipFile, member_name: str) -> bool:
    try:
        snapshot_zip.getinfo(member_name)
    except KeyError:
        return False
    return True


def _normalize_optional_text(value: Any) -> str | None:
    if value is None:
        return None

    text = str(value).strip()
    return text or None


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


@contextmanager
def _resolved_artifact_file(storage: StorageService, artifact_path: str) -> Iterator[Path]:
    resolved_path = storage.resolve_artifact_path(artifact_path)
    try:
        yield resolved_path
    finally:
        if artifact_path.startswith("gcs://") and resolved_path.exists():
            resolved_path.unlink()
