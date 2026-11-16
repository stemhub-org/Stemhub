"""PostgreSQL regressions for promotion storage safety and terminal-state races.

Run with STEMHUB_TEST_DATABASE_URL pointing to a disposable PostgreSQL database.
Each test creates and removes its own schema; existing schemas are untouched.
"""
import asyncio
import os
import uuid
from contextlib import asynccontextmanager
from datetime import datetime, timedelta, timezone

import pytest
from fastapi import HTTPException
from sqlalchemy import select, text
from sqlalchemy.ext.asyncio import async_sessionmaker, create_async_engine

from stemhub.blob_gc import sweep_orphan_blobs
from stemhub.database import Base
from stemhub.models import Blob, Branch, Project, PullRequest, User, Version
from stemhub.routers import pull_requests
from stemhub.routers.versions import delete_version
from stemhub.storage import LocalFilesystemStorageService

DATABASE_URL = os.getenv("STEMHUB_TEST_DATABASE_URL")
pytestmark = pytest.mark.skipif(not DATABASE_URL, reason="Requires STEMHUB_TEST_DATABASE_URL (PostgreSQL)")


@asynccontextmanager
async def _database():
    schema = f"pr_test_{uuid.uuid4().hex}"
    admin = create_async_engine(DATABASE_URL)
    engine = None
    try:
        async with admin.begin() as conn:
            await conn.execute(text(f'CREATE SCHEMA "{schema}"'))
        engine = create_async_engine(DATABASE_URL, connect_args={"server_settings": {"search_path": schema}})
        async with engine.begin() as conn:
            await conn.run_sync(Base.metadata.create_all)
        yield engine, async_sessionmaker(engine, expire_on_commit=False)
    finally:
        if engine is not None:
            await engine.dispose()
        async with admin.begin() as conn:
            await conn.execute(text(f'DROP SCHEMA IF EXISTS "{schema}" CASCADE'))
        await admin.dispose()


async def _seed(sessions, *, manifest_version=2):
    async with sessions() as db:
        owner = User(email="owner@example.com", username="owner", password_hash="unused")
        db.add(owner)
        await db.flush()
        project = Project(owner_id=owner.id, name="Promotion test")
        other_project = Project(owner_id=owner.id, name="Other project")
        db.add_all([project, other_project])
        await db.flush()
        source = Branch(project_id=project.id, name="feature")
        target = Branch(project_id=project.id, name="main")
        db.add_all([source, target])
        await db.flush()
        # Repeated hashes must still count once; the same hash in another
        # project must never be incremented by this project's promotion.
        assets_key = "tracks" if manifest_version == 1 else "assets"
        manifest = {
            "manifest_version": manifest_version,
            "project_file": {"sha256": "a" * 64},
            assets_key: [{"sha256": sha} for sha in ("a" * 64, "b" * 64, "b" * 64)],
        }
        version = Version(branch_id=source.id, created_by=owner.id, manifest_json=manifest,
                          manifest_version=manifest_version, message="Source version")
        db.add(version)
        for sha in ("a" * 64, "b" * 64):
            db.add(Blob(project_id=project.id, sha256=sha, size_bytes=1,
                        storage_uri=f"{sha}.bin", ref_count=1,
                        created_at=datetime.now(timezone.utc) - timedelta(days=2)))
        db.add(Blob(project_id=other_project.id, sha256="a" * 64, size_bytes=1,
                    storage_uri="unrelated.bin", ref_count=7))
        await db.flush()
        pr = PullRequest(project_id=project.id, source_branch_id=source.id, target_branch_id=target.id,
                         source_head_version_id=version.id, title="Promote source", created_by=owner.id)
        db.add(pr)
        await db.commit()
        return owner, project.id, target.id, version.id, pr.id


@pytest.mark.parametrize("manifest_version", [1, 2])
def test_promoted_files_survive_source_deletion_and_gc(tmp_path, manifest_version):
    async def scenario():
        async with _database() as (_, sessions):
            owner, project_id, target_id, source_id, pr_id = await _seed(sessions, manifest_version=manifest_version)
            storage = LocalFilesystemStorageService(tmp_path)
            for sha in ("a" * 64, "b" * 64):
                (tmp_path / f"{sha}.bin").write_bytes(b"x")
            async with sessions() as db:
                await pull_requests.accept_pull_request(pr_id, current_user=owner, db=db)
            async with sessions() as db:
                counts = (await db.execute(select(Blob.ref_count).where(Blob.project_id == project_id))).scalars().all()
                assert counts == [2, 2]
                unrelated = (await db.execute(select(Blob).where(Blob.project_id != project_id))).scalar_one()
                assert unrelated.ref_count == 7
                # An already accepted PR must neither promote nor count again.
                with pytest.raises(HTTPException) as exc:
                    await pull_requests.accept_pull_request(pr_id, current_user=owner, db=db)
                assert exc.value.status_code == 409
                await db.rollback()
            async with sessions() as db:
                await delete_version(source_id, current_user=owner, db=db)
            async with sessions() as db:
                result = await sweep_orphan_blobs(db=db, storage=storage)
                assert result.deleted == 0
                blobs = (await db.execute(select(Blob).where(Blob.project_id == project_id))).scalars().all()
                assert len(blobs) == 2
                assert all(blob.ref_count == 1 for blob in blobs)
                assert all(storage.resolve_blob_path(blob.storage_uri).read_bytes() == b"x" for blob in blobs)
                promoted = (await db.execute(select(Version).where(Version.branch_id == target_id))).scalar_one()
                assert not promoted.is_deleted
    asyncio.run(scenario())


@pytest.mark.parametrize("winner", ["accept", "close"])
def test_concurrent_close_and_accept_have_only_one_terminal_transition(monkeypatch, winner):
    async def scenario():
        async with _database() as (engine, sessions):
            owner, project_id, target_id, _, pr_id = await _seed(sessions)
            first_locked = asyncio.Event()
            release_first = asyncio.Event()
            original = pull_requests._get_pull_request_or_404
            async with sessions() as first, sessions() as second:
                async def gated_read(**kwargs):
                    pr = await original(**kwargs)
                    if kwargs["db"] is first:
                        first_locked.set()
                        await release_first.wait()
                    return pr
                monkeypatch.setattr(pull_requests, "_get_pull_request_or_404", gated_read)
                second_pid = (await second.execute(text("SELECT pg_backend_pid()"))).scalar_one()

                async def transition(action, db):
                    handler = getattr(pull_requests, f"{action}_pull_request")
                    try:
                        result = await handler(pr_id, current_user=owner, db=db)
                        return result.status
                    except HTTPException as exc:
                        await db.rollback()
                        return exc.status_code

                first_task = asyncio.create_task(transition(winner, first))
                second_task = None
                try:
                    await asyncio.wait_for(first_locked.wait(), timeout=5)
                    second_task = asyncio.create_task(transition("close" if winner == "accept" else "accept", second))
                    # Observe a real PostgreSQL lock wait before releasing the
                    # first transaction; no assumption about task scheduling.
                    for _ in range(200):
                        async with engine.connect() as conn:
                            waiting = (await conn.execute(text(
                                "SELECT wait_event_type = 'Lock' FROM pg_stat_activity WHERE pid = :pid"
                            ), {"pid": second_pid})).scalar_one()
                        if waiting:
                            break
                        await asyncio.sleep(0.01)
                    else:
                        pytest.fail("The second transition never waited for the first transaction")
                    release_first.set()
                    results = await asyncio.wait_for(asyncio.gather(first_task, second_task), timeout=5)
                    expected_status = "MERGED" if winner == "accept" else "CLOSED"
                    assert results == [expected_status, 409]
                finally:
                    release_first.set()
                    tasks = [task for task in (first_task, second_task) if task is not None]
                    for task in tasks:
                        if not task.done():
                            task.cancel()
                    await asyncio.gather(*tasks, return_exceptions=True)
            async with sessions() as db:
                pr = await db.get(PullRequest, pr_id)
                assert pr.status == expected_status
                promoted = (await db.execute(select(Version).where(Version.branch_id == target_id))).scalars().all()
                assert len(promoted) == (1 if winner == "accept" else 0)
                counts = (await db.execute(select(Blob.ref_count).where(Blob.project_id == project_id))).scalars().all()
                assert counts == ([2, 2] if winner == "accept" else [1, 1])
    asyncio.run(scenario())
