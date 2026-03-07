"""add missing user profile columns

Revision ID: c8b5d9c7e4aa
Revises: cb5f23d4c1b9
Create Date: 2026-03-05 14:45:00.000000

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "c8b5d9c7e4aa"
down_revision: Union[str, None] = "2b7c42ab9a50"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if "users" not in tables:
        return

    user_columns = {column["name"] for column in inspector.get_columns("users")}

    if "avatar_url" not in user_columns:
        op.add_column("users", sa.Column("avatar_url", sa.String(), nullable=True))
    if "bio" not in user_columns:
        op.add_column("users", sa.Column("bio", sa.Text(), nullable=True))
    if "location" not in user_columns:
        op.add_column("users", sa.Column("location", sa.String(), nullable=True))
    if "website" not in user_columns:
        op.add_column("users", sa.Column("website", sa.String(), nullable=True))
    if "created_at" not in user_columns:
        op.add_column(
            "users",
            sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.text("now()")),
        )
    if "is_active" not in user_columns:
        op.add_column(
            "users",
            sa.Column("is_active", sa.Boolean(), nullable=False, server_default=sa.text("true")),
        )


def downgrade() -> None:
    bind = op.get_bind()
    inspector = sa.inspect(bind)
    tables = set(inspector.get_table_names())

    if "users" not in tables:
        return

    user_columns = {column["name"] for column in inspector.get_columns("users")}

    if "is_active" in user_columns:
        op.drop_column("users", "is_active")
    if "created_at" in user_columns:
        op.drop_column("users", "created_at")
    if "website" in user_columns:
        op.drop_column("users", "website")
    if "location" in user_columns:
        op.drop_column("users", "location")
    if "bio" in user_columns:
        op.drop_column("users", "bio")
    if "avatar_url" in user_columns:
        op.drop_column("users", "avatar_url")
