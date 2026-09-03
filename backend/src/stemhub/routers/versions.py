from fastapi import APIRouter, Depends, HTTPException, Query, status
from datetime import datetime, timezone
from typing import List
from uuid import UUID

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from sqlalchemy.orm import selectinload

from stemhub.database import get_db
from stemhub.models import Blob, Collaborator, Track, Version, Branch, Project, User
from stemhub.schemas import (
    MixerDiffChange,
    MixerDiffResponse,
    MixerDiffSummary,
    OwnerSummary,
    TrackSummary,
    VersionCreate,
    VersionDiffHistoryEntry,
    VersionFromManifestCreate,
    VersionResponse,
    VersionWithAuthor,
)
from stemhub.auth import get_current_user
from stemhub.flp_mixer_snapshot import MixerSnapshotError, diff_mixer_project_snapshots, load_fl_studio_mixer_snapshot
from stemhub.storage import StorageError, StorageService, get_storage_service

router = APIRouter(tags=["versions"])


def _extract_manifest_blob_shas(manifest: dict) -> set[str]:
    """Return every sha256 referenced by a v1 manifest.

    Defensive: reads only the fields we know about and skips anything else.
    Callers should already have validated the manifest at write time.
    """
    shas: set[str] = set()
    project_file = manifest.get("project_file") or {}
    if isinstance(project_file, dict):
        sha = project_file.get("sha256")
        if isinstance(sha, str):
            shas.add(sha)
    for track in manifest.get("tracks") or []:
        if isinstance(track, dict):
            sha = track.get("sha256")
            if isinstance(sha, str):
                shas.add(sha)
    return shas


async def _can_access_project(
    *,
    project_id: UUID,
    current_user: User,
    db: AsyncSession,
) -> bool:
    project_result = await db.execute(
        select(Project).where(
            Project.id == project_id,
            Project.is_deleted == False,
        )
    )
    project = project_result.scalars().first()
    if not project:
        return False

    if project.owner_id == current_user.id:
        return True

    collaborator_result = await db.execute(
        select(Collaborator).where(
            Collaborator.project_id == project_id,
            Collaborator.user_id == current_user.id,
        )
    )
    return collaborator_result.scalars().first() is not None


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
    if not branch:
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")
    if not await _can_access_project(project_id=branch.project_id, current_user=current_user, db=db):
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")
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
        commit_message=version.commit_message,
        created_at=version.created_at,
        branch_name=branch_name,
        author=author_summary,
        has_artifact=version.artifact_path is not None,
        source_daw=version.source_daw,
        source_project_filename=version.source_project_filename,
    )


def _is_fl_studio_version(version: Version) -> bool:
    if version.source_daw:
        return version.source_daw.strip().casefold() == "fl studio"
    if version.source_project_filename and version.source_project_filename.lower().endswith(".flp"):
        return True
    manifest_path = None
    if isinstance(version.snapshot_manifest, dict):
        manifest_path = version.snapshot_manifest.get("flp_relative_path")
    return isinstance(manifest_path, str) and manifest_path.lower().endswith(".flp")


def _snapshot_compare_error_detail(exc: Exception) -> str:
    if isinstance(exc, (MixerSnapshotError, StorageError, RuntimeError)):
        return str(exc)
    return f"Failed to read FL Studio snapshot: {exc}"


def _load_snapshot_for_compare(
    *,
    version: Version,
    storage: StorageService,
) -> object:
    try:
        return load_fl_studio_mixer_snapshot(
            artifact_path=version.artifact_path,
            snapshot_manifest=version.snapshot_manifest,
            storage=storage,
        )
    except Exception as exc:
        raise HTTPException(status_code=422, detail=_snapshot_compare_error_detail(exc)) from exc

@router.post("/branches/{branch_id}/versions/", response_model=VersionResponse, status_code=status.HTTP_201_CREATED)
async def create_version(
    branch_id: UUID,
    version_in: VersionCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Create a new version (commit) for a specific branch.
    """
    # Verify the branch exists and the user has access to its project
    result = await db.execute(
        select(Branch).join(Project).where(
            Branch.id == branch_id,
            Branch.is_deleted == False,
            Project.is_deleted == False
        )
    )
    branch = result.scalars().first()
    if not branch:
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")
    if not await _can_access_project(project_id=branch.project_id, current_user=current_user, db=db):
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")

    db_version = Version(**version_in.model_dump(), branch_id=branch_id, created_by=current_user.id)
    db.add(db_version)
    await db.commit()
    await db.refresh(db_version)
    return db_version


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

    The manifest lists SHA-256 hashes for the project file and every track.
    All referenced blobs must already exist in the project (uploaded via
    PUT /projects/{pid}/blobs/{sha256}). ref_count is bumped atomically
    with the Version row insert.

    See docs/content-addressed-storage.md.
    """
    result = await db.execute(
        select(Branch).join(Project).where(
            Branch.id == branch_id,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    branch = result.scalars().first()
    if branch is None:
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")
    if not await _can_access_project(project_id=branch.project_id, current_user=current_user, db=db):
        raise HTTPException(status_code=404, detail="Branch not found or you don't have access")

    manifest = payload.manifest
    required_shas = manifest.all_blob_shas()

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
        commit_message=payload.commit_message,
        parent_version_id=payload.parent_version_id,
        source_daw=manifest.source_daw,
        source_project_filename=manifest.source_project_filename,
        manifest_json=manifest.model_dump(mode="json"),
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
    Compare two FL Studio versions from the same branch using mixer-state semantics.
    """
    if base_version_id == target_version_id:
        raise HTTPException(status_code=400, detail="base_version_id and target_version_id must be different")

    await _get_branch_with_access(branch_id=branch_id, current_user=current_user, db=db)
    base_version = await _get_version_for_branch(branch_id=branch_id, version_id=base_version_id, db=db)
    target_version = await _get_version_for_branch(branch_id=branch_id, version_id=target_version_id, db=db)

    if not base_version.artifact_path or not target_version.artifact_path:
        raise HTTPException(status_code=422, detail="Both versions must have snapshot artifacts to be compared")

    if not _is_fl_studio_version(base_version) or not _is_fl_studio_version(target_version):
        raise HTTPException(status_code=422, detail="Only FL Studio versions can be compared")

    base_snapshot = _load_snapshot_for_compare(version=base_version, storage=storage)
    target_snapshot = _load_snapshot_for_compare(version=target_version, storage=storage)

    diff_result = diff_mixer_project_snapshots(base_snapshot, target_snapshot)
    return MixerDiffResponse(
        summary=MixerDiffSummary(
            total_changes=diff_result.summary.total_changes,
            inserts_changed=diff_result.summary.inserts_changed,
            slots_changed=diff_result.summary.slots_changed,
            parameter_changes=diff_result.summary.parameter_changes,
        ),
        changes=[
            MixerDiffChange(
                type=change.type,
                insert_iid=change.insert_iid,
                insert_name=change.insert_name,
                slot_index=change.slot_index,
                before=change.before,
                after=change.after,
                message=change.message,
            )
            for change in diff_result.changes
        ],
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
    snapshots_cache = {}
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
                    status_message="Initial snapshot on this branch.",
                    summary=None,
                    changes=[],
                )
            )
            continue

        if not version.artifact_path or not compared_to_version.artifact_path:
            history_entries.append(
                VersionDiffHistoryEntry(
                    version=version_payload,
                    compared_to_version_id=compared_to_version.id,
                    status="unsupported",
                    status_message="Automatic mixer diff unavailable because one of the versions has no snapshot artifact.",
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
            current_snapshot = snapshots_cache.get(version.id)
            if current_snapshot is None:
                current_snapshot = _load_snapshot_for_compare(version=version, storage=storage)
                snapshots_cache[version.id] = current_snapshot

            base_snapshot = snapshots_cache.get(compared_to_version.id)
            if base_snapshot is None:
                base_snapshot = _load_snapshot_for_compare(version=compared_to_version, storage=storage)
                snapshots_cache[compared_to_version.id] = base_snapshot
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

        diff_result = diff_mixer_project_snapshots(base_snapshot, current_snapshot)
        history_entries.append(
            VersionDiffHistoryEntry(
                version=version_payload,
                compared_to_version_id=compared_to_version.id,
                status="compared",
                status_message=None,
                summary=MixerDiffSummary(
                    total_changes=diff_result.summary.total_changes,
                    inserts_changed=diff_result.summary.inserts_changed,
                    slots_changed=diff_result.summary.slots_changed,
                    parameter_changes=diff_result.summary.parameter_changes,
                ),
                changes=[
                    MixerDiffChange(
                        type=change.type,
                        insert_iid=change.insert_iid,
                        insert_name=change.insert_name,
                        slot_index=change.slot_index,
                        before=change.before,
                        after=change.after,
                        message=change.message,
                    )
                    for change in diff_result.changes
                ],
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
            Project.is_deleted == False
        )
    )
    row = result.first()
    if not row:
        raise HTTPException(status_code=404, detail="Version not found")
    version, project_id = row
    if not await _can_access_project(project_id=project_id, current_user=current_user, db=db):
        raise HTTPException(status_code=404, detail="Version not found")
    return version


def _file_type_from_filename(filename: str) -> str | None:
    if "." not in filename:
        return None
    return filename.rsplit(".", 1)[-1].lower()


def _tracks_from_manifest(manifest: dict) -> list[TrackSummary]:
    """Map manifest_json["tracks"] (see VersionManifestV1) to TrackSummary rows."""
    summaries: list[TrackSummary] = []
    for index, track in enumerate(manifest.get("tracks") or []):
        if not isinstance(track, dict):
            continue
        sha256 = track.get("sha256")
        name = track.get("name")
        if not isinstance(sha256, str) or not isinstance(name, str):
            continue
        filename = track.get("filename")
        summaries.append(
            TrackSummary(
                # sha256 alone isn't guaranteed unique across tracks (two
                # tracks may intentionally reference identical audio), so
                # suffix with the manifest position to keep ids unique.
                id=f"{sha256}:{index}",
                name=name,
                file_type=_file_type_from_filename(filename) if isinstance(filename, str) else None,
                bpm=track.get("bpm"),
                key=track.get("key"),
                duration_seconds=track.get("duration_seconds"),
                size_bytes=track.get("size_bytes"),
                source="manifest",
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


@router.get("/versions/{version_id}/tracks", response_model=List[TrackSummary])
async def list_version_tracks(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    List the stems/tracks known for a version.

    Reads whichever track-data source the version actually has: the
    content-addressed manifest (populated by the from-manifest create flow)
    or the legacy Track table (populated by nothing today, kept for forward
    compatibility). Returns an empty list, not an error, when neither is
    populated — most FL Studio snapshot-flow versions have no per-track data.
    """
    version = await _get_version_with_access(version_id=version_id, current_user=current_user, db=db)

    if isinstance(version.manifest_json, dict):
        return _tracks_from_manifest(version.manifest_json)

    result = await db.execute(select(Track).where(Track.version_id == version_id))
    return [
        TrackSummary(
            id=str(track.id),
            name=track.name,
            # Normalize to match the manifest path's format (no leading dot) —
            # Track.file_type is stored with a leading dot (e.g. ".wav").
            file_type=track.file_type.lstrip(".") if track.file_type else None,
            bpm=track.bpm,
            key=track.key,
            duration_seconds=track.duration,
            size_bytes=None,
            source="legacy",
        )
        for track in result.scalars().all()
    ]

@router.delete("/versions/{version_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_version(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Delete a version.
    """
    result = await db.execute(
        select(Version, Branch.project_id).join(Branch, Version.branch_id == Branch.id).join(Project, Branch.project_id == Project.id).where(
            Version.id == version_id,
            Project.owner_id == current_user.id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False
        )
    )
    row = result.first()
    if not row:
        raise HTTPException(status_code=404, detail="Version not found")
    version, project_id = row

    # If this is a CAS version, decrement ref_count on each referenced blob.
    # Blobs whose ref_count drops to 0 will be reclaimed by the GC sweep after
    # the grace window (see docs/content-addressed-storage.md).
    if version.manifest_json:
        try:
            referenced_shas = _extract_manifest_blob_shas(version.manifest_json)
        except (KeyError, TypeError):
            referenced_shas = set()
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
