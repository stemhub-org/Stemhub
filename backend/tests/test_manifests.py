"""Tests for stemhub.manifests, the single reader of stored v1 and v2 manifests."""
from __future__ import annotations

from stemhub.manifests import FileRef, blob_refs, blob_shas

PROJECT_SHA = "f" * 64
KICK_SHA = "a" * 64
SNARE_SHA = "b" * 64


def _v1_manifest() -> dict:
    # As the v1 plugin wrote it: assets under "tracks", paths under "filename".
    return {
        "manifest_version": 1,
        "source_daw": "FL Studio",
        "source_project_filename": "Song.flp",
        "project_file": {"sha256": PROJECT_SHA, "size_bytes": 10, "filename": "Song.flp"},
        "tracks": [
            {"sha256": KICK_SHA, "size_bytes": 20, "filename": "Samples/kick.wav", "name": "kick"},
            {"sha256": SNARE_SHA, "size_bytes": 30, "filename": "Samples/snare.wav", "name": "Snare (layered)"},
        ],
        "mixer_state": {"never": "read"},
    }


def _v2_manifest() -> dict:
    return {
        "manifest_version": 2,
        "source_daw": "FL Studio",
        "source_project_filename": "Song.flp",
        "project_file": {"sha256": PROJECT_SHA, "size_bytes": 10, "path": "Song.flp"},
        "assets": [
            {"sha256": KICK_SHA, "size_bytes": 20, "path": "Samples/kick.wav"},
            {"sha256": SNARE_SHA, "size_bytes": 30, "path": "Samples/snare.wav"},
        ],
    }


def test_blob_refs_reads_a_v1_manifest() -> None:
    project_file, assets = blob_refs(_v1_manifest())

    assert project_file == FileRef(sha256=PROJECT_SHA, size_bytes=10, path="Song.flp", name="Song")
    assert assets == (
        FileRef(sha256=KICK_SHA, size_bytes=20, path="Samples/kick.wav", name="kick", index=0),
        FileRef(sha256=SNARE_SHA, size_bytes=30, path="Samples/snare.wav", name="Snare (layered)", index=1),
    )


def test_blob_refs_reads_a_v2_manifest_and_derives_names_from_paths() -> None:
    project_file, assets = blob_refs(_v2_manifest())

    assert project_file == FileRef(sha256=PROJECT_SHA, size_bytes=10, path="Song.flp", name="Song")
    assert assets == (
        FileRef(sha256=KICK_SHA, size_bytes=20, path="Samples/kick.wav", name="kick", index=0),
        FileRef(sha256=SNARE_SHA, size_bytes=30, path="Samples/snare.wav", name="snare", index=1),
    )


def test_blob_refs_reads_a_manifest_without_a_version_as_v1() -> None:
    manifest = _v1_manifest()
    del manifest["manifest_version"]

    project_file, assets = blob_refs(manifest)

    assert project_file is not None and project_file.path == "Song.flp"
    assert [asset.path for asset in assets] == ["Samples/kick.wav", "Samples/snare.wav"]


def test_blob_refs_does_not_mix_up_the_layouts() -> None:
    # A v2 manifest is never read through the v1 keys, and vice versa.
    v2_with_v1_keys = {
        "manifest_version": 2,
        "project_file": {"sha256": PROJECT_SHA, "size_bytes": 1, "filename": "Song.flp"},
        "tracks": [{"sha256": KICK_SHA, "size_bytes": 1, "filename": "kick.wav"}],
    }

    project_file, assets = blob_refs(v2_with_v1_keys)

    assert project_file == FileRef(sha256=PROJECT_SHA, size_bytes=1, path=None, name=None)
    assert assets == ()


def test_blob_refs_returns_nothing_for_missing_or_unknown_manifests() -> None:
    assert blob_refs(None) == (None, ())
    assert blob_refs([]) == (None, ())
    assert blob_refs({"manifest_version": 99, "assets": [{"sha256": KICK_SHA}]}) == (None, ())


def test_blob_refs_skips_malformed_entries() -> None:
    manifest = {
        "manifest_version": 2,
        "project_file": "not an object",
        "assets": [
            "not an object",
            {"size_bytes": 1, "path": "no-hash.wav"},
            {"sha256": 42, "path": "bad-hash.wav"},
            {"sha256": KICK_SHA, "size_bytes": True, "path": ""},
        ],
    }

    project_file, assets = blob_refs(manifest)

    assert project_file is None
    # Kept for ref counting even though its size and path are unusable, with
    # its position in the stored list (the skipped entries before it still count).
    assert assets == (FileRef(sha256=KICK_SHA, size_bytes=None, path=None, name=None, index=3),)


def test_blob_shas_lists_each_referenced_hash_once() -> None:
    manifest = _v2_manifest()
    manifest["assets"].append({"sha256": KICK_SHA, "size_bytes": 20, "path": "Samples/kick copy.wav"})
    manifest["assets"].append({"sha256": PROJECT_SHA, "size_bytes": 10, "path": "Backup/Song.flp"})

    assert blob_shas(manifest) == {PROJECT_SHA, KICK_SHA, SNARE_SHA}


def test_blob_shas_is_the_same_for_equivalent_v1_and_v2_manifests() -> None:
    assert blob_shas(_v1_manifest()) == blob_shas(_v2_manifest()) == {PROJECT_SHA, KICK_SHA, SNARE_SHA}


def test_blob_shas_is_empty_without_a_manifest() -> None:
    assert blob_shas(None) == set()
    assert blob_shas({}) == set()
