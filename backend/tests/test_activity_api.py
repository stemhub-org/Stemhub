"""Tests for GET /projects/{project_id}/activity (issue #217).

The feed is derived from versions, branches and pull requests; see
stemhub.activity. Covers each activity type, ordering, cursor pagination
(including items sharing a timestamp), the `types` filter, `limit` bounds,
and the same read access as the stats endpoints.
"""
from __future__ import annotations

import operator
import uuid
from datetime import datetime, timedelta, timezone
from typing import Any

import pytest
from fastapi import FastAPI
from fastapi.testclient import TestClient
from sqlalchemy.sql import operators
from sqlalchemy.sql.elements import (
    BinaryExpression,
    BindParameter,
    BooleanClauseList,
    False_,
    Grouping,
    Null,
    True_,
    UnaryExpression,
)

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Branch, Collaborator, Project, PullRequest, User, Version
from stemhub.routers.activity import router as activity_router
from stemhub.routers.stats import router as stats_router

T0 = datetime(2026, 11, 1, 9, 0, tzinfo=timezone.utc)


def _at(minutes: int) -> datetime:
    return T0 + timedelta(minutes=minutes)


# ── Fixtures ──


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


def _project(owner_id: uuid.UUID, *, is_public: bool = False) -> Project:
    return Project(
        id=uuid.uuid4(),
        owner_id=owner_id,
        name="Demo",
        created_at=T0,
        is_deleted=False,
        is_public=is_public,
    )


def _branch(
    project_id: uuid.UUID,
    name: str = "main",
    *,
    created_at: datetime = T0,
    deleted_at: datetime | None = None,
) -> Branch:
    return Branch(
        id=uuid.uuid4(),
        project_id=project_id,
        name=name,
        created_at=created_at,
        is_deleted=deleted_at is not None,
        deleted_at=deleted_at,
    )


# ── Fake session ──


_COMPARISONS = {
    operators.eq: operator.eq,
    operators.ne: operator.ne,
    operators.lt: operator.lt,
    operators.le: operator.le,
    operators.gt: operator.gt,
    operators.ge: operator.ge,
    operators.in_op: lambda left, right: left in right,
}


class FakeSession:
    """In-memory stand-in for AsyncSession. Statements are answered by
    evaluating their WHERE (AND/OR of comparisons, IN, IS [NOT] NULL), ORDER
    BY and LIMIT against the in-memory rows, with SQL's NULL semantics for
    comparisons. Anything else raises so a new query shape fails loudly."""

    def __init__(self, *, user: User) -> None:
        self.user = user
        self.users: list[User] = [user]
        self.projects: list[Project] = []
        self.collaborators: list[Collaborator] = []
        self.branches: list[Branch] = []
        self.versions: list[Version] = []
        self.pull_requests: list[PullRequest] = []
        self.statements: list[Any] = []

    async def execute(self, stmt):
        self.statements.append(stmt)
        return _FakeResult(self, stmt)

    def queries_for(self, entity: type) -> int:
        return sum(1 for stmt in self.statements if stmt.column_descriptions[0]["entity"] is entity)


class _FakeResult:
    def __init__(self, session: FakeSession, stmt) -> None:
        self.session = session
        self.stmt = stmt

    def _match(self) -> list[Any]:
        entity = self.stmt.column_descriptions[0]["entity"]
        s = self.session
        rows_by_entity = {
            User: s.users,
            Project: s.projects,
            Collaborator: s.collaborators,
            Branch: s.branches,
            Version: s.versions,
            PullRequest: s.pull_requests,
        }
        if entity not in rows_by_entity:
            raise NotImplementedError(f"FakeSession does not know how to answer: {entity}")
        rows = [row for row in rows_by_entity[entity] if _evaluate(row, self.stmt.whereclause)]
        for order in reversed(list(self.stmt._order_by_clauses)):
            if isinstance(order, UnaryExpression):
                column, descending = order.element, order.modifier is operators.desc_op
            else:
                column, descending = order, False
            rows.sort(key=lambda row: getattr(row, column.key), reverse=descending)
        if self.stmt._limit is not None:
            rows = rows[: self.stmt._limit]
        return rows

    def scalars(self):
        return _FakeScalars(self._match())

    def scalar_one_or_none(self):
        rows = self._match()
        return rows[0] if rows else None


class _FakeScalars:
    def __init__(self, rows: list[Any]) -> None:
        self._rows = rows

    def first(self):
        return self._rows[0] if self._rows else None

    def all(self):
        return self._rows


def _evaluate(row: Any, clause) -> bool:
    if clause is None:
        return True
    if isinstance(clause, Grouping):
        return _evaluate(row, clause.element)
    if isinstance(clause, BooleanClauseList):
        results = [_evaluate(row, sub) for sub in clause.clauses]
        if clause.operator is operators.and_:
            return all(results)
        if clause.operator is operators.or_:
            return any(results)
    if isinstance(clause, BinaryExpression):
        left = getattr(row, clause.left.key)
        right = _value(clause.right)
        if clause.operator is operators.is_:
            return left is right
        if clause.operator is operators.is_not:
            return left is not right
        if clause.operator in _COMPARISONS:
            if left is None or right is None:
                return False  # SQL: a comparison with NULL is never true
            return _COMPARISONS[clause.operator](left, right)
    raise NotImplementedError(f"FakeSession does not support clause {clause!r}")


def _value(right) -> Any:
    if isinstance(right, (True_, False_)):
        return isinstance(right, True_)
    if isinstance(right, Null):
        return None
    if isinstance(right, BindParameter):
        return right.value
    raise NotImplementedError(f"FakeSession does not support operand {right!r}")


# ── Test client factory ──


def _make_client(session: FakeSession) -> TestClient:
    app = FastAPI()
    app.include_router(activity_router)
    app.include_router(stats_router)

    async def override_db():
        yield session

    async def override_user():
        return session.user

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_current_user] = override_user
    return TestClient(app)


def _owned_project_session(*, is_public: bool = False) -> tuple[FakeSession, Project, Branch]:
    user = _user()
    project = _project(user.id, is_public=is_public)
    main = _branch(project.id, "main")
    session = FakeSession(user=user)
    session.projects.append(project)
    session.branches.append(main)
    return session, project, main


def _activity(client: TestClient, project: Project, **params: Any):
    return client.get(f"/projects/{project.id}/activity", params=params)


# ── Access ──


def test_activity_of_a_project_without_events_is_empty() -> None:
    session, project, _ = _owned_project_session()
    session.branches.clear()
    client = _make_client(session)

    response = _activity(client, project)

    assert response.status_code == 200, response.text
    assert response.json() == {"items": [], "next_cursor": None}


def test_activity_returns_404_for_unknown_project() -> None:
    session, _, _ = _owned_project_session()
    client = _make_client(session)

    response = client.get(f"/projects/{uuid.uuid4()}/activity")

    assert response.status_code == 404


def test_activity_returns_404_for_soft_deleted_project() -> None:
    session, project, _ = _owned_project_session()
    project.is_deleted = True
    client = _make_client(session)

    response = _activity(client, project)

    assert response.status_code == 404


def test_activity_of_private_project_is_hidden_from_non_members_like_stats() -> None:
    session, project, _ = _owned_project_session()
    session.user = _user("stranger")
    client = _make_client(session)

    activity = _activity(client, project)
    stats = client.get(f"/projects/{project.id}/stats/activity")

    assert activity.status_code == 404
    assert activity.status_code == stats.status_code


def test_activity_of_private_project_is_readable_by_a_viewer() -> None:
    session, project, _ = _owned_project_session()
    viewer = _user("viewer")
    session.users.append(viewer)
    session.collaborators.append(Collaborator(project_id=project.id, user_id=viewer.id, role="Viewer"))
    session.user = viewer
    client = _make_client(session)

    response = _activity(client, project)

    assert response.status_code == 200, response.text


def test_activity_of_public_project_is_readable_by_anyone_signed_in() -> None:
    session, project, _ = _owned_project_session(is_public=True)
    session.user = _user("stranger")
    client = _make_client(session)

    response = _activity(client, project)

    assert response.status_code == 200, response.text


# ── Query parameters ──


@pytest.mark.parametrize("limit", [0, 101])
def test_activity_rejects_limit_out_of_bounds(limit: int) -> None:
    session, project, _ = _owned_project_session()
    client = _make_client(session)

    response = _activity(client, project, limit=limit)

    assert response.status_code == 422
