"""add version table

Revision ID: ced97673c3f5
Revises: 253ab8a9a44f
Create Date: 2026-03-01 17:14:26.267261

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import UUID


# revision identifiers, used by Alembic.
revision: str = 'ced97673c3f5'
down_revision: Union[str, None] = '253ab8a9a44f'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        'version',
        sa.Column('id', UUID(as_uuid=True), primary_key=True, index=True),
        sa.Column('branch_id', UUID(as_uuid=True), sa.ForeignKey('branch.id'), nullable=False),
        sa.Column('created_by', UUID(as_uuid=True), sa.ForeignKey('users.id'), nullable=True),
        sa.Column('parent_version_id', UUID(as_uuid=True), sa.ForeignKey('version.id'), nullable=True),
        sa.Column('commit_message', sa.String(500), nullable=True),
        sa.Column('created_at', sa.DateTime(timezone=True), nullable=True),
    )


def downgrade() -> None:
    op.drop_table('version')
