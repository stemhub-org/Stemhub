// ── Pull requests (backend `PullRequestResponse`, routers/pull_requests.py) ──

import type { ActivityActor, ActivityBranch } from "./activity";
import type { MixerDiffChange, MixerDiffSummary } from "./project";

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

export interface AcceptPullRequest {
    pullRequestId: string;
    /**
     * The source version whose changes the reviewer saw (the diff's
     * `head_version_id`). The backend refuses to accept if the branch has a
     * newer one. Null when no diff was loaded: accepted without that check.
     */
    reviewedSourceHeadId: string | null;
}

// ── Mixer diff (backend `PullRequestDiffResponse`) ──

/** "compared", or the reason there is nothing to compare. */
export type PullRequestDiffStatus = "compared" | "unsupported" | "no_source_version" | "no_target_version" | "target_moved";

/** What accepting an open pull request would change in the mixer: target head (base) vs source head. */
export interface PullRequestMixerDiff {
    status: PullRequestDiffStatus;
    /** Why the comparison is unsupported; null otherwise. */
    status_message: string | null;
    base_version_id: string | null;
    head_version_id: string | null;
    /** Set when status is "compared". */
    summary: MixerDiffSummary | null;
    changes: MixerDiffChange[];
}
