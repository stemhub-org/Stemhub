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


def _version(
    branch: Branch,
    *,
    created_at: datetime,
    created_by: User | None = None,
    deleted_at: datetime | None = None,
    message: str | None = "Mixed the drums",
) -> Version:
    return Version(
        id=uuid.uuid4(),
        branch_id=branch.id,
        created_by=created_by.id if created_by else None,
        message=message,
        created_at=created_at,
        is_deleted=deleted_at is not None,
        deleted_at=deleted_at,
        source_daw="FL Studio",
        source_project_filename="beat.flp",
    )


def _pull_request(
    project: Project,
    source: Branch,
    target: Branch,
    *,
    created_at: datetime,
    created_by: User | None = None,
    status: str = "OPEN",
    closed_at: datetime | None = None,
    closed_by: User | None = None,
    is_deleted: bool = False,
) -> PullRequest:
    return PullRequest(
        id=uuid.uuid4(),
        project_id=project.id,
        source_branch_id=source.id,
        target_branch_id=target.id,
        title="Faster tempo",
        description=None,
        status=status,
        created_by=created_by.id if created_by else None,
        created_at=created_at,
        closed_by=closed_by.id if closed_by else None,
        closed_at=closed_at,
        is_deleted=is_deleted,
        deleted_at=closed_at if is_deleted else None,
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


def _items(client: TestClient, project: Project, **params: Any) -> list[dict[str, Any]]:
    response = _activity(client, project, **params)
    assert response.status_code == 200, response.text
    return response.json()["items"]


def _of_type(items: list[dict[str, Any]], activity_type: str) -> list[dict[str, Any]]:
    return [item for item in items if item["type"] == activity_type]


def _ids(items: list[dict[str, Any]]) -> list[str]:
    return [item["id"] for item in items]


def _iso(moment: datetime) -> str:
    # Pydantic serialises UTC as "Z".
    return moment.isoformat().replace("+00:00", "Z")


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


# ── Versions saved ──


def test_activity_lists_a_saved_version() -> None:
    session, project, main = _owned_project_session()
    version = _version(main, created_at=_at(5), created_by=session.user)
    session.versions.append(version)
    client = _make_client(session)

    items = _of_type(_items(client, project), "version_saved")

    assert items == [
        {
            "id": f"version_saved:{version.id}",
            "type": "version_saved",
            "occurred_at": _iso(_at(5)),
            "actor": {"id": str(session.user.id), "username": "demo"},
            "branch": {"id": str(main.id), "name": "main", "is_deleted": False},
            "version": {
                "id": str(version.id),
                "message": "Mixed the drums",
                "source_daw": "FL Studio",
                "source_project_filename": "beat.flp",
            },
            "pull_request": None,
        }
    ]


def test_activity_lists_saved_versions_newest_first() -> None:
    session, project, main = _owned_project_session()
    first = _version(main, created_at=_at(1))
    third = _version(main, created_at=_at(3))
    second = _version(main, created_at=_at(2))
    session.versions.extend([first, third, second])
    client = _make_client(session)

    items = _of_type(_items(client, project), "version_saved")

    assert _ids(items) == [f"version_saved:{v.id}" for v in (third, second, first)]


def test_activity_lists_versions_on_deleted_branches() -> None:
    session, project, _ = _owned_project_session()
    old = _branch(project.id, "old-idea", deleted_at=_at(10))
    session.branches.append(old)
    version = _version(old, created_at=_at(5))
    session.versions.append(version)
    client = _make_client(session)

    items = _of_type(_items(client, project), "version_saved")

    assert _ids(items) == [f"version_saved:{version.id}"]
    assert items[0]["branch"] == {"id": str(old.id), "name": "old-idea", "is_deleted": True}


def test_activity_lists_versions_of_this_project_only() -> None:
    session, project, main = _owned_project_session()
    other_project = _project(session.user.id)
    other_branch = _branch(other_project.id)
    session.projects.append(other_project)
    session.branches.append(other_branch)
    mine = _version(main, created_at=_at(1))
    session.versions.extend([mine, _version(other_branch, created_at=_at(2))])
    client = _make_client(session)

    items = _of_type(_items(client, project), "version_saved")

    assert _ids(items) == [f"version_saved:{mine.id}"]


def test_activity_saved_version_without_author_has_no_actor() -> None:
    session, project, main = _owned_project_session()
    session.versions.append(_version(main, created_at=_at(1), created_by=None))
    client = _make_client(session)

    items = _of_type(_items(client, project), "version_saved")

    assert items[0]["actor"] is None


def test_activity_loads_actors_in_one_query() -> None:
    session, project, main = _owned_project_session()
    for minute in range(5):
        author = _user(f"producer{minute}")
        session.users.append(author)
        session.versions.append(_version(main, created_at=_at(minute), created_by=author))
    client = _make_client(session)

    items = _of_type(_items(client, project), "version_saved")

    assert [item["actor"]["username"] for item in items] == [f"producer{m}" for m in range(4, -1, -1)]
    assert session.queries_for(User) == 1


def test_activity_returns_at_most_limit_items() -> None:
    session, project, main = _owned_project_session()
    versions = [_version(main, created_at=_at(minute)) for minute in range(5)]
    session.versions.extend(versions)
    client = _make_client(session)

    items = _items(client, project, limit=2)

    assert _ids(items) == [f"version_saved:{v.id}" for v in (versions[4], versions[3])]


# ── Versions deleted ──


def test_activity_lists_a_deleted_version_as_saved_then_deleted() -> None:
    session, project, main = _owned_project_session()
    version = _version(main, created_at=_at(1), created_by=session.user, deleted_at=_at(7))
    session.versions.append(version)
    client = _make_client(session)

    items = _items(client, project)

    deleted = _of_type(items, "version_deleted")
    assert deleted == [
        {
            "id": f"version_deleted:{version.id}",
            "type": "version_deleted",
            "occurred_at": _iso(_at(7)),
            "actor": None,  # who deleted a version is not recorded
            "branch": {"id": str(main.id), "name": "main", "is_deleted": False},
            "version": {
                "id": str(version.id),
                "message": "Mixed the drums",
                "source_daw": "FL Studio",
                "source_project_filename": "beat.flp",
            },
            "pull_request": None,
        }
    ]
    assert _ids(_of_type(items, "version_saved")) == [f"version_saved:{version.id}"]
    assert _ids(items)[:2] == [f"version_deleted:{version.id}", f"version_saved:{version.id}"]


def test_activity_skips_a_deletion_without_timestamp() -> None:
    session, project, main = _owned_project_session()
    version = _version(main, created_at=_at(1))
    version.is_deleted = True  # deleted_at unset: the deletion can't be placed in time
    session.versions.append(version)
    client = _make_client(session)

    items = _items(client, project)

    assert _of_type(items, "version_deleted") == []
    assert _ids(_of_type(items, "version_saved")) == [f"version_saved:{version.id}"]


# ── Branches ──


def test_activity_lists_a_created_branch() -> None:
    session, project, main = _owned_project_session()
    feature = _branch(project.id, "feature-fast-tempo", created_at=_at(4))
    session.branches.append(feature)
    client = _make_client(session)

    items = _of_type(_items(client, project), "branch_created")

    assert items == [
        {
            "id": f"branch_created:{feature.id}",
            "type": "branch_created",
            "occurred_at": _iso(_at(4)),
            "actor": None,  # Branch records no author
            "branch": {"id": str(feature.id), "name": "feature-fast-tempo", "is_deleted": False},
            "version": None,
            "pull_request": None,
        },
        {
            "id": f"branch_created:{main.id}",
            "type": "branch_created",
            "occurred_at": _iso(T0),
            "actor": None,
            "branch": {"id": str(main.id), "name": "main", "is_deleted": False},
            "version": None,
            "pull_request": None,
        },
    ]


def test_activity_lists_a_deleted_branch_as_created_then_deleted() -> None:
    session, project, _ = _owned_project_session()
    old = _branch(project.id, "old-idea", created_at=_at(2), deleted_at=_at(9))
    session.branches.append(old)
    client = _make_client(session)

    items = _items(client, project)

    assert items[0] == {
        "id": f"branch_deleted:{old.id}",
        "type": "branch_deleted",
        "occurred_at": _iso(_at(9)),
        "actor": None,
        "branch": {"id": str(old.id), "name": "old-idea", "is_deleted": True},
        "version": None,
        "pull_request": None,
    }
    assert f"branch_created:{old.id}" in _ids(_of_type(items, "branch_created"))


def test_activity_skips_a_branch_deletion_without_timestamp() -> None:
    session, project, _ = _owned_project_session()
    old = _branch(project.id, "old-idea", created_at=_at(2))
    old.is_deleted = True
    session.branches.append(old)
    client = _make_client(session)

    items = _items(client, project)

    assert _of_type(items, "branch_deleted") == []


def test_activity_lists_branches_of_this_project_only() -> None:
    session, project, main = _owned_project_session()
    other_project = _project(session.user.id)
    session.projects.append(other_project)
    session.branches.append(_branch(other_project.id, "elsewhere", created_at=_at(3), deleted_at=_at(4)))
    client = _make_client(session)

    items = _items(client, project)

    assert _ids(items) == [f"branch_created:{main.id}"]


# ── Pull requests ──


def _project_with_feature_branch() -> tuple[FakeSession, Project, Branch, Branch]:
    session, project, main = _owned_project_session()
    feature = _branch(project.id, "feature-fast-tempo", created_at=_at(1))
    session.branches.append(feature)
    return session, project, main, feature


def test_activity_lists_an_opened_pull_request() -> None:
    session, project, main, feature = _project_with_feature_branch()
    pr = _pull_request(project, feature, main, created_at=_at(6), created_by=session.user)
    session.pull_requests.append(pr)
    client = _make_client(session)

    items = _of_type(_items(client, project), "pull_request_opened")

    assert items == [
        {
            "id": f"pull_request_opened:{pr.id}",
            "type": "pull_request_opened",
            "occurred_at": _iso(_at(6)),
            "actor": {"id": str(session.user.id), "username": "demo"},
            "branch": None,
            "version": None,
            "pull_request": {
                "id": str(pr.id),
                "title": "Faster tempo",
                "status": "OPEN",
                "source_branch": {"id": str(feature.id), "name": "feature-fast-tempo", "is_deleted": False},
                "target_branch": {"id": str(main.id), "name": "main", "is_deleted": False},
            },
        }
    ]


def test_activity_skips_soft_deleted_pull_requests() -> None:
    session, project, main, feature = _project_with_feature_branch()
    deleted = _pull_request(
        project,
        feature,
        main,
        created_at=_at(6),
        status="CLOSED",
        closed_at=_at(7),
        closed_by=session.user,
        is_deleted=True,
    )
    session.pull_requests.append(deleted)
    client = _make_client(session)

    items = _items(client, project)

    assert [item for item in items if item["pull_request"] is not None] == []


def test_activity_lists_pull_requests_of_this_project_only() -> None:
    session, project, main, feature = _project_with_feature_branch()
    other_project = _project(session.user.id)
    other_main = _branch(other_project.id)
    other_feature = _branch(other_project.id, "other")
    session.projects.append(other_project)
    session.branches.extend([other_main, other_feature])
    session.pull_requests.append(_pull_request(other_project, other_feature, other_main, created_at=_at(6)))
    client = _make_client(session)

    items = _items(client, project)

    assert _of_type(items, "pull_request_opened") == []


@pytest.mark.parametrize(
    ("status", "activity_type"),
    [("MERGED", "pull_request_merged"), ("CLOSED", "pull_request_closed")],
)
def test_activity_lists_a_pull_request_merged_or_closed_by_its_closer(status: str, activity_type: str) -> None:
    session, project, main, feature = _project_with_feature_branch()
    reviewer = _user("reviewer")
    session.users.append(reviewer)
    pr = _pull_request(
        project,
        feature,
        main,
        created_at=_at(6),
        created_by=session.user,
        status=status,
        closed_at=_at(8),
        closed_by=reviewer,
    )
    session.pull_requests.append(pr)
    client = _make_client(session)

    items = _items(client, project)

    assert _ids(items)[:2] == [f"{activity_type}:{pr.id}", f"pull_request_opened:{pr.id}"]
    assert items[0]["occurred_at"] == _iso(_at(8))
    assert items[0]["actor"] == {"id": str(reviewer.id), "username": "reviewer"}
    assert items[0]["pull_request"]["status"] == status
    assert items[1]["actor"] == {"id": str(session.user.id), "username": "demo"}
    assert items[1]["pull_request"]["status"] == status  # the PR's current status


def test_activity_open_pull_request_has_no_closing_event() -> None:
    session, project, main, feature = _project_with_feature_branch()
    session.pull_requests.append(_pull_request(project, feature, main, created_at=_at(6)))
    client = _make_client(session)

    items = _items(client, project)

    assert _of_type(items, "pull_request_merged") == []
    assert _of_type(items, "pull_request_closed") == []
