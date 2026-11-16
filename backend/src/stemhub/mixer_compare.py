"""Comparing the FL Studio mixers of two versions.

Shared by the version history (routers/versions.py) and the pull request diff
(routers/pull_requests.py), so both describe a mixer change the same way.
"""
from uuid import UUID

from fastapi import HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.fl_mixer import FlMixer, MixerDiffResult, MixerReadError, load_fl_mixer
from stemhub.manifests import FileRef, blob_refs
from stemhub.models import Blob, Version
from stemhub.schemas import MixerDiffChange, MixerDiffSummary
from stemhub.storage import StorageError, StorageService


def _project_file_ref(version: Version) -> FileRef | None:
    project_file, _ = blob_refs(version.manifest_json)
    return project_file


def is_fl_studio_version(version: Version) -> bool:
    # Stored rows are read as they are (source_daw is only normalized on write).
    if version.source_daw:
        return version.source_daw.strip().casefold() == "fl studio"
    if version.source_project_filename and version.source_project_filename.lower().endswith(".flp"):
        return True
    project_file = _project_file_ref(version)
    path = project_file.path if project_file else None
    return path is not None and path.lower().endswith(".flp")


def _mixer_read_error_detail(exc: Exception) -> str:
    if isinstance(exc, (MixerReadError, StorageError, RuntimeError)):
        return str(exc)
    return f"Failed to read the FL Studio project file: {exc}"


async def load_mixer_for_compare(
    *,
    version: Version,
    project_id: UUID,
    db: AsyncSession,
    storage: StorageService,
) -> FlMixer:
    project_file = _project_file_ref(version)
    if project_file is None:
        raise HTTPException(status_code=422, detail="This version has no project file to compare.")

    blob_result = await db.execute(
        select(Blob).where(Blob.project_id == project_id, Blob.sha256 == project_file.sha256)
    )
    blob = blob_result.scalars().first()
    if blob is None:
        raise HTTPException(status_code=422, detail="The project file of this version is missing from storage.")

    try:
        return load_fl_mixer(storage_uri=blob.storage_uri, storage=storage)
    except Exception as exc:
        raise HTTPException(status_code=422, detail=_mixer_read_error_detail(exc)) from exc


def mixer_diff_summary(diff_result: MixerDiffResult) -> MixerDiffSummary:
    return MixerDiffSummary(
        total_changes=diff_result.summary.total_changes,
        inserts_changed=diff_result.summary.inserts_changed,
        slots_changed=diff_result.summary.slots_changed,
        parameter_changes=diff_result.summary.parameter_changes,
    )


def mixer_diff_changes(diff_result: MixerDiffResult) -> list[MixerDiffChange]:
    return [
        MixerDiffChange(
            type=change.type,
            insert_index=change.insert_index,
            insert_name=change.insert_name,
            slot_index=change.slot_index,
            before=change.before,
            after=change.after,
            message=change.message,
        )
        for change in diff_result.changes
    ]
