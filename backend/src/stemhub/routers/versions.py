import logging
from datetime import datetime, timezone
from typing import List
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from sqlalchemy.orm import selectinload

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.fl_mixer import (
    FlMixer,
    MixerDiffResult,
    MixerReadError,
    diff_fl_mixers,
    load_fl_mixer,
)
from stemhub.manifests import FileRef, blob_refs, blob_shas
from stemhub.models import Blob, Branch, Project, Version, User
from stemhub.routers._project_access import (
    get_project_with_owner_access,
    get_project_with_read_access,
)
from stemhub.schemas import (
    AssetSummary,
    MixerDiffChange,
    MixerDiffResponse,
    MixerDiffSummary,
    OwnerSummary,
    VersionDiffHistoryEntry,
    VersionFromManifestCreate,
    VersionResponse,
    VersionWithAuthor,
)
from stemhub.storage import StorageError, StorageService, get_storage_service

logger = logging.getLogger(__name__)

router = APIRouter(tags=["versions"])


async def _get_branch_with_access(
    *,
    branch_id: UUID,
    current_user: User,
    db: AsyncSession,
) -> Branch:
    result = await db.execute(
        select(Branch).join(Project).where(
            Branch.id == branch_id,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    branch = result.scalars().first()
    if branch is None:
        raise HTTPException(status_code=404, detail="Branch not found")
    await get_project_with_read_access(
        project_id=branch.project_id, current_user=current_user, db=db
    )
    return branch


async def _get_version_for_branch(
    *,
    branch_id: UUID,
    version_id: UUID,
    db: AsyncSession,
) -> Version:
    result = await db.execute(
        select(Version).where(
            Version.id == version_id,
            Version.branch_id == branch_id,
            Version.is_deleted == False,
        )
    )
    version = result.scalars().first()
    if not version:
        raise HTTPException(status_code=404, detail="Version not found in branch")
    return version


async def _list_branch_versions_for_history(
    *,
    branch_id: UUID,
    db: AsyncSession,
) -> list[Version]:
    result = await db.execute(
        select(Version)
        .options(selectinload(Version.author))
        .where(
            Version.branch_id == branch_id,
            Version.is_deleted == False,
        )
        .order_by(Version.created_at.desc())
    )
    return list(result.scalars().all())


def _build_version_with_author(*, version: Version, branch_name: str) -> VersionWithAuthor:
    author_summary = None
    if version.author:
        author_summary = OwnerSummary(id=version.author.id, username=version.author.username)

    return VersionWithAuthor(
        id=version.id,
        message=version.message,
        created_at=version.created_at,
        branch_name=branch_name,
        author=author_summary,
        source_daw=version.source_daw,
        source_project_filename=version.source_project_filename,
    )


def _project_file_ref(version: Version) -> FileRef | None:
    project_file, _ = blob_refs(version.manifest_json)
    return project_file


def _is_fl_studio_version(version: Version) -> bool:
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


async def _load_mixer_for_compare(
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


def _mixer_diff_summary(diff_result: MixerDiffResult) -> MixerDiffSummary:
    return MixerDiffSummary(
        total_changes=diff_result.summary.total_changes,
        inserts_changed=diff_result.summary.inserts_changed,
        slots_changed=diff_result.summary.slots_changed,
        parameter_changes=diff_result.summary.parameter_changes,
    )


def _mixer_diff_changes(diff_result: MixerDiffResult) -> list[MixerDiffChange]:
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


@router.post(
    "/branches/{branch_id}/versions/from-manifest",
    response_model=VersionResponse,
    status_code=status.HTTP_201_CREATED,
)
async def create_version_from_manifest(
    branch_id: UUID,
    payload: VersionFromManifestCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Create a new version by referencing already-uploaded blobs.

    The manifest (v1 or v2) lists SHA-256 hashes for the project file and
    every asset. All referenced blobs must already exist in the project
    (uploaded via PUT /projects/{pid}/blobs/{sha256}). ref_count is bumped
    atomically with the Version row insert, once per distinct hash.

    See docs/content-addressed-storage.md.
    """
    branch = await _get_branch_with_access(branch_id=branch_id, current_user=current_user, db=db)

    if payload.parent_version_id is not None:
        parent_result = await db.execute(
            select(Version.id).where(
                Version.id == payload.parent_version_id,
                Version.branch_id == branch_id,
                Version.is_deleted == False,
            )
        )
        if parent_result.scalar_one_or_none() is None:
            raise HTTPException(
                status_code=400,
                detail="parent_version_id must reference a live version on this branch",
            )

    manifest = payload.manifest
    manifest_json = manifest.model_dump(mode="json")
    # Read back through the same reader delete_version uses, so the increment
    # here and the decrement there always cover the same hashes.
    required_shas = blob_shas(manifest_json)

    existing_result = await db.execute(
        select(Blob).where(
            Blob.project_id == branch.project_id,
            Blob.sha256.in_(required_shas),
        )
    )
    existing_blobs = list(existing_result.scalars().all())
    existing_shas = {b.sha256 for b in existing_blobs}
    missing = required_shas - existing_shas
    if missing:
        raise HTTPException(
            status_code=409,
            detail={
                "error": "missing_blobs",
                "missing": sorted(missing),
            },
        )

    for blob in existing_blobs:
        blob.ref_count = blob.ref_count + 1

    db_version = Version(
        branch_id=branch_id,
        created_by=current_user.id,
        message=payload.message,
        parent_version_id=payload.parent_version_id,
        source_daw=manifest.source_daw,
        source_project_filename=manifest.source_project_filename,
        manifest_json=manifest_json,
        manifest_version=manifest.manifest_version,
    )
    db.add(db_version)
    await db.commit()
    await db.refresh(db_version)
    return db_version


@router.get("/branches/{branch_id}/versions/", response_model=List[VersionResponse])
async def list_versions(
    branch_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    List all versions for a specific branch.
    """
    await _get_branch_with_access(branch_id=branch_id, current_user=current_user, db=db)

    result_versions = await db.execute(select(Version).where(Version.branch_id == branch_id, Version.is_deleted == False))
    return result_versions.scalars().all()


@router.get("/branches/{branch_id}/versions/compare", response_model=MixerDiffResponse)
async def compare_versions(
    branch_id: UUID,
    base_version_id: UUID = Query(...),
    target_version_id: UUID = Query(...),
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    """
    Compare the mixers of two FL Studio versions from the same branch.
    """
    if base_version_id == target_version_id:
        raise HTTPException(status_code=400, detail="base_version_id and target_version_id must be different")

    branch = await _get_branch_with_access(branch_id=branch_id, current_user=current_user, db=db)
    base_version = await _get_version_for_branch(branch_id=branch_id, version_id=base_version_id, db=db)
    target_version = await _get_version_for_branch(branch_id=branch_id, version_id=target_version_id, db=db)

    if not _is_fl_studio_version(base_version) or not _is_fl_studio_version(target_version):
        raise HTTPException(status_code=422, detail="Only FL Studio versions can be compared")

    base_mixer = await _load_mixer_for_compare(
        version=base_version, project_id=branch.project_id, db=db, storage=storage
    )
    target_mixer = await _load_mixer_for_compare(
        version=target_version, project_id=branch.project_id, db=db, storage=storage
    )

    diff_result = diff_fl_mixers(base_mixer, target_mixer)
    return MixerDiffResponse(
        summary=_mixer_diff_summary(diff_result),
        changes=_mixer_diff_changes(diff_result),
    )


@router.get("/branches/{branch_id}/versions/diff-history", response_model=List[VersionDiffHistoryEntry])
async def list_version_diff_history(
    branch_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    """
    Return branch history where each version is compared against its parent or previous version.
    """
    branch = await _get_branch_with_access(branch_id=branch_id, current_user=current_user, db=db)
    versions = await _list_branch_versions_for_history(branch_id=branch_id, db=db)
    versions_by_id = {version.id: version for version in versions}
    mixers_cache: dict[UUID, FlMixer] = {}
    history_entries: list[VersionDiffHistoryEntry] = []

    for index, version in enumerate(versions):
        compared_to_version = None
        if version.parent_version_id:
            compared_to_version = versions_by_id.get(version.parent_version_id)
        if compared_to_version is None and index + 1 < len(versions):
            compared_to_version = versions[index + 1]

        version_payload = _build_version_with_author(version=version, branch_name=branch.name)

        if compared_to_version is None:
            history_entries.append(
                VersionDiffHistoryEntry(
                    version=version_payload,
                    compared_to_version_id=None,
                    status="initial",
                    status_message="First version on this branch.",
                    summary=None,
                    changes=[],
                )
            )
            continue

        if not _is_fl_studio_version(version) or not _is_fl_studio_version(compared_to_version):
            history_entries.append(
                VersionDiffHistoryEntry(
                    version=version_payload,
                    compared_to_version_id=compared_to_version.id,
                    status="unsupported",
                    status_message="Automatic mixer diff is only available for FL Studio versions.",
                    summary=None,
                    changes=[],
                )
            )
            continue

        try:
            current_mixer = mixers_cache.get(version.id)
            if current_mixer is None:
                current_mixer = await _load_mixer_for_compare(
                    version=version, project_id=branch.project_id, db=db, storage=storage
                )
                mixers_cache[version.id] = current_mixer

            base_mixer = mixers_cache.get(compared_to_version.id)
            if base_mixer is None:
                base_mixer = await _load_mixer_for_compare(
                    version=compared_to_version, project_id=branch.project_id, db=db, storage=storage
                )
                mixers_cache[compared_to_version.id] = base_mixer
        except HTTPException as exc:
            history_entries.append(
                VersionDiffHistoryEntry(
                    version=version_payload,
                    compared_to_version_id=compared_to_version.id,
                    status="unsupported",
                    status_message=str(exc.detail),
                    summary=None,
                    changes=[],
                )
            )
            continue

        diff_result = diff_fl_mixers(base_mixer, current_mixer)
        history_entries.append(
            VersionDiffHistoryEntry(
                version=version_payload,
                compared_to_version_id=compared_to_version.id,
                status="compared",
                status_message=None,
                summary=_mixer_diff_summary(diff_result),
                changes=_mixer_diff_changes(diff_result),
            )
        )

    return history_entries


async def _get_version_with_access(
    *,
    version_id: UUID,
    current_user: User,
    db: AsyncSession,
) -> Version:
    result = await db.execute(
        select(Version, Branch.project_id).join(Branch).join(Project).where(
            Version.id == version_id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    row = result.first()
    if not row:
        raise HTTPException(status_code=404, detail="Version not found")
    version, project_id = row
    await get_project_with_read_access(
        project_id=project_id, current_user=current_user, db=db
    )
    return version


def _file_type_from_path(path: str) -> str | None:
    filename = path.rsplit("/", 1)[-1]
    if "." not in filename:
        return None
    return filename.rsplit(".", 1)[-1].lower() or None


def _assets_from_manifest(manifest_json: dict) -> list[AssetSummary]:
    """Map a manifest's assets (v1 or v2, read by stemhub.manifests) to AssetSummary rows.

    Skips entries without a usable path so one bad row (unexpected shape from
    an older plugin build, hand-edited row) doesn't 500 the endpoint. Logs
    each skip so corrupt manifests remain visible in ops.
    """
    _, assets = blob_refs(manifest_json)
    summaries: list[AssetSummary] = []
    for asset in assets:
        if asset.path is None or asset.name is None:
            logger.warning("Skipping manifest asset %s: missing or invalid path", asset.index)
            continue
        summaries.append(
            AssetSummary(
                # sha256 alone isn't unique across assets (two assets may
                # intentionally hold identical audio), so suffix with the
                # position in the stored manifest list, which skipped entries
                # don't shift. Stable as long as the plugin lists assets in a
                # deterministic order.
                id=f"{asset.sha256}:{asset.index}",
                path=asset.path,
                name=asset.name,
                file_type=_file_type_from_path(asset.path),
                size_bytes=asset.size_bytes,
            )
        )
    return summaries


@router.get("/versions/{version_id}", response_model=VersionResponse)
async def get_version(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific version by ID.
    """
    return await _get_version_with_access(version_id=version_id, current_user=current_user, db=db)


@router.get("/versions/{version_id}/assets", response_model=List[AssetSummary])
async def list_version_assets(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    List the audio and MIDI files (assets) of a version, without its project file.

    Reads from `Version.manifest_json` (content-addressed manifest, spec §7),
    v1 or v2. Returns an empty list, not an error, when the version has no
    manifest (versions saved before content-addressed storage).
    """
    version = await _get_version_with_access(version_id=version_id, current_user=current_user, db=db)

    if isinstance(version.manifest_json, dict):
        return _assets_from_manifest(version.manifest_json)
    return []

@router.delete("/versions/{version_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_version(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """Delete a version (owner only)."""
    result = await db.execute(
        select(Version, Branch.project_id)
        .join(Branch, Version.branch_id == Branch.id)
        .join(Project, Branch.project_id == Project.id)
        .where(
            Version.id == version_id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    row = result.first()
    if not row:
        raise HTTPException(status_code=404, detail="Version not found")
    version, project_id = row
    await get_project_with_owner_access(
        project_id=project_id, current_user=current_user, db=db
    )

    # If this is a CAS version, decrement ref_count on each referenced blob,
    # read through the same reader create_version_from_manifest used to bump it.
    # Blobs whose ref_count drops to 0 will be reclaimed by the GC sweep after
    # the grace window (see docs/content-addressed-storage.md).
    if version.manifest_json:
        referenced_shas = blob_shas(version.manifest_json)
        if referenced_shas:
            blob_result = await db.execute(
                select(Blob).where(
                    Blob.project_id == project_id,
                    Blob.sha256.in_(referenced_shas),
                )
            )
            for blob in blob_result.scalars().all():
                if blob.ref_count > 0:
                    blob.ref_count = blob.ref_count - 1

    version.is_deleted = True
    version.deleted_at = datetime.now(timezone.utc)
    await db.commit()
