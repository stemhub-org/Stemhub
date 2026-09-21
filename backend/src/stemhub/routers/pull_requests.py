"""Pull-request lifecycle endpoints (issue #250).

A pull request proposes merging a source branch into a target branch of the
same project. This router covers the data model and the OPEN → CLOSED
transition; the merge engine (OPEN → MERGED) is issue #253. Both MERGED and
CLOSED are terminal — a closed PR is not reopened, users open a new one
(SPECIFICATION.md §7, §19).

Access control: creating and closing require write access (owner or
Admin/Editor collaborator); listing and reading require read access. Both
helpers return 404 on miss, never 403, so a non-member cannot probe whether a
project or PR exists.
"""
from datetime import datetime, timezone
from typing import List
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.models import Branch, PullRequest, User, Version
from stemhub.routers._project_access import (
    get_project_with_read_access,
    get_project_with_write_access,
)
from stemhub.schemas import PullRequestCreate, PullRequestResponse

router = APIRouter(tags=["pull-requests"])


async def _branch_head_id(*, branch_id: UUID, db: AsyncSession) -> UUID | None:
    """Id of the latest live version on a branch, or None if it has none.

    Branch carries no head pointer; "head" is defined by created_at, the same
    rule the version-history endpoint uses.
    """
    result = await db.execute(
        select(Version)
        .where(Version.branch_id == branch_id, Version.is_deleted == False)
        .order_by(Version.created_at.desc())
        .limit(1)
    )
    head = result.scalars().first()
    return head.id if head else None


async def _ensure_no_other_open_pull_request(
    *, source_branch_id: UUID, target_branch_id: UUID, db: AsyncSession
) -> None:
    """Application-level twin of the partial unique index
    uq_pull_request_open_pair, so the client gets a 409 rather than a 500
    from the IntegrityError. The index remains the actual guarantee under
    concurrent requests.
    """
    result = await db.execute(
        select(PullRequest).where(
            PullRequest.source_branch_id == source_branch_id,
            PullRequest.target_branch_id == target_branch_id,
            PullRequest.status == "OPEN",
            PullRequest.is_deleted == False,
        )
    )
    if result.scalars().first() is not None:
        raise HTTPException(
            status_code=409,
            detail="An open pull request already exists for these branches",
        )


async def _get_pull_request_or_404(*, pull_request_id: UUID, db: AsyncSession) -> PullRequest:
    result = await db.execute(
        select(PullRequest).where(
            PullRequest.id == pull_request_id,
            PullRequest.is_deleted == False,
        )
    )
    pull_request = result.scalar_one_or_none()
    if pull_request is None:
        raise HTTPException(status_code=404, detail="Pull request not found")
    return pull_request


@router.get("/projects/{project_id}/pull-requests", response_model=List[PullRequestResponse])
async def list_pull_requests(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """List the pull requests of a project (all statuses)."""
    project = await get_project_with_read_access(project_id=project_id, current_user=current_user, db=db)

    result = await db.execute(
        select(PullRequest).where(
            PullRequest.project_id == project.id,
            PullRequest.is_deleted == False,
        )
    )
    return result.scalars().all()


@router.post(
    "/projects/{project_id}/pull-requests",
    response_model=PullRequestResponse,
    status_code=status.HTTP_201_CREATED,
)
async def create_pull_request(
    project_id: UUID,
    pr_in: PullRequestCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Open a pull request from ``source_branch_id`` into ``target_branch_id``."""
    project = await get_project_with_write_access(project_id=project_id, current_user=current_user, db=db)

    if pr_in.source_branch_id == pr_in.target_branch_id:
        raise HTTPException(status_code=400, detail="Source and target branches must differ")

    # Both branches must be live and belong to this project. A branch from
    # another project answers 404 (not 400) so the response does not reveal
    # that the foreign branch id exists.
    result = await db.execute(
        select(Branch).where(
            Branch.id.in_([pr_in.source_branch_id, pr_in.target_branch_id]),
            Branch.project_id == project.id,
            Branch.is_deleted == False,
        )
    )
    found_ids = {branch.id for branch in result.scalars().all()}
    if found_ids != {pr_in.source_branch_id, pr_in.target_branch_id}:
        raise HTTPException(status_code=404, detail="Branch not found")

    await _ensure_no_other_open_pull_request(
        source_branch_id=pr_in.source_branch_id, target_branch_id=pr_in.target_branch_id, db=db
    )

    pull_request = PullRequest(
        project_id=project.id,
        source_branch_id=pr_in.source_branch_id,
        target_branch_id=pr_in.target_branch_id,
        title=pr_in.title,
        description=pr_in.description,
        status="OPEN",
        source_head_version_id=await _branch_head_id(branch_id=pr_in.source_branch_id, db=db),
        target_head_version_id=await _branch_head_id(branch_id=pr_in.target_branch_id, db=db),
        created_by=current_user.id,
    )
    db.add(pull_request)
    await db.commit()
    await db.refresh(pull_request)
    return pull_request


@router.get("/pull-requests/{pull_request_id}", response_model=PullRequestResponse)
async def get_pull_request(
    pull_request_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Get a pull request by id."""
    pull_request = await _get_pull_request_or_404(pull_request_id=pull_request_id, db=db)
    await get_project_with_read_access(project_id=pull_request.project_id, current_user=current_user, db=db)
    return pull_request


@router.post("/pull-requests/{pull_request_id}/close", response_model=PullRequestResponse)
async def close_pull_request(
    pull_request_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Close a pull request without merging (OPEN → CLOSED).

    Not to be confused with the future ``/merge`` endpoint (issue #253).
    """
    pull_request = await _get_pull_request_or_404(pull_request_id=pull_request_id, db=db)
    await get_project_with_write_access(project_id=pull_request.project_id, current_user=current_user, db=db)

    # 409 rather than 400: the request is well-formed, it is the resource's
    # current state (already MERGED or CLOSED) that forbids the transition.
    if pull_request.status != "OPEN":
        raise HTTPException(
            status_code=409,
            detail=f"Pull request is already {pull_request.status}",
        )

    pull_request.status = "CLOSED"
    pull_request.closed_at = datetime.now(timezone.utc)
    pull_request.closed_by = current_user.id
    await db.commit()
    await db.refresh(pull_request)
    return pull_request
