"""GET /auth/me/storage: the space each project the user owns takes, summed
from the content-addressed blob table, for the Settings storage tab."""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from unittest.mock import AsyncMock, MagicMock

from fastapi import FastAPI
from fastapi.testclient import TestClient
from sqlalchemy.dialects import postgresql

from stemhub import auth
from stemhub.database import get_db
from stemhub.models import User
from stemhub.storage_usage import storage_usage_query

USER = User(
    id=uuid.uuid4(), email="a@example.com", username="demo", password_hash="x",
    created_at=datetime.now(timezone.utc), is_active=True, is_admin=False,
)


def _client(rows: list[tuple]) -> TestClient:
    result = MagicMock()
    result.all.return_value = rows
    db = MagicMock()
    db.execute = AsyncMock(return_value=result)

    async def override_db():
        yield db

    app = FastAPI()
    app.include_router(auth.router)
    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[auth.get_current_user] = lambda: USER
    return TestClient(app)


def test_storage_lists_each_project_and_the_total() -> None:
    night, sketch = uuid.uuid4(), uuid.uuid4()
    # Rows as the query returns them: id, name, bytes, files; largest first.
    body = _client([(night, "Night Drive", 820_000_000, 212), (sketch, "Empty idea", 0, 0)]).get("/auth/me/storage").json()

    assert body == {
        "total_bytes": 820_000_000,
        "projects": [
            {"id": str(night), "name": "Night Drive", "size_bytes": 820_000_000, "file_count": 212},
            {"id": str(sketch), "name": "Empty idea", "size_bytes": 0, "file_count": 0},
        ],
    }


def test_storage_without_project_is_empty() -> None:
    assert _client([]).get("/auth/me/storage").json() == {"total_bytes": 0, "projects": []}


def test_storage_query_counts_owned_live_projects_with_or_without_files() -> None:
    sql = str(storage_usage_query(USER.id).compile(dialect=postgresql.dialect()))

    # LEFT JOIN: a project without any file is listed with 0, not dropped.
    assert "LEFT OUTER JOIN blob ON blob.project_id = project.id" in sql
    # Owned projects only (not collaborations), and not the deleted ones.
    assert "project.owner_id = " in sql
    assert "project.is_deleted IS false" in sql
    assert "ORDER BY" in sql
