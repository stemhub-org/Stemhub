"""Deleting one's account: confirmed by typing the username (and the
password when the account has one), it hides the account and its projects
and signs it out everywhere at once; the data is erased 30 days later.
Signing in again before then (password or Google) cancels the deletion."""
from __future__ import annotations

import asyncio
import uuid
from datetime import datetime, timedelta, timezone
from typing import Any
from unittest.mock import AsyncMock, MagicMock

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from sqlalchemy.dialects import postgresql

from stemhub import account_deletion, auth
from stemhub.account_deletion import DELETION_GRACE, erase_account, is_restorable
from stemhub.database import get_db
from stemhub.models import User
from stemhub.security import UNUSABLE_PASSWORD_HASH, get_password_hash
from stemhub.storage import StorageError

PASSWORD = "a-strong-password"


def _user(password_hash: str | None = None) -> User:
    return User(
        id=uuid.uuid4(), email="demo@example.com", username="demo",
        password_hash=password_hash or get_password_hash(PASSWORD),
        created_at=datetime.now(timezone.utc), is_active=True, is_admin=False, is_deleted=False,
    )


def _delete(user: User, body: dict[str, Any]) -> tuple[Any, MagicMock]:
    db = MagicMock()
    db.execute = AsyncMock()
    db.commit = AsyncMock()
    app = FastAPI()
    app.include_router(auth.router)

    async def override_db():
        yield db

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[auth.get_current_user] = lambda: user
    return TestClient(app).post("/auth/me/delete", json=body), db


def _sql(db: MagicMock) -> list[str]:
    return [
        str(call.args[0].compile(dialect=postgresql.dialect(), compile_kwargs={"literal_binds": True}))
        for call in db.execute.await_args_list
    ]


def test_deleting_hides_the_account_and_its_projects_and_signs_out_everywhere() -> None:
    user = _user()
    response, db = _delete(user, {"confirm_username": "demo", "password": PASSWORD})

    assert response.status_code == 200
    assert user.is_deleted is True
    erased_at = datetime.fromisoformat(response.json()["erased_at"])
    assert erased_at - user.deleted_at == DELETION_GRACE == timedelta(days=30)

    projects, sessions = _sql(db)
    assert projects.startswith("UPDATE project SET is_deleted=true")
    assert f"project.owner_id = '{user.id}'" in projects and "project.is_deleted = false" in projects
    assert sessions.startswith("UPDATE user_session SET revoked_at=")
    assert f"user_session.user_id = '{user.id}'" in sessions and "user_session.revoked_at IS NULL" in sessions
    db.commit.assert_awaited_once()
    assert 'access_token=""' in response.headers["set-cookie"]


@pytest.mark.parametrize(
    "body, detail",
    [
        ({"confirm_username": "Demo", "password": PASSWORD}, "The username you typed does not match your account"),
        ({"confirm_username": "demo", "password": "wrong-password"}, "Incorrect password"),
        ({"confirm_username": "demo"}, "Incorrect password"),
    ],
)
def test_deleting_needs_the_username_and_password(body: dict[str, Any], detail: str) -> None:
    user = _user()
    response, db = _delete(user, body)
    assert response.status_code == 400
    assert response.json()["detail"] == detail
    assert user.is_deleted is False
    db.execute.assert_not_awaited()
    db.commit.assert_not_awaited()


def test_a_google_only_account_confirms_with_its_username_alone() -> None:
    user = _user(password_hash=UNUSABLE_PASSWORD_HASH)
    response, _ = _delete(user, {"confirm_username": "demo"})
    assert response.status_code == 200
    assert user.is_deleted is True


# ── Cancelling by signing in again ──

def _deleted_user(days_ago: float) -> User:
    user = _user()
    user.is_deleted = True
    user.deleted_at = datetime.now(timezone.utc) - timedelta(days=days_ago)
    return user


@pytest.mark.parametrize("days_ago, restorable", [(0, True), (29.9, True), (30.1, False)])
def test_an_account_can_be_restored_for_30_days(days_ago: float, restorable: bool) -> None:
    assert is_restorable(_deleted_user(days_ago)) is restorable
    assert is_restorable(_user()) is True


def _login(user: User | None) -> tuple[Any, MagicMock]:
    found = MagicMock()
    found.scalars.return_value.first.return_value = user
    db = MagicMock()
    db.execute = AsyncMock(return_value=found)
    db.commit = AsyncMock()
    app = FastAPI()
    app.include_router(auth.router)

    async def override_db():
        yield db

    app.dependency_overrides[get_db] = override_db
    response = TestClient(app).post("/auth/login", json={"email": "demo@example.com", "password": PASSWORD})
    return response, db


@pytest.mark.usefixtures("no_login_throttle")
def test_signing_in_within_the_grace_period_restores_the_account_and_its_projects() -> None:
    user = _deleted_user(days_ago=3)
    deleted_at = user.deleted_at
    response, db = _login(user)

    assert response.status_code == 200
    assert response.json()["account_restored"] is True
    assert (user.is_deleted, user.deleted_at) == (False, None)
    restore = next(sql for sql in _sql(db) if sql.startswith("UPDATE project"))
    assert restore.startswith("UPDATE project SET is_deleted=false, deleted_at=NULL")
    assert f"project.owner_id = '{user.id}'" in restore
    # Only the projects the account deletion hid, not ones deleted before.
    assert f"project.deleted_at = '{deleted_at.isoformat(sep=' ')}'" in restore


@pytest.mark.usefixtures("no_login_throttle")
def test_a_normal_sign_in_restores_nothing() -> None:
    response, db = _login(_user())
    assert response.json()["account_restored"] is False
    assert not any(sql.startswith("UPDATE project") for sql in _sql(db))


# ── Erasure after the grace period ──

def _erase_db(project_ids: list[uuid.UUID], storage_uris: list[str]) -> MagicMock:
    def answer(rows: list[Any]) -> MagicMock:
        result = MagicMock()
        result.scalars.return_value.all.return_value = rows
        return result

    answers = [answer(project_ids), answer(storage_uris)] if project_ids else [answer(project_ids)]
    db = MagicMock()
    db.execute = AsyncMock(side_effect=answers + [MagicMock()] * 20)
    db.commit = AsyncMock()
    return db


def _writes(db: MagicMock) -> list[str]:
    return [sql for sql in _sql(db) if not sql.startswith("SELECT")]


def test_erasing_deletes_the_files_then_the_projects_then_the_account() -> None:
    user = _deleted_user(days_ago=31)
    project = uuid.uuid4()
    storage = MagicMock()
    db = _erase_db([project], ["projects/p/blobs/ab/abc"])

    assert asyncio.run(erase_account(db, storage, user)) is True

    storage.delete_project_preview.assert_called_once_with(project)
    storage.delete_blob.assert_called_once_with("projects/p/blobs/ab/abc")
    writes = _writes(db)
    assert [sql.split(" WHERE")[0] for sql in writes] == [
        "DELETE FROM pull_request",
        "UPDATE version SET parent_version_id=NULL",
        "DELETE FROM version",
        "DELETE FROM branch",
        "DELETE FROM collaborator",
        "DELETE FROM project",
        "UPDATE version SET created_by=NULL",
        "UPDATE pull_request SET created_by=NULL",
        "UPDATE pull_request SET closed_by=NULL",
        "DELETE FROM collaborator",
        "DELETE FROM login_attempt",
        "DELETE FROM users",
    ]
    assert f"project.id IN ('{project}')" in writes[5]
    assert f"version.created_by = '{user.id}'" in writes[6]
    assert f"collaborator.user_id = '{user.id}'" in writes[9]
    assert "login_attempt.email = 'demo@example.com'" in writes[10]
    assert f"users.id = '{user.id}'" in writes[11]
    db.commit.assert_awaited_once()


def test_an_account_without_projects_only_loses_its_personal_data() -> None:
    user = _deleted_user(days_ago=31)
    storage = MagicMock()
    db = _erase_db([], [])

    assert asyncio.run(erase_account(db, storage, user)) is True
    storage.delete_blob.assert_not_called()
    assert not any("project" in sql.split(" WHERE")[0] for sql in _writes(db))
    assert _writes(db)[-1].startswith("DELETE FROM users")


@pytest.mark.parametrize(
    "failure",
    # Also errors the storage does not wrap, such as expired bucket credentials.
    [StorageError("bucket unreachable"), OSError("disk full"), RuntimeError("invalid_grant")],
)
def test_a_file_that_cannot_be_deleted_postpones_the_whole_erasure(failure: Exception) -> None:
    user = _deleted_user(days_ago=31)
    storage = MagicMock()
    storage.delete_blob.side_effect = failure
    db = _erase_db([uuid.uuid4()], ["projects/p/blobs/ab/abc"])

    assert asyncio.run(erase_account(db, storage, user)) is False
    assert _writes(db) == []
    db.commit.assert_not_awaited()


def test_only_accounts_past_the_grace_period_are_erased(monkeypatch) -> None:
    due = [_deleted_user(days_ago=31), _deleted_user(days_ago=40)]
    found = MagicMock()
    found.scalars.return_value.all.return_value = due
    db = MagicMock()
    db.execute = AsyncMock(return_value=found)
    erased: list[User] = []

    async def erase(db, storage, user):
        erased.append(user)
        return True

    monkeypatch.setattr(account_deletion, "erase_account", erase)
    assert asyncio.run(account_deletion.erase_due_accounts(db, MagicMock())) == 2
    assert erased == due
    query = _sql(db)[0]
    assert "users.is_deleted = true" in query and "users.deleted_at <=" in query
