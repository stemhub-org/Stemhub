"""lock the guessable passwords of Google sign-up accounts

Accounts created through Google sign-in used to get the password
quote(email) + "oauth_dummy", so anyone who knew such a user's email could
log in to their account with /auth/login. This replaces every such hash with
UNUSABLE_PASSWORD_HASH, which no password matches. Users who chose their own
password are left untouched.

Each user's hash is checked with bcrypt, so the upgrade takes roughly a
quarter of a second per user.

Revision ID: 2e0e729975d1
Revises: a3f1c9e2b7d4
Create Date: 2026-11-15

"""
import urllib.parse
from typing import Sequence, Union

import bcrypt
from alembic import op
import sqlalchemy as sa


revision: str = "2e0e729975d1"
down_revision: Union[str, None] = "a3f1c9e2b7d4"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None

# Mirrors stemhub.security.UNUSABLE_PASSWORD_HASH at the time of writing.
UNUSABLE_PASSWORD_HASH = "!"


def _has_guessable_password(email: str, password_hash: str) -> bool:
    guessable = (urllib.parse.quote(email) + "oauth_dummy").encode("utf-8")
    try:
        return bcrypt.checkpw(guessable, password_hash.encode("utf-8"))
    except ValueError:
        return False


def upgrade() -> None:
    connection = op.get_bind()
    users = connection.execute(sa.text("SELECT id, email, password_hash FROM users")).all()
    for user_id, email, password_hash in users:
        if _has_guessable_password(email, password_hash):
            connection.execute(
                sa.text("UPDATE users SET password_hash = :hash WHERE id = :id"),
                {"hash": UNUSABLE_PASSWORD_HASH, "id": user_id},
            )


def downgrade() -> None:
    # The old hashes were a vulnerability; they are not restored.
    pass
