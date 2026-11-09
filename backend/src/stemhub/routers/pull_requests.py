"""Pull-request lifecycle endpoints (issue #250).

A pull request proposes bringing a source branch into a target branch of the
same project. This router covers the data model and the OPEN → CLOSED
transition; accepting (OPEN → MERGED, a promotion with no content merge) is
issue #253. Both MERGED and CLOSED are terminal — a closed PR is not
reopened, users open a new one (SPECIFICATION.md §19).

Access control: creating and closing require write access (owner or
Admin/Editor collaborator); listing and reading require read access. Both
helpers return 404 on miss, never 403, so a non-member cannot probe whether a
project or PR exists.
"""
import copy
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
    is_project_admin,
)
from stemhub.schemas import PullRequestCreate, PullRequestResponse

router = APIRouter(tags=["pull-requests"])


async def _branch_head(*, branch_id: UUID, db: AsyncSession) -> Version | None:
    """Latest live version on a branch, or None if it has none.

    Branch carries no head pointer; "head" is defined by created_at, the same
    rule the version-history endpoint uses.
    """
    result = await db.execute(
        select(Version)
        .where(Version.branch_id == branch_id, Version.is_deleted == False)
        .order_by(Version.created_at.desc())
        .limit(1)
    )
    return result.scalars().first()


async def _branch_head_id(*, branch_id: UUID, db: AsyncSession) -> UUID | None:
    """Id of the latest live version on a branch, or None if it has none."""
    head = await _branch_head(branch_id=branch_id, db=db)
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
    """Close a pull request without accepting it (OPEN → CLOSED).

    Not to be confused with accepting it (issue #253).
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


@router.post("/pull-requests/{pull_request_id}/accept", response_model=PullRequestResponse)
async def accept_pull_request(
    pull_request_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Accept a pull request (OPEN → MERGED) by promoting its source branch.

    Promotion, not merge (SPECIFICATION.md §8, §19): a new version on the
    target branch reuses the source head's manifest, so the target's latest
    state becomes the source's. Files are content-addressed, so nothing is
    copied in storage.
    """
    pull_request = await _get_pull_request_or_404(pull_request_id=pull_request_id, db=db)
    project = await get_project_with_write_access(
        project_id=pull_request.project_id, current_user=current_user, db=db
    )
    # Accepting is the owner's review (§19): Editors may open and close pull
    # requests, but not accept them.
    if not await is_project_admin(project=project, user=current_user, db=db):
        raise HTTPException(
            status_code=403,
            detail="Only the project owner or an Admin can accept a pull request",
        )

    if pull_request.status != "OPEN":
        raise HTTPException(status_code=409, detail=f"Pull request is already {pull_request.status}")

    branch_ids = [pull_request.source_branch_id, pull_request.target_branch_id]
    live_branches = await db.execute(
        select(Branch).where(Branch.id.in_(branch_ids), Branch.is_deleted == False)
    )
    if len(live_branches.scalars().all()) != len(branch_ids):
        raise HTTPException(status_code=409, detail="The source or target branch was deleted")

    source_head = await _branch_head(branch_id=pull_request.source_branch_id, db=db)
    if source_head is None:
        raise HTTPException(status_code=409, detail="The source branch has no version to promote")
    target_head_id = await _branch_head_id(branch_id=pull_request.target_branch_id, db=db)
    # The reviewer saw the target as it was when the PR was opened. Promoting
    # over newer versions would bury them without anyone having reviewed them.
    if target_head_id != pull_request.target_head_version_id:
        raise HTTPException(
            status_code=409,
            detail=(
                "The target branch has new versions since this pull request was opened: "
                "open a new pull request to review them"
            ),
        )

    db.add(
        Version(
            branch_id=pull_request.target_branch_id,
            created_by=current_user.id,
            parent_version_id=target_head_id,
            message=f"Accept PR: {pull_request.title}",
            source_daw=source_head.source_daw,
            source_project_filename=source_head.source_project_filename,
            # A copy, so the two versions never share a mutable JSON document.
            manifest_json=copy.deepcopy(source_head.manifest_json),
            manifest_version=source_head.manifest_version,
        )
    )
    pull_request.status = "MERGED"
    pull_request.closed_at = datetime.now(timezone.utc)
    pull_request.closed_by = current_user.id
    await db.commit()
    await db.refresh(pull_request)
    return pull_request

