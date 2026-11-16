"""add rate_limit_hit table

Uses of rate-limited actions per client (rate_limit.py), starting with
sign-up: 10 attempts per IP per hour.

Revision ID: 68aab61727ac
Revises: 5f51c1503dc0
Create Date: 2026-11-15

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "68aab61727ac"
down_revision: Union[str, None] = "5f51c1503dc0"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "rate_limit_hit",
        sa.Column("id", sa.UUID(), nullable=False),
        sa.Column("action", sa.String(length=64), nullable=False),
        sa.Column("key", sa.String(length=255), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index(
        "ix_rate_limit_hit_action_key_created_at", "rate_limit_hit", ["action", "key", "created_at"], unique=False
    )


def downgrade() -> None:
    op.drop_index("ix_rate_limit_hit_action_key_created_at", table_name="rate_limit_hit")
    op.drop_table("rate_limit_hit")
