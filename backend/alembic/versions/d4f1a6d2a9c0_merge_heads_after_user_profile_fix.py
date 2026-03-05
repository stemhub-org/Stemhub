"""merge alembic heads after user profile fix

Revision ID: d4f1a6d2a9c0
Revises: 2b7c42ab9a50, c8b5d9c7e4aa
Create Date: 2026-03-05 14:58:00.000000

"""
from typing import Sequence, Union


# revision identifiers, used by Alembic.
revision: str = "d4f1a6d2a9c0"
down_revision: Union[str, Sequence[str], None] = ("2b7c42ab9a50", "c8b5d9c7e4aa")
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    pass


def downgrade() -> None:
    pass
