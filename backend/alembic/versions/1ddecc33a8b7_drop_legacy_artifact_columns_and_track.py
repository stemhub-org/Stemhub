"""drop legacy artifact columns and track table

Removes the legacy ZIP-bundle artifact flow now that content-addressed
storage (Blob + Version.manifest_json) is the only ingest path. Drops the
Track ORM table, which was never populated — track metadata now lives
inside manifest_json.tracks (see docs/content-addressed-storage.md and
SPECIFICATION.md §7).

Revision ID: 1ddecc33a8b7
Revises: 766188bc9149
Create Date: 2026-09-18

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "1ddecc33a8b7"
down_revision: Union[str, None] = "766188bc9149"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    # Track table: never had any writer. Version.manifest_json.tracks replaces it.
    op.drop_table("track")

    # Legacy version columns tied to the old ZIP-bundle artifact upload flow.
    with op.batch_alter_table("version") as batch:
        batch.drop_column("snapshot_manifest")
        batch.drop_column("artifact_checksum")
        batch.drop_column("artifact_size_bytes")
        batch.drop_column("artifact_path")


def downgrade() -> None:
    with op.batch_alter_table("version") as batch:
        batch.add_column(sa.Column("artifact_path", sa.String(), nullable=True))
        batch.add_column(sa.Column("artifact_size_bytes", sa.BigInteger(), nullable=True))
        batch.add_column(sa.Column("artifact_checksum", sa.String(length=128), nullable=True))
        batch.add_column(sa.Column("snapshot_manifest", sa.dialects.postgresql.JSONB(), nullable=True))

    op.create_table(
        "track",
        sa.Column("id", sa.dialects.postgresql.UUID(as_uuid=True), primary_key=True),
        sa.Column("version_id", sa.dialects.postgresql.UUID(as_uuid=True), sa.ForeignKey("version.id"), nullable=False),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("file_type", sa.String(length=50), server_default=".json"),
        sa.Column("bpm", sa.Integer(), nullable=True),
        sa.Column("key", sa.String(length=10), nullable=True),
        sa.Column("duration", sa.Integer(), nullable=True),
        sa.Column("storage_path", sa.String(), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=True),
    )
