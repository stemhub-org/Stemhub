"""record who creates and deletes branches and versions

Adds branch.created_by, branch.deleted_by and version.deleted_by (issue #290),
so the activity feed can name who created or deleted a branch and who deleted
a version. version.created_by already exists (2b7c42ab9a50).

All three are nullable and existing rows stay NULL: nothing records who did
those actions before, so there is nothing to backfill from.

The foreign keys are ON DELETE SET NULL: deleting a user (account erasure)
clears these columns instead of failing, and the branch or version stays.

Batch mode so the migration also runs on SQLite (test_migrations.py), which
cannot add a foreign key with ALTER TABLE. On PostgreSQL it emits plain
ALTER TABLE statements. Idempotent in the style of 316caf0fcfa9: a column that
already exists is skipped.

Revision ID: c021108cacff
Revises: 316caf0fcfa9
Create Date: 2026-11-16

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "c021108cacff"
down_revision: Union[str, None] = "316caf0fcfa9"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

# (table, column) pairs this revision adds, each a nullable FK to users.id.
ACTOR_COLUMNS = (
    ("branch", "created_by"),
    ("branch", "deleted_by"),
    ("version", "deleted_by"),
)


def _foreign_key_name(table: str, column: str) -> str:
    return f"fk_{table}_{column}_users_id"


def _columns(table: str) -> set[str] | None:
    inspector = sa.inspect(op.get_bind())
    if table not in set(inspector.get_table_names()):
        return None
    return {column["name"] for column in inspector.get_columns(table)}


def upgrade() -> None:
    for table in ("branch", "version"):
        existing = _columns(table)
        if existing is None:
            continue
        missing = [column for t, column in ACTOR_COLUMNS if t == table and column not in existing]
        if not missing:
            continue
        with op.batch_alter_table(table) as batch:
            for column in missing:
                batch.add_column(sa.Column(column, sa.UUID(), nullable=True))
                batch.create_foreign_key(
                    _foreign_key_name(table, column), "users", [column], ["id"], ondelete="SET NULL"
                )


def downgrade() -> None:
    for table in ("branch", "version"):
        existing = _columns(table)
        if existing is None:
            continue
        present = [column for t, column in ACTOR_COLUMNS if t == table and column in existing]
        if not present:
            continue
        with op.batch_alter_table(table) as batch:
            for column in present:
                batch.drop_constraint(_foreign_key_name(table, column), type_="foreignkey")
                batch.drop_column(column)
