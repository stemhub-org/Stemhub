"""add is_admin to user

Revision ID: a1b2c3d4e5f6
Revises: 136d2794af3b
Create Date: 2026-03-11 00:00:00.000000

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa

# revision identifiers, used by Alembic.
revision: str = 'a1b2c3d4e5f6'
down_revision: Union[str, None] = '136d2794af3b'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if 'users' in tables:
        user_columns = {column['name'] for column in inspector.get_columns('users')}
        if 'is_admin' not in user_columns:
            op.add_column(
                'users',
                sa.Column('is_admin', sa.Boolean(), nullable=False, server_default=sa.text('false')),
            )


def downgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if 'users' in tables:
        user_columns = {column['name'] for column in inspector.get_columns('users')}
        if 'is_admin' in user_columns:
            op.drop_column('users', 'is_admin')
