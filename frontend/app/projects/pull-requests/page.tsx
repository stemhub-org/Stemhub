"use client";

import Link from "next/link";
import { Suspense } from "react";
import { useSearchParams } from "next/navigation";
import { ArrowLeft, Loader2 } from "lucide-react";
import { ProjectHeader } from "../components/ProjectHeader";
import { PullRequestDiff } from "../components/PullRequestDiff";
import { PullRequestStatusBadge, branchLabel } from "../components/PullRequestList";
import { Button } from "@/components/ui/Button";
import { Card } from "@/components/ui/Card";
import { ErrorState } from "@/components/ui/ErrorState";
import { errorMessage } from "@/lib/api";
import { projectRole, pullRequestActions } from "@/lib/pullRequestPermissions";
import {
    useAcceptPullRequest,
    useClosePullRequest,
    useCollaborators,
    useCurrentUser,
    usePullRequestDiff,
    usePullRequest,
    useProjectSummary,
} from "@/lib/queries";
import { formatDateTime, formatTimeAgo } from "@/lib/time";
import type { PullRequest } from "@/types/pullRequest";

function ClosedLine({ pullRequest }: { pullRequest: PullRequest }) {
    if (!pullRequest.closed_at) return null;
    const verb = pullRequest.status === "MERGED" ? "Accepted" : "Closed";
    return (
        <p className="text-sm text-foreground-muted" title={formatDateTime(pullRequest.closed_at)}>
            {`${verb} ${formatTimeAgo(pullRequest.closed_at)}`}
        </p>
    );
}

function PullRequestActions({ projectId, pullRequest }: { projectId: string; pullRequest: PullRequest }) {
    const { data: currentUser } = useCurrentUser();
    const { data: collaborators } = useCollaborators(projectId);
    const { data: summary } = useProjectSummary(projectId, null);
    const closePullRequest = useClosePullRequest(projectId);
    const acceptPullRequest = useAcceptPullRequest(projectId);
    // Same cached request as the "Changes" section: no extra call.
    const { data: diff } = usePullRequestDiff(pullRequest.id, pullRequest.status === "OPEN");

    const ownerId = summary?.summary.project.owner.id;
    const role = ownerId ? projectRole(ownerId, currentUser?.id, collaborators) : null;
    const { canClose, canAccept } = pullRequestActions(role, pullRequest.status);
    const failed = acceptPullRequest.error ?? closePullRequest.error;
    const busy = acceptPullRequest.isPending || closePullRequest.isPending;

    if (!canClose && !canAccept) return null;
    return (
        <div className="flex flex-col gap-3">
            <div className="flex flex-wrap gap-3">
                {canAccept && (
                    <Button
                        disabled={busy}
                        loading={acceptPullRequest.isPending}
                        // Accept exactly the source version whose changes are on screen.
                        onClick={() =>
                            acceptPullRequest.mutate({
                                pullRequestId: pullRequest.id,
                                reviewedSourceHeadId: diff?.head_version_id ?? null,
                            })
                        }
                    >
                        Accept
                    </Button>
                )}
                {canClose && (
                    <Button
                        variant="outline"
                        disabled={busy}
                        loading={closePullRequest.isPending}
                        onClick={() => closePullRequest.mutate(pullRequest.id)}
                    >
                        Close
                    </Button>
                )}
            </div>
            {failed && (
                <p role="alert" className="text-sm text-red-500">
                    {errorMessage(failed, "Couldn't update the pull request")}
                </p>
            )}
        </div>
    );
}

/**
 * What the pull request changes. Only an open one has pending changes to
 * compare: a closed one applied nothing, and an accepted one is already a
 * version in the target branch's history.
 */
function PullRequestChanges({ projectId, pullRequest }: { projectId: string; pullRequest: PullRequest }) {
    const targetName = branchLabel(pullRequest.target_branch);
    if (pullRequest.status === "OPEN") {
        return (
            <PullRequestDiff
                pullRequestId={pullRequest.id}
                targetName={targetName}
                sourceName={branchLabel(pullRequest.source_branch)}
            />
        );
    }
    if (pullRequest.status === "CLOSED") {
        return <p className="text-sm text-foreground-muted">This pull request was closed: nothing was applied.</p>;
    }
    return (
        <p className="text-sm text-foreground-muted">
            {`The accepted changes are in the history of ${targetName}. `}
            <Link
                href={`/projects/history?id=${projectId}&branch_id=${pullRequest.target_branch_id}`}
                className="text-accent hover:underline"
            >
                View history
            </Link>
        </p>
    );
}

function PullRequestContent({ projectId, pullRequestId }: { projectId: string; pullRequestId: string }) {
    const { data: pullRequest, isPending, isError, error, refetch } = usePullRequest(pullRequestId);

    if (isPending) {
        return <Loader2 className="mx-auto size-6 animate-spin text-accent" aria-label="Loading pull request…" />;
    }
    if (isError) {
        return <ErrorState title="Couldn't load this pull request" error={error} onRetry={() => void refetch()} />;
    }

    return (
        <>
            <Card className="flex flex-col gap-4">
                <div className="flex flex-wrap items-start justify-between gap-3">
                    <h1 className="text-lg font-medium" style={{ fontFamily: "var(--font-syne)" }}>
                        {pullRequest.title}
                    </h1>
                    <PullRequestStatusBadge status={pullRequest.status} />
                </div>
                <p className="text-sm">{`${branchLabel(pullRequest.source_branch)} → ${branchLabel(pullRequest.target_branch)}`}</p>
                <p className="text-sm text-foreground-muted" title={formatDateTime(pullRequest.created_at)}>
                    {`${pullRequest.author?.username ?? "Unknown"} opened this ${formatTimeAgo(pullRequest.created_at)}`}
                </p>
                <ClosedLine pullRequest={pullRequest} />
                {pullRequest.description && <p className="whitespace-pre-wrap text-sm">{pullRequest.description}</p>}
                <PullRequestActions projectId={projectId} pullRequest={pullRequest} />
            </Card>
            <PullRequestChanges projectId={projectId} pullRequest={pullRequest} />
        </>
    );
}

function PullRequestPageContent() {
    const params = useSearchParams();
    const projectId = params.get("id") ?? "";
    const pullRequestId = params.get("pr");
    const { data: currentUser } = useCurrentUser();

    return (
        <div className="min-h-screen bg-background text-foreground">
            <ProjectHeader userAvatarUrl={currentUser?.avatar_url ?? null} username={currentUser?.username || "Producer"} />
            <div className="p-6 space-y-6">
                <Link
                    href={`/projects?id=${projectId}`}
                    className="inline-flex items-center gap-2 text-sm text-foreground/70 transition-colors hover:text-foreground"
                >
                    <ArrowLeft className="size-4" aria-hidden />
                    Back to project
                </Link>
                {pullRequestId ? (
                    <PullRequestContent projectId={projectId} pullRequestId={pullRequestId} />
                ) : (
                    <p className="text-sm text-foreground-muted">Pull request not found.</p>
                )}
            </div>
        </div>
    );
}

export default function PullRequestPage() {
    return (
        <Suspense
            fallback={
                <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                    <Loader2 className="size-6 animate-spin text-accent" />
                </div>
            }
        >
            <PullRequestPageContent />
        </Suspense>
    );
}
