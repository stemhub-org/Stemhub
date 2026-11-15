"""Accounts created through Google sign-in have no usable password.

They used to get the password quote(email) + "oauth_dummy", so knowing a
Google user's email was enough to log in as them with /auth/login.
"""
from __future__ import annotations

import urllib.parse
from pathlib import Path

import bcrypt
from alembic.config import Config
from alembic.script import ScriptDirectory

from stemhub.security import UNUSABLE_PASSWORD_HASH, get_password_hash, verify_password

BACKEND_DIR = Path(__file__).resolve().parents[1]
LOCK_REVISION = "2e0e729975d1"


def _lock_migration():
    config = Config(str(BACKEND_DIR / "alembic.ini"))
    config.set_main_option("script_location", str(BACKEND_DIR / "alembic"))
    return ScriptDirectory.from_config(config).get_revision(LOCK_REVISION).module


def _old_oauth_hash(email: str) -> str:
    return bcrypt.hashpw((urllib.parse.quote(email) + "oauth_dummy").encode(), bcrypt.gensalt(4)).decode()


def test_no_password_matches_the_unusable_hash() -> None:
    for password in ["", "!", "a-password", "demo%40example.comoauth_dummy"]:
        assert verify_password(password, UNUSABLE_PASSWORD_HASH) is False


def test_regular_passwords_still_verify() -> None:
    password_hash = get_password_hash("a-strong-password")
    assert verify_password("a-strong-password", password_hash) is True
    assert verify_password("wrong", password_hash) is False


def test_the_migration_finds_old_google_passwords() -> None:
    migration = _lock_migration()
    assert migration._has_guessable_password("demo@example.com", _old_oauth_hash("demo@example.com"))


def test_the_migration_leaves_chosen_passwords_alone() -> None:
    migration = _lock_migration()
    chosen = bcrypt.hashpw(b"a-strong-password", bcrypt.gensalt(4)).decode()
    assert not migration._has_guessable_password("demo@example.com", chosen)
    assert not migration._has_guessable_password("demo@example.com", UNUSABLE_PASSWORD_HASH)


def test_the_migration_uses_the_current_unusable_hash() -> None:
    assert _lock_migration().UNUSABLE_PASSWORD_HASH == UNUSABLE_PASSWORD_HASH
