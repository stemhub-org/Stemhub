import asyncio
import uuid
from datetime import datetime, timezone
from unittest.mock import AsyncMock, Mock

import pytest
from fastapi import HTTPException

from stemhub.models import Project, User
from stemhub.routers.projects import create_project, update_project
from stemhub.schemas import ProjectCreate, ProjectUpdate


class _ScalarResult:
    def __init__(self, value):
        self._value = value

    def first(self):
        return self._value


class _ExecuteResult:
    def __init__(self, *, first_value=None, scalar_first_value=None):
        self._first_value = first_value
        self._scalar_first_value = scalar_first_value

    def first(self):
        return self._first_value

    def scalars(self):
        return _ScalarResult(self._scalar_first_value)


def _build_user() -> User:
    return User(
        id=uuid.uuid4(),
        email="producer@example.com",
        username="producer",
        password_hash="hashed-password",
        created_at=datetime.now(timezone.utc),
        is_active=True,
    )


def test_create_project_rejects_duplicate_name_case_insensitive() -> None:
    current_user = _build_user()
    db = Mock()
    db.execute = AsyncMock(return_value=_ExecuteResult(first_value=(uuid.uuid4(),)))
    db.add = Mock()
    db.flush = AsyncMock()
    db.commit = AsyncMock()
    db.refresh = AsyncMock()

    with pytest.raises(HTTPException) as exc:
        asyncio.run(
            create_project(
                project_in=ProjectCreate(name="  demo beat  "),
                current_user=current_user,
                db=db,
            )
        )

    assert exc.value.status_code == 409
    assert exc.value.detail == "Project name already exists"
    db.add.assert_not_called()
    db.flush.assert_not_called()
    db.commit.assert_not_called()


def test_update_project_rejects_rename_to_existing_name() -> None:
    current_user = _build_user()
    current_project = Project(
        id=uuid.uuid4(),
        owner_id=current_user.id,
        name="Current Project",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
    )

    db = Mock()
    db.execute = AsyncMock(
        side_effect=[
            _ExecuteResult(scalar_first_value=current_project),
            _ExecuteResult(first_value=(uuid.uuid4(),)),
        ]
    )
    db.commit = AsyncMock()
    db.refresh = AsyncMock()

    with pytest.raises(HTTPException) as exc:
        asyncio.run(
            update_project(
                project_id=current_project.id,
                project_in=ProjectUpdate(name=" Demo Beat "),
                current_user=current_user,
                db=db,
            )
        )

    assert exc.value.status_code == 409
    assert exc.value.detail == "Project name already exists"
    assert current_project.name == "Current Project"
    db.commit.assert_not_called()
