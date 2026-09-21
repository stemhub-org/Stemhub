from datetime import datetime, timezone
from pathlib import Path
from typing import List
from uuid import UUID

from fastapi import APIRouter, Depends, File, HTTPException, Query, UploadFile, status
from fastapi.responses import FileResponse
from sqlalchemy import or_
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from sqlalchemy.orm import selectinload

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Branch, Collaborator, Project, User, Version
from stemhub.routers._project_access import (
    get_project_with_owner_access,
    get_project_with_read_access,
    get_project_with_write_access,
)
from stemhub.schemas import (
    BranchResponse,
    OwnerSummary,
    ProjectCreate,
    ProjectDetail,
    ProjectResponse,
    ProjectSummaryResponse,
    ProjectUpdate,
    VersionWithAuthor,
)
from stemhub.storage import (
    StorageNotFoundError,
    StorageService,
    get_storage_service,
)

router = APIRouter(prefix="/projects", tags=["projects"])


def _resolve_media_type(file_path: Path) -> str:
    extension = file_path.suffix.lower()
    if extension == ".wav":
        return "audio/wav"
    if extension == ".mp3":
        return "audio/mpeg"
    if extension == ".ogg":
        return "audio/ogg"
    if extension == ".flac":
        return "audio/flac"
    return "application/octet-stream"

@router.post("/", response_model=ProjectResponse, status_code=status.HTTP_201_CREATED)
async def create_project(
    project_in: ProjectCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Create a new project.
    """
    db_project = Project(**project_in.model_dump(), owner_id=current_user.id)
    db.add(db_project)
    await db.flush()

    db.add(Branch(project_id=db_project.id, name="main"))

    await db.commit()
    await db.refresh(db_project)
    return db_project

@router.get("/", response_model=List[ProjectResponse])
async def list_projects(
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    List all projects the current user can access (owner or collaborator).
    """
    result = await db.execute(
        select(Project)
        .outerjoin(
            Collaborator,
            Collaborator.project_id == Project.id,
        )
        .where(
            Project.is_deleted == False,
            or_(
                Project.owner_id == current_user.id,
                Collaborator.user_id == current_user.id,
            ),
        )
        .distinct()
    )
    projects = result.scalars().all()
    return projects

RECENT_VERSIONS_LIMIT = 10

@router.get("/{project_id}/summary", response_model=ProjectSummaryResponse)
async def get_project(
    project_id: UUID,
    branch_id: UUID | None = Query(default=None),
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    """Get a specific project by ID, including branches and recent versions."""
    project = await get_project_with_read_access(
        project_id=project_id, current_user=current_user, db=db
    )
    # Owner is used for the response payload — load it explicitly since the
    # shared access helper doesn't eager-load relationships.
    await db.refresh(project, attribute_names=["owner"])

    # ── 2. Fetch branches ──
    branch_result = await db.execute(
        select(Branch).where(
            Branch.project_id == project_id,
            Branch.is_deleted == False,
        )
    )
    branches: List[Branch] = list(branch_result.scalars().all())
    branch_ids = [b.id for b in branches]
    selected_branch_id = branch_id

    if selected_branch_id and selected_branch_id not in branch_ids:
        raise HTTPException(status_code=404, detail="Branch not found in project")

    # ── 3. Fetch recent versions with authors ──
    recent_versions_data: List[VersionWithAuthor] = []
    if branch_ids:
        versions_query = (
            select(Version)
            .options(selectinload(Version.author), selectinload(Version.branch))
            .where(
                Version.is_deleted == False,
            )
            .order_by(Version.created_at.desc())
            .limit(RECENT_VERSIONS_LIMIT)
        )

        if selected_branch_id:
            versions_query = versions_query.where(Version.branch_id == selected_branch_id)
        else:
            versions_query = versions_query.where(Version.branch_id.in_(branch_ids))

        versions_result = await db.execute(versions_query)
        versions = versions_result.scalars().all()

        for v in versions:
            author_summary = None
            if v.author:
                author_summary = OwnerSummary(id=v.author.id, username=v.author.username)
            recent_versions_data.append(
                VersionWithAuthor(
                    id=v.id,
                    commit_message=v.commit_message,
                    created_at=v.created_at,
                    branch_name=v.branch.name if v.branch else "unknown",
                    author=author_summary,
                    has_artifact=v.artifact_path is not None,
                    source_daw=v.source_daw,
                    source_project_filename=v.source_project_filename,
                )
            )

    has_preview = storage.has_project_preview(project.id)

    # ── 4. Build response ──
    project_detail = ProjectDetail(
        id=project.id,
        name=project.name,
        description=project.description,
        category=project.category,
        is_public=project.is_public,
        created_at=project.created_at,
        owner=OwnerSummary(id=project.owner.id, username=project.owner.username),
    )

    return ProjectSummaryResponse(
        project=project_detail,
        branches=[BranchResponse.model_validate(b) for b in branches],
        recent_versions=recent_versions_data,
        latest_version_id=recent_versions_data[0].id if recent_versions_data else None,
        has_preview=has_preview,
    )


@router.post("/{project_id}/preview", status_code=status.HTTP_200_OK)
async def upload_project_preview(
    project_id: UUID,
    preview: UploadFile = File(...),
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    project = await get_project_with_write_access(
        project_id=project_id, current_user=current_user, db=db
    )

    if not preview.filename:
        raise HTTPException(status_code=400, detail="Uploaded preview must include a filename")

    extension = Path(preview.filename).suffix.lower()
    if extension not in {".wav", ".mp3", ".ogg", ".flac"}:
        raise HTTPException(status_code=400, detail="Unsupported preview audio format")

    storage.store_project_preview(
        project_id=project.id,
        filename=preview.filename,
        source=preview.file,
    )

    await preview.close()
    return {"project_id": str(project.id), "has_preview": True}


@router.get("/{project_id}/preview")
async def download_project_preview(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    project = await get_project_with_read_access(
        project_id=project_id, current_user=current_user, db=db
    )

    try:
        preview_path = storage.resolve_project_preview_path(project.id)
    except StorageNotFoundError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc

    return FileResponse(
        path=preview_path,
        media_type=_resolve_media_type(preview_path),
        filename=f"{project.name}-preview{preview_path.suffix or '.wav'}",
    )


@router.delete("/{project_id}/preview", status_code=status.HTTP_204_NO_CONTENT)
async def delete_project_preview(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    project = await get_project_with_write_access(
        project_id=project_id, current_user=current_user, db=db
    )
    storage.delete_project_preview(project.id)

@router.put("/{project_id}", response_model=ProjectResponse)
async def update_project(
    project_id: UUID,
    project_in: ProjectUpdate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Update a project by ID (owner only)."""
    project = await get_project_with_owner_access(
        project_id=project_id, current_user=current_user, db=db
    )
    for field, value in project_in.model_dump(exclude_unset=True).items():
        setattr(project, field, value)

    await db.commit()
    await db.refresh(project)
    return project


@router.delete("/{project_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_project(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Delete a project by ID (owner only)."""
    project = await get_project_with_owner_access(
        project_id=project_id, current_user=current_user, db=db
    )
    project.is_deleted = True
    project.deleted_at = datetime.now(timezone.utc)
    await db.commit()
