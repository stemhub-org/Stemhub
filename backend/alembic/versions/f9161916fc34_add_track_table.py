"""add track table

Revision ID: f9161916fc34
Revises: ced97673c3f5
Create Date: 2026-03-01 17:18:24.840971

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import UUID


# revision identifiers, used by Alembic.
revision: str = 'f9161916fc34'
down_revision: Union[str, None] = 'ced97673c3f5'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        'track',
        sa.Column('id', UUID(as_uuid=True), primary_key=True, index=True),
        sa.Column('version_id', UUID(as_uuid=True), sa.ForeignKey('version.id'), nullable=False),
        sa.Column('name', sa.String(255), nullable=False),
        sa.Column('file_type', sa.String(50), server_default='.json'),
        sa.Column('storage_path', sa.String(), nullable=True),
        sa.Column('created_at', sa.DateTime(timezone=True), nullable=True),
    )


def downgrade() -> None:
    op.drop_table('track')
