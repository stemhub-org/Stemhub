import os
import bcrypt
import jwt
from datetime import datetime, timedelta, timezone

_INSECURE_DEFAULTS = {"", "temporary-secret-key-change-it", "replace_with_secret", "changeme"}


def _load_secret_key() -> str:
    key = (os.getenv("SECRET_KEY") or "").strip()
    if key in _INSECURE_DEFAULTS:
        raise RuntimeError(
            "SECRET_KEY environment variable must be set to a strong, non-default value "
            "(minimum 32 characters). Refusing to start with an insecure default."
        )
    if len(key) < 32:
        raise RuntimeError(
            "SECRET_KEY must be at least 32 characters long. Generate one with "
            "`python -c 'import secrets; print(secrets.token_urlsafe(48))'`."
        )
    return key


SECRET_KEY = _load_secret_key()
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 60 * 24  # 24 hours


def verify_password(plain_password: str, password_hash: str) -> bool:
    try:
        return bcrypt.checkpw(plain_password.encode('utf-8'), password_hash.encode('utf-8'))
    except ValueError:
        return False


def get_password_hash(password: str) -> str:
    salt = bcrypt.gensalt()
    return bcrypt.hashpw(password.encode('utf-8'), salt).decode('utf-8')


def create_access_token(data: dict) -> str:
    to_encode = data.copy()
    expire = datetime.now(timezone.utc) + timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES)
    to_encode.update({"exp": expire})
    return jwt.encode(to_encode, SECRET_KEY, algorithm=ALGORITHM)
