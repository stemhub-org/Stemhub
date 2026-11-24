"""FL Studio zipped projects ("Zipped loop package"): reading one safely, in memory.

What FL writes (docs/fl-studio-format.md#zipped-projects), checked on six bundles
saved by FL 20.6 and 20.7 holding 139 samples:

- a ZIP holding one .flp and one file per distinct sample, all at the root, each
  sample under its own file name (factory samples included);
- the .flp is not rewritten: each sample path (event 196) stays as it was saved,
  an absolute path, a ``%FL...%`` folder variable, or the temporary folder FL
  extracted an earlier bundle to (``...\\Temp\\Image-Line\\{GUID}\\Zip\\``);
- FL opens a bundle by extracting it and finding each sample by its file name.
  FLEngine says "Could not find any FLP project file in this zipped archive."
  when there is no .flp.

``read_bundle`` resolves the sample paths the same way, ignoring case as Windows
and macOS disks do, without extracting anything. It refuses what FL could not
open unambiguously (no .flp, several, two names differing only in case) and what
is unsafe to extract later: every entry name must be a valid asset path of the
project model (relative, ``/``-separated, no ``..``, drive, backslash, control
character or reserved Windows name). Error messages give entry indexes, never
names, which can name people.

Only channel sample paths (event 196) are resolved; a file named inside a
plugin's state (Slicex, DirectWave, FPC...) is not, and stays in ``unused``.
"""
from __future__ import annotations

import io
import os
import struct
import zipfile
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Dict, List, Optional, Tuple, Type, Union

from pydantic import TypeAdapter, ValidationError

from .flp_events import FlpFile, FlpFormatError, decode_text, fl_version, parse
from .schema import AssetPath

try:  # absent from some minimal Python builds, which then can't read LZMA entries anyway
    import lzma
except ImportError:  # pragma: no cover
    lzma = None  # type: ignore[assignment]

SAMPLE_PATH_ID = 196  # a channel's sample: an absolute path, a %FL...% variable, or empty
PROJECT_SUFFIX = ".flp"
_ENCRYPTED_FLAG = 0x01  # ZIP general purpose bit 0
_ASSET_PATH = TypeAdapter(AssetPath)
# What zipfile lets through for a damaged archive or entry: it wraps none of the decompressors' errors.
_ARCHIVE_ERRORS: Tuple[Type[BaseException], ...] = (zipfile.BadZipFile, EOFError, OSError, ValueError, struct.error, NotImplementedError)
_ENTRY_ERRORS: Tuple[Type[BaseException], ...] = _ARCHIVE_ERRORS + (zlib.error,) + ((lzma.LZMAError,) if lzma else ())

Source = Union[bytes, bytearray, memoryview, str, "os.PathLike[str]", BinaryIO]


class FlBundleError(ValueError):
    """The bytes are not a zipped FL project StemHub can read safely."""


@dataclass(frozen=True)
class BundleLimits:
    """What ``read_bundle`` accepts. The project file is the only entry it inflates."""

    max_entries: int = 4096
    max_project_bytes: int = 64 * 1024 * 1024  # the largest .flp seen is about 3.5 MiB
    max_total_bytes: int = 4 * 1024 * 1024 * 1024  # the declared size of every file, once extracted
    max_events: Optional[int] = 1_000_000  # flp_events.parse's cap (about 20 000 seen)


DEFAULT_LIMITS = BundleLimits()


@dataclass(frozen=True)
class BundledSample:
    """One sample path of the project and the bundle file it resolves to."""

    event_index: int  # in ``FlBundle.project.events``
    stored_path: str  # as the .flp holds it: often an absolute path, so personal data
    file_name: str  # its last segment, what FL looks for
    asset_path: Optional[str]  # the bundle entry FL would open, or None when the bundle lacks it


@dataclass(frozen=True)
class FlBundle:
    project_path: str  # the .flp entry
    project: FlpFile
    samples: Tuple[BundledSample, ...]  # in event order, one per non-empty sample path
    entries: Tuple[str, ...]  # every file entry, in archive order
    unused: Tuple[str, ...]  # file entries no sample path resolves to, the .flp left out


def read_bundle(source: Source, *, limits: BundleLimits = DEFAULT_LIMITS) -> FlBundle:
    """Read a zipped FL project from bytes, a path or a binary file. Raises FlBundleError."""
    with _open_archive(source) as archive:
        files = _checked_entries(archive, limits)
        project_info = _project_entry(files)
        project = _read_project(archive, project_info, limits)
    names = tuple(info.filename for info in files)
    at_root = {name.casefold(): name for name in names if "/" not in name and name != project_info.filename}
    samples = _samples(project, at_root)
    used = {sample.asset_path for sample in samples}
    unused = tuple(name for name in names if name != project_info.filename and name not in used)
    return FlBundle(project_info.filename, project, samples, names, unused)


def sample_file_name(stored_path: str) -> str:
    """The file name FL looks for: the last segment of a sample path, '\\' or '/' separated."""
    return stored_path.replace("\\", "/").rsplit("/", 1)[-1]


# --- the archive ---------------------------------------------------------------------------


def _open_archive(source: Source) -> zipfile.ZipFile:
    if isinstance(source, (bytes, bytearray, memoryview)):
        source = io.BytesIO(bytes(source))
    elif isinstance(source, (str, os.PathLike)):
        source = Path(source)
    elif not hasattr(source, "read"):
        raise TypeError("read_bundle() takes bytes, a path or a binary file")
    try:
        return zipfile.ZipFile(source)
    except (FileNotFoundError, PermissionError, IsADirectoryError):
        raise  # the caller's path, not the archive
    except _ARCHIVE_ERRORS as error:
        raise FlBundleError("not a ZIP archive") from error


def _checked_entries(archive: zipfile.ZipFile, limits: BundleLimits) -> List[zipfile.ZipInfo]:
    infos = archive.infolist()
    if len(infos) > limits.max_entries:
        raise FlBundleError(f"the archive holds more than {limits.max_entries} entries")
    files, seen, total = [], set(), 0
    for index, info in enumerate(infos):
        if info.is_dir():
            continue
        _check_name(info.filename, index)
        if info.flag_bits & _ENCRYPTED_FLAG:
            raise FlBundleError(f"entry #{index} is encrypted")
        folded = info.filename.casefold()
        if folded in seen:
            raise FlBundleError(f"entry #{index} duplicates an earlier name, or they differ only in case")
        seen.add(folded)
        total += info.file_size
        if total > limits.max_total_bytes:
            raise FlBundleError(f"the files would take more than {limits.max_total_bytes} bytes once extracted")
        files.append(info)
    return files


def _check_name(name: str, index: int) -> None:
    try:
        _ASSET_PATH.validate_python(name)
    except ValidationError:
        raise FlBundleError(f"entry #{index} has a name that is not a safe relative path") from None


def _project_entry(files: List[zipfile.ZipInfo]) -> zipfile.ZipInfo:
    projects = [info for info in files if "/" not in info.filename and info.filename.casefold().endswith(PROJECT_SUFFIX)]
    if not projects:
        raise FlBundleError("no FLP project file at the root of the archive")
    if len(projects) > 1:
        raise FlBundleError(f"several FLP project files at the root of the archive ({len(projects)})")
    return projects[0]


def _read_project(archive: zipfile.ZipFile, info: zipfile.ZipInfo, limits: BundleLimits) -> FlpFile:
    if info.file_size > limits.max_project_bytes:
        raise FlBundleError(f"the project file is larger than {limits.max_project_bytes} bytes")
    try:
        with archive.open(info) as stream:
            data = stream.read(limits.max_project_bytes + 1)  # never more, whatever the size declared
    except _ENTRY_ERRORS as error:
        raise FlBundleError("the project file can't be read from the archive") from error
    if len(data) > limits.max_project_bytes:
        raise FlBundleError(f"the project file is larger than {limits.max_project_bytes} bytes")
    try:
        return parse(data, max_events=limits.max_events)
    except FlpFormatError as error:
        raise FlBundleError(f"the project file is not a valid FLP: {error}") from error


# --- the samples ---------------------------------------------------------------------------


def _samples(project: FlpFile, at_root: Dict[str, str]) -> Tuple[BundledSample, ...]:
    version = fl_version(project)
    samples = []
    for index, event in enumerate(project.events):
        if event.id != SAMPLE_PATH_ID:
            continue
        stored = decode_text(event, version)
        name = sample_file_name(stored)
        if name:
            samples.append(BundledSample(index, stored, name, at_root.get(name.casefold())))
    return tuple(samples)
