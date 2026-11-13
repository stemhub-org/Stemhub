"""rename version.commit_message to message

A version's text is its "message" (SPECIFICATION.md §19). The API still
accepts "commit_message" as an input alias for plugins built before the
rename. Rows holding the placeholder older plugins sent when the user wrote
no message ("Save from plugin") become NULL, which is how "no message" is
stored from now on.

Idempotent in the style of cb5f23d4c1b9: the rename is skipped when the
column already has its target name.

Revision ID: 316caf0fcfa9
Revises: 1ddecc33a8b7
Create Date: 2026-09-29

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "316caf0fcfa9"
down_revision: Union[str, None] = "1ddecc33a8b7"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

# What plugins sent when the user wrote no message.
PLUGIN_PLACEHOLDER_MESSAGE = "Save from plugin"


def _version_columns() -> set[str] | None:
    inspector = sa.inspect(op.get_bind())
    if "version" not in set(inspector.get_table_names()):
        return None
    return {column["name"] for column in inspector.get_columns("version")}


def upgrade() -> None:
    version_columns = _version_columns()
    if version_columns is None:
        return

    if "commit_message" in version_columns and "message" not in version_columns:
        op.alter_column(
            "version",
            "commit_message",
            new_column_name="message",
            existing_type=sa.String(length=500),
            existing_nullable=True,
        )

    version = sa.table("version", sa.column("message", sa.String(length=500)))
    op.execute(
        version.update()
        .where(version.c.message == PLUGIN_PLACEHOLDER_MESSAGE)
        .values(message=None)
    )


def downgrade() -> None:
    # The placeholder cleared by upgrade() cannot be told apart from a version
    # saved without a message, so those rows stay NULL.
    version_columns = _version_columns()
    if version_columns is None:
        return

    if "message" in version_columns and "commit_message" not in version_columns:
        op.alter_column(
            "version",
            "message",
            new_column_name="commit_message",
            existing_type=sa.String(length=500),
            existing_nullable=True,
        )
