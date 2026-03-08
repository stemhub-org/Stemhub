from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from sqlalchemy.orm import selectinload
from typing import List
from uuid import UUID

from stemhub.database import get_db
from stemhub.models import Branch, Project, Track, User, Version
from stemhub.schemas import (
    BranchResponse,
    OwnerSummary,
    ProjectDetail,
    ProjectSummaryResponse,
    TrackResponse,
    VersionWithAuthor,
)
from stemhub.auth import get_current_user

router = APIRouter(tags=["project-summary"])

RECENT_VERSIONS_LIMIT = 10


@router.get("/projects/{project_id}/summary", response_model=ProjectSummaryResponse)
async def get_project_summary(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Get an enriched project summary including owner, branches,
    recent versions with authors, and tracks from the latest version.
    """

    # ── 1. Fetch project with owner ──
    project_result = await db.execute(
        select(Project)
        .options(selectinload(Project.owner))
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
        from stemhub.models import Collaborator

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
