"""merge all heads

Revision ID: c1a2b3d4e5f7
Revises: b9f8e7d6c5a4, b71f10e958d7
Create Date: 2026-03-19 23:30:00.000000

"""
from typing import Sequence, Union

# revision identifiers, used by Alembic.
revision: str = 'c1a2b3d4e5f7'
down_revision: Union[str, Sequence[str], None] = ('b9f8e7d6c5a4', 'b71f10e958d7')
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    pass


def downgrade() -> None:
    pass
