"""Project-scoped persistent extraction cache; blocking work runs in a worker."""

from __future__ import annotations

import asyncio
import hashlib
import logging
from uuid import UUID

from anyio import CapacityLimiter, to_thread
from sqlalchemy import select
from sqlalchemy.dialects.postgresql import insert
from sqlalchemy.ext.asyncio import AsyncSession

from stemhub.database import AsyncSessionLocal
from stemhub.manifests import blob_refs
from stemhub.models import Blob, CachedProjectModel, Version
from stemhub.storage import StorageService, get_storage_service
from .fl_reader import EXTRACTOR_VERSION, ProjectReadError, read_flp

logger = logging.getLogger(__name__)
_prefetch_tasks: set[asyncio.Task] = set()
MAX_PREFETCH_TASKS = 16
_extraction_slots = CapacityLimiter(2)


def _extract(storage: StorageService, uri: str, sha256: str) -> dict:
    with storage.materialize_blob(uri) as path:
        with path.open("rb") as source:
            checksum = hashlib.sha256()
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                checksum.update(chunk)
            actual = checksum.hexdigest()
        if actual != sha256:
            raise ProjectReadError("The stored project file checksum does not match its manifest.")
        return read_flp(path).model_dump(mode="json")


async def get_project_model(
    *, project_id: UUID, sha256: str, db: AsyncSession, storage: StorageService
) -> dict:
    cached = await db.get(CachedProjectModel, (project_id, sha256, EXTRACTOR_VERSION))
    if cached is not None:
        if cached.error:
            raise ProjectReadError(cached.error)
        return cached.model_json
    result = await db.execute(select(Blob).where(Blob.project_id == project_id, Blob.sha256 == sha256))
    blob = result.scalars().first()
    if blob is None:
        raise ProjectReadError("The project file of this version is missing from storage.")
    error = None
    document = None
    try:
        document = await to_thread.run_sync(
            _extract, storage, blob.storage_uri, sha256, limiter=_extraction_slots
        )
    except ProjectReadError as exc:
        error = str(exc)
    # Storage/network failures are transient and are deliberately not cached.
    values = dict(
        project_id=project_id,
        project_file_sha256=sha256,
        extractor_version=EXTRACTOR_VERSION,
        model_json=document,
        error=error,
    )
    await db.execute(insert(CachedProjectModel).values(**values).on_conflict_do_nothing())
    await db.commit()
    if error:
        raise ProjectReadError(error)
    return document


async def model_for_version(
    *, version: Version, project_id: UUID, db: AsyncSession, storage: StorageService
) -> dict:
    project_file, _ = blob_refs(version.manifest_json)
    if project_file is None:
        raise ProjectReadError("This version has no project file to read.")
    return await get_project_model(project_id=project_id, sha256=project_file.sha256, db=db, storage=storage)


async def _prefetch(project_id: UUID, sha256: str) -> None:
    try:
        async with AsyncSessionLocal() as db:
            await get_project_model(
                project_id=project_id, sha256=sha256, db=db, storage=get_storage_service()
            )
    except Exception:
        logger.warning("Project model prefetch failed for project %s", project_id)


def prefetch_after_save(project_id: UUID, version: Version) -> None:
    """Schedule after commit with a separate session; never await parsing."""
    if len(_prefetch_tasks) >= MAX_PREFETCH_TASKS:
        return
    project_file, _ = blob_refs(version.manifest_json)
    if project_file is None or not (project_file.path or "").lower().endswith(".flp"):
        return
    task = asyncio.create_task(_prefetch(project_id, project_file.sha256))
    _prefetch_tasks.add(task)
    task.add_done_callback(_prefetch_tasks.discard)
