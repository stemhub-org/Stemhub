"""merge heads (recreated to fix crash)

Revision ID: d1e2f3a4b5c6
Revises: 2b7c42ab9a50, c8b5d9c7e4aa
Create Date: 2026-03-05 15:15:00.000000

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = 'd1e2f3a4b5c6'
down_revision: Union[str, None] = ('2b7c42ab9a50', 'c8b5d9c7e4aa')
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    pass


def downgrade() -> None:
    pass
