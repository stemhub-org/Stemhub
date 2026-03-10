from __future__ import annotations

import sys
import types
import zipfile
from pathlib import Path

from stemhub.flp_mixer_snapshot import (
    MixerInsertSnapshot,
    MixerProjectSnapshot,
    MixerSlotSnapshot,
    MixerSnapshotError,
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
    def __init__(self, artifact_file: Path) -> None:
        self.artifact_file = artifact_file

    def resolve_artifact_path(self, artifact_path: str) -> Path:
        del artifact_path
        return self.artifact_file


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


def test_load_fl_studio_mixer_snapshot_extracts_manifest_flp_and_cleans_remote_temp_file(
    tmp_path,
    monkeypatch,
) -> None:
    snapshot_zip = tmp_path / "artifact.zip"
    with zipfile.ZipFile(snapshot_zip, "w") as archive:
        archive.writestr("Projects/demo/project.flp", b"fake flp bytes")

    parsed_paths: list[Path] = []

    def fake_parse(path: Path):
        parsed_paths.append(Path(path))
        return FakeProject(mixer=[FakeInsert(iid=0, name="Master")])

    monkeypatch.setattr("stemhub.flp_mixer_snapshot.ensure_pyflp_available", lambda: None)
    monkeypatch.setitem(sys.modules, "pyflp", types.SimpleNamespace(parse=fake_parse))

    storage = FakeStorage(snapshot_zip)
    snapshot = load_fl_studio_mixer_snapshot(
        artifact_path="gcs://demo-bucket/projects/demo/snapshot.zip",
        snapshot_manifest={"flp_relative_path": "Projects/demo/project.flp"},
        storage=storage,
    )

    assert snapshot == MixerProjectSnapshot(
        inserts=(
            MixerInsertSnapshot(
                iid=0,
                name="Master",
                enabled=None,
                volume=None,
                pan=None,
                slots=(),
            ),
        )
    )
    assert len(parsed_paths) == 1
    assert parsed_paths[0].name == "project.flp"
    assert not parsed_paths[0].exists()
    assert not snapshot_zip.exists()


def test_load_fl_studio_mixer_snapshot_falls_back_to_first_flp_entry(tmp_path, monkeypatch) -> None:
    snapshot_zip = tmp_path / "artifact.zip"
    with zipfile.ZipFile(snapshot_zip, "w") as archive:
        archive.writestr("manifest.json", "{}")
        archive.writestr("nested/beat.flp", b"fake flp bytes")

    def fake_parse(path: Path):
        assert Path(path).name == "beat.flp"
        return FakeProject(mixer=[])

    monkeypatch.setattr("stemhub.flp_mixer_snapshot.ensure_pyflp_available", lambda: None)
    monkeypatch.setitem(sys.modules, "pyflp", types.SimpleNamespace(parse=fake_parse))

    storage = FakeStorage(snapshot_zip)
    snapshot = load_fl_studio_mixer_snapshot(
        artifact_path="projects/demo/artifact.zip",
        snapshot_manifest={"flp_relative_path": "missing/project.flp"},
        storage=storage,
    )

    assert snapshot == MixerProjectSnapshot(inserts=())
    assert snapshot_zip.exists()


def test_load_fl_studio_mixer_snapshot_errors_when_snapshot_has_no_flp(tmp_path, monkeypatch) -> None:
    snapshot_zip = tmp_path / "artifact.zip"
    with zipfile.ZipFile(snapshot_zip, "w") as archive:
        archive.writestr("manifest.json", "{}")
        archive.writestr("preview/latest_track.wav", b"no project here")

    monkeypatch.setattr("stemhub.flp_mixer_snapshot.ensure_pyflp_available", lambda: None)
    monkeypatch.setitem(sys.modules, "pyflp", types.SimpleNamespace(parse=lambda path: path))

    storage = FakeStorage(snapshot_zip)

    try:
        load_fl_studio_mixer_snapshot(
            artifact_path="projects/demo/artifact.zip",
            snapshot_manifest={"flp_relative_path": "missing/project.flp"},
            storage=storage,
        )
    except MixerSnapshotError as exc:
        assert str(exc) == "Snapshot archive does not contain an FL Studio project file."
    else:
        raise AssertionError("Expected MixerSnapshotError when snapshot zip does not contain a .flp file")
