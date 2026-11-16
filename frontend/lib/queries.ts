"use client";

import { keepPreviousData, useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import type {
    ActivityStatsResponse,
    AssetSummary,
    Collaborator,
    Project,
    ProjectSummaryResponse,
    TopContributorsResponse,
    VersionDiffHistoryEntry,
} from "@/types/project";
import type { NewPullRequest, PullRequest } from "@/types/pullRequest";
import type { UserProfile } from "@/types/user";
import { ApiError, apiFetch } from "./api";

/** Cache keys, shared so mutations can invalidate them (e.g. after a profile update). */
export const queryKeys = {
    currentUser: ["auth", "me"] as const,
    projects: ["projects"] as const,
    /** Prefix of everything cached for one project; invalidate it after a branch change. */
    project: (projectId: string) => ["projects", projectId] as const,
    projectSummary: (projectId: string, branchId: string | null) =>
        ["projects", projectId, "summary", branchId] as const,
    projectActivity: (projectId: string) => ["projects", projectId, "activity"] as const,
    /** The activity feed, per filter (`null` = every type). */
    projectActivityFeed: (projectId: string, types: readonly string[] | null) =>
        ["projects", projectId, "activity-feed", types ?? "all"] as const,
    topContributors: (projectId: string) => ["projects", projectId, "top-contributors"] as const,
    versionAssets: (versionId: string) => ["versions", versionId, "assets"] as const,
    branchDiffHistory: (branchId: string) => ["branches", branchId, "diff-history"] as const,
    pullRequests: (projectId: string) => ["projects", projectId, "pull-requests"] as const,
    pullRequest: (pullRequestId: string) => ["pull-requests", pullRequestId] as const,
    collaborators: (projectId: string) => ["projects", projectId, "collaborators"] as const,
};

export function useCurrentUser() {
    return useQuery<UserProfile, ApiError>({
        queryKey: queryKeys.currentUser,
        queryFn: () => apiFetch<UserProfile>("/auth/me"),
    });
}

export function useProjects() {
    return useQuery<Project[], ApiError>({
        queryKey: queryKeys.projects,
        queryFn: () => apiFetch<Project[]>("/projects/"),
        select: (projects) => projects.filter((project) => !project.is_deleted),
    });
}

export interface NewProject {
    name: string;
    description: string | null;
    category: string;
}

export function useCreateProject() {
    const queryClient = useQueryClient();
    return useMutation<Project, ApiError, NewProject>({
        mutationFn: (project) =>
            apiFetch<Project>("/projects/", { method: "POST", body: JSON.stringify(project) }),
        onSuccess: (created) => {
            queryClient.setQueryData<Project[]>(queryKeys.projects, (projects) =>
                projects ? [created, ...projects] : projects,
            );
        },
    });
}

export interface ProjectSummaryResult {
    summary: ProjectSummaryResponse;
    /** The branch the summary is for: the requested one if it exists, else the first one, else "". */
    branchId: string;
}

/**
 * Loads a project summary for one branch. Without a branch, or with one that
 * no longer exists, it falls back to the project's first branch, and to the
 * all-branches summary when the project has none.
 */
export async function fetchProjectSummary(
    projectId: string,
    branchId: string | null,
): Promise<ProjectSummaryResult> {
    const summaryPath = `/projects/${projectId}/summary`;

    if (branchId) {
        try {
            const summary = await apiFetch<ProjectSummaryResponse>(`${summaryPath}?branch_id=${branchId}`);
            return { summary, branchId };
        } catch (error) {
            // 404: the branch was deleted or belongs to another project.
            if (!(error instanceof ApiError && error.status === 404)) throw error;
        }
    }

    const allBranches = await apiFetch<ProjectSummaryResponse>(summaryPath);
    const firstBranchId = allBranches.branches[0]?.id;
    if (!firstBranchId) return { summary: allBranches, branchId: "" };

    const summary = await apiFetch<ProjectSummaryResponse>(`${summaryPath}?branch_id=${firstBranchId}`);
    return { summary, branchId: firstBranchId };
}

export function useProjectSummary(projectId: string, branchId: string | null) {
    return useQuery<ProjectSummaryResult, ApiError>({
        queryKey: queryKeys.projectSummary(projectId, branchId),
        queryFn: () => fetchProjectSummary(projectId, branchId),
        // Keep showing the previous branch while the next one loads.
        placeholderData: keepPreviousData,
    });
}

export function useProjectActivity(projectId: string) {
    return useQuery<ActivityStatsResponse, ApiError>({
        queryKey: queryKeys.projectActivity(projectId),
        queryFn: () => apiFetch<ActivityStatsResponse>(`/projects/${projectId}/stats/activity`),
    });
}

export function useTopContributors(projectId: string) {
    return useQuery<TopContributorsResponse, ApiError>({
        queryKey: queryKeys.topContributors(projectId),
        queryFn: () => apiFetch<TopContributorsResponse>(`/projects/${projectId}/stats/top-contributors`),
    });
}

export function useVersionAssets(versionId: string | null) {
    return useQuery<AssetSummary[], ApiError>({
        queryKey: queryKeys.versionAssets(versionId ?? ""),
        queryFn: () => apiFetch<AssetSummary[]>(`/versions/${versionId}/assets`),
        enabled: Boolean(versionId),
    });
}

export function useBranchDiffHistory(branchId: string) {
    return useQuery<VersionDiffHistoryEntry[], ApiError>({
        queryKey: queryKeys.branchDiffHistory(branchId),
        queryFn: () => apiFetch<VersionDiffHistoryEntry[]>(`/branches/${branchId}/versions/diff-history`),
        enabled: Boolean(branchId),
    });
}

// ── Pull requests ──

export function usePullRequests(projectId: string) {
    return useQuery<PullRequest[], ApiError>({
        queryKey: queryKeys.pullRequests(projectId),
        queryFn: () => apiFetch<PullRequest[]>(`/projects/${projectId}/pull-requests`),
        enabled: Boolean(projectId),
    });
}

export function usePullRequest(pullRequestId: string) {
    return useQuery<PullRequest, ApiError>({
        queryKey: queryKeys.pullRequest(pullRequestId),
        queryFn: () => apiFetch<PullRequest>(`/pull-requests/${pullRequestId}`),
        enabled: Boolean(pullRequestId),
    });
}

export function useCollaborators(projectId: string) {
    return useQuery<Collaborator[], ApiError>({
        queryKey: queryKeys.collaborators(projectId),
        queryFn: () => apiFetch<Collaborator[]>(`/projects/${projectId}/collaborators/`),
        enabled: Boolean(projectId),
    });
}

/**
 * After a change, the pull request's own cache gets the response, and the
 * whole project is refreshed: its PR list, its activity feed, and (on
 * accept, which adds a version to the target branch) its summary.
 */
function usePullRequestMutation<Variables>(projectId: string, request: (variables: Variables) => Promise<PullRequest>) {
    const queryClient = useQueryClient();
    return useMutation<PullRequest, ApiError, Variables>({
        mutationFn: request,
        onSuccess: (pullRequest) => {
            queryClient.setQueryData(queryKeys.pullRequest(pullRequest.id), pullRequest);
            void queryClient.invalidateQueries({ queryKey: queryKeys.project(projectId) });
        },
    });
}

export function useCreatePullRequest(projectId: string) {
    return usePullRequestMutation<NewPullRequest>(projectId, (pullRequest) =>
        apiFetch<PullRequest>(`/projects/${projectId}/pull-requests`, {
            method: "POST",
            body: JSON.stringify(pullRequest),
        }),
    );
}

export function useClosePullRequest(projectId: string) {
    return usePullRequestMutation<string>(projectId, (pullRequestId) =>
        apiFetch<PullRequest>(`/pull-requests/${pullRequestId}/close`, { method: "POST" }),
    );
}

export function useAcceptPullRequest(projectId: string) {
    return usePullRequestMutation<string>(projectId, (pullRequestId) =>
        apiFetch<PullRequest>(`/pull-requests/${pullRequestId}/accept`, { method: "POST" }),
    );
}
