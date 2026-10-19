"""The single reader of stored version manifests, v1 and v2.

This is the only module that knows how each manifest version lists its files
(see docs/content-addressed-storage.md):

- v1 (legacy, still stored): assets under ``tracks``, each file's path under
  ``filename``, plus a ``name`` per asset.
- v2: assets under ``assets``, paths under ``path``; names derive from paths.

Stored ``manifest_json`` rows are never rewritten, so both versions stay
readable. The reader is defensive: a malformed entry is skipped and logged
instead of failing the request. Blob ref counts are bumped on version create
and dropped on version delete through ``blob_shas``, so both sides always see
the same hashes.
"""
from __future__ import annotations

import logging
from dataclasses import dataclass, replace
from typing import Any

logger = logging.getLogger(__name__)

# manifest_version -> (key of the asset list, key of each file's path).
_LAYOUTS: dict[int, tuple[str, str]] = {
    1: ("tracks", "filename"),  # legacy v1
    2: ("assets", "path"),
}
# Manifests written before manifest_version existed are v1.
_DEFAULT_MANIFEST_VERSION = 1


@dataclass(frozen=True)
class FileRef:
    """One file of a manifest: the project file or an asset.

    ``path`` and ``size_bytes`` are None when the stored entry lacks a usable
    value; ``sha256`` is always set, so ref counting never misses a file.
    ``index`` is an asset's position in the stored asset list, counting the
    malformed entries that were skipped; it is None for the project file.
    """

    sha256: str
    size_bytes: int | None
    path: str | None
    name: str | None
    index: int | None = None


def blob_refs(manifest_json: Any) -> tuple[FileRef | None, tuple[FileRef, ...]]:
    """Return ``(project_file, assets)`` listed by a stored manifest of any supported version."""
    if not isinstance(manifest_json, dict):
        return None, ()

    manifest_version = manifest_json.get("manifest_version", _DEFAULT_MANIFEST_VERSION)
    layout = _LAYOUTS.get(manifest_version) if isinstance(manifest_version, int) else None
    if layout is None:
        logger.warning("Ignoring manifest with unsupported manifest_version %r", manifest_version)
        return None, ()

    assets_key, path_key = layout
    project_file = _read_file_ref(manifest_json.get("project_file"), path_key=path_key, label="project file")

    entries = manifest_json.get(assets_key) or []
    if not isinstance(entries, list):
        logger.warning("Ignoring manifest %r: expected a list", assets_key)
        entries = []

    assets = tuple(
        replace(ref, index=index)
        for index, entry in enumerate(entries)
        if (ref := _read_file_ref(entry, path_key=path_key, label=f"asset {index}")) is not None
    )
    return project_file, assets


def blob_shas(manifest_json: Any) -> set[str]:
    """Every distinct SHA-256 a manifest references (a hash listed twice counts once)."""
    project_file, assets = blob_refs(manifest_json)
    shas = {asset.sha256 for asset in assets}
    if project_file is not None:
        shas.add(project_file.sha256)
    return shas


def _read_file_ref(entry: Any, *, path_key: str, label: str) -> FileRef | None:
    if not isinstance(entry, dict):
        logger.warning("Skipping manifest %s: not an object", label)
        return None

    sha256 = entry.get("sha256")
    if not isinstance(sha256, str) or not sha256:
        logger.warning("Skipping manifest %s: missing or invalid sha256", label)
        return None

    path = entry.get(path_key)
    path = path if isinstance(path, str) and path else None
    size_bytes = entry.get("size_bytes")
    size_bytes = size_bytes if isinstance(size_bytes, int) and not isinstance(size_bytes, bool) else None
    # Only v1 stored a name; it was derived from the path the same way.
    stored_name = entry.get("name")
    name = stored_name if isinstance(stored_name, str) and stored_name else _name_from_path(path)

    return FileRef(sha256=sha256, size_bytes=size_bytes, path=path, name=name)


def _name_from_path(path: str | None) -> str | None:
    if path is None:
        return None
    filename = path.rsplit("/", 1)[-1]
    without_extension = filename.rsplit(".", 1)[0] if "." in filename else filename
    return without_extension or filename or None
