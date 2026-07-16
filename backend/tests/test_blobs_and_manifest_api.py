"""Tests for the content-addressed blob endpoints and manifest-based version create.

Covers:
- PUT /projects/{pid}/blobs/{sha256}: idempotent upload, sha256 mismatch → 400.
- POST /branches/{bid}/versions/from-manifest: 409 on missing blobs, ref_count bump.
- DELETE /versions/{vid}: ref_count decrement for CAS versions.
- blob_gc.sweep_orphan_blobs: deletes ref_count==0 blobs older than grace window.
"""
from __future__ import annotations

import asyncio
import hashlib
import io
import uuid
from datetime import datetime, timedelta, timezone
from typing import Any

from fastapi import FastAPI
from fastapi.testclient import TestClient

from stemhub.auth import get_current_user
from stemhub.blob_gc import sweep_orphan_blobs
from stemhub.database import get_db
from stemhub.models import Blob, Branch, Project, User, Version
from stemhub.routers.blobs import router as blobs_router
from stemhub.routers.versions import router as versions_router
from stemhub.storage import LocalFilesystemStorageService, get_storage_service


# ── Fixtures ──


def _user() -> User:
    return User(
        id=uuid.uuid4(),
        email="demo@example.com",
        username="demo",
        password_hash="x",
        created_at=datetime.now(timezone.utc),
        is_active=True,
        is_deleted=False,
    )


def _project(owner_id: uuid.UUID) -> Project:
    return Project(
        id=uuid.uuid4(),
        owner_id=owner_id,
        name="Demo",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        is_public=False,
    )


def _branch(project_id: uuid.UUID) -> Branch:
    return Branch(
        id=uuid.uuid4(),
        project_id=project_id,
        name="main",
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
    )


# ── Fake session ──


class FakeSession:
    """In-memory stand-in for AsyncSession that knows about projects, branches,
    versions and blobs. Only the query shapes used by the endpoints under test
    are handled — anything else raises."""

    def __init__(self, *, user: User, project: Project, branch: Branch) -> None:
        self.user = user
        self.project = project
        self.branch = branch
        self.blobs: list[Blob] = []
        self.versions: list[Version] = []
        self.commit_calls = 0

    # ── SQLAlchemy surface ──

    async def execute(self, stmt):
        return _FakeResult(self, stmt)

    def add(self, obj) -> None:
        if isinstance(obj, Blob):
            self.blobs.append(obj)
        elif isinstance(obj, Version):
            # Real SQLAlchemy would populate these from column defaults on flush.
            if obj.id is None:
                obj.id = uuid.uuid4()
            if obj.created_at is None:
                obj.created_at = datetime.now(timezone.utc)
            if obj.is_deleted is None:
                obj.is_deleted = False
            self.versions.append(obj)
        else:
            raise TypeError(f"FakeSession does not track {type(obj).__name__}")

    async def delete(self, obj) -> None:
        if isinstance(obj, Blob):
            self.blobs.remove(obj)
        else:
            raise TypeError(f"FakeSession does not delete {type(obj).__name__}")

    async def commit(self) -> None:
        self.commit_calls += 1

    async def refresh(self, obj) -> None:
        pass


class _FakeResult:
    def __init__(self, session: FakeSession, stmt) -> None:
        self.session = session
        # Interpret the statement lazily on scalars()/first()/all().
        self.stmt = stmt

    def _match(self) -> list[Any]:
        stmt_text = str(self.stmt).lower()
        s = self.session
        if "from project" in stmt_text and "join" not in stmt_text:
            return [s.project]
        if "from branch" in stmt_text and "join project" in stmt_text:
            return [s.branch]
        if "from blob" in stmt_text:
            # GC sweep matches WHERE ref_count = 0 AND created_at < cutoff.
            # The blobs router match filters by (project_id, sha256 IN ...).
            if "ref_count = " in stmt_text:
                cutoff = _extract_cutoff(self.stmt)
                candidates = [b for b in s.blobs if b.ref_count == 0]
                if cutoff is not None:
                    candidates = [b for b in candidates if b.created_at < cutoff]
                return candidates
            wanted = _extract_sha_set(self.stmt)
            if wanted is not None:
                return [b for b in s.blobs if b.sha256 in wanted]
            return list(s.blobs)
        if "from version" in stmt_text:
            wanted_id = _extract_version_id(self.stmt)
            if wanted_id is not None:
                # Return (Version, project_id) row tuple as delete_version expects.
                for v in s.versions:
                    if v.id == wanted_id and not v.is_deleted:
                        return [(v, s.project.id)]
                return []
            return [v for v in s.versions if not v.is_deleted]
        if "from users" in stmt_text or "from user" in stmt_text:
            return [s.user]
        raise NotImplementedError(f"FakeSession does not know how to answer: {stmt_text[:200]}")

    def scalars(self):
        rows = self._match()
        # If _match returned tuples (delete_version case), keep them as-is.
        return _FakeScalars(rows)

    def scalar_one_or_none(self):
        rows = self._match()
        return rows[0] if rows else None

    def first(self):
        rows = self._match()
        return rows[0] if rows else None

    def all(self):
        rows = self._match()
        # For blob-sha listing, return list of (sha,) tuples.
        return [(r.sha256,) if isinstance(r, Blob) else r for r in rows]


class _FakeScalars:
    def __init__(self, rows: list[Any]) -> None:
        self._rows = rows

    def first(self):
        return self._rows[0] if self._rows else None

    def all(self):
        return self._rows


def _extract_sha_set(stmt) -> set[str] | None:
    """Best-effort extraction of the IN (...) sha256 set from a SQLAlchemy stmt."""
    try:
        for clause in stmt.whereclause.get_children():
            # Look for an IN clause on Blob.sha256
            if hasattr(clause, "right") and hasattr(clause.right, "value"):
                value = clause.right.value
                if isinstance(value, (list, tuple, set)):
                    if all(isinstance(v, str) and len(v) == 64 for v in value):
                        return set(value)
    except AttributeError:
        pass
    return None


def _extract_cutoff(stmt) -> datetime | None:
    try:
        for clause in stmt.whereclause.get_children():
            if hasattr(clause, "right") and hasattr(clause.right, "value"):
                value = clause.right.value
                if isinstance(value, datetime):
                    return value
    except AttributeError:
        pass
    return None


def _extract_version_id(stmt) -> uuid.UUID | None:
    try:
        for clause in stmt.whereclause.get_children():
            if hasattr(clause, "right") and hasattr(clause.right, "value"):
                value = clause.right.value
                if isinstance(value, uuid.UUID):
                    return value
    except AttributeError:
        pass
    return None


# ── Test client factory ──


def _make_client(session: FakeSession, storage_service) -> TestClient:
    app = FastAPI()
    app.include_router(blobs_router)
    app.include_router(versions_router)

    async def override_db():
        yield session

    async def override_user():
        return session.user

    app.dependency_overrides[get_db] = override_db
    app.dependency_overrides[get_current_user] = override_user
    app.dependency_overrides[get_storage_service] = lambda: storage_service
    return TestClient(app)


# ── Blob upload tests ──


def test_blob_upload_stores_bytes_and_row(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)
    storage = LocalFilesystemStorageService(tmp_path)
    client = _make_client(session, storage)

    payload = b"hello world"
    sha = hashlib.sha256(payload).hexdigest()

    response = client.put(
        f"/projects/{project.id}/blobs/{sha}",
        files={"file": ("kick.wav", io.BytesIO(payload), "audio/wav")},
    )
    assert response.status_code == 200
    body = response.json()
    assert body["sha256"] == sha
    assert body["size_bytes"] == len(payload)
    assert len(session.blobs) == 1
    assert session.blobs[0].ref_count == 0


def test_blob_upload_rejects_sha_mismatch(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)
    storage = LocalFilesystemStorageService(tmp_path)
    client = _make_client(session, storage)

    # Claim a hash that doesn't match the bytes.
    lying_sha = "0" * 64
    response = client.put(
        f"/projects/{project.id}/blobs/{lying_sha}",
        files={"file": ("kick.wav", io.BytesIO(b"different bytes"), "audio/wav")},
    )
    assert response.status_code == 400
    assert "mismatch" in response.json()["detail"]
    assert session.blobs == []


# ── from-manifest tests ──


def _manifest(project_sha: str, project_size: int, track_sha: str, track_size: int) -> dict:
    return {
        "manifest_version": 1,
        "source_daw": "FL Studio",
        "source_project_filename": "song.flp",
        "project_file": {
            "sha256": project_sha,
            "size_bytes": project_size,
            "filename": "song.flp",
        },
        "tracks": [
            {
                "sha256": track_sha,
                "size_bytes": track_size,
                "filename": "kick.wav",
                "name": "Kick",
            },
        ],
    }


def test_create_version_from_manifest_bumps_ref_count(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)

    proj_bytes = b"flp bytes"
    track_bytes = b"wav bytes"
    proj_sha = hashlib.sha256(proj_bytes).hexdigest()
    track_sha = hashlib.sha256(track_bytes).hexdigest()

    session.blobs.append(Blob(
        project_id=project.id, sha256=proj_sha, size_bytes=len(proj_bytes),
        storage_uri="x", ref_count=0,
        created_at=datetime.now(timezone.utc),
    ))
    session.blobs.append(Blob(
        project_id=project.id, sha256=track_sha, size_bytes=len(track_bytes),
        storage_uri="y", ref_count=0,
        created_at=datetime.now(timezone.utc),
    ))

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{branch.id}/versions/from-manifest",
        json={
            "commit_message": "first cut",
            "manifest": _manifest(proj_sha, len(proj_bytes), track_sha, len(track_bytes)),
        },
    )
    assert response.status_code == 201, response.text
    body = response.json()
    assert body["source_daw"] == "FL Studio"
    assert body["manifest_version"] == 1
    # ref_count bumped exactly once per referenced blob
    assert all(b.ref_count == 1 for b in session.blobs)
    assert len(session.versions) == 1


def test_create_version_from_manifest_returns_409_when_blobs_missing(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)

    proj_sha = "a" * 64
    track_sha = "b" * 64

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{branch.id}/versions/from-manifest",
        json={
            "manifest": _manifest(proj_sha, 10, track_sha, 20),
        },
    )
    assert response.status_code == 409
    detail = response.json()["detail"]
    assert detail["error"] == "missing_blobs"
    assert set(detail["missing"]) == {proj_sha, track_sha}
    assert session.versions == []


# ── delete_version ref_count decrement ──


def test_delete_cas_version_decrements_ref_count(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)

    proj_sha = "c" * 64
    track_sha = "d" * 64
    session.blobs.append(Blob(
        project_id=project.id, sha256=proj_sha, size_bytes=1,
        storage_uri="x", ref_count=1,
        created_at=datetime.now(timezone.utc),
    ))
    session.blobs.append(Blob(
        project_id=project.id, sha256=track_sha, size_bytes=1,
        storage_uri="y", ref_count=2,  # referenced by another version too
        created_at=datetime.now(timezone.utc),
    ))
    version = Version(
        id=uuid.uuid4(),
        branch_id=branch.id,
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        manifest_json=_manifest(proj_sha, 1, track_sha, 1),
        manifest_version=1,
    )
    session.versions.append(version)

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.delete(f"/versions/{version.id}")
    assert response.status_code == 204
    assert version.is_deleted is True
    assert next(b for b in session.blobs if b.sha256 == proj_sha).ref_count == 0
    assert next(b for b in session.blobs if b.sha256 == track_sha).ref_count == 1


# ── GC sweep ──


def test_gc_sweep_deletes_old_orphan_blobs(tmp_path) -> None:
    asyncio.run(_gc_sweep_body(tmp_path))


async def _gc_sweep_body(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)
    storage = LocalFilesystemStorageService(tmp_path)

    # Two blobs: one orphan+old (should be deleted), one orphan+recent (kept),
    # one referenced+old (kept).
    now = datetime.now(timezone.utc)
    stored_old = storage.store_blob(project_id=project.id, source=io.BytesIO(b"old"))
    stored_recent = storage.store_blob(project_id=project.id, source=io.BytesIO(b"recent"))
    stored_ref = storage.store_blob(project_id=project.id, source=io.BytesIO(b"ref"))

    session.blobs.append(Blob(
        project_id=project.id, sha256=stored_old.checksum_sha256, size_bytes=stored_old.size_bytes,
        storage_uri=stored_old.path, ref_count=0,
        created_at=now - timedelta(hours=48),
    ))
    session.blobs.append(Blob(
        project_id=project.id, sha256=stored_recent.checksum_sha256, size_bytes=stored_recent.size_bytes,
        storage_uri=stored_recent.path, ref_count=0,
        created_at=now,
    ))
    session.blobs.append(Blob(
        project_id=project.id, sha256=stored_ref.checksum_sha256, size_bytes=stored_ref.size_bytes,
        storage_uri=stored_ref.path, ref_count=3,
        created_at=now - timedelta(hours=48),
    ))

    result = await sweep_orphan_blobs(db=session, storage=storage, grace_hours=24)
    assert result.deleted == 1
    assert result.failed == 0
    remaining_shas = {b.sha256 for b in session.blobs}
    assert stored_old.checksum_sha256 not in remaining_shas
    assert stored_recent.checksum_sha256 in remaining_shas
    assert stored_ref.checksum_sha256 in remaining_shas
