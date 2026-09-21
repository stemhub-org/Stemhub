"""add_blob_and_manifest

Revision ID: f7d2c1a48901
Revises: e3a1c9f2b7d0
Create Date: 2026-07-12 00:05:00.000000

Adds content-addressed Blob table (project-scoped) and manifest_json/manifest_version
columns to Version. See docs/content-addressed-storage.md for the design rationale.
"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql


revision: str = 'f7d2c1a48901'
down_revision: Union[str, None] = 'e3a1c9f2b7d0'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        'blob',
        sa.Column('project_id', postgresql.UUID(as_uuid=True), nullable=False),
        sa.Column('sha256', sa.String(length=64), nullable=False),
        sa.Column('size_bytes', sa.BigInteger(), nullable=False),
        sa.Column('mime_type', sa.String(length=80), nullable=True),
        sa.Column('storage_uri', sa.Text(), nullable=False),
        sa.Column('created_at', sa.DateTime(timezone=True), server_default=sa.text('now()'), nullable=False),
        sa.Column('ref_count', sa.Integer(), server_default='0', nullable=False),
        sa.ForeignKeyConstraint(['project_id'], ['project.id'], ondelete='CASCADE'),
        sa.PrimaryKeyConstraint('project_id', 'sha256'),
    )
    op.create_index('ix_blob_ref_count', 'blob', ['ref_count'], unique=False)

    op.add_column('version', sa.Column('manifest_json', postgresql.JSONB(astext_type=sa.Text()), nullable=True))
    op.add_column('version', sa.Column('manifest_version', sa.Integer(), nullable=True))


def downgrade() -> None:
    op.drop_column('version', 'manifest_version')
    op.drop_column('version', 'manifest_json')
    op.drop_index('ix_blob_ref_count', table_name='blob')
    op.drop_table('blob')
