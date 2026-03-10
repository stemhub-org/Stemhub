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


@contextmanager
def _resolved_artifact_file(storage: StorageService, artifact_path: str) -> Iterator[Path]:
    resolved_path = storage.resolve_artifact_path(artifact_path)
    try:
        yield resolved_path
    finally:
        if artifact_path.startswith("gcs://") and resolved_path.exists():
            resolved_path.unlink()
