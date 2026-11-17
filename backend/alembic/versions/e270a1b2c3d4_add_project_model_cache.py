"""Cache extracted project models by project, file hash and extractor version."""

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql

revision = "e270a1b2c3d4"
down_revision = "d6e7f8a9b0c1"
branch_labels = None
depends_on = None


def upgrade():
    op.create_table(
        "project_model",
        sa.Column("project_id", postgresql.UUID(as_uuid=True), nullable=False),
        sa.Column("project_file_sha256", sa.String(64), nullable=False),
        sa.Column("extractor_version", sa.String(96), nullable=False),
        sa.Column("model_json", postgresql.JSONB(), nullable=True),
        sa.Column("error", sa.String(255), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
        sa.ForeignKeyConstraint(
            ["project_id", "project_file_sha256"], ["blob.project_id", "blob.sha256"], ondelete="CASCADE"
        ),
        sa.PrimaryKeyConstraint("project_id", "project_file_sha256", "extractor_version"),
    )


def downgrade():
    op.drop_table("project_model")
