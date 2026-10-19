from __future__ import annotations

import sys
import types
from pathlib import Path

import pytest

from stemhub.fl_mixer import (
    FlEffectSlot,
    FlMixer,
    FlMixerInsert,
    MixerReadError,
    load_fl_mixer,
    parse_fl_mixer,
)


class FakePlugin:
    pass


class FakeSlot:
    """Mimics a PyFLP mixer slot (PyFLP's attribute names, e.g. ``mix``)."""

    def __init__(
        self,
        *,
        index: int | None,
        name: str | None = None,
        internal_name: str | None = None,
        enabled: bool | None = None,
        mix: int | None = None,
        plugin=None,
    ) -> None:
        self.index = index
        self.name = name
        self.internal_name = internal_name
        self.enabled = enabled
        self.mix = mix
        self.plugin = plugin


class FakeInsert:
    """Mimics a PyFLP mixer insert (PyFLP numbering: Master is iid -1)."""

    def __init__(
        self,
        *,
        iid: int | None,
        name: str | None = None,
        enabled: bool | None = None,
        volume: int | None = None,
        pan: int | None = None,
        slots: list[FakeSlot] | None = None,
    ) -> None:
        self.iid = iid
        self.name = name
        self.enabled = enabled
        self.volume = volume
        self.pan = pan
        self._slots = slots or []

    def __iter__(self):
        return iter(self._slots)


class FakeProject:
    def __init__(self, mixer) -> None:
        self.mixer = mixer


class FakeStorage:
    """Fake CAS storage that hands back a pre-written .flp path for any storage_uri."""

    def __init__(self, blob_file: Path) -> None:
        self.blob_file = blob_file

    def resolve_blob_path(self, storage_uri: str) -> Path:
        del storage_uri
        return self.blob_file


def test_parse_fl_mixer_normalizes_inserts_and_effect_slots() -> None:
    project = FakeProject(
        mixer=[
            FakeInsert(
                iid=3,
                name="Drums",
                enabled=True,
                volume=12800,
                pan=-120,
                slots=[
                    FakeSlot(index=1, name="Soft Clipper", internal_name="Fruity Soft Clipper", enabled=True, mix=6400, plugin=FakePlugin()),
                    FakeSlot(index=4, name=None, internal_name=None, enabled=False, mix=0, plugin=None),
                ],
            ),
            FakeInsert(
                iid=1,
                name=None,
                enabled=False,
                volume=10000,
                pan=0,
                slots=[
                    FakeSlot(index=2, name="Valhalla", internal_name="Fruity Wrapper", enabled=True, mix=3200, plugin=FakePlugin()),
                ],
            ),
        ]
    )

    mixer = parse_fl_mixer(project)

    assert mixer == FlMixer(
        inserts=(
            FlMixerInsert(
                index=2,
                name=None,
                enabled=False,
                volume=10000,
                pan=0,
                slots=(
                    FlEffectSlot(
                        index=2,
                        name="Valhalla",
                        internal_name="Fruity Wrapper",
                        enabled=True,
                        dry_wet=3200,
                        plugin_name="Valhalla",
                    ),
                ),
            ),
            FlMixerInsert(
                index=4,
                name="Drums",
                enabled=True,
                volume=12800,
                pan=-120,
                slots=(
                    FlEffectSlot(
                        index=1,
                        name="Soft Clipper",
                        internal_name="Fruity Soft Clipper",
                        enabled=True,
                        dry_wet=6400,
                        plugin_name="Fruity Soft Clipper",
                    ),
                ),
            ),
        )
    )


def test_parse_fl_mixer_keeps_master_insert_and_uses_fl_numbering() -> None:
    # PyFLP yields Master as iid -1 and FL insert N as iid N - 1.
    project = FakeProject(
        mixer=[
            FakeInsert(
                iid=-1,
                name="Master",
                enabled=True,
                volume=12800,
                pan=0,
                slots=[
                    FakeSlot(index=0, name="Limiter", internal_name="Fruity Limiter", enabled=True, mix=12800, plugin=FakePlugin()),
                ],
            ),
            FakeInsert(iid=0, name="Audio track"),
            FakeInsert(iid=None, name="Unindexed"),
        ]
    )

    mixer = parse_fl_mixer(project)

    assert mixer.inserts == (
        FlMixerInsert(
            index=0,
            name="Master",
            enabled=True,
            volume=12800,
            pan=0,
            slots=(
                FlEffectSlot(
                    index=0,
                    name="Limiter",
                    internal_name="Fruity Limiter",
                    enabled=True,
                    dry_wet=12800,
                    plugin_name="Fruity Limiter",
                ),
            ),
        ),
        FlMixerInsert(index=1, name="Audio track", enabled=None, volume=None, pan=None, slots=()),
    )


def test_load_fl_mixer_reads_project_file_and_records_hash(tmp_path, monkeypatch) -> None:
    flp_bytes = b"fake flp bytes"
    flp_blob = tmp_path / "project.flp"
    flp_blob.write_bytes(flp_bytes)

    parsed_paths: list[Path] = []

    def fake_parse(path: Path):
        parsed_paths.append(Path(path))
        return FakeProject(mixer=[FakeInsert(iid=-1, name="Master")])

    monkeypatch.setattr("stemhub.fl_mixer.ensure_pyflp_available", lambda: None)
    monkeypatch.setitem(sys.modules, "pyflp", types.SimpleNamespace(parse=fake_parse))

    storage = FakeStorage(flp_blob)
    mixer = load_fl_mixer(
        storage_uri="projects/demo/blobs/00/deadbeef",
        storage=storage,
    )

    assert mixer.inserts == (
        FlMixerInsert(index=0, name="Master", enabled=None, volume=None, pan=None, slots=()),
    )
    assert mixer.flp_size_bytes == len(flp_bytes)
    assert mixer.flp_sha256 is not None
    assert mixer.mixer_supported is True
    assert parsed_paths == [flp_blob]


def test_load_fl_mixer_keeps_the_project_file_hash_when_the_parser_fails(tmp_path, monkeypatch) -> None:
    flp_bytes = b"fake flp bytes"
    flp_blob = tmp_path / "project.flp"
    flp_blob.write_bytes(flp_bytes)

    def fake_parse(path: Path):
        del path
        raise RuntimeError("low-level parser crash")

    monkeypatch.setattr("stemhub.fl_mixer.ensure_pyflp_available", lambda: None)
    monkeypatch.setitem(sys.modules, "pyflp", types.SimpleNamespace(parse=fake_parse))

    mixer = load_fl_mixer(
        storage_uri="projects/demo/blobs/00/deadbeef",
        storage=FakeStorage(flp_blob),
    )

    assert mixer.inserts == ()
    assert mixer.flp_size_bytes == len(flp_bytes)
    assert mixer.flp_sha256 is not None
    assert mixer.mixer_supported is False
    assert mixer.parse_error == "low-level parser crash"


def test_load_fl_mixer_error_for_a_missing_project_file_does_not_mention_blobs() -> None:
    with pytest.raises(MixerReadError) as excinfo:
        load_fl_mixer(storage_uri="", storage=FakeStorage(Path("unused")))

    message = str(excinfo.value).lower()
    assert "project file" in message
    assert "blob" not in message
    assert "snapshot" not in message
