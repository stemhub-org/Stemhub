from __future__ import annotations

import sys
import types
from pathlib import Path

from stemhub.flp_mixer_snapshot import (
    MixerInsertSnapshot,
    MixerProjectSnapshot,
    MixerSlotSnapshot,
    build_mixer_snapshot,
    load_fl_studio_mixer_snapshot,
)


class FakePlugin:
    pass


class FakeSlot:
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


def test_build_mixer_snapshot_normalizes_inserts_and_slots() -> None:
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
                iid=-1,
                name="Current",
                slots=[FakeSlot(index=0, name="Ignore me", plugin=FakePlugin())],
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

    snapshot = build_mixer_snapshot(project)

    assert snapshot == MixerProjectSnapshot(
        inserts=(
            MixerInsertSnapshot(
                iid=1,
                name=None,
                enabled=False,
                volume=10000,
                pan=0,
                slots=(
                    MixerSlotSnapshot(
                        index=2,
                        name="Valhalla",
                        internal_name="Fruity Wrapper",
                        enabled=True,
                        mix=3200,
                        plugin_key="Valhalla",
                    ),
                ),
            ),
            MixerInsertSnapshot(
                iid=3,
                name="Drums",
                enabled=True,
                volume=12800,
                pan=-120,
                slots=(
                    MixerSlotSnapshot(
                        index=1,
                        name="Soft Clipper",
                        internal_name="Fruity Soft Clipper",
                        enabled=True,
                        mix=6400,
                        plugin_key="Fruity Soft Clipper",
                    ),
                ),
            ),
        )
    )


def test_load_fl_studio_mixer_snapshot_reads_blob_and_records_hash(tmp_path, monkeypatch) -> None:
    flp_bytes = b"fake flp bytes"
    flp_blob = tmp_path / "project.flp"
    flp_blob.write_bytes(flp_bytes)

    parsed_paths: list[Path] = []

    def fake_parse(path: Path):
        parsed_paths.append(Path(path))
        return FakeProject(mixer=[FakeInsert(iid=0, name="Master")])

    monkeypatch.setattr("stemhub.flp_mixer_snapshot.ensure_pyflp_available", lambda: None)
    monkeypatch.setitem(sys.modules, "pyflp", types.SimpleNamespace(parse=fake_parse))

    storage = FakeStorage(flp_blob)
    snapshot = load_fl_studio_mixer_snapshot(
        storage_uri="projects/demo/blobs/00/deadbeef",
        storage=storage,
    )

    assert snapshot.inserts == (
        MixerInsertSnapshot(iid=0, name="Master", enabled=None, volume=None, pan=None, slots=()),
    )
    assert snapshot.flp_size_bytes == len(flp_bytes)
    assert snapshot.flp_sha256 is not None
    assert snapshot.mixer_supported is True
    assert parsed_paths == [flp_blob]


def test_load_fl_studio_mixer_snapshot_falls_back_to_binary_snapshot_on_parser_failure(tmp_path, monkeypatch) -> None:
    flp_bytes = b"fake flp bytes"
    flp_blob = tmp_path / "project.flp"
    flp_blob.write_bytes(flp_bytes)

    def fake_parse(path: Path):
        del path
        raise RuntimeError("low-level parser crash")

    monkeypatch.setattr("stemhub.flp_mixer_snapshot.ensure_pyflp_available", lambda: None)
    monkeypatch.setitem(sys.modules, "pyflp", types.SimpleNamespace(parse=fake_parse))

    snapshot = load_fl_studio_mixer_snapshot(
        storage_uri="projects/demo/blobs/00/deadbeef",
        storage=FakeStorage(flp_blob),
    )

    assert snapshot.inserts == ()
    assert snapshot.flp_size_bytes == len(flp_bytes)
    assert snapshot.flp_sha256 is not None
    assert snapshot.mixer_supported is False
    assert snapshot.parse_error == "low-level parser crash"
