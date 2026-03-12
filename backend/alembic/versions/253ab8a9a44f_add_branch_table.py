"""add branch table

Revision ID: 253ab8a9a44f
Revises: 4503f6717f87
Create Date: 2026-03-01 17:13:11.307337

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import UUID


# revision identifiers, used by Alembic.
revision: str = '253ab8a9a44f'
down_revision: Union[str, None] = '4503f6717f87'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        'branch',
        sa.Column('id', UUID(as_uuid=True), primary_key=True, index=True),
        sa.Column('project_id', UUID(as_uuid=True), sa.ForeignKey('project.id'), nullable=False),
        sa.Column('name', sa.String(255), nullable=False),
        sa.Column('created_at', sa.DateTime(timezone=True), nullable=True),
    )


def downgrade() -> None:
    op.drop_table('branch')
