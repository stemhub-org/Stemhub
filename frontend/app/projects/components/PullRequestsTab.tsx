"use client";

import { useRouter } from "next/navigation";
import { Card } from "@/components/ui/Card";
import { projectRole } from "@/lib/pullRequestPermissions";
import { useCollaborators, usePullRequests } from "@/lib/queries";
import type { Branch } from "@/types/project";
import type { PullRequest } from "@/types/pullRequest";
import { NewPullRequestForm } from "./NewPullRequestForm";
import { PullRequestList } from "./PullRequestList";

interface PullRequestsTabProps {
    projectId: string;
    ownerId: string;
    currentUserId: string | null;
    /** The project's live branches. */
    branches: Branch[];
}

/** "2 open · 1 accepted · 1 closed". */
function statusCounts(pullRequests: PullRequest[]): string {
    const count = (status: PullRequest["status"]) => pullRequests.filter((pr) => pr.status === status).length;
    return `${count("OPEN")} open · ${count("MERGED")} accepted · ${count("CLOSED")} closed`;
}

/**
 * The project's pull requests, with the creation form in a side column for
 * owners, Admins and Editors (the same main + side layout as the Project tab).
 */
export function PullRequestsTab({ projectId, ownerId, currentUserId, branches }: PullRequestsTabProps) {
    const router = useRouter();
    const { data: collaborators } = useCollaborators(projectId);
    const { data: pullRequests } = usePullRequests(projectId);
    const role = projectRole(ownerId, currentUserId ?? undefined, collaborators);
    const canOpen = role === "owner" || role === "Admin" || role === "Editor";

    return (
        // Columns stretch to the same height, so a short list doesn't leave a gap under it.
        <div className="flex flex-col gap-6 lg:flex-row">
            <Card padding="none" className="flex min-w-0 flex-1 flex-col">
                <div className="flex flex-wrap items-baseline justify-between gap-2 border-b border-border-subtle px-6 py-4">
                    <h2 className="text-base font-medium" style={{ fontFamily: "var(--font-syne)" }}>
                        Pull requests
                    </h2>
                    {pullRequests && pullRequests.length > 0 && (
                        <span className="text-sm text-foreground-muted">{statusCounts(pullRequests)}</span>
                    )}
                </div>
                <div className="flex flex-1 flex-col py-2">
                    <PullRequestList projectId={projectId} />
                </div>
            </Card>
            {canOpen && (
                <Card className="lg:w-96 lg:shrink-0">
                    <NewPullRequestForm
                        projectId={projectId}
                        branches={branches}
                        onCreated={(created) => router.push(`/projects/pull-requests?id=${projectId}&pr=${created.id}`)}
                    />
                </Card>
            )}
        </div>
    );
}
