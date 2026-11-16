"""make usernames unique regardless of case

The existing unique index on users.username only rejects exact duplicates,
so "Hubert" and "hubert" could coexist. This adds a unique index on
lower(username). The upgrade stops with the conflicting names listed if the
table already holds case-insensitive duplicates, so they can be renamed first.

Revision ID: a3f1c9e2b7d4
Revises: 316caf0fcfa9
Create Date: 2026-11-15

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "a3f1c9e2b7d4"
down_revision: Union[str, None] = "316caf0fcfa9"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    duplicates = op.get_bind().execute(
        sa.text(
            "SELECT lower(username) FROM users "
            "GROUP BY lower(username) HAVING count(*) > 1"
        )
    ).scalars().all()
    if duplicates:
        raise RuntimeError(
            "Cannot add uq_users_username_lower: these usernames differ only "
            f"by case and must be renamed first: {', '.join(duplicates)}"
        )
    op.create_index(
        "uq_users_username_lower",
        "users",
        [sa.text("lower(username)")],
        unique=True,
    )


def downgrade() -> None:
    op.drop_index("uq_users_username_lower", table_name="users")
