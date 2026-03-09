from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from datetime import datetime, timezone
from sqlalchemy.future import select
from typing import List
from uuid import UUID

from sqlalchemy.orm import joinedload, selectinload
from stemhub.database import get_db
from stemhub.models import Branch, Project, User, Track, Version, Collaborator
from stemhub.schemas import (
    ProjectCreate, 
    ProjectUpdate, 
    ProjectResponse, 
    ProjectSummaryResponse,
    ProjectDetail,
    OwnerSummary,
    BranchResponse,
    VersionWithAuthor,
    TrackResponse
)
from stemhub.auth import get_current_user

router = APIRouter(prefix="/projects", tags=["projects"])

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
    List all projects owned by the current user.
    """
    result = await db.execute(select(Project).where(Project.owner_id == current_user.id, Project.is_deleted == False))
    projects = result.scalars().all()
    return projects

RECENT_VERSIONS_LIMIT = 10

@router.get("/{project_id}", response_model=ProjectSummaryResponse)
async def get_project(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific project by ID, including its summary data (branches, versions, tracks).
    """
    # ── 1. Fetch project with owner ──
    project_result = await db.execute(
        select(Project)
        .options(joinedload(Project.owner))
        .where(
            Project.id == project_id,
            Project.is_deleted == False,
        )
    )
    project = project_result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")

    # Access check: owner or collaborator
    if project.owner_id != current_user.id:
        collab_result = await db.execute(
            select(Collaborator).where(
                Collaborator.project_id == project_id,
                Collaborator.user_id == current_user.id,
            )
        )
        if not collab_result.scalars().first():
            raise HTTPException(status_code=403, detail="Access denied")

    # ── 2. Fetch branches ──
    branch_result = await db.execute(
        select(Branch).where(
            Branch.project_id == project_id,
            Branch.is_deleted == False,
        )
    )
    branches: List[Branch] = list(branch_result.scalars().all())
    branch_ids = [b.id for b in branches]

    # ── 3. Fetch recent versions with authors ──
    recent_versions_data: List[VersionWithAuthor] = []
    if branch_ids:
        versions_result = await db.execute(
            select(Version)
            .options(selectinload(Version.author), selectinload(Version.branch))
            .where(
                Version.branch_id.in_(branch_ids),
                Version.is_deleted == False,
            )
            .order_by(Version.created_at.desc())
            .limit(RECENT_VERSIONS_LIMIT)
        )
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
                )
            )

    # ── 4. Fetch tracks from the latest version ──
    tracks_data: List[Track] = []
    if recent_versions_data:
        latest_version_id = recent_versions_data[0].id
        tracks_result = await db.execute(
            select(Track)
            .where(Track.version_id == latest_version_id)
            .order_by(Track.created_at.desc())
        )
        tracks_data = list(tracks_result.scalars().all())

    # ── 5. Build response ──
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
        tracks=[TrackResponse.model_validate(t) for t in tracks_data],
    )

@router.put("/{project_id}", response_model=ProjectResponse)
async def update_project(
    project_id: UUID,
    project_in: ProjectUpdate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Update a project by ID.
    """
    result = await db.execute(select(Project).where(Project.id == project_id, Project.owner_id == current_user.id, Project.is_deleted == False))
    project = result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")
    
    update_data = project_in.model_dump(exclude_unset=True)
    for field, value in update_data.items():
        setattr(project, field, value)
        
    await db.commit()
    await db.refresh(project)
    return project

@router.delete("/{project_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_project(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Delete a project by ID.
    """
    result = await db.execute(select(Project).where(Project.id == project_id, Project.owner_id == current_user.id, Project.is_deleted == False))
    project = result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")
        
    project.is_deleted = True
    project.deleted_at = datetime.now(timezone.utc)
    await db.commit()
