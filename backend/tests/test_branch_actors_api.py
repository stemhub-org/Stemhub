"""Branch endpoints record who created and who deleted a branch (issue #290).

Covers POST /projects/{id}/branches/, DELETE /branches/{id} and the "main"
branch that POST /projects/ creates. Who deleted a version is covered with the
CAS delete test in test_blobs_and_manifest_api.py.

Access control is not under test here: the access helpers are replaced with
fakes that let the current user in, so only the recorded actor is checked.
"""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI
from fastapi.testclient import TestClient

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Branch, Project, User
from stemhub.routers import branches as branches_router_module
from stemhub.routers.branches import router as branches_router
from stemhub.routers.projects import router as projects_router

T0 = datetime(2026, 11, 1, 9, 0, tzinfo=timezone.utc)


def _user(username: str = "demo") -> User:
    return User(
        id=uuid.uuid4(),
        email=f"{uuid.uuid4().hex[:8]}@example.com",
        username=username,
        password_hash="x",
        created_at=T0,
        is_active=True,
        is_deleted=False,
    )


def _project(owner_id: uuid.UUID) -> Project:
    return Project(id=uuid.uuid4(), owner_id=owner_id, name="Demo", created_at=T0, is_deleted=False)


class _FakeScalars:
    def __init__(self, rows: list[Any]) -> None:
        self._rows = rows

    def first(self) -> Any:
        return self._rows[0] if self._rows else None


class _FakeResult:
    def __init__(self, rows: list[Any]) -> None:
        self._rows = rows

    def scalars(self) -> _FakeScalars:
        return _FakeScalars(self._rows)


class FakeSession:
    """Keeps added rows in memory and fills the column defaults the database
    would. execute() only answers the branch lookup of DELETE /branches/{id}."""

    def __init__(self, *, user: User) -> None:
        self.user = user
        self.added: list[Any] = []
        self.branches: list[Branch] = []

    def add(self, obj: Any) -> None:
        self.added.append(obj)
        if isinstance(obj, Branch):
            self.branches.append(obj)

    async def flush(self) -> None:
        for obj in self.added:
            if getattr(obj, "id", None) is None:
                obj.id = uuid.uuid4()
            if getattr(obj, "created_at", None) is None:
                obj.created_at = T0
            if getattr(obj, "is_deleted", None) is None:
                obj.is_deleted = False

    async def commit(self) -> None:
        await self.flush()

    async def refresh(self, obj: Any) -> None:
        await self.flush()

    async def execute(self, stmt) -> _FakeResult:
        return _FakeResult([branch for branch in self.branches if not branch.is_deleted])


def _make_client(session: FakeSession, project: Project, monkeypatch) -> TestClient:
    async def let_in(*, project_id, current_user, db):
        del project_id, current_user, db
        return project

    monkeypatch.setattr(branches_router_module, "get_project_with_write_access", let_in)
    monkeypatch.setattr(branches_router_module, "get_project_with_owner_access", let_in)

    app = FastAPI()
    app.include_router(branches_router)
    app.include_router(projects_router)

    async def override_db():
        yield session

    async def override_user():
        return session.user

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_current_user] = override_user
    return TestClient(app)


def test_create_branch_records_and_returns_its_creator(monkeypatch) -> None:
    session = FakeSession(user=_user())
    project = _project(session.user.id)
    client = _make_client(session, project, monkeypatch)

    response = client.post(f"/projects/{project.id}/branches/", json={"name": "feature-fast-tempo"})

    assert response.status_code == 201, response.text
    assert response.json()["created_by"] == str(session.user.id)
    assert session.branches[0].created_by == session.user.id


def test_delete_branch_records_who_deleted_it(monkeypatch) -> None:
    session = FakeSession(user=_user("editor"))
    project = _project(uuid.uuid4())
    branch = Branch(id=uuid.uuid4(), project_id=project.id, name="old-idea", created_at=T0, is_deleted=False)
    session.branches.append(branch)
    client = _make_client(session, project, monkeypatch)

    response = client.delete(f"/branches/{branch.id}")

    assert response.status_code == 204, response.text
    assert branch.is_deleted is True
    assert branch.deleted_by == session.user.id
    # The creator is left as it was (unknown here).
    assert branch.created_by is None


def test_create_project_records_its_creator_on_the_main_branch(monkeypatch) -> None:
    session = FakeSession(user=_user())
    client = _make_client(session, _project(session.user.id), monkeypatch)

    response = client.post("/projects/", json={"name": "Demo"})

    assert response.status_code == 201, response.text
    [main] = session.branches
    assert main.name == "main"
    assert main.created_by == session.user.id
