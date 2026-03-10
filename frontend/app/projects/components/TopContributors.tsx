"use client";

import { Users } from "lucide-react";
import type { ContributorStats } from "@/types/project";

interface TopContributorsProps {
    contributors: ContributorStats[];
}

export function TopContributors({ contributors }: TopContributorsProps) {
    const maxCommits = Math.max(1, ...contributors.map((c) => c.commits));

    return (
        <div className="flex flex-col gap-3">
            <div className="flex items-center gap-2">
                <Users className="size-4 text-foreground/60" aria-hidden />
                <h3
                    className="text-sm font-medium text-foreground"
                    style={{ fontFamily: "var(--font-syne)" }}
                >
                    Top Contributors
                </h3>
            </div>
            {contributors.length === 0 ? (
                <p className="text-sm text-foreground/50">No contributors yet</p>
            ) : (
                <ul className="flex flex-col gap-3" role="list">
                    {contributors.map((contributor) => (
                        <li key={contributor.user_id} className="flex items-center gap-3">
                            <div
                                className="flex size-8 shrink-0 items-center justify-center rounded-full bg-accent/20 text-xs font-medium text-accent"
                                aria-hidden
                            >
                                {contributor.initials}
                            </div>
                            <div className="min-w-0 flex-1">
                                <p className="truncate text-sm font-medium text-foreground">
                                    {contributor.username}
                                </p>
                                <div className="mt-1 h-1.5 w-full overflow-hidden rounded-full bg-foreground/10">
                                    <div
                                        className="h-full rounded-full bg-accent"
                                        style={{
                                            width: `${(contributor.commits / maxCommits) * 100}%`,
                                        }}
                                    />
                                </div>
                            </div>
                            <span className="shrink-0 text-sm text-foreground/70">
                                {contributor.commits} changes
                            </span>
                        </li>
                    ))}
                </ul>
            )}
        </div>
    );
}
