// ── Project activity feed (GET /projects/{id}/activity, backend `ActivityFeedResponse`) ──

export type ActivityType =
    | "version_saved"
    | "version_deleted"
    | "branch_created"
    | "branch_deleted"
    | "pull_request_opened"
    | "pull_request_merged"
    | "pull_request_closed";

export interface ActivityActor {
    id: string;
    username: string;
}

/** A branch as it is now: `is_deleted` is its current state, not the one at the event. */
export interface ActivityBranch {
    id: string;
    name: string;
    is_deleted: boolean;
}

export interface ActivityVersion {
    id: string;
    message: string | null;
    source_daw: string | null;
    source_project_filename: string | null;
}

/** A pull request as it is now: `status` is its current one (OPEN, MERGED or CLOSED). */
export interface ActivityPullRequest {
    id: string;
    title: string;
    status: string;
    source_branch: ActivityBranch;
    target_branch: ActivityBranch;
}

export interface ActivityItem {
    /** "<type>:<entity id>", unique and stable. */
    id: string;
    type: ActivityType;
    occurred_at: string;
    /** Null when the author isn't recorded (deletions, branch events) or unknown. */
    actor: ActivityActor | null;
    /** Set for version and branch events. */
    branch: ActivityBranch | null;
    /** Set for version events. */
    version: ActivityVersion | null;
    /** Set for pull request events. */
    pull_request: ActivityPullRequest | null;
}

export interface ActivityFeedPage {
    items: ActivityItem[];
    /** Pass it back as `cursor` for the next page; null on the last page. */
    next_cursor: string | null;
}
