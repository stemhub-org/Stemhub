"""FL Studio zipped projects (stemhub.project_model.flp_bundle).

FL's "Zipped loop package" is a ZIP holding one .flp and one file per sample, all
at the root under each sample's file name. The .flp is not rewritten: its sample
paths stay absolute, and FL finds each sample by file name when it opens the
bundle. These tests build such bundles in memory. Point STEMHUB_EXTENDED_CORPUS
at folders holding real bundles (*.zip) to check them too; they are never committed.
"""
from __future__ import annotations

import io
import random
import zipfile
from pathlib import Path
from typing import Optional, Sequence, Tuple

import pytest

from flp_helpers import (
    EXTENDED_BUNDLES,
    FAKE_USER,
    FL_VERSION,
    SAMPLE_PATH,
    TEMPO,
    ascii_text,
    fixed_event,
    fl2025_file,
    flp_data,
    utf16_text,
    var_event,
)
from stemhub.project_model.flp_bundle import (
    BundleLimits,
    FlBundleError,
    read_bundle,
    sample_file_name,
)
from stemhub.project_model.flp_events import fl_version

# Sample paths as FL stores them in a bundle's .flp: a folder on another drive, a factory
# sample, and the temporary folder FL extracted an earlier bundle to (then saved again).
STORED_PATHS = (
    f"D:\\Musique\\FL\\kits\\{FAKE_USER} kit\\Kick 01.wav",
    "%FLStudioFactoryData%\\Data\\Patches\\Packs\\Legacy\\Drums\\Dance\\Basic 808 Snare.wav",
    f"C:\\Users\\{FAKE_USER}\\AppData\\Local\\Temp\\Image-Line\\{{DFC32806-4139-4FD9-B6FE-0CBD9D06C2E1}}\\Zip\\hat (pippa).wav",
)


def _project(sample_paths: Sequence[str] = STORED_PATHS) -> bytes:
    return flp_data(
        var_event(FL_VERSION, ascii_text("20.7.2.1863"))
        + fixed_event(TEMPO, 140000, 4)
        + b"".join(var_event(SAMPLE_PATH, utf16_text(path)) for path in sample_paths)
    )


def _bundle(
    entries: Sequence[Tuple[str, bytes]],
    *,
    compression: int = zipfile.ZIP_DEFLATED,
) -> bytes:
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", compression) as archive:
        for name, data in entries:
            archive.writestr(name, data)
    return buffer.getvalue()


def _fl_bundle(flp_name: str = "beat.flp") -> bytes:
    return _bundle(
        [
            (flp_name, _project()),
            ("Kick 01.wav", b"RIFF kick"),
            ("Basic 808 Snare.wav", b"RIFF snare"),
            ("hat (pippa).wav", b"RIFF hat"),
        ]
    )


# --- what FL writes ------------------------------------------------------------------------


def test_every_sample_path_resolves_to_its_file_at_the_root() -> None:
    bundle = read_bundle(_fl_bundle())

    assert bundle.project_path == "beat.flp"
    assert fl_version(bundle.project) == (20, 7, 2, 1863)
    assert [(sample.file_name, sample.asset_path) for sample in bundle.samples] == [
        ("Kick 01.wav", "Kick 01.wav"),
        ("Basic 808 Snare.wav", "Basic 808 Snare.wav"),
        ("hat (pippa).wav", "hat (pippa).wav"),
    ]
    assert [sample.stored_path for sample in bundle.samples] == list(STORED_PATHS)
    assert [bundle.project.events[sample.event_index].id for sample in bundle.samples] == [SAMPLE_PATH] * 3
    assert bundle.unused == ()
    assert bundle.entries == ("beat.flp", "Kick 01.wav", "Basic 808 Snare.wav", "hat (pippa).wav")


def test_file_names_match_ignoring_case_as_on_windows_and_macos() -> None:
    data = _bundle([("Beat.FLP", _project(["D:\\kits\\KICK 01.WAV"])), ("kick 01.wav", b"RIFF")])

    bundle = read_bundle(data)

    assert bundle.project_path == "Beat.FLP"
    assert bundle.samples[0].asset_path == "kick 01.wav"


def test_a_missing_sample_and_an_unused_file_are_reported() -> None:
    data = _bundle([("beat.flp", _project(["D:\\kits\\gone.wav"])), ("extra.wav", b"RIFF")])

    bundle = read_bundle(data)

    assert [(sample.file_name, sample.asset_path) for sample in bundle.samples] == [("gone.wav", None)]
    assert bundle.unused == ("extra.wav",)


def test_one_file_used_by_several_channels_is_bundled_once() -> None:
    paths = ["D:\\a\\loop.wav", "%FLStudioUserData%\\Audio\\Sliced audio\\loop.wav"]
    bundle = read_bundle(_bundle([("beat.flp", _project(paths)), ("loop.wav", b"RIFF")]))

    assert [sample.asset_path for sample in bundle.samples] == ["loop.wav", "loop.wav"]
    assert bundle.unused == ()


def test_an_empty_sample_path_is_not_a_sample() -> None:
    bundle = read_bundle(_bundle([("beat.flp", _project(["", "D:\\kick.wav"])), ("kick.wav", b"RIFF")]))

    assert [sample.file_name for sample in bundle.samples] == ["kick.wav"]


def test_files_in_a_folder_are_listed_but_not_matched() -> None:
    # FL writes every sample at the root; one in a folder is not what FL would find by name.
    data = _bundle([("beat.flp", _project(["D:\\kick.wav"])), ("Samples/kick.wav", b"RIFF")])

    bundle = read_bundle(data)

    assert bundle.samples[0].asset_path is None
    assert bundle.unused == ("Samples/kick.wav",)


@pytest.mark.parametrize(
    ("stored", "name"),
    [
        ("D:\\Musique\\FL\\kit\\kick.wav", "kick.wav"),
        ("/Users/someone/Samples/snare.wav", "snare.wav"),
        ("%FLStudioFactoryData%\\Data\\Patches\\hat.wav", "hat.wav"),
        ("mixed/sep\\clap.wav", "clap.wav"),
        ("plain.wav", "plain.wav"),
        ("", ""),
    ],
)
def test_sample_file_name_takes_the_last_segment_of_either_separator(stored: str, name: str) -> None:
    assert sample_file_name(stored) == name


@pytest.mark.parametrize("source", ["bytes", "path", "file"])
def test_a_bundle_reads_from_bytes_a_path_or_a_file(tmp_path: Path, source: str) -> None:
    data = _fl_bundle()
    path = tmp_path / "beat.zip"
    path.write_bytes(data)
    given = {"bytes": data, "path": path, "file": io.BytesIO(data)}[source]

    assert read_bundle(given).project_path == "beat.flp"


def test_stored_entries_read_as_well_as_deflated_ones() -> None:
    assert read_bundle(_bundle([("beat.flp", fl2025_file())], compression=zipfile.ZIP_STORED)).samples == ()


# --- refusing what FL wouldn't open or what isn't safe to extract ----------------------------


@pytest.mark.parametrize(
    ("entries", "message"),
    [
        pytest.param([("kick.wav", b"RIFF")], "no FLP project file", id="no project"),
        pytest.param([("a.flp", fl2025_file()), ("b.flp", fl2025_file())], "several FLP project files", id="two projects"),
        pytest.param([("Samples/beat.flp", fl2025_file())], "no FLP project file", id="project in a folder"),
        pytest.param([("beat.flp", b"RIFF not an flp")], "not a valid FLP", id="not an flp"),
        pytest.param(
            [("beat.flp", fl2025_file()), ("Kick.wav", b"1"), ("kick.WAV", b"2")],
            "differ only in case",
            id="names differing in case",
        ),
    ],
)
def test_bundles_fl_could_not_open_are_refused(entries, message: str) -> None:
    with pytest.raises(FlBundleError, match=message):
        read_bundle(_bundle(entries))


@pytest.mark.parametrize(
    "name",
    [
        "../evil.wav",
        "Samples/../../evil.wav",
        "/etc/evil.wav",
        "C:evil.wav",
        "dir\\evil.wav",
        "CON.wav",
        "trailing.wav.",
        "bell\x07.wav",
    ],
)
def test_unsafe_entry_names_are_refused_without_quoting_them(name: str) -> None:
    data = _bundle([("beat.flp", fl2025_file()), (name, b"RIFF " + FAKE_USER.encode())])

    with pytest.raises(FlBundleError, match="entry #1") as raised:
        read_bundle(data)

    assert name not in str(raised.value) and "evil" not in str(raised.value)


def test_a_duplicated_entry_name_is_refused() -> None:
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as archive:
        archive.writestr("beat.flp", fl2025_file())
        with pytest.warns(UserWarning):
            archive.writestr("beat.flp", fl2025_file())

    with pytest.raises(FlBundleError, match="differ only in case|duplicate"):
        read_bundle(buffer.getvalue())


def _flag_encrypted(data: bytes, name: bytes) -> bytes:
    """The archive with ``name``'s central directory entry flagged as encrypted (bit 0)."""
    patched = bytearray(data)
    central = data.find(b"PK\x01\x02")
    while central != -1:
        name_length = int.from_bytes(data[central + 28 : central + 30], "little")
        if data[central + 46 : central + 46 + name_length] == name:
            patched[central + 8] |= 0x01
        central = data.find(b"PK\x01\x02", central + 4)
    return bytes(patched)


def test_an_encrypted_entry_is_refused() -> None:
    with pytest.raises(FlBundleError, match="encrypted"):
        read_bundle(_flag_encrypted(_fl_bundle(), b"Kick 01.wav"))


@pytest.mark.parametrize(
    ("limits", "message"),
    [
        (BundleLimits(max_entries=3), "more than 3 entries"),
        (BundleLimits(max_project_bytes=100), "project file is larger than 100 bytes"),
        (BundleLimits(max_total_bytes=50), "more than 50 bytes"),
    ],
)
def test_bundles_past_the_limits_are_refused(limits: BundleLimits, message: str) -> None:
    with pytest.raises(FlBundleError, match=message):
        read_bundle(_fl_bundle(), limits=limits)


def test_a_project_with_a_forged_size_is_refused() -> None:
    # A forged size must never let the project inflate unchecked: it is read up to the limit only.
    data = bytearray(_bundle([("beat.flp", flp_data(var_event(FL_VERSION, ascii_text("x" * 5000))))]))
    for signature, size_offset in ((b"PK\x03\x04", 22), (b"PK\x01\x02", 24)):
        position = bytes(data).find(signature)
        data[position + size_offset : position + size_offset + 4] = (10).to_bytes(4, "little")

    with pytest.raises(FlBundleError):
        read_bundle(bytes(data), limits=BundleLimits(max_project_bytes=1000))


@pytest.mark.parametrize("data", [b"", b"not a zip at all", b"PK\x03\x04" + bytes(40)])
def test_what_is_not_a_zip_is_refused(data: bytes) -> None:
    with pytest.raises(FlBundleError, match="not a ZIP"):
        read_bundle(data)


def test_a_corrupted_project_entry_is_refused() -> None:
    data = bytearray(_bundle([("beat.flp", _project())], compression=zipfile.ZIP_STORED))
    position = bytes(data).find(b"FLdt")
    data[position + 10] ^= 0xFF  # inside the project's bytes: the CRC no longer matches

    with pytest.raises(FlBundleError):
        read_bundle(bytes(data))


@pytest.mark.parametrize("compression", [zipfile.ZIP_DEFLATED, zipfile.ZIP_LZMA, zipfile.ZIP_BZIP2])
def test_a_damaged_compressed_project_is_refused(compression: int) -> None:
    # A truncated or tampered upload: the decompressor's own error must not leak out.
    project = _project()
    data = bytearray(_bundle([("beat.flp", project)], compression=compression))
    archive = zipfile.ZipFile(io.BytesIO(bytes(data)))
    info = archive.getinfo("beat.flp")
    start = info.header_offset + 30 + len(info.filename)
    for offset in range(start + 2, start + info.compress_size - 2, 3):
        data[offset] ^= 0x5A

    with pytest.raises(FlBundleError):
        read_bundle(bytes(data))


def test_a_malformed_project_error_names_no_entry_nor_text() -> None:
    secret_name = f"{FAKE_USER} beat.flp"
    body = var_event(FL_VERSION, ascii_text("25.2.4.4960")) + bytes([SAMPLE_PATH, 0x7F]) + utf16_text(FAKE_USER)

    with pytest.raises(FlBundleError, match="not a valid FLP") as raised:
        read_bundle(_bundle([(secret_name, flp_data(body))]))

    assert FAKE_USER not in str(raised.value)


@pytest.mark.parametrize("compression", [zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED, zipfile.ZIP_LZMA])
def test_random_byte_flips_only_raise_fl_bundle_error(compression: int) -> None:
    original = _bundle(
        [("beat.flp", _project()), ("Kick 01.wav", b"RIFF kick"), ("hat (pippa).wav", b"RIFF hat")],
        compression=compression,
    )
    generator = random.Random(0xB0D1E)
    for _ in range(600):
        mutated = bytearray(original)
        for _ in range(generator.randint(1, 4)):
            mutated[generator.randrange(len(mutated))] = generator.randrange(256)
        try:
            read_bundle(bytes(mutated))
        except FlBundleError:
            pass


@pytest.mark.parametrize("bad", [None, 3.5, "beat.zip\x00"])
def test_wrong_sources_are_refused(bad: object) -> None:
    with pytest.raises((TypeError, FlBundleError, ValueError)):
        read_bundle(bad)  # type: ignore[arg-type]


# --- real bundles (never committed) --------------------------------------------------------


@pytest.mark.parametrize("path", EXTENDED_BUNDLES)
def test_every_local_bundle_resolves_every_sample(path: Optional[Path]) -> None:
    if path is None:
        pytest.fail("no bundle in STEMHUB_EXTENDED_CORPUS")
    bundle = read_bundle(path)

    assert all(sample.asset_path is not None for sample in bundle.samples)
    assert bundle.unused == ()
