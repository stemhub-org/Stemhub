from __future__ import annotations

import importlib
import json
from pathlib import Path
from typing import Any

import pytest

from stemhub.dependency_guard import ensure_pyflp_available
from stemhub.flp_mixer_snapshot import MixerSnapshotError, build_mixer_snapshot, load_fl_studio_mixer_snapshot


REPO_ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = Path(__file__).resolve().parent / "fixtures" / "parser_corpus" / "manifest.json"


def _load_manifest() -> list[dict[str, Any]]:
    with MANIFEST_PATH.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    return payload["fixtures"]


def _load_pyflp_modules() -> tuple[Any, Any]:
    ensure_pyflp_available()
    return importlib.import_module("pyflp"), importlib.import_module("pyflp.exceptions")


class _FixtureStorage:
    def __init__(self, fixture_path: Path) -> None:
        self._fixture_path = fixture_path

    def resolve_artifact_path(self, artifact_path: str) -> Path:
        del artifact_path
        return self._fixture_path


def _resolve_exception_type(type_name: str, pyflp_exceptions: Any) -> type[Exception]:
    if hasattr(pyflp_exceptions, type_name):
        return getattr(pyflp_exceptions, type_name)
    if type_name == "MixerSnapshotError":
        return MixerSnapshotError
    raise AssertionError(f"Unsupported fixture exception type: {type_name}")


def _assert_mixer_snapshot_expectations(snapshot: Any, mixer_expectations: dict[str, Any]) -> None:
    if "mixer_supported" in mixer_expectations:
        assert snapshot.mixer_supported is mixer_expectations["mixer_supported"]
    if "insert_count" in mixer_expectations:
        assert len(snapshot.inserts) == mixer_expectations["insert_count"]
    if "flp_size_bytes" in mixer_expectations:
        assert snapshot.flp_size_bytes == mixer_expectations["flp_size_bytes"]
    if "flp_sha256" in mixer_expectations:
        assert snapshot.flp_sha256 == mixer_expectations["flp_sha256"]
    if "named_inserts_prefix" in mixer_expectations:
        named_inserts = [insert.name for insert in snapshot.inserts if insert.name]
        prefix = mixer_expectations["named_inserts_prefix"]
        assert named_inserts[: len(prefix)] == prefix


FIXTURES = _load_manifest()


@pytest.mark.parametrize("fixture_spec", FIXTURES, ids=[fixture["id"] for fixture in FIXTURES])
def test_parser_fixture_paths_exist(fixture_spec: dict[str, Any]) -> None:
    fixture_path = REPO_ROOT / fixture_spec["path"]
    assert fixture_path.is_file(), f"Fixture file is missing: {fixture_path}"


@pytest.mark.parametrize("fixture_spec", FIXTURES, ids=[fixture["id"] for fixture in FIXTURES])
def test_parser_fixture_corpus_matches_expectations(fixture_spec: dict[str, Any]) -> None:
    pyflp, pyflp_exceptions = _load_pyflp_modules()
    fixture_path = REPO_ROOT / fixture_spec["path"]
    expectations = fixture_spec["expectations"]

    if fixture_spec["kind"] == "stemhub_snapshot_archive":
        if expectations["parse"] == "error":
            error_expectations = expectations["error"]
            exception_type = _resolve_exception_type(error_expectations["type"], pyflp_exceptions)
            with pytest.raises(exception_type, match=error_expectations["message_contains"]):
                load_fl_studio_mixer_snapshot(
                    artifact_path=fixture_spec.get("artifact_path", fixture_spec["path"]),
                    snapshot_manifest=fixture_spec.get("snapshot_manifest"),
                    storage=_FixtureStorage(fixture_path),
                )
            return

        snapshot = load_fl_studio_mixer_snapshot(
            artifact_path=fixture_spec.get("artifact_path", fixture_spec["path"]),
            snapshot_manifest=fixture_spec.get("snapshot_manifest"),
            storage=_FixtureStorage(fixture_path),
        )
        mixer_expectations = expectations.get("mixer_snapshot", {})
        _assert_mixer_snapshot_expectations(snapshot, mixer_expectations)
        return

    if expectations["parse"] == "error":
        error_expectations = expectations["error"]
        exception_type = _resolve_exception_type(error_expectations["type"], pyflp_exceptions)
        with pytest.raises(exception_type, match=error_expectations["message_contains"]):
            pyflp.parse(fixture_path)
        return

    project = pyflp.parse(fixture_path)

    project_expectations = expectations.get("project", {})
    if "title" in project_expectations:
        assert getattr(project, "title", None) == project_expectations["title"]
    if "channel_count" in project_expectations:
        assert getattr(project, "channel_count", None) == project_expectations["channel_count"]
    if "tempo" in project_expectations:
        assert float(getattr(project, "tempo", 0.0)) == pytest.approx(project_expectations["tempo"])
    if "version" in project_expectations:
        assert str(getattr(project, "version", "")) == project_expectations["version"]

    mixer_expectations = expectations.get("mixer_snapshot")
    if mixer_expectations is None:
        return

    snapshot = build_mixer_snapshot(project)
    _assert_mixer_snapshot_expectations(snapshot, mixer_expectations)
