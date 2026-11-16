import type { Collaborator } from "@/types/project";
import type { PullRequestStatus } from "@/types/pullRequest";

/** The signed-in user's role in a project; null when they have none (or it is still loading). */
export type ProjectRole = "owner" | "Admin" | "Editor" | "Viewer" | null;

export function projectRole(
    ownerId: string,
    userId: string | undefined,
    collaborators: Collaborator[] | undefined,
): ProjectRole {
    if (!userId) return null;
    if (userId === ownerId) return "owner";
    const role = collaborators?.find((collaborator) => collaborator.user_id === userId)?.role;
    return role === "Admin" || role === "Editor" || role === "Viewer" ? role : null;
}

/**
 * Which buttons a pull request shows, mirroring the backend's rules: owners,
 * Admins and Editors may close; only owners and Admins may accept. The
 * backend still checks, so a stale role only shows a button that gets refused.
 */
export function pullRequestActions(
    role: ProjectRole,
    status: PullRequestStatus,
): { canClose: boolean; canAccept: boolean } {
    const open = status === "OPEN";
    const canAccept = open && (role === "owner" || role === "Admin");
    return { canClose: canAccept || (open && role === "Editor"), canAccept };
}
