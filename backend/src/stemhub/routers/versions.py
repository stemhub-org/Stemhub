from fastapi import APIRouter, Depends, HTTPException, Query, status
from datetime import datetime, timezone
from typing import List
from uuid import UUID

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.database import get_db
from stemhub.models import Collaborator, Version, Branch, Project, User
from stemhub.schemas import MixerDiffChange, MixerDiffResponse, MixerDiffSummary, VersionCreate, VersionResponse
from stemhub.auth import get_current_user
from stemhub.flp_mixer_snapshot import MixerSnapshotError, diff_mixer_project_snapshots, load_fl_studio_mixer_snapshot
from stemhub.storage import StorageService, get_storage_service

router = APIRouter(tags=["versions"])


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


def _is_fl_studio_version(version: Version) -> bool:
    if version.source_daw:
        return version.source_daw.strip().casefold() == "fl studio"
    if version.source_project_filename and version.source_project_filename.lower().endswith(".flp"):
        return True
    manifest_path = None
    if isinstance(version.snapshot_manifest, dict):
        manifest_path = version.snapshot_manifest.get("flp_relative_path")
    return isinstance(manifest_path, str) and manifest_path.lower().endswith(".flp")

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

    try:
        base_snapshot = load_fl_studio_mixer_snapshot(
            artifact_path=base_version.artifact_path,
            snapshot_manifest=base_version.snapshot_manifest,
            storage=storage,
        )
        target_snapshot = load_fl_studio_mixer_snapshot(
            artifact_path=target_version.artifact_path,
            snapshot_manifest=target_version.snapshot_manifest,
            storage=storage,
        )
    except MixerSnapshotError as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc

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

@router.get("/versions/{version_id}", response_model=VersionResponse)
async def get_version(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific version by ID.
    """
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
        select(Version).join(Branch).join(Project).where(
            Version.id == version_id,
            Project.owner_id == current_user.id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False
        )
    )
    version = result.scalars().first()
    if not version:
        raise HTTPException(status_code=404, detail="Version not found")

    version.is_deleted = True
    version.deleted_at = datetime.now(timezone.utc)
    await db.commit()
