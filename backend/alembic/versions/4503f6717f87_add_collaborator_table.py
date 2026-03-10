"""add collaborator table

Revision ID: 4503f6717f87
Revises: 7ccbce320c45
Create Date: 2026-03-01 16:56:13.178361

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects.postgresql import UUID


# revision identifiers, used by Alembic.
revision: str = '4503f6717f87'
down_revision: Union[str, None] = '7ccbce320c45'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        'collaborator',
        sa.Column('project_id', UUID(as_uuid=True), sa.ForeignKey('project.id'), primary_key=True),
        sa.Column('user_id', UUID(as_uuid=True), sa.ForeignKey('users.id'), primary_key=True),
        sa.Column('role', sa.String(50), nullable=False, server_default='Viewer'),
        sa.Column('created_at', sa.DateTime(timezone=True), nullable=True),
    )


def downgrade() -> None:
    op.drop_table('collaborator')
