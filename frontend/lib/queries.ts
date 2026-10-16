"use client";

import { useQuery } from "@tanstack/react-query";
import type { Project } from "@/types/project";
import type { UserProfile } from "@/types/user";
import { apiFetch } from "./api";
import type { ApiError } from "./api";

/** Cache keys, shared so mutations can invalidate them (e.g. after a profile update). */
export const queryKeys = {
    currentUser: ["auth", "me"] as const,
    projects: ["projects"] as const,
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
