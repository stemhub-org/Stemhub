"""Test bootstrap.

Sets required environment variables before the stemhub package is imported.
This is necessary because stemhub.security enforces a strong SECRET_KEY at
module load time (see backend/src/stemhub/security.py).
"""
import os

import pytest

os.environ.setdefault(
    "SECRET_KEY",
    "test-secret-key-for-pytest-runs-not-for-production-use-0123456789",
)


@pytest.fixture(autouse=True)
def no_pwned_passwords_lookup(monkeypatch):
    """Keep tests off the network: no password counts as breached.
    test_pwned_passwords.py covers the real lookup with a fake API."""
    from stemhub import auth

    async def never_pwned(password):
        return False

    monkeypatch.setattr(auth, "is_password_pwned", never_pwned)


@pytest.fixture
def no_login_throttle(monkeypatch):
    """For tests of sign-in itself, whose fake database does not answer the
    throttle's queries. Throttling has its own tests (test_login_throttle.py)."""
    from stemhub import auth

    async def allowed(*args, **kwargs):
        return None

    monkeypatch.setattr(auth, "ensure_login_allowed", allowed)
