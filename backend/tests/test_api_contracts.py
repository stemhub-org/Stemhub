"""JSON field names the frontend and the StemHub plugin rely on.

Pins the vocabulary renames (docs/SPECIFICATION.md §19): versions, not
commits; a version's `message`; `tempo_bpm` in explore; `insert_index` in the
mixer diff.
"""
from __future__ import annotations

import uuid
from datetime import datetime, timezone

from stemhub.schemas import (
    ActivityStatsResponse,
    AssetSummary,
    ContributorStats,
    ExploreProjectResponse,
    MixerDiffChange,
    OwnerSummary,
    VersionResponse,
    VersionWithAuthor,
)

NOW = datetime(2026, 9, 29, tzinfo=timezone.utc)


def test_activity_stats_count_versions() -> None:
    stats = ActivityStatsResponse(daily_activity=[], total_versions=3, total_contributors=1)

    assert stats.model_dump() == {"daily_activity": [], "total_versions": 3, "total_contributors": 1}


def test_top_contributors_count_versions() -> None:
    contributor = ContributorStats(user_id=uuid.uuid4(), username="demo", initials="DE", versions=4)

    assert set(contributor.model_dump()) == {"user_id", "username", "initials", "versions"}


def test_explore_projects_expose_tempo_bpm() -> None:
    project = ExploreProjectResponse(
        id=uuid.uuid4(),
        name="Demo",
        category="General",
        created_at=NOW,
        owner=OwnerSummary(id=uuid.uuid4(), username="demo"),
        tempo_bpm=128.5,
    )

    payload = project.model_dump()
    assert payload["tempo_bpm"] == 128.5
    assert "bpm" not in payload


def test_version_payloads_use_message() -> None:
    summary = VersionWithAuthor(id=uuid.uuid4(), message="Mixed the drums", created_at=NOW, branch_name="main")
    version = VersionResponse(
        id=uuid.uuid4(), branch_id=uuid.uuid4(), message=None, created_at=NOW, is_deleted=False
    )

    assert summary.model_dump()["message"] == "Mixed the drums"
    assert "commit_message" not in summary.model_dump()
    assert "message" in version.model_dump()
    assert "commit_message" not in version.model_dump()


def test_asset_summary_fields() -> None:
    asset = AssetSummary(id=f"{'a' * 64}:0", path="Samples/kick.wav", name="kick", file_type="wav", size_bytes=1)

    assert set(asset.model_dump()) == {"id", "path", "name", "file_type", "size_bytes"}


def test_mixer_diff_change_allows_a_project_level_change_without_an_insert() -> None:
    change = MixerDiffChange(
        type="project_file_changed",
        insert_index=None,
        before={"sha256": "a", "size_bytes": 1},
        after={"sha256": "b", "size_bytes": 2},
        message="The FL Studio project file changed.",
    )

    payload = change.model_dump()
    assert payload["insert_index"] is None
    assert "insert_iid" not in payload
