"""add_user_soft_delete

Revision ID: e3a1c9f2b7d0
Revises: c1a2b3d4e5f7
Create Date: 2026-07-12 00:00:00.000000

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = 'e3a1c9f2b7d0'
down_revision: Union[str, None] = 'c1a2b3d4e5f7'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.add_column('users', sa.Column('is_deleted', sa.Boolean(), server_default='false', nullable=False))
    op.add_column('users', sa.Column('deleted_at', sa.DateTime(timezone=True), nullable=True))
    op.create_index(op.f('ix_users_is_deleted'), 'users', ['is_deleted'], unique=False)


def downgrade() -> None:
    op.drop_index(op.f('ix_users_is_deleted'), table_name='users')
    op.drop_column('users', 'deleted_at')
    op.drop_column('users', 'is_deleted')
