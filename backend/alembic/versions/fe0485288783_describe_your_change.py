"""describe your change

Revision ID: fe0485288783
Revises: 58dbef40b2ff
Create Date: 2026-03-03 16:54:27.342720

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = 'fe0485288783'
down_revision: Union[str, None] = '58dbef40b2ff'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    # Compatibility placeholder to keep local Alembic history connected.
    pass


def downgrade() -> None:
    pass
