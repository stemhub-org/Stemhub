"use client";

import { useState } from "react";
import { useRouter } from "next/navigation";
import { GitPullRequest } from "lucide-react";
import { Button } from "@/components/ui/Button";
import { Card } from "@/components/ui/Card";
import { projectRole } from "@/lib/pullRequestPermissions";
import { useCollaborators } from "@/lib/queries";
import type { Branch } from "@/types/project";
import { NewPullRequestForm } from "./NewPullRequestForm";
import { PullRequestList } from "./PullRequestList";

interface PullRequestsTabProps {
    projectId: string;
    ownerId: string;
    currentUserId: string | null;
    /** The project's live branches. */
    branches: Branch[];
}

/** The project's pull requests; owners, Admins and Editors can also open one. */
export function PullRequestsTab({ projectId, ownerId, currentUserId, branches }: PullRequestsTabProps) {
    const router = useRouter();
    const [formOpen, setFormOpen] = useState(false);
    const { data: collaborators } = useCollaborators(projectId);
    const role = projectRole(ownerId, currentUserId ?? undefined, collaborators);
    const canOpen = role === "owner" || role === "Admin" || role === "Editor";

    return (
        <div className="flex flex-col gap-6">
            {canOpen && (
                <Card className="flex flex-col gap-4">
                    {formOpen ? (
                        <NewPullRequestForm
                            projectId={projectId}
                            branches={branches}
                            onCreated={(created) => router.push(`/projects/pull-requests?id=${projectId}&pr=${created.id}`)}
                        />
                    ) : (
                        <div>
                            <Button icon={<GitPullRequest className="size-4" aria-hidden="true" />} onClick={() => setFormOpen(true)}>
                                New pull request
                            </Button>
                        </div>
                    )}
                </Card>
            )}
            <Card padding="none" className="py-2">
                <PullRequestList projectId={projectId} />
            </Card>
        </div>
    );
}
