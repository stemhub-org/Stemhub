"use client";

import Link from "next/link";
import { GitPullRequest, Loader2 } from "lucide-react";
import { Badge } from "@/components/ui/Badge";
import { ErrorState } from "@/components/ui/ErrorState";
import { usePullRequests } from "@/lib/queries";
import { formatDateTime, formatTimeAgo } from "@/lib/time";
import type { ActivityBranch } from "@/types/activity";
import type { PullRequest, PullRequestStatus } from "@/types/pullRequest";

/** A branch name, marked when the branch has since been deleted. */
export function branchLabel(branch: ActivityBranch): string {
    return branch.is_deleted ? `${branch.name} (deleted)` : branch.name;
}

export function PullRequestStatusBadge({ status }: { status: PullRequestStatus }) {
    const tone = status === "OPEN" ? "accent" : status === "MERGED" ? "inverse" : "neutral";
    return (
        <Badge tone={tone}>
            {status}
        </Badge>
    );
}

function PullRequestRow({ projectId, pullRequest }: { projectId: string; pullRequest: PullRequest }) {
    return (
        <li>
            <Link
                href={`/projects/pull-requests?id=${projectId}&pr=${pullRequest.id}`}
                className="flex items-center gap-4 rounded-lg px-4 py-3 hover:bg-foreground/5 transition-colors"
            >
                <div className="min-w-0 flex-1">
                    <p className="truncate font-medium">{pullRequest.title}</p>
                    <p className="text-sm text-foreground-muted">
                        {`${branchLabel(pullRequest.source_branch)} → ${branchLabel(pullRequest.target_branch)}`}
                    </p>
                    <p className="text-xs text-foreground-muted" title={formatDateTime(pullRequest.created_at)}>
                        {`${pullRequest.author?.username ?? "Unknown"} · ${formatTimeAgo(pullRequest.created_at)}`}
                    </p>
                </div>
                <PullRequestStatusBadge status={pullRequest.status} />
            </Link>
        </li>
    );
}

/** The project's pull requests, newest first. */
export function PullRequestList({ projectId }: { projectId: string }) {
    const { data, isPending, isError, error, refetch } = usePullRequests(projectId);

    if (isPending) {
        return <Loader2 className="mx-auto size-5 animate-spin text-accent" aria-label="Loading pull requests…" />;
    }
    if (isError) {
        return <ErrorState title="Couldn't load the pull requests" error={error} onRetry={() => void refetch()} />;
    }
    if (data.length === 0) {
        return (
            <div className="flex flex-1 flex-col items-center justify-center gap-2 px-6 py-10 text-center">
                <GitPullRequest className="size-6 text-foreground-muted" aria-hidden="true" />
                <p className="text-sm font-medium">No pull requests yet.</p>
                <p className="max-w-sm text-sm text-foreground-muted">
                    Open one to propose bringing a branch into another, then review and accept it here.
                </p>
            </div>
        );
    }

    const newestFirst = [...data].sort((a, b) => b.created_at.localeCompare(a.created_at));
    return (
        <ul className="divide-y divide-border-subtle">
            {newestFirst.map((pullRequest) => (
                <PullRequestRow key={pullRequest.id} projectId={projectId} pullRequest={pullRequest} />
            ))}
        </ul>
    );
}
