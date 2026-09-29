"""Tests for the content-addressed blob endpoints and manifest-based version create.

Covers:
- PUT /projects/{pid}/blobs/{sha256}: idempotent upload, sha256 mismatch → 400.
- POST /branches/{bid}/versions/from-manifest: v1 and v2 manifests, 409 on
  missing blobs, ref_count bump, `message` (and its `commit_message` input
  alias, and the old plugin placeholder stored as no message), source_daw
  validation.
- DELETE /versions/{vid}: ref_count decrement for CAS versions, symmetric with
  the create-time increment for both manifest versions.
- blob_gc.sweep_orphan_blobs: deletes ref_count==0 blobs older than grace window.
"""
from __future__ import annotations

import asyncio
import hashlib
import io
import uuid
from datetime import datetime, timedelta, timezone
from typing import Any

import pytest
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


def _manifest(project_sha: str, project_size: int, asset_sha: str, asset_size: int) -> dict:
    """A legacy v1 manifest, as plugins before v2 send it: assets under
    "tracks", paths under "filename"."""
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
                "sha256": asset_sha,
                "size_bytes": asset_size,
                "filename": "kick.wav",
                "name": "Kick",
            },
        ],
    }


def _manifest_v2(project_sha: str, project_size: int, assets: list[tuple[str, int, str]]) -> dict:
    return {
        "manifest_version": 2,
        "source_daw": "FL Studio",
        "source_project_filename": "Song.flp",
        "project_file": {"sha256": project_sha, "size_bytes": project_size, "path": "Song.flp"},
        "assets": [
            {"sha256": sha, "size_bytes": size, "path": path}
            for sha, size, path in assets
        ],
    }


def _add_blob(session: FakeSession, sha: str, *, ref_count: int = 0) -> Blob:
    blob = Blob(
        project_id=session.project.id, sha256=sha, size_bytes=1,
        storage_uri=f"projects/{session.project.id}/blobs/{sha[:2]}/{sha}", ref_count=ref_count,
        created_at=datetime.now(timezone.utc),
    )
    session.blobs.append(blob)
    return blob


def _new_session() -> FakeSession:
    user = _user()
    project = _project(user.id)
    return FakeSession(user=user, project=project, branch=_branch(project.id))


def test_create_version_from_manifest_bumps_ref_count(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)

    proj_bytes = b"flp bytes"
    asset_bytes = b"wav bytes"
    proj_sha = hashlib.sha256(proj_bytes).hexdigest()
    asset_sha = hashlib.sha256(asset_bytes).hexdigest()

    session.blobs.append(Blob(
        project_id=project.id, sha256=proj_sha, size_bytes=len(proj_bytes),
        storage_uri="x", ref_count=0,
        created_at=datetime.now(timezone.utc),
    ))
    session.blobs.append(Blob(
        project_id=project.id, sha256=asset_sha, size_bytes=len(asset_bytes),
        storage_uri="y", ref_count=0,
        created_at=datetime.now(timezone.utc),
    ))

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{branch.id}/versions/from-manifest",
        json={
            "message": "first cut",
            "manifest": _manifest(proj_sha, len(proj_bytes), asset_sha, len(asset_bytes)),
        },
    )
    assert response.status_code == 201, response.text
    body = response.json()
    assert body["source_daw"] == "FL Studio"
    assert body["manifest_version"] == 1
    assert body["message"] == "first cut"
    # ref_count bumped exactly once per referenced blob
    assert all(b.ref_count == 1 for b in session.blobs)
    assert len(session.versions) == 1


def test_create_version_from_a_v2_manifest_stores_it_as_sent(tmp_path) -> None:
    session = _new_session()
    project_sha, kick_sha = "1" * 64, "2" * 64
    _add_blob(session, project_sha)
    _add_blob(session, kick_sha)
    manifest = _manifest_v2(project_sha, 10, [(kick_sha, 20, "Samples/kick.wav")])

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"message": "v2 save", "manifest": manifest},
    )

    assert response.status_code == 201, response.text
    body = response.json()
    assert body["manifest_version"] == 2
    assert body["manifest_json"] == manifest
    assert body["source_daw"] == "FL Studio"
    assert body["source_project_filename"] == "Song.flp"
    assert all(b.ref_count == 1 for b in session.blobs)


def test_create_version_drops_v1_fields_that_are_never_read(tmp_path) -> None:
    session = _new_session()
    project_sha, kick_sha = "1" * 64, "2" * 64
    _add_blob(session, project_sha)
    _add_blob(session, kick_sha)
    manifest = _manifest(project_sha, 10, kick_sha, 20)
    manifest["mixer_state"] = {"inserts": []}

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"manifest": manifest},
    )

    assert response.status_code == 201, response.text
    assert "mixer_state" not in response.json()["manifest_json"]


def test_create_version_reads_a_manifest_without_a_version_as_v1(tmp_path) -> None:
    session = _new_session()
    project_sha, kick_sha = "1" * 64, "2" * 64
    _add_blob(session, project_sha)
    _add_blob(session, kick_sha)
    manifest = _manifest(project_sha, 10, kick_sha, 20)
    del manifest["manifest_version"]

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"manifest": manifest},
    )

    assert response.status_code == 201, response.text
    assert response.json()["manifest_version"] == 1


def test_create_version_rejects_an_unknown_manifest_version(tmp_path) -> None:
    session = _new_session()
    manifest = _manifest_v2("1" * 64, 10, [])
    manifest["manifest_version"] = 3

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"manifest": manifest},
    )

    assert response.status_code == 422
    assert session.versions == []


def test_create_version_rejects_a_v2_manifest_that_also_lists_tracks(tmp_path) -> None:
    # Otherwise the "tracks" list would be dropped silently and its blobs
    # neither checked for existence nor ref-counted.
    session = _new_session()
    project_sha, kick_sha, snare_sha = "1" * 64, "2" * 64, "3" * 64
    for sha in (project_sha, kick_sha, snare_sha):
        _add_blob(session, sha)
    manifest = _manifest_v2(project_sha, 10, [(kick_sha, 20, "Samples/kick.wav")])
    manifest["tracks"] = [{"sha256": snare_sha, "size_bytes": 30, "filename": "snare.wav", "name": "Snare"}]

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"manifest": manifest},
    )

    assert response.status_code == 422
    assert "tracks" in response.text
    assert session.versions == []
    assert all(b.ref_count == 0 for b in session.blobs)


def test_create_version_rejects_a_v1_manifest_that_also_lists_assets(tmp_path) -> None:
    session = _new_session()
    project_sha, kick_sha, snare_sha = "1" * 64, "2" * 64, "3" * 64
    for sha in (project_sha, kick_sha, snare_sha):
        _add_blob(session, sha)
    manifest = _manifest(project_sha, 10, kick_sha, 20)
    manifest["assets"] = [{"sha256": snare_sha, "size_bytes": 30, "path": "Samples/snare.wav"}]

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"manifest": manifest},
    )

    assert response.status_code == 422
    assert "assets" in response.text
    assert session.versions == []
    assert all(b.ref_count == 0 for b in session.blobs)


def test_create_version_accepts_commit_message_as_an_alias_of_message(tmp_path) -> None:
    # Plugins built before the rename still send "commit_message".
    session = _new_session()
    project_sha = "1" * 64
    _add_blob(session, project_sha)

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"commit_message": "old plugin", "manifest": _manifest_v2(project_sha, 10, [])},
    )

    assert response.status_code == 201, response.text
    body = response.json()
    assert body["message"] == "old plugin"
    assert "commit_message" not in body
    assert session.versions[0].message == "old plugin"


def test_create_version_without_a_message_stores_none(tmp_path) -> None:
    session = _new_session()
    project_sha = "1" * 64
    _add_blob(session, project_sha)

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"manifest": _manifest_v2(project_sha, 10, [])},
    )

    assert response.status_code == 201, response.text
    assert response.json()["message"] is None


@pytest.mark.parametrize("message_field", ["message", "commit_message"])
def test_create_version_stores_the_old_plugin_placeholder_as_no_message(tmp_path, message_field) -> None:
    # Plugins built before the rename send "Save from plugin" when the user
    # wrote nothing; stored rows holding it were cleared by migration 316caf0fcfa9.
    session = _new_session()
    project_sha = "1" * 64
    _add_blob(session, project_sha)

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={message_field: "Save from plugin", "manifest": _manifest_v2(project_sha, 10, [])},
    )

    assert response.status_code == 201, response.text
    assert response.json()["message"] is None
    assert session.versions[0].message is None


@pytest.mark.parametrize("message", ["save from plugin", "Save from plugin!", " Save from plugin"])
def test_create_version_keeps_messages_that_only_resemble_the_placeholder(tmp_path, message) -> None:
    # Same exact match as the migration: anything else is the user's own text.
    session = _new_session()
    project_sha = "1" * 64
    _add_blob(session, project_sha)

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"message": message, "manifest": _manifest_v2(project_sha, 10, [])},
    )

    assert response.status_code == 201, response.text
    assert response.json()["message"] == message


def test_create_version_normalizes_source_daw_case_and_spacing(tmp_path) -> None:
    session = _new_session()
    project_sha = "1" * 64
    _add_blob(session, project_sha)
    manifest = _manifest_v2(project_sha, 10, [])
    manifest["source_daw"] = "  ableton   LIVE "

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"manifest": manifest},
    )

    assert response.status_code == 201, response.text
    body = response.json()
    assert body["source_daw"] == "Ableton Live"
    assert body["manifest_json"]["source_daw"] == "Ableton Live"


def test_create_version_rejects_an_unsupported_source_daw(tmp_path) -> None:
    session = _new_session()
    project_sha = "1" * 64
    _add_blob(session, project_sha)
    manifest = _manifest_v2(project_sha, 10, [])
    manifest["source_daw"] = "Logic Pro"

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"manifest": manifest},
    )

    assert response.status_code == 422
    assert "FL Studio" in response.text and "Ableton Live" in response.text
    assert session.versions == []
    assert session.blobs[0].ref_count == 0


def test_create_version_from_manifest_returns_409_when_blobs_missing(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)

    proj_sha = "a" * 64
    asset_sha = "b" * 64

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.post(
        f"/branches/{branch.id}/versions/from-manifest",
        json={
            "manifest": _manifest(proj_sha, 10, asset_sha, 20),
        },
    )
    assert response.status_code == 409
    detail = response.json()["detail"]
    assert detail["error"] == "missing_blobs"
    assert set(detail["missing"]) == {proj_sha, asset_sha}
    assert session.versions == []


# ── delete_version ref_count decrement ──


def test_delete_cas_version_decrements_ref_count(tmp_path) -> None:
    user = _user()
    project = _project(user.id)
    branch = _branch(project.id)
    session = FakeSession(user=user, project=project, branch=branch)

    proj_sha = "c" * 64
    asset_sha = "d" * 64
    session.blobs.append(Blob(
        project_id=project.id, sha256=proj_sha, size_bytes=1,
        storage_uri="x", ref_count=1,
        created_at=datetime.now(timezone.utc),
    ))
    session.blobs.append(Blob(
        project_id=project.id, sha256=asset_sha, size_bytes=1,
        storage_uri="y", ref_count=2,  # referenced by another version too
        created_at=datetime.now(timezone.utc),
    ))
    version = Version(
        id=uuid.uuid4(),
        branch_id=branch.id,
        created_at=datetime.now(timezone.utc),
        is_deleted=False,
        manifest_json=_manifest(proj_sha, 1, asset_sha, 1),
        manifest_version=1,
    )
    session.versions.append(version)

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    response = client.delete(f"/versions/{version.id}")
    assert response.status_code == 204
    assert version.is_deleted is True
    assert next(b for b in session.blobs if b.sha256 == proj_sha).ref_count == 0
    assert next(b for b in session.blobs if b.sha256 == asset_sha).ref_count == 1


def test_ref_counts_return_to_their_start_after_creating_and_deleting_v1_and_v2_versions(tmp_path) -> None:
    """Create-time increments and delete-time decrements go through the same
    manifest reader, so a v1 and a v2 version created then deleted leave every
    blob where it started — including a blob shared by both versions and a
    blob listed twice in one manifest (counted once per version)."""
    session = _new_session()
    v1_project, v2_project, shared, doubled = "1" * 64, "2" * 64, "3" * 64, "4" * 64
    start = {v1_project: 0, v2_project: 3, shared: 1, doubled: 0}
    for sha, ref_count in start.items():
        _add_blob(session, sha, ref_count=ref_count)

    client = _make_client(session, LocalFilesystemStorageService(tmp_path))
    v1_response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={"commit_message": "v1", "manifest": _manifest(v1_project, 1, shared, 1)},
    )
    v2_response = client.post(
        f"/branches/{session.branch.id}/versions/from-manifest",
        json={
            "message": "v2",
            "manifest": _manifest_v2(
                v2_project,
                1,
                [(shared, 1, "Samples/shared.wav"), (doubled, 1, "Samples/a.wav"), (doubled, 1, "Samples/b.wav")],
            ),
        },
    )
    assert v1_response.status_code == 201, v1_response.text
    assert v2_response.status_code == 201, v2_response.text

    ref_counts = {b.sha256: b.ref_count for b in session.blobs}
    assert ref_counts == {v1_project: 1, v2_project: 4, shared: 3, doubled: 1}

    for response in (v1_response, v2_response):
        delete_response = client.delete(f"/versions/{response.json()['id']}")
        assert delete_response.status_code == 204

    assert {b.sha256: b.ref_count for b in session.blobs} == start


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
