"use client";

import type { LucideIcon } from "lucide-react";
import {
    Activity,
    GitBranch,
    GitBranchPlus,
    GitMerge,
    GitPullRequest,
    GitPullRequestClosed,
    Loader2,
    Save,
    Trash2,
} from "lucide-react";
import { ErrorState } from "@/components/ui/ErrorState";
import { useProjectActivityFeed } from "@/lib/activityFeed";
import { describeActivity, groupActivityByDay } from "@/lib/activityText";
import { formatDateTime, formatTimeAgo } from "@/lib/time";
import type { ActivityItem, ActivityType } from "@/types/activity";

const ICONS: Record<ActivityType, LucideIcon> = {
    version_saved: Save,
    version_deleted: Trash2,
    branch_created: GitBranchPlus,
    branch_deleted: GitBranch,
    pull_request_opened: GitPullRequest,
    pull_request_merged: GitMerge,
    pull_request_closed: GitPullRequestClosed,
};

function ActivityRow({ item }: { item: ActivityItem }) {
    const Icon = ICONS[item.type];
    const { title, detail, meta } = describeActivity(item);

    return (
        <li className="flex gap-3">
            <div
                className="flex size-8 shrink-0 items-center justify-center rounded-full bg-foreground/5 text-foreground/70"
                aria-hidden
            >
                <Icon className="size-4" />
            </div>
            <div className="min-w-0 flex-1">
                <div className="flex flex-wrap items-baseline justify-between gap-x-2">
                    <p className="text-sm text-foreground">{title}</p>
                    <time
                        dateTime={item.occurred_at}
                        title={formatDateTime(item.occurred_at)}
                        className="shrink-0 text-xs text-foreground-muted"
                    >
                        {formatTimeAgo(item.occurred_at)}
                    </time>
                </div>
                {detail && <p className="mt-0.5 truncate text-sm text-foreground/80">{detail}</p>}
                {meta && <p className="mt-0.5 truncate font-mono text-xs text-foreground-muted">{meta}</p>}
            </div>
        </li>
    );
}

interface ActivityFeedProps {
    projectId: string;
}

/** What happened in a project, newest first, one page at a time. */
export function ActivityFeed({ projectId }: ActivityFeedProps) {
    const feed = useProjectActivityFeed(projectId, null);
    const items = feed.data?.pages.flatMap((page) => page.items) ?? [];

    return (
        <div className="flex flex-col gap-4">
            <div className="flex items-center gap-2">
                <Activity className="size-4 text-foreground/60" aria-hidden />
                <h3 className="text-sm font-medium text-foreground" style={{ fontFamily: "var(--font-syne)" }}>
                    Activity
                </h3>
            </div>

            {feed.isError ? (
                <ErrorState
                    error={feed.error}
                    title="Couldn't load the activity"
                    onRetry={() => feed.refetch()}
                    className="p-6"
                />
            ) : feed.isPending ? (
                <div className="flex justify-center py-6">
                    <Loader2 className="size-5 animate-spin text-foreground-muted" aria-label="Loading activity" />
                </div>
            ) : items.length === 0 ? (
                <p className="text-sm text-foreground-muted">
                    Nothing has happened in this project yet.
                </p>
            ) : (
                <div className="flex flex-col gap-5">
                    {groupActivityByDay(items).map((day) => (
                        <section key={day.label} aria-label={day.label} className="flex flex-col gap-3">
                            <h4 className="text-xs font-bold uppercase tracking-wider text-foreground-muted">
                                {day.label}
                            </h4>
                            <ul className="flex flex-col gap-4" role="list">
                                {day.items.map((item) => (
                                    <ActivityRow key={item.id} item={item} />
                                ))}
                            </ul>
                        </section>
                    ))}
                </div>
            )}

            {feed.hasNextPage && (
                <button
                    type="button"
                    onClick={() => feed.fetchNextPage()}
                    disabled={feed.isFetchingNextPage}
                    className="self-start rounded-lg border border-border-subtle px-3 py-1.5 text-xs font-medium text-foreground transition-colors hover:border-border-hover hover:bg-foreground/5 disabled:opacity-50"
                >
                    {feed.isFetchingNextPage ? "Loading…" : "Load older activity"}
                </button>
            )}
        </div>
    );
}
