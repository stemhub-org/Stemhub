"""Test bootstrap.

Sets required environment variables before the stemhub package is imported.
This is necessary because stemhub.security enforces a strong SECRET_KEY at
module load time (see backend/src/stemhub/security.py).
"""
import os

os.environ.setdefault(
    "SECRET_KEY",
    "test-secret-key-for-pytest-runs-not-for-production-use-0123456789",
)
