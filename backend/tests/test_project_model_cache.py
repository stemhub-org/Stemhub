"""Real Postgres contracts; uses an isolated schema, never a developer database's tables."""

from __future__ import annotations

import asyncio
import io
import os
import threading
import time
import uuid
from contextlib import asynccontextmanager
from datetime import datetime, timedelta, timezone
from pathlib import Path

import httpx
import pytest
from alembic.config import Config
from alembic.operations import Operations
from alembic.runtime.migration import MigrationContext
from alembic.script import ScriptDirectory
from fastapi import FastAPI
from sqlalchemy import select, text, func
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker
from sqlalchemy.pool import NullPool

from flp_helpers import MULTI_CHANNEL
from stemhub.database import Base, get_db
from stemhub.auth import get_current_user
from stemhub.models import User, Project, Branch, Version, Blob, CachedProjectModel
from stemhub.project_model import cache
from stemhub.project_model.fl_reader import ProjectReadError
from stemhub.routers.versions import router as versions_router
from stemhub.routers.admin import router as admin_router
from stemhub.storage import LocalFilesystemStorageService, get_storage_service
from stemhub.blob_gc import sweep_orphan_blobs

pytestmark = pytest.mark.skipif(not os.getenv("STEMHUB_TEST_DATABASE_URL"), reason="requires test Postgres")


@asynccontextmanager
async def database():
    url = os.environ["STEMHUB_TEST_DATABASE_URL"]
    schema = "test_project_model_" + uuid.uuid4().hex
    admin = create_async_engine(url, poolclass=NullPool)
    async with admin.begin() as connection:
        await connection.execute(text(f'CREATE SCHEMA "{schema}"'))
    engine = create_async_engine(
        url, poolclass=NullPool, connect_args={"server_settings": {"search_path": schema}}
    )
    try:
        async with engine.begin() as connection:
            # Exercise the actual migration, rather than metadata.create_all for
            # the new table. Other tables are merely the integration-test setup.
            await connection.run_sync(
                lambda c: Base.metadata.create_all(
                    c, tables=[t for t in Base.metadata.sorted_tables if t.name != "project_model"]
                )
            )
            config = Config(str(Path(__file__).parents[1] / "alembic.ini"))
            config.set_main_option("script_location", str(Path(__file__).parents[1] / "alembic"))
            migration = ScriptDirectory.from_config(config).get_revision("e270a1b2c3d4").module

            def upgrade(c):
                with Operations.context(MigrationContext.configure(c)):
                    migration.upgrade()
                    migration.downgrade()
                    migration.upgrade()

            await connection.run_sync(upgrade)
        yield async_sessionmaker(engine, expire_on_commit=False)
    finally:
        await engine.dispose()
        async with admin.begin() as connection:
            await connection.execute(text(f'DROP SCHEMA "{schema}" CASCADE'))
        await admin.dispose()


async def seed(sessions, tmp_path, data=None):
    storage = LocalFilesystemStorageService(tmp_path)
    uid, pid, bid, vid = [uuid.uuid4() for _ in range(4)]
    stored = storage.store_blob(
        project_id=pid, source=io.BytesIO(data if data is not None else MULTI_CHANNEL.read_bytes())
    )
    manifest = {
        "manifest_version": 2,
        "source_daw": "FL Studio",
        "source_project_filename": "song.flp",
        "project_file": {
            "sha256": stored.checksum_sha256,
            "size_bytes": stored.size_bytes,
            "path": "song.flp",
        },
        "assets": [],
    }
    async with sessions() as db:
        user = User(id=uid, username="owner", email="owner@example.com", password_hash="test", is_admin=True)
        db.add(user)
        await db.flush()
        db.add(Project(id=pid, owner_id=uid, name="test", is_public=False))
        await db.flush()
        db.add(Branch(id=bid, project_id=pid, name="main"))
        db.add(
            Blob(
                project_id=pid,
                sha256=stored.checksum_sha256,
                size_bytes=stored.size_bytes,
                storage_uri=stored.path,
                ref_count=1,
                created_at=datetime.now(timezone.utc) - timedelta(days=2),
            )
        )
        await db.flush()
        db.add(
            Version(
                id=vid,
                branch_id=bid,
                created_by=uid,
                source_daw="FL Studio",
                manifest_json=manifest,
                manifest_version=2,
                source_project_filename="song.flp",
            )
        )
        await db.commit()
    return user, pid, bid, vid, manifest, storage


def client_for(sessions, user, storage):
    app = FastAPI()
    app.include_router(versions_router)
    app.include_router(admin_router)
    identity = [user]

    async def db():
        async with sessions() as session:
            yield session

    app.dependency_overrides[get_db] = db
    app.dependency_overrides[get_current_user] = lambda: identity[0]
    app.dependency_overrides[get_storage_service] = lambda: storage
    return app, identity


def test_cache_api_permissions_units_backfill_and_latency(tmp_path, monkeypatch):
    async def scenario():
        async with database() as sessions:
            user, pid, bid, vid, manifest, storage = await seed(sessions, tmp_path)
            app, identity = client_for(sessions, user, storage)
            original = cache.read_flp
            calls = []
            thread = threading.get_ident()

            def extract(path):
                calls.append(threading.get_ident())
                return original(path)

            monkeypatch.setattr(cache, "read_flp", extract)
            async with httpx.AsyncClient(
                transport=httpx.ASGITransport(app=app), base_url="http://test"
            ) as client:
                endpoint = f"/versions/{vid}/project-model"
                first = await client.get(endpoint)
                assert first.status_code == 200, first.text
                assert first.json()["source"]["ppq"] == 96
                assert calls and all(t != thread for t in calls)
                times = []
                for _ in range(20):
                    started = time.perf_counter()
                    result = await client.get(endpoint)
                    times.append(time.perf_counter() - started)
                    assert result.json() == first.json()
                assert len(calls) == 1
                p95 = sorted(times)[18]
                print(f"warm project-model API p95: {p95 * 1000:.1f} ms")
                assert p95 < 0.3
                identity[0] = User(id=uuid.uuid4(), is_admin=False)
                assert (await client.get(endpoint)).status_code == 404
                assert (await client.post("/api/admin/project-models/backfill")).status_code == 403
                identity[0] = user
                # The backfill reuses the identical persistent cache.
                backfill = await client.post("/api/admin/project-models/backfill", params={"limit": 1})
                assert backfill.status_code == 200, backfill.text
                assert backfill.json()["ready"] == 1
                assert len(calls) == 1
                other = uuid.uuid4()
                async with sessions() as db:
                    db.add(
                        Version(
                            id=other,
                            branch_id=bid,
                            source_daw="FL Studio",
                            manifest_json=manifest,
                            manifest_version=2,
                        )
                    )
                    await db.commit()
                assert (await client.get(f"/versions/{other}/project-model")).status_code == 200
                comparison = await client.get(
                    f"/branches/{bid}/versions/compare",
                    params={"base_version_id": str(vid), "target_version_id": str(other)},
                )
                assert comparison.status_code == 200, comparison.text
                assert comparison.json()["summary"]["total_changes"] == 0
                history = await client.get(f"/branches/{bid}/versions/diff-history")
                assert history.status_code == 200, history.text
                assert len(calls) == 1
                async with sessions() as db:
                    assert (await db.scalar(select(func.count()).select_from(CachedProjectModel))) == 1
                    row = await db.get(Version, vid)
                    row.is_deleted = True
                    await db.commit()
                assert (await client.get(endpoint)).status_code == 404

    asyncio.run(scenario())


def test_backfill_continues_after_bad_file_and_resumes(tmp_path):
    async def scenario():
        async with database() as sessions:
            user, pid, bid, vid, bad_manifest, storage = await seed(sessions, tmp_path, b"bad FLP")
            stored = storage.store_blob(project_id=pid, source=io.BytesIO(MULTI_CHANNEL.read_bytes()))
            manifest = dict(
                bad_manifest,
                project_file={
                    "sha256": stored.checksum_sha256,
                    "size_bytes": stored.size_bytes,
                    "path": "song.flp",
                },
            )
            async with sessions() as db:
                bad_version = await db.get(Version, vid)
                bad_version.id = uuid.UUID(int=1)
                db.add(
                    Blob(
                        project_id=pid,
                        sha256=stored.checksum_sha256,
                        size_bytes=stored.size_bytes,
                        storage_uri=stored.path,
                        ref_count=2,
                    )
                )
                await db.flush()
                for index in (2, 3):
                    db.add(
                        Version(
                            id=uuid.UUID(int=index),
                            branch_id=bid,
                            source_daw="FL Studio",
                            manifest_json=manifest,
                            manifest_version=2,
                        )
                    )
                db.add(Version(id=uuid.UUID(int=4), branch_id=bid, source_daw="Ableton Live"))
                await db.commit()
            app, _ = client_for(sessions, user, storage)
            async with httpx.AsyncClient(
                transport=httpx.ASGITransport(app=app), base_url="http://test"
            ) as client:
                first = await client.post(
                    "/api/admin/project-models/backfill", params={"limit": 2, "project_id": str(pid)}
                )
                assert first.status_code == 200, first.text
                result = first.json()
                assert result["failed"] == 1 and result["ready"] == 1
                second = await client.post(
                    "/api/admin/project-models/backfill",
                    params={"after_version_id": result["next_after_version_id"], "limit": 2},
                )
                assert second.status_code == 200, second.text
                assert second.json()["skipped"] == 1 and second.json()["ready"] == 1
                assert second.json()["next_after_version_id"] is None

    asyncio.run(scenario())


def test_transient_storage_failure_is_retryable(tmp_path, monkeypatch):
    async def scenario():
        async with database() as sessions:
            user, pid, bid, vid, manifest, storage = await seed(sessions, tmp_path)
            app, _ = client_for(sessions, user, storage)
            original = cache._extract

            def unavailable(*args):
                from stemhub.storage import StorageError

                raise StorageError("private /Users/Alice/secret.flp")

            monkeypatch.setattr(cache, "_extract", unavailable)
            async with httpx.AsyncClient(
                transport=httpx.ASGITransport(app=app), base_url="http://test"
            ) as client:
                response = await client.get(f"/versions/{vid}/project-model")
                assert response.status_code == 503
                assert "Alice" not in response.text
                async with sessions() as db:
                    assert await db.scalar(select(func.count()).select_from(CachedProjectModel)) == 0
                monkeypatch.setattr(cache, "_extract", original)
                assert (await client.get(f"/versions/{vid}/project-model")).status_code == 200

    asyncio.run(scenario())


def test_cache_is_project_scoped_versioned_and_gc_cascades(tmp_path, monkeypatch):
    async def scenario():
        async with database() as sessions:
            _, pid, _, _, manifest, storage = await seed(sessions, tmp_path)
            sha = manifest["project_file"]["sha256"]

            async def load(project_id=pid):
                async with sessions() as db:
                    return await cache.get_project_model(
                        project_id=project_id, sha256=sha, db=db, storage=storage
                    )

            await asyncio.gather(load(), load(), load())
            with pytest.raises(ProjectReadError, match="missing from storage"):
                await load(uuid.uuid4())
            monkeypatch.setattr(cache, "EXTRACTOR_VERSION", "next-extractor")
            await load()
            async with sessions() as db:
                assert await db.scalar(select(func.count()).select_from(CachedProjectModel)) == 2
                blob = await db.get(Blob, (pid, sha))
                assert blob.ref_count == 1
                blob.ref_count = 0
                await db.commit()
                result = await sweep_orphan_blobs(db=db, storage=storage)
                assert result.deleted == 1
                assert await db.scalar(select(func.count()).select_from(CachedProjectModel)) == 0
                assert await db.get(Blob, (pid, sha)) is None

    asyncio.run(scenario())


def test_parser_failure_is_cached_and_does_not_fail_save(tmp_path, monkeypatch):
    async def scenario():
        async with database() as sessions:
            user, pid, bid, vid, manifest, storage = await seed(sessions, tmp_path, b"corrupt flp")
            monkeypatch.setattr(cache, "AsyncSessionLocal", sessions)
            monkeypatch.setattr(cache, "get_storage_service", lambda: storage)
            app, _ = client_for(sessions, user, storage)
            async with httpx.AsyncClient(
                transport=httpx.ASGITransport(app=app), base_url="http://test"
            ) as client:
                result = await client.post(
                    f"/branches/{bid}/versions/from-manifest",
                    json={"message": "safe save", "manifest": manifest},
                )
                assert result.status_code == 201, result.text
                # Background work is independently awaited only by the test.
                await asyncio.gather(*list(cache._prefetch_tasks))
                async with sessions() as db:
                    rows = (await db.execute(select(CachedProjectModel))).scalars().all()
                    assert len(rows) == 1 and rows[0].error
                monkeypatch.setattr(
                    cache, "_extract", lambda *args: pytest.fail("negative cache must not reparse")
                )
                response = await client.get(f"/versions/{vid}/project-model")
                assert response.status_code == 422
                assert "structurally invalid" in response.json()["detail"]

    asyncio.run(scenario())
