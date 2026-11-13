"use client";

import { useInfiniteQuery } from "@tanstack/react-query";
import type { ActivityFeedPage, ActivityType } from "@/types/activity";
import { ApiError, apiFetch } from "./api";
import { queryKeys } from "./queries";

export const ACTIVITY_PAGE_SIZE = 20;

export interface ActivityPageRequest {
    /** `next_cursor` of the previous page; omitted for the first one. */
    cursor?: string | null;
    /** Only these types; omitted or empty for every type. */
    types?: readonly ActivityType[] | null;
}

/** One page of a project's activity, newest first. */
export function fetchProjectActivityPage(
    projectId: string,
    { cursor, types }: ActivityPageRequest,
): Promise<ActivityFeedPage> {
    const params = new URLSearchParams({ limit: String(ACTIVITY_PAGE_SIZE) });
    if (cursor) params.set("cursor", cursor);
    for (const type of types ?? []) params.append("types", type);
    return apiFetch<ActivityFeedPage>(`/projects/${projectId}/activity?${params}`);
}

/** A project's activity feed, one page at a time (`fetchNextPage` loads older items). */
export function useProjectActivityFeed(projectId: string, types: readonly ActivityType[] | null) {
    return useInfiniteQuery<ActivityFeedPage, ApiError>({
        queryKey: queryKeys.projectActivityFeed(projectId, types),
        queryFn: ({ pageParam }) =>
            fetchProjectActivityPage(projectId, { cursor: pageParam as string | null, types }),
        initialPageParam: null,
        getNextPageParam: (lastPage) => lastPage.next_cursor,
    });
}
