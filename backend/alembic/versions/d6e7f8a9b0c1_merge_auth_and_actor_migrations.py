"""merge authentication and branch/version actor migrations

Join the independent migration chains from #298 and #300 without changing
either history, including databases that have already applied either head.

Revision ID: d6e7f8a9b0c1
Revises: 2a23991f61e9, c021108cacff
Create Date: 2026-11-16

"""
from typing import Sequence, Union


revision: str = "d6e7f8a9b0c1"
down_revision: Union[str, Sequence[str], None] = ("2a23991f61e9", "c021108cacff")
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    pass


def downgrade() -> None:
    pass
