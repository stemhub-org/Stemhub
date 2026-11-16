// ── Pull requests (backend `PullRequestResponse`, routers/pull_requests.py) ──

import type { ActivityActor, ActivityBranch } from "./activity";

/** OPEN, then MERGED (accepted) or CLOSED; both are final (SPECIFICATION.md §19). */
export type PullRequestStatus = "OPEN" | "MERGED" | "CLOSED";

export interface PullRequest {
    id: string;
    project_id: string;
    source_branch_id: string;
    target_branch_id: string;
    title: string;
    description: string | null;
    status: PullRequestStatus;
    source_head_version_id: string | null;
    target_head_version_id: string | null;
    created_by: string | null;
    created_at: string;
    closed_by: string | null;
    closed_at: string | null;
    is_deleted: boolean;
    deleted_at: string | null;
    /** Who opened it; null when unknown. */
    author: ActivityActor | null;
    /** Branches as they are now: `is_deleted` is their current state. */
    source_branch: ActivityBranch;
    target_branch: ActivityBranch;
}

export interface NewPullRequest {
    source_branch_id: string;
    target_branch_id: string;
    title: string;
    description?: string;
}
