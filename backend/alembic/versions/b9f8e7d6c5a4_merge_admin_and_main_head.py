"""merge admin and main head

Revision ID: b9f8e7d6c5a4
Revises: e70ee25e901a, a1b2c3d4e5f6
Create Date: 2026-03-11 20:00:00.000000

"""
from typing import Sequence, Union

# revision identifiers, used by Alembic.
revision: str = 'b9f8e7d6c5a4'
down_revision: Union[str, Sequence[str], None] = ('e70ee25e901a', 'a1b2c3d4e5f6')
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    pass


def downgrade() -> None:
    pass
