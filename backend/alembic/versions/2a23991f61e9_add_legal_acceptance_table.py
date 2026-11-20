"""add legal_acceptance table

Proof of consent: which version of the Terms of Service and the Privacy
Policy each user accepted, and when. Accounts created before have none.

Revision ID: 2a23991f61e9
Revises: 68aab61727ac
Create Date: 2026-11-15

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "2a23991f61e9"
down_revision: Union[str, None] = "68aab61727ac"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        "legal_acceptance",
        sa.Column("id", sa.UUID(), nullable=False),
        sa.Column("user_id", sa.UUID(), nullable=False),
        sa.Column("document", sa.String(length=32), nullable=False),
        sa.Column("version", sa.String(length=32), nullable=False),
        sa.Column("accepted_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.CheckConstraint("document IN ('terms', 'privacy')", name="ck_legal_acceptance_document"),
        sa.ForeignKeyConstraint(["user_id"], ["users.id"], ondelete="CASCADE"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("user_id", "document", "version", name="uq_legal_acceptance_user_document_version"),
    )
    op.create_index(op.f("ix_legal_acceptance_user_id"), "legal_acceptance", ["user_id"], unique=False)


def downgrade() -> None:
    op.drop_index(op.f("ix_legal_acceptance_user_id"), table_name="legal_acceptance")
    op.drop_table("legal_acceptance")
