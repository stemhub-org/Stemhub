"use client";

import Link from "next/link";
import { Clock, ChevronRight } from "lucide-react";
import type { VersionWithAuthor } from "@/types/project";

interface RecentChangesProps {
    versions: VersionWithAuthor[];
    projectId?: string | null;
    branchId?: string | null;
}

function formatTimeAgo(dateString: string): string {
    const now = new Date();
    const date = new Date(dateString);
    const diffMs = now.getTime() - date.getTime();
    const diffMinutes = Math.floor(diffMs / 60000);
    const diffHours = Math.floor(diffMinutes / 60);
    const diffDays = Math.floor(diffHours / 24);

    if (diffMinutes < 1) return "just now";
    if (diffMinutes < 60) return `${diffMinutes}m ago`;
    if (diffHours < 24) return `${diffHours}h ago`;
    if (diffDays < 7) return `${diffDays}d ago`;
    return date.toLocaleDateString();
}

function getInitials(username: string): string {
    return username.slice(0, 2).toUpperCase();
}

export function RecentChanges({ versions, projectId, branchId }: RecentChangesProps) {
    const changesHref = projectId
        ? `/projects/changes?id=${projectId}${branchId ? `&branch_id=${branchId}` : ""}`
        : "/projects/changes";

    return (
        <div className="flex flex-col gap-4">
            <div className="flex items-center justify-between gap-2">
                <div className="flex items-center gap-2">
                    <Clock className="size-4 text-foreground/60" aria-hidden />
                    <h3
                        className="pb-1 text-sm font-medium leading-relaxed text-foreground"
                        style={{ fontFamily: "var(--font-syne)" }}
                    >
                        Recent Changes
                    </h3>
                    <span className="text-xs text-foreground/50">
                        • {versions.length} recent
                    </span>
                </div>
                <Link
                    href={changesHref}
                    className="flex items-center gap-1 rounded-lg px-2 py-1 text-xs font-medium text-foreground/70 transition-colors hover:bg-foreground/5 hover:text-foreground"
                    aria-label="View all changes"
                >
                    View all
                    <ChevronRight className="size-3.5" aria-hidden />
                </Link>
            </div>
            {versions.length === 0 ? (
                <div className="rounded-xl border border-foreground/[0.08] bg-foreground/[0.02] p-6 text-center">
                    <p className="text-sm text-foreground/50">No changes yet</p>
                </div>
            ) : (
                <ul className="flex flex-col gap-3" role="list">
                    {versions.map((version) => {
                        const authorName = version.author?.username || "Unknown";
                        const initials = version.author
                            ? getInitials(version.author.username)
                            : "??";

                        return (
                            <li key={version.id}>
                                <div className="rounded-xl border border-foreground/[0.08] bg-foreground/[0.02] p-4">
                                    <div className="flex gap-3">
                                        <div
                                            className="flex size-8 shrink-0 items-center justify-center rounded-full bg-accent/80 text-xs font-medium text-white"
                                            aria-hidden
                                        >
                                            {initials}
                                        </div>
                                        <div className="min-w-0 flex-1">
                                            <div className="flex flex-wrap items-center gap-2 text-sm">
                                                <span className="font-medium text-foreground">
                                                    {authorName}
                                                </span>
                                                <span className="text-foreground/50">
                                                    {formatTimeAgo(version.created_at)}
                                                </span>
                                                <span className="rounded-md border bg-accent/15 text-accent border-accent/30 px-2.5 py-0.5 text-[10px] font-medium">
                                                    Saved
                                                </span>
                                            </div>
                                            <p className="mt-1 text-sm text-foreground/80">
                                                {version.commit_message || "No message"}
                                            </p>
                                            <p className="mt-0.5 font-mono text-xs text-foreground/60">
                                                {version.branch_name}
                                            </p>
                                        </div>
                                    </div>
                                </div>
                            </li>
                        );
                    })}
                </ul>
            )}
        </div>
    );
}
