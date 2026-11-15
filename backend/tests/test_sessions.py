"""Server-side sessions: every sign-in records a session, its id travels in
the token ("sid"), a token is only accepted while its session is neither
revoked nor expired, and logging out revokes it. "Remember me" makes the
session and its cookie last 30 days instead of one day and the browser
session."""
from __future__ import annotations

import asyncio
import uuid
from datetime import datetime, timedelta, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import jwt
import pytest
from fastapi import FastAPI, HTTPException, Request
from fastapi.testclient import TestClient
from sqlalchemy.dialects import postgresql

from stemhub.auth import REMEMBERED_SESSION_LIFETIME, SESSION_LIFETIME, get_current_user, router as auth_router
from stemhub.database import get_db
from stemhub.models import User, UserSession
from stemhub.security import ALGORITHM, SECRET_KEY, create_access_token, get_password_hash

PASSWORD = "a-strong-password"

pytestmark = pytest.mark.usefixtures("no_login_throttle")


def _user() -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username="demo",
        password_hash=get_password_hash(PASSWORD),
        created_at=datetime.now(timezone.utc),
        is_active=True,
        is_admin=False,
        is_deleted=False,
    )


def _session(user: User | None = None) -> MagicMock:
    result = MagicMock()
    result.scalars.return_value.first.return_value = user
    session = MagicMock()
    session.execute = AsyncMock(return_value=result)
    session.commit = AsyncMock()
    return session


def _client(session: MagicMock) -> TestClient:
    app = FastAPI()
    app.include_router(auth_router)

    async def override_db():
        yield session

    app.dependency_overrides[get_db] = override_db
    return TestClient(app)


def _sql(statement: Any) -> str:
    return str(statement.compile(dialect=postgresql.dialect(), compile_kwargs={"literal_binds": True}))


def _token(sid: Any = None, **claims: Any) -> str:
    if sid is not None:
        claims["sid"] = str(sid)
    return create_access_token(data={"sub": "demo@example.com", **claims})


def _current_user(token: str | None, session: MagicMock) -> User:
    headers = [(b"authorization", f"Bearer {token}".encode())] if token else []
    request = Request({"type": "http", "headers": headers, "query_string": b""})
    return asyncio.run(get_current_user(request, db=session))


# ── Signing in ──

def test_login_records_a_session_and_puts_its_id_in_the_token() -> None:
    db = _session(_user())
    response = _client(db).post("/auth/login", json={"email": "demo@example.com", "password": PASSWORD})
    assert response.status_code == 200

    recorded = db.add.call_args.args[0]
    assert isinstance(recorded, UserSession)
    claims = jwt.decode(response.json()["access_token"], SECRET_KEY, algorithms=[ALGORITHM])
    assert claims["sid"] == str(recorded.id)
    assert claims["exp"] == int(recorded.expires_at.timestamp())
    assert abs(recorded.expires_at - datetime.now(timezone.utc) - SESSION_LIFETIME) < timedelta(minutes=1)
    db.commit.assert_awaited()


# ── Using a token ──

def test_a_token_is_accepted_only_while_its_session_lives() -> None:
    sid = uuid.uuid4()
    user = _user()
    db = _session(user)
    assert _current_user(_token(sid), db) is user

    sql = _sql(db.execute.await_args.args[0])
    assert f"user_session.id = '{sid}'" in sql
    assert "user_session.revoked_at IS NULL" in sql
    assert "user_session.expires_at > now()" in sql


def test_a_revoked_or_expired_session_is_refused() -> None:
    # The query above finds no live session.
    with pytest.raises(HTTPException) as refused:
        _current_user(_token(uuid.uuid4()), _session(None))
    assert refused.value.status_code == 401


@pytest.mark.parametrize("token", [None, "not-a-jwt", _token(), _token(sid="not-a-uuid")])
def test_tokens_without_a_valid_session_id_are_refused_without_a_lookup(token: str | None) -> None:
    db = _session(_user())
    with pytest.raises(HTTPException) as refused:
        _current_user(token, db)
    assert refused.value.status_code == 401
    db.execute.assert_not_awaited()


# ── Logging out ──

def test_logout_revokes_the_session_of_the_token() -> None:
    sid = uuid.uuid4()
    db = _session()
    response = _client(db).post("/auth/logout", headers={"Authorization": f"Bearer {_token(sid)}"})
    assert response.status_code == 200

    sql = _sql(db.execute.await_args.args[0])
    assert sql.startswith("UPDATE user_session SET revoked_at=now()")
    assert f"user_session.id = '{sid}'" in sql
    db.commit.assert_awaited_once()
    assert 'access_token=""' in response.headers["set-cookie"]


def test_logout_reads_the_cookie_too() -> None:
    sid = uuid.uuid4()
    db = _session()
    client = _client(db)
    client.cookies.set("access_token", _token(sid))
    assert client.post("/auth/logout").status_code == 200
    assert f"user_session.id = '{sid}'" in _sql(db.execute.await_args.args[0])


@pytest.mark.parametrize("headers", [{}, {"Authorization": "Bearer not-a-jwt"}])
def test_logout_without_a_valid_token_still_clears_the_cookie(headers: dict[str, str]) -> None:
    db = _session()
    response = _client(db).post("/auth/logout", headers=headers)
    assert response.status_code == 200
    db.execute.assert_not_awaited()


# ── Remember me ──

def _login(remember_me: bool | None) -> tuple[Any, MagicMock]:
    db = _session(_user())
    body: dict[str, Any] = {"email": "demo@example.com", "password": PASSWORD}
    if remember_me is not None:
        body["remember_me"] = remember_me
    return _client(db).post("/auth/login", json=body), db


@pytest.mark.parametrize(
    "remember_me, lifetime, cookie_max_age",
    [(None, SESSION_LIFETIME, None), (False, SESSION_LIFETIME, None), (True, REMEMBERED_SESSION_LIFETIME, 30 * 24 * 3600)],
)
def test_remember_me_sets_the_session_and_cookie_lifetime(remember_me, lifetime, cookie_max_age) -> None:
    response, db = _login(remember_me)
    assert response.status_code == 200

    recorded = db.add.call_args.args[0]
    assert abs(recorded.expires_at - datetime.now(timezone.utc) - lifetime) < timedelta(minutes=1)
    cookie = response.headers["set-cookie"]
    if cookie_max_age is None:
        # A browser-session cookie: no lifetime, gone when the browser closes.
        assert "Max-Age" not in cookie and "expires" not in cookie.lower()
    else:
        assert f"Max-Age={cookie_max_age}" in cookie


def test_the_plugin_login_body_still_works() -> None:
    # The DAW plugin sends only email and password.
    response, _ = _login(None)
    assert response.json()["token_type"] == "bearer"


# ── Changing the password ──

def _change_password(current_password: str, token: str) -> tuple[Any, MagicMock, User]:
    user = _user()
    db = _session()
    app = FastAPI()
    app.include_router(auth_router)

    async def override_db():
        yield db

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_current_user] = lambda: user
    response = TestClient(app).put(
        "/auth/me/password",
        json={"current_password": current_password, "new_password": "a-new-strong-password"},
        headers={"Authorization": f"Bearer {token}"},
    )
    return response, db, user


def test_changing_the_password_signs_out_the_other_sessions() -> None:
    current = uuid.uuid4()
    response, db, user = _change_password(PASSWORD, _token(current))
    assert response.status_code == 200

    sql = _sql(db.execute.await_args.args[0])
    assert sql.startswith("UPDATE user_session SET revoked_at=now()")
    assert f"user_session.user_id = '{user.id}'" in sql
    assert f"user_session.id != '{current}'" in sql
    assert "user_session.revoked_at IS NULL" in sql
    db.commit.assert_awaited_once()


def test_a_wrong_current_password_signs_out_nobody() -> None:
    response, db, _ = _change_password("wrong-password", _token(uuid.uuid4()))
    assert response.status_code == 400
    db.execute.assert_not_awaited()
    db.commit.assert_not_awaited()
