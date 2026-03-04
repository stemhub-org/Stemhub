import uuid
from datetime import datetime, timezone

from sqlalchemy import BigInteger, String
from sqlalchemy.dialects.postgresql import JSONB

from stemhub.models import Version
from stemhub.schemas import VersionCreate, VersionResponse


def test_version_model_exposes_snapshot_metadata_columns() -> None:
    version_table = Version.__table__

    assert "artifact_path" in version_table.c
    assert isinstance(version_table.c.artifact_path.type, String)

    assert "artifact_size_bytes" in version_table.c
    assert isinstance(version_table.c.artifact_size_bytes.type, BigInteger)

    assert "artifact_checksum" in version_table.c
    assert isinstance(version_table.c.artifact_checksum.type, String)

    assert "source_daw" in version_table.c
    assert isinstance(version_table.c.source_daw.type, String)

    assert "source_project_filename" in version_table.c
    assert isinstance(version_table.c.source_project_filename.type, String)

    assert "snapshot_manifest" in version_table.c
    assert isinstance(version_table.c.snapshot_manifest.type, JSONB)


def test_version_create_accepts_snapshot_metadata() -> None:
    payload = VersionCreate(
        commit_message="snapshot commit",
        artifact_path="snapshots/demo/project-v1.flp",
        artifact_size_bytes=1048576,
        artifact_checksum="a" * 64,
        source_daw="FL Studio",
        source_project_filename="project-v1.flp",
        snapshot_manifest={
            "tempo": 140,
            "channels": [{"name": "Kick", "muted": False}],
            "playlist": {"patterns": 8},
        },
    )

    assert payload.artifact_path == "snapshots/demo/project-v1.flp"
    assert payload.artifact_size_bytes == 1048576
    assert payload.snapshot_manifest == {
        "tempo": 140,
        "channels": [{"name": "Kick", "muted": False}],
        "playlist": {"patterns": 8},
    }


def test_version_response_round_trips_snapshot_metadata_from_model() -> None:
    version = Version(
        id=uuid.uuid4(),
        branch_id=uuid.uuid4(),
        parent_version_id=uuid.uuid4(),
        commit_message="snapshot commit",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        deleted_at=None,
        artifact_path="snapshots/demo/project-v1.flp",
        artifact_size_bytes=1048576,
        artifact_checksum="b" * 64,
        source_daw="FL Studio",
        source_project_filename="project-v1.flp",
        snapshot_manifest={"tempo": 128, "playlist": {"arrangement": "demo"}},
    )

    response = VersionResponse.model_validate(version)

    assert response.artifact_path == "snapshots/demo/project-v1.flp"
    assert response.artifact_size_bytes == 1048576
    assert response.artifact_checksum == "b" * 64
    assert response.source_daw == "FL Studio"
    assert response.source_project_filename == "project-v1.flp"
    assert response.snapshot_manifest == {"tempo": 128, "playlist": {"arrangement": "demo"}}
