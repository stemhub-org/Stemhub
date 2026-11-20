"""Pull-request lifecycle endpoints (issue #250).

A pull request proposes bringing a source branch into a target branch of the
same project. This router covers the data model, closing (OPEN → CLOSED) and
accepting (OPEN → MERGED, a promotion with no content merge, issue #253).
Both MERGED and CLOSED are terminal — a closed PR is not
reopened, users open a new one (SPECIFICATION.md §19).

Access control: creating and closing require write access (owner or
Admin/Editor collaborator); accepting requires the owner or an Admin
collaborator; listing and reading require read access. Both
helpers return 404 on miss, never 403, so a non-member cannot probe whether a
project or PR exists.
"""
import copy
from datetime import datetime, timezone
from typing import List
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import update
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select

from stemhub.auth import get_current_user
from stemhub.database import get_db
from stemhub.fl_mixer import diff_fl_mixers
from stemhub.mixer_compare import (
    is_fl_studio_version,
    load_mixer_for_compare,
    mixer_diff_changes,
    mixer_diff_summary,
)
from stemhub.manifests import blob_shas
from stemhub.models import Blob, Branch, PullRequest, User, Version
from stemhub.routers._project_access import (
    get_project_with_read_access,
    get_project_with_write_access,
    is_project_admin,
)
from stemhub.schemas import (
    ActivityActor,
    ActivityBranch,
    PullRequestAccept,
    PullRequestCreate,
    PullRequestDiffResponse,
    PullRequestResponse,
)
from stemhub.storage import StorageService, get_storage_service

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


async def _get_pull_request_or_404(
    *, pull_request_id: UUID, db: AsyncSession, for_update: bool = False
) -> PullRequest:
    """Load a live pull request. `for_update` locks its row until the commit,
    so two concurrent state changes run one after the other."""
    query = select(PullRequest).where(
        PullRequest.id == pull_request_id,
        PullRequest.is_deleted == False,
    )
    if for_update:
        query = query.with_for_update()
    result = await db.execute(query)
    pull_request = result.scalar_one_or_none()
    if pull_request is None:
        raise HTTPException(status_code=404, detail="Pull request not found")
    return pull_request


_REFERENCE_FIELDS = {"author", "source_branch", "target_branch"}


def _branch_ref(branch: Branch) -> ActivityBranch:
    return ActivityBranch(id=branch.id, name=branch.name, is_deleted=branch.is_deleted)


async def _to_responses(db: AsyncSession, pull_requests: list[PullRequest]) -> list[PullRequestResponse]:
    """Responses with each PR's author and branches. Built explicitly rather
    than from the ORM relationships, whose lazy loads fail on an async session.
    Two queries whatever the number of pull requests; deleted branches are
    included, since a PR keeps pointing at them."""
    if not pull_requests:
        return []

    branch_ids = {pr.source_branch_id for pr in pull_requests} | {pr.target_branch_id for pr in pull_requests}
    branch_result = await db.execute(select(Branch).where(Branch.id.in_(branch_ids)))
    branches = {branch.id: branch for branch in branch_result.scalars().all()}

    author_ids = {pr.created_by for pr in pull_requests if pr.created_by is not None}
    authors: dict[UUID, User] = {}
    if author_ids:
        author_result = await db.execute(select(User).where(User.id.in_(author_ids)))
        authors = {user.id: user for user in author_result.scalars().all()}

    responses = []
    for pr in pull_requests:
        fields = {name: getattr(pr, name) for name in PullRequestResponse.model_fields if name not in _REFERENCE_FIELDS}
        author = authors.get(pr.created_by) if pr.created_by is not None else None
        responses.append(
            PullRequestResponse(
                **fields,
                author=ActivityActor(id=author.id, username=author.username) if author else None,
                source_branch=_branch_ref(branches[pr.source_branch_id]),
                target_branch=_branch_ref(branches[pr.target_branch_id]),
            )
        )
    return responses


async def _to_response(db: AsyncSession, pull_request: PullRequest) -> PullRequestResponse:
    return (await _to_responses(db, [pull_request]))[0]


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
    return await _to_responses(db, list(result.scalars().all()))


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
    return await _to_response(db, pull_request)


@router.get("/pull-requests/{pull_request_id}", response_model=PullRequestResponse)
async def get_pull_request(
    pull_request_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Get a pull request by id."""
    pull_request = await _get_pull_request_or_404(pull_request_id=pull_request_id, db=db)
    await get_project_with_read_access(project_id=pull_request.project_id, current_user=current_user, db=db)
    return await _to_response(db, pull_request)


@router.get("/pull-requests/{pull_request_id}/diff", response_model=PullRequestDiffResponse)
async def get_pull_request_diff(
    pull_request_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
    storage: StorageService = Depends(get_storage_service),
):
    """Mixer changes accepting this pull request would apply.

    Compares the target branch's head with the source branch's *current* head:
    the same two versions accept_pull_request works on, so the reviewer never
    accepts something the diff did not show. Computed on each call rather than
    stored, because the source branch can receive versions while the pull
    request is open.
    """
    pull_request = await _get_pull_request_or_404(pull_request_id=pull_request_id, db=db)
    await get_project_with_read_access(project_id=pull_request.project_id, current_user=current_user, db=db)

    # A closed pull request applies nothing; an accepted one is already in the
    # target branch's history. Only an open one has pending changes.
    if pull_request.status != "OPEN":
        raise HTTPException(status_code=409, detail="Only an open pull request has a diff")

    source_head = await _branch_head(branch_id=pull_request.source_branch_id, db=db)
    if source_head is None:
        return PullRequestDiffResponse(status="no_source_version")
    target_head = await _branch_head(branch_id=pull_request.target_branch_id, db=db)
    target_head_id = target_head.id if target_head else None
    # Same check as accept: it would refuse, so there is no pending change to show.
    if target_head_id != pull_request.target_head_version_id:
        return PullRequestDiffResponse(status="target_moved", head_version_id=source_head.id)
    if target_head is None:
        return PullRequestDiffResponse(status="no_target_version", head_version_id=source_head.id)

    def unsupported(reason: str) -> PullRequestDiffResponse:
        return PullRequestDiffResponse(
            status="unsupported",
            status_message=reason,
            base_version_id=target_head.id,
            head_version_id=source_head.id,
        )

    if not is_fl_studio_version(target_head) or not is_fl_studio_version(source_head):
        return unsupported("Only FL Studio projects can be compared")
    try:
        base_mixer = await load_mixer_for_compare(
            version=target_head, project_id=pull_request.project_id, db=db, storage=storage
        )
        head_mixer = await load_mixer_for_compare(
            version=source_head, project_id=pull_request.project_id, db=db, storage=storage
        )
    except HTTPException as exc:
        # A missing or unreadable project file must not break the page.
        return unsupported(str(exc.detail))
    # A project file the parser cannot read does not raise: it loads as a mixer
    # with no insert. Diffing it would report every insert of the other side as
    # added or removed, which is false.
    for side, mixer in (("target", base_mixer), ("source", head_mixer)):
        if not mixer.mixer_supported:
            return unsupported(f"The mixer of the {side} branch's FL Studio project could not be read")

    diff_result = diff_fl_mixers(base_mixer, head_mixer)
    return PullRequestDiffResponse(
        status="compared",
        base_version_id=target_head.id,
        head_version_id=source_head.id,
        summary=mixer_diff_summary(diff_result),
        changes=mixer_diff_changes(diff_result),
    )


@router.post("/pull-requests/{pull_request_id}/close", response_model=PullRequestResponse)
async def close_pull_request(
    pull_request_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Close a pull request without accepting it (OPEN → CLOSED)."""
    pull_request = await _get_pull_request_or_404(pull_request_id=pull_request_id, db=db, for_update=True)
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
    return await _to_response(db, pull_request)


@router.post("/pull-requests/{pull_request_id}/accept", response_model=PullRequestResponse)
async def accept_pull_request(
    pull_request_id: UUID,
    payload: PullRequestAccept | None = None,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Accept a pull request (OPEN → MERGED) by promoting its source branch.

    Promotion, not merge (SPECIFICATION.md §8, §19): a new version on the
    target branch reuses the source head's manifest, so the target's latest
    state becomes the source's. Files are content-addressed, so nothing is
    copied in storage.
    """
    pull_request = await _get_pull_request_or_404(pull_request_id=pull_request_id, db=db, for_update=True)
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
    # The diff is computed when the page loads, the promotion when the reviewer
    # clicks. If the source got a version in between, the reviewer never saw it.
    expected_source_head_id = payload.expected_source_head_version_id if payload else None
    if expected_source_head_id is not None and source_head.id != expected_source_head_id:
        raise HTTPException(
            status_code=409,
            detail="The source branch has new versions since you reviewed the changes: review them before accepting",
        )
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

    # Promotion creates another live manifest. Count each distinct blob once,
    # through the same reader deletion uses, including legacy v1 manifests.
    # Increment in SQL so simultaneous promotions cannot lose an increment.
    referenced_shas = blob_shas(source_head.manifest_json)
    if referenced_shas:
        await db.execute(
            update(Blob)
            .where(Blob.project_id == project.id, Blob.sha256.in_(referenced_shas))
            .values(ref_count=Blob.ref_count + 1)
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
    return await _to_response(db, pull_request)

