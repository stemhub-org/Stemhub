"""Garbage collection for content-addressed blobs.

Blobs with ``ref_count == 0`` and older than the grace window get their
storage bytes deleted and their DB row removed. The grace window prevents
races between blob upload (idempotent, by SHA-256) and the subsequent
version-create call that would increment ref_count.

This module exports a callable. It is NOT a scheduler — wire it into a
cron job, a background worker, or a manual admin endpoint depending on
deployment target.

See docs/content-addressed-storage.md.
"""
from __future__ import annotations

import logging
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from .models import Blob
from .storage import StorageError, StorageService

logger = logging.getLogger(__name__)

DEFAULT_GRACE_HOURS = 24


@dataclass(frozen=True)
class GcResult:
    scanned: int
    deleted: int
    failed: int


async def sweep_orphan_blobs(
    *,
    db: AsyncSession,
    storage: StorageService,
    grace_hours: int = DEFAULT_GRACE_HOURS,
    batch_size: int = 500,
) -> GcResult:
    """Delete blobs with ref_count == 0 older than the grace window.

    Storage bytes are deleted before the DB row so that a mid-sweep crash
    leaves the DB pointing at real bytes (never the reverse). Failed
    storage deletions are logged and the DB row is left intact for the
    next sweep to retry.
    """
    cutoff = datetime.now(timezone.utc) - timedelta(hours=grace_hours)
    result = await db.execute(
        select(Blob)
        .where(Blob.ref_count == 0, Blob.created_at < cutoff)
        .limit(batch_size)
    )
    candidates = list(result.scalars().all())

    deleted = 0
    failed = 0
    for blob in candidates:
        try:
            storage.delete_blob(blob.storage_uri)
        except StorageError as exc:
            logger.warning(
                "gc: failed to delete blob %s/%s from storage: %s",
                blob.project_id, blob.sha256, exc,
            )
            failed += 1
            continue
        await db.delete(blob)
        deleted += 1

    if deleted:
        await db.commit()

    return GcResult(scanned=len(candidates), deleted=deleted, failed=failed)
