import uuid
from datetime import datetime, timezone

from fastapi import FastAPI
from fastapi.testclient import TestClient

from stemhub.auth import get_current_user, get_current_admin_user
from stemhub.database import get_db
from stemhub.models import User
from stemhub.routers.admin import router as admin_router


# ── Helpers ──

def _build_user(*, is_admin: bool = False) -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username="demo",
        password_hash="hashed-password",
        created_at=datetime.now(timezone.utc),
        is_active=True,
        is_admin=is_admin,
    )


class DummyAsyncSession:
    """Minimal async-session stub that records queries without a real DB."""

    def __init__(self, *, user_count: int = 5, project_count: int = 3, users: list[User] | None = None) -> None:
        self._user_count = user_count
        self._project_count = project_count
        self._users = users or []

    class _ScalarResult:
        def __init__(self, value):
            self._value = value

        def scalar(self):
            return self._value

        def all(self):
            return self._value if isinstance(self._value, list) else [self._value]

    class _ScalarsProxy:
        def __init__(self, rows):
            self._rows = rows

        def all(self):
            return self._rows

    class _Result:
        def __init__(self, value):
            self._value = value

        def scalar(self):
            return self._value

        def scalars(self):
            return DummyAsyncSession._ScalarsProxy(self._value if isinstance(self._value, list) else [self._value])

    async def execute(self, stmt):
        stmt_str = str(stmt)
        if "count" in stmt_str.lower() and "users" in stmt_str.lower():
            return self._ScalarResult(self._user_count)
        if "count" in stmt_str.lower() and "project" in stmt_str.lower():
            return self._ScalarResult(self._project_count)
        # list query – return users
        return self._Result(self._users)


def _create_test_client(*, current_user: User | None = None, session: DummyAsyncSession | None = None):
    app = FastAPI()
    app.include_router(admin_router)

    db_session = session or DummyAsyncSession()

    async def override_db():
        yield db_session

    app.dependency_overrides[get_db] = override_db

    if current_user is not None:
        async def override_current_user():
            return current_user

        app.dependency_overrides[get_current_user] = override_current_user

    client = TestClient(app)
    return client


# ── Tests ──

def test_admin_stats_returns_counts():
    admin = _build_user(is_admin=True)
    session = DummyAsyncSession(user_count=10, project_count=7)
    client = _create_test_client(current_user=admin, session=session)

    response = client.get("/api/admin/stats")

    assert response.status_code == 200
    data = response.json()
    assert data["total_users"] == 10
    assert data["total_projects"] == 7


def test_admin_users_returns_paginated_list():
    admin = _build_user(is_admin=True)
    users = [_build_user(is_admin=False) for _ in range(3)]
    session = DummyAsyncSession(users=users)
    client = _create_test_client(current_user=admin, session=session)

    response = client.get("/api/admin/users?limit=10&offset=0")

    assert response.status_code == 200
    data = response.json()
    assert len(data) == 3


def test_admin_stats_rejects_non_admin():
    regular_user = _build_user(is_admin=False)
    client = _create_test_client(current_user=regular_user)

    response = client.get("/api/admin/stats")

    assert response.status_code == 403
    assert response.json()["detail"] == "Admin privileges required"


def test_admin_users_rejects_non_admin():
    regular_user = _build_user(is_admin=False)
    client = _create_test_client(current_user=regular_user)

    response = client.get("/api/admin/users")

    assert response.status_code == 403
    assert response.json()["detail"] == "Admin privileges required"


def test_admin_stats_rejects_unauthenticated():
    client = _create_test_client()

    response = client.get("/api/admin/stats")

    assert response.status_code == 401
    assert response.json()["detail"] == "Could not validate credentials"
