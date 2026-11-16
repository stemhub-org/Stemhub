"""store emails in lowercase and make them unique regardless of case

Foo@x.com and foo@x.com could be two accounts, and signing in failed when
the capitals were typed differently. Existing emails are trimmed and
lowercased, and a unique index on lower(email) keeps it that way. The
upgrade stops with the conflicting emails listed if two accounts differ
only by case, so they can be merged or changed first.

Revision ID: c16b2f7519ee
Revises: 57279bedec89
Create Date: 2026-11-15

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


revision: str = "c16b2f7519ee"
down_revision: Union[str, None] = "57279bedec89"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    connection = op.get_bind()
    duplicates = connection.execute(
        sa.text(
            "SELECT lower(btrim(email)) FROM users "
            "GROUP BY lower(btrim(email)) HAVING count(*) > 1"
        )
    ).scalars().all()
    if duplicates:
        raise RuntimeError(
            "Cannot lowercase emails: these accounts differ only by case and "
            f"must be merged or changed first: {', '.join(duplicates)}"
        )
    connection.execute(
        sa.text("UPDATE users SET email = lower(btrim(email)) WHERE email <> lower(btrim(email))")
    )
    op.create_index(
        "uq_users_email_lower",
        "users",
        [sa.text("lower(email)")],
        unique=True,
    )


def downgrade() -> None:
    # The original capitalization is not kept, so emails stay lowercase.
    op.drop_index("uq_users_email_lower", table_name="users")
