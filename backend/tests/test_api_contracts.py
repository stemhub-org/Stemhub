"""JSON field names the frontend and the StemHub plugin rely on.

Pins the vocabulary renames (docs/SPECIFICATION.md §19): versions, not
commits; a version's `message`; `tempo_bpm` in explore; `insert_index` in the
mixer diff.
"""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import get_args

from stemhub.schemas import (
    ActivityActor,
    ActivityBranch,
    ActivityFeedResponse,
    ActivityItem,
    ActivityPullRequest,
    ActivityStatsResponse,
    ActivityType,
    ActivityVersion,
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


def test_activity_feed_event_types() -> None:
    assert set(get_args(ActivityType)) == {
        "version_saved",
        "version_deleted",
        "branch_created",
        "branch_deleted",
        "pull_request_opened",
        "pull_request_merged",
        "pull_request_closed",
    }


def test_activity_feed_item_fields() -> None:
    main = ActivityBranch(id=uuid.uuid4(), name="main", is_deleted=False)
    feature = ActivityBranch(id=uuid.uuid4(), name="feature-fast-tempo", is_deleted=True)
    version = ActivityVersion(
        id=uuid.uuid4(), message="Mixed the drums", source_daw="FL Studio", source_project_filename="beat.flp"
    )
    pull_request = ActivityPullRequest(
        id=uuid.uuid4(), title="Faster tempo", status="OPEN", source_branch=feature, target_branch=main
    )
    item = ActivityItem(
        id=f"version_saved:{version.id}",
        type="version_saved",
        occurred_at=NOW,
        actor=ActivityActor(id=uuid.uuid4(), username="demo"),
        branch=main,
        version=version,
        pull_request=None,
    )
    feed = ActivityFeedResponse(items=[item], next_cursor=None)

    payload = feed.model_dump()
    assert set(payload) == {"items", "next_cursor"}
    assert set(payload["items"][0]) == {
        "id", "type", "occurred_at", "actor", "branch", "version", "pull_request",
    }
    assert set(payload["items"][0]["actor"]) == {"id", "username"}
    assert set(payload["items"][0]["branch"]) == {"id", "name", "is_deleted"}
    assert set(payload["items"][0]["version"]) == {"id", "message", "source_daw", "source_project_filename"}
    assert set(pull_request.model_dump()) == {"id", "title", "status", "source_branch", "target_branch"}
