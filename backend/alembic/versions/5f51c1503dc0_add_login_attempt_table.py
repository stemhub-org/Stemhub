"""add login_attempt table

Email + password sign-in attempts, kept to lock an email out after
repeated failures and to limit failures per IP (login_throttle.py).

Revision ID: 5f51c1503dc0
Revises: 958a62c01475
Create Date: 2026-11-15

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "5f51c1503dc0"
down_revision: Union[str, None] = "958a62c01475"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "login_attempt",
        sa.Column("id", sa.UUID(), nullable=False),
        sa.Column("email", sa.String(), nullable=False),
        sa.Column("ip", sa.String(length=45), nullable=True),
        sa.Column("succeeded", sa.Boolean(), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index("ix_login_attempt_email_created_at", "login_attempt", ["email", "created_at"], unique=False)
    op.create_index("ix_login_attempt_ip_created_at", "login_attempt", ["ip", "created_at"], unique=False)


def downgrade() -> None:
    op.drop_index("ix_login_attempt_ip_created_at", table_name="login_attempt")
    op.drop_index("ix_login_attempt_email_created_at", table_name="login_attempt")
    op.drop_table("login_attempt")
