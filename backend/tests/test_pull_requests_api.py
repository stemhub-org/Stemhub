"""Tests for the pull-request lifecycle endpoints (issue #250).

Covers:
- POST /projects/{pid}/pull-requests: create (captures both branch heads),
  source == target → 400, branch from another project (or soft-deleted) → 404,
  duplicate OPEN PR on the same branch pair → 409.
- GET /projects/{pid}/pull-requests: list, 404 without project access.
- GET /pull-requests/{id}: get by id, 404 without project access.
- POST /pull-requests/{id}/close: OPEN → CLOSED with closed_at/closed_by; non-OPEN → 409.

CLOSED is terminal (SPECIFICATION.md §7, §19): a closed PR is not reopened,
users open a new one. The merge engine (issue #253) is out of scope: there is
no /merge endpoint here.
"""
from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI
from fastapi.testclient import TestClient
from sqlalchemy.sql import operators
from sqlalchemy.sql.elements import False_, True_

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Branch, Collaborator, Project, PullRequest, User, Version
from stemhub.routers.pull_requests import router as pull_requests_router


# ── Fixtures ──


def _user() -> User:
    return User(
        id=uuid.uuid4(),
        email=f"{uuid.uuid4().hex[:8]}@example.com",
        username="demo",
        password_hash="x",
        created_at=datetime.now(timezone.utc),
        is_active=True,
        is_deleted=False,
    )


def _project(owner_id: uuid.UUID, *, is_public: bool = False) -> Project:
    return Project(
        id=uuid.uuid4(),
        owner_id=owner_id,
        name="Demo",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        is_public=is_public,
    )


def _branch(project_id: uuid.UUID, name: str = "main", *, is_deleted: bool = False) -> Branch:
    return Branch(
        id=uuid.uuid4(),
        project_id=project_id,
        name=name,
        created_at=datetime.now(timezone.utc),
        is_deleted=is_deleted,
    )


def _version(branch_id: uuid.UUID, *, created_at: datetime, is_deleted: bool = False) -> Version:
    return Version(
        id=uuid.uuid4(),
        branch_id=branch_id,
        commit_message="v",
        created_at=created_at,
        is_deleted=is_deleted,
    )


def _pull_request(
    project: Project,
    source: Branch,
    target: Branch,
    *,
    status: str = "OPEN",
    created_by: uuid.UUID | None = None,
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
        created_by=created_by,
        created_at=datetime.now(timezone.utc),
        closed_by=created_by if status == "CLOSED" else None,
        closed_at=datetime.now(timezone.utc) if status == "CLOSED" else None,
        is_deleted=is_deleted,
        deleted_at=datetime.now(timezone.utc) if is_deleted else None,
    )


# ── Fake session ──


class FakeSession:
    """In-memory stand-in for AsyncSession holding projects, collaborators,
    branches and pull requests. Statements are answered by filtering the
    in-memory rows with the `column == value` / `column IN (...)` clauses of
    the WHERE — anything else raises so a new query shape fails loudly."""

    def __init__(self, *, user: User) -> None:
        self.user = user
        self.projects: list[Project] = []
        self.collaborators: list[Collaborator] = []
        self.branches: list[Branch] = []
        self.versions: list[Version] = []
        self.pull_requests: list[PullRequest] = []
        self.commit_calls = 0

    # ── SQLAlchemy surface ──

    async def execute(self, stmt):
        return _FakeResult(self, stmt)

    def add(self, obj) -> None:
        if isinstance(obj, PullRequest):
            # Real SQLAlchemy would populate these from column defaults on flush.
            if obj.id is None:
                obj.id = uuid.uuid4()
            if obj.created_at is None:
                obj.created_at = datetime.now(timezone.utc)
            if obj.status is None:
                obj.status = "OPEN"
            if obj.is_deleted is None:
                obj.is_deleted = False
            self.pull_requests.append(obj)
        else:
            raise TypeError(f"FakeSession does not track {type(obj).__name__}")

    async def commit(self) -> None:
        self.commit_calls += 1

    async def refresh(self, obj) -> None:
        pass


class _FakeResult:
    def __init__(self, session: FakeSession, stmt) -> None:
        self.session = session
        self.stmt = stmt

    def _match(self) -> list[Any]:
        entity = self.stmt.column_descriptions[0]["entity"]
        s = self.session
        rows_by_entity = {
            Project: s.projects,
            Collaborator: s.collaborators,
            Branch: s.branches,
            Version: s.versions,
            PullRequest: s.pull_requests,
        }
        if entity not in rows_by_entity:
            raise NotImplementedError(f"FakeSession does not know how to answer: {entity}")
        rows = [row for row in rows_by_entity[entity] if _row_matches(row, self.stmt.whereclause)]
        # Honour ORDER BY so `.first()` on the head-version query is meaningful.
        for order in reversed(list(self.stmt._order_by_clauses)):
            rows.sort(
                key=lambda row: getattr(row, order.element.name),
                reverse=order.modifier is operators.desc_op,
            )
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


def _row_matches(row: Any, whereclause) -> bool:
    """Evaluate an AND-ed WHERE of `col == value` / `col IN (values)` clauses
    against an ORM instance. Mirrors the `_extract_*` helpers used by the blob
    tests, generalised so several columns can be filtered at once."""
    if whereclause is None:
        return True
    clauses = list(whereclause.clauses) if hasattr(whereclause, "clauses") else [whereclause]
    for clause in clauses:
        attr = getattr(row, clause.left.name)
        if clause.operator is operators.eq:
            if attr != _bound_value(clause.right):
                return False
        elif clause.operator is operators.in_op:
            if attr not in _bound_value(clause.right):
                return False
        else:
            raise NotImplementedError(f"FakeSession does not support operator {clause.operator}")
    return True


def _bound_value(right) -> Any:
    """Python value behind the right-hand side of a comparison. `col == False`
    is rendered by SQLAlchemy as a `False_` constant, not a bind parameter."""
    if isinstance(right, (True_, False_)):
        return isinstance(right, True_)
    return right.value


# ── Test client factory ──


def _make_client(session: FakeSession) -> TestClient:
    app = FastAPI()
    app.include_router(pull_requests_router)

    async def override_db():
        yield session

    async def override_user():
        return session.user

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_current_user] = override_user
    return TestClient(app)


def _owned_project_session() -> tuple[FakeSession, Project, Branch, Branch]:
    user = _user()
    project = _project(user.id)
    main = _branch(project.id, "main")
    feature = _branch(project.id, "feature-fast-tempo")
    session = FakeSession(user=user)
    session.projects.append(project)
    session.branches.extend([main, feature])
    return session, project, main, feature


# ── Create ──


def test_create_pull_request_returns_open_pr() -> None:
    session, project, main, feature = _owned_project_session()
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={
            "source_branch_id": str(feature.id),
            "target_branch_id": str(main.id),
            "title": "Faster tempo",
            "description": "Bump to 128 BPM",
        },
    )

    assert response.status_code == 201, response.text
    body = response.json()
    assert body["project_id"] == str(project.id)
    assert body["source_branch_id"] == str(feature.id)
    assert body["target_branch_id"] == str(main.id)
    assert body["title"] == "Faster tempo"
    assert body["description"] == "Bump to 128 BPM"
    assert body["status"] == "OPEN"
    assert body["created_by"] == str(session.user.id)
    assert body["closed_at"] is None
    assert body["closed_by"] is None
    assert body["is_deleted"] is False
    assert body["deleted_at"] is None
    assert len(session.pull_requests) == 1
    assert session.commit_calls == 1


def test_create_pull_request_captures_branch_heads() -> None:
    """The head of each branch (latest live version) is snapshotted at open time
    so the merge engine can later detect that the target moved (issue #253)."""
    session, project, main, feature = _owned_project_session()
    old = datetime(2026, 1, 1, tzinfo=timezone.utc)
    new = datetime(2026, 2, 1, tzinfo=timezone.utc)
    main_old = _version(main.id, created_at=old)
    main_head = _version(main.id, created_at=new)
    feature_head = _version(feature.id, created_at=old)
    # A newer but soft-deleted version must not be picked as head.
    feature_deleted = _version(feature.id, created_at=new, is_deleted=True)
    session.versions.extend([main_old, main_head, feature_head, feature_deleted])
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={"source_branch_id": str(feature.id), "target_branch_id": str(main.id), "title": "t"},
    )

    assert response.status_code == 201, response.text
    body = response.json()
    assert body["source_head_version_id"] == str(feature_head.id)
    assert body["target_head_version_id"] == str(main_head.id)


def test_create_pull_request_heads_are_null_on_empty_branches() -> None:
    session, project, main, feature = _owned_project_session()
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={"source_branch_id": str(feature.id), "target_branch_id": str(main.id), "title": "t"},
    )

    assert response.status_code == 201, response.text
    assert response.json()["source_head_version_id"] is None
    assert response.json()["target_head_version_id"] is None


def test_create_pull_request_rejects_duplicate_open_pr() -> None:
    session, project, main, feature = _owned_project_session()
    session.pull_requests.append(_pull_request(project, feature, main, status="OPEN"))
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={"source_branch_id": str(feature.id), "target_branch_id": str(main.id), "title": "t"},
    )

    assert response.status_code == 409, response.text
    assert len(session.pull_requests) == 1


def test_create_pull_request_allows_new_pr_after_previous_closed() -> None:
    """Only OPEN PRs block a duplicate; a closed or soft-deleted one on the
    same pair does not (mirrors the partial unique index)."""
    session, project, main, feature = _owned_project_session()
    session.pull_requests.append(_pull_request(project, feature, main, status="CLOSED"))
    session.pull_requests.append(_pull_request(project, feature, main, status="OPEN", is_deleted=True))
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={"source_branch_id": str(feature.id), "target_branch_id": str(main.id), "title": "t"},
    )

    assert response.status_code == 201, response.text
    assert len(session.pull_requests) == 3


def test_create_pull_request_allows_reverse_direction() -> None:
    """(A → B) open does not block (B → A): the pair is ordered."""
    session, project, main, feature = _owned_project_session()
    session.pull_requests.append(_pull_request(project, feature, main, status="OPEN"))
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={"source_branch_id": str(main.id), "target_branch_id": str(feature.id), "title": "t"},
    )

    assert response.status_code == 201, response.text


def test_create_pull_request_rejects_same_source_and_target() -> None:
    session, project, main, _ = _owned_project_session()
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={
            "source_branch_id": str(main.id),
            "target_branch_id": str(main.id),
            "title": "Self merge",
        },
    )

    assert response.status_code == 400
    assert session.pull_requests == []


def test_create_pull_request_rejects_branch_from_other_project() -> None:
    session, project, main, _ = _owned_project_session()
    other_project = _project(session.user.id)
    foreign_branch = _branch(other_project.id, "foreign")
    session.projects.append(other_project)
    session.branches.append(foreign_branch)
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={
            "source_branch_id": str(foreign_branch.id),
            "target_branch_id": str(main.id),
            "title": "Cross-project",
        },
    )

    assert response.status_code == 404
    assert session.pull_requests == []


def test_create_pull_request_rejects_soft_deleted_branch() -> None:
    session, project, main, _ = _owned_project_session()
    deleted = _branch(project.id, "old", is_deleted=True)
    session.branches.append(deleted)
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={
            "source_branch_id": str(deleted.id),
            "target_branch_id": str(main.id),
            "title": "From deleted",
        },
    )

    assert response.status_code == 404
    assert session.pull_requests == []


def test_create_pull_request_requires_write_access() -> None:
    # A Viewer collaborator can read the project but must not open PRs.
    session, project, main, feature = _owned_project_session()
    viewer = _user()
    session.collaborators.append(
        Collaborator(project_id=project.id, user_id=viewer.id, role="Viewer")
    )
    session.user = viewer
    client = _make_client(session)

    response = client.post(
        f"/projects/{project.id}/pull-requests",
        json={
            "source_branch_id": str(feature.id),
            "target_branch_id": str(main.id),
            "title": "Nope",
        },
    )

    assert response.status_code == 404
    assert session.pull_requests == []


# ── List ──


def test_list_pull_requests_returns_project_prs_only() -> None:
    session, project, main, feature = _owned_project_session()
    other_project = _project(session.user.id)
    other_branch = _branch(other_project.id)
    session.projects.append(other_project)
    session.branches.append(other_branch)
    mine = _pull_request(project, feature, main)
    session.pull_requests.append(mine)
    session.pull_requests.append(_pull_request(other_project, other_branch, other_branch))
    client = _make_client(session)

    response = client.get(f"/projects/{project.id}/pull-requests")

    assert response.status_code == 200
    ids = [pr["id"] for pr in response.json()]
    assert ids == [str(mine.id)]


def test_list_pull_requests_excludes_soft_deleted() -> None:
    session, project, main, feature = _owned_project_session()
    live = _pull_request(project, feature, main)
    session.pull_requests.append(live)
    session.pull_requests.append(_pull_request(project, feature, main, is_deleted=True))
    client = _make_client(session)

    response = client.get(f"/projects/{project.id}/pull-requests")

    assert response.status_code == 200
    assert [pr["id"] for pr in response.json()] == [str(live.id)]


def test_list_pull_requests_returns_404_without_access() -> None:
    session, project, main, feature = _owned_project_session()
    session.pull_requests.append(_pull_request(project, feature, main))
    session.user = _user()  # neither owner nor collaborator, project is private
    client = _make_client(session)

    response = client.get(f"/projects/{project.id}/pull-requests")

    assert response.status_code == 404


# ── Get by id ──


def test_get_pull_request_returns_pr() -> None:
    session, project, main, feature = _owned_project_session()
    pr = _pull_request(project, feature, main)
    session.pull_requests.append(pr)
    client = _make_client(session)

    response = client.get(f"/pull-requests/{pr.id}")

    assert response.status_code == 200
    assert response.json()["id"] == str(pr.id)
    assert response.json()["status"] == "OPEN"


def test_get_pull_request_returns_404_without_access() -> None:
    session, project, main, feature = _owned_project_session()
    pr = _pull_request(project, feature, main)
    session.pull_requests.append(pr)
    session.user = _user()
    client = _make_client(session)

    response = client.get(f"/pull-requests/{pr.id}")

    assert response.status_code == 404


def test_get_pull_request_returns_404_when_soft_deleted() -> None:
    session, project, main, feature = _owned_project_session()
    pr = _pull_request(project, feature, main, is_deleted=True)
    session.pull_requests.append(pr)
    client = _make_client(session)

    response = client.get(f"/pull-requests/{pr.id}")

    assert response.status_code == 404


def test_get_pull_request_returns_404_for_unknown_id() -> None:
    session, _, _, _ = _owned_project_session()
    client = _make_client(session)

    response = client.get(f"/pull-requests/{uuid.uuid4()}")

    assert response.status_code == 404


# ── Close ──


def test_close_pull_request_marks_closed_with_timestamp_and_closer() -> None:
    session, project, main, feature = _owned_project_session()
    pr = _pull_request(project, feature, main)
    session.pull_requests.append(pr)
    client = _make_client(session)

    response = client.post(f"/pull-requests/{pr.id}/close")

    assert response.status_code == 200, response.text
    body = response.json()
    assert body["status"] == "CLOSED"
    assert body["closed_at"] is not None
    assert body["closed_by"] == str(session.user.id)
    assert pr.status == "CLOSED"
    assert pr.closed_at is not None
    assert pr.closed_by == session.user.id
    assert session.commit_calls == 1


def test_close_pull_request_already_closed_returns_409() -> None:
    session, project, main, feature = _owned_project_session()
    pr = _pull_request(project, feature, main, status="CLOSED")
    session.pull_requests.append(pr)
    client = _make_client(session)

    response = client.post(f"/pull-requests/{pr.id}/close")

    assert response.status_code == 409
    assert session.commit_calls == 0


def test_close_pull_request_already_merged_returns_409() -> None:
    session, project, main, feature = _owned_project_session()
    pr = _pull_request(project, feature, main, status="MERGED")
    session.pull_requests.append(pr)
    client = _make_client(session)

    response = client.post(f"/pull-requests/{pr.id}/close")

    assert response.status_code == 409
    assert pr.status == "MERGED"


def test_close_pull_request_requires_write_access() -> None:
    session, project, main, feature = _owned_project_session()
    pr = _pull_request(project, feature, main)
    session.pull_requests.append(pr)
    viewer = _user()
    session.collaborators.append(
        Collaborator(project_id=project.id, user_id=viewer.id, role="Viewer")
    )
    session.user = viewer
    client = _make_client(session)

    response = client.post(f"/pull-requests/{pr.id}/close")

    assert response.status_code == 404
    assert pr.status == "OPEN"
