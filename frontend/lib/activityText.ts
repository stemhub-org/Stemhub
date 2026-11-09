import type { ActivityItem } from "@/types/activity";

export interface ActivityText {
    /** What happened, e.g. "jb saved a version on main". */
    title: string;
    /** The version message or pull request title, when there is one. */
    detail: string | null;
    /** Secondary facts: the DAW and source file, or the pull request's branches. */
    meta: string | null;
}

function joinPresent(parts: (string | null | undefined)[], separator: string): string | null {
    const present = parts.filter((part): part is string => Boolean(part));
    return present.length > 0 ? present.join(separator) : null;
}

/** Plain-words description of one activity item, using the glossary's terms. */
export function describeActivity(item: ActivityItem): ActivityText {
    const who = item.actor?.username ?? "Someone";
    const branchName = item.branch?.name ?? "a deleted branch";
    const version = item.version;
    const pullRequest = item.pull_request;
    const pullRequestMeta = pullRequest
        ? `${pullRequest.source_branch.name} → ${pullRequest.target_branch.name}`
        : null;

    switch (item.type) {
        case "version_saved":
            return {
                title: `${who} saved a version on ${branchName}`,
                detail: version?.message ?? null,
                meta: joinPresent([version?.source_daw, version?.source_project_filename], " · "),
            };
        case "version_deleted":
            return { title: `A version was deleted on ${branchName}`, detail: version?.message ?? null, meta: null };
        case "branch_created":
            return { title: `Branch ${branchName} was created`, detail: null, meta: null };
        case "branch_deleted":
            return { title: `Branch ${branchName} was deleted`, detail: null, meta: null };
        case "pull_request_opened":
            return { title: `${who} opened a pull request`, detail: pullRequest?.title ?? null, meta: pullRequestMeta };
        case "pull_request_merged":
            return { title: `${who} accepted a pull request`, detail: pullRequest?.title ?? null, meta: pullRequestMeta };
        case "pull_request_closed":
            return { title: `${who} closed a pull request`, detail: pullRequest?.title ?? null, meta: pullRequestMeta };
    }
}

export interface ActivityDay {
    /** "Today", "Yesterday", or the local date. */
    label: string;
    items: ActivityItem[];
}

function localDayKey(date: Date): string {
    return `${date.getFullYear()}-${date.getMonth()}-${date.getDate()}`;
}

/** Groups items (already newest first) by local day, keeping their order. */
export function groupActivityByDay(items: ActivityItem[], now: Date = new Date()): ActivityDay[] {
    const yesterday = new Date(now);
    yesterday.setDate(now.getDate() - 1);
    const todayKey = localDayKey(now);
    const yesterdayKey = localDayKey(yesterday);

    const days: ActivityDay[] = [];
    let currentKey: string | null = null;
    for (const item of items) {
        const date = new Date(item.occurred_at);
        const key = localDayKey(date);
        if (key !== currentKey) {
            const label =
                key === todayKey
                    ? "Today"
                    : key === yesterdayKey
                      ? "Yesterday"
                      : date.toLocaleDateString(undefined, { weekday: "long", day: "numeric", month: "long" });
            days.push({ label, items: [] });
            currentKey = key;
        }
        days[days.length - 1].items.push(item);
    }
    return days;
}
