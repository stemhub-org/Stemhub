"use client";

import { Users } from "lucide-react";

type Contributor = {
    id: string;
    name: string;
    initials: string;
    commits: number;
};

const PLACEHOLDER_CONTRIBUTORS: Contributor[] = [
    { id: "1", name: "Skrillex", initials: "SK", commits: 143 },
    { id: "2", name: "Metro Boomin", initials: "MB", commits: 87 },
    { id: "3", name: "deadmau5", initials: "D5", commits: 52 },
];

const MAX_COMMITS = Math.max(
    ...PLACEHOLDER_CONTRIBUTORS.map((c) => c.commits)
);

export function TopContributors() {
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
            <ul className="flex flex-col gap-3" role="list">
                {PLACEHOLDER_CONTRIBUTORS.map((contributor) => (
                    <li key={contributor.id} className="flex items-center gap-3">
                        <div
                            className="flex size-8 shrink-0 items-center justify-center rounded-full bg-accent/20 text-xs font-medium text-accent"
                            aria-hidden
                        >
                            {contributor.initials}
                        </div>
                        <div className="min-w-0 flex-1">
                            <p className="truncate text-sm font-medium text-foreground">
                                {contributor.name}
                            </p>
                            <div className="mt-1 h-1.5 w-full overflow-hidden rounded-full bg-foreground/10">
                                <div
                                    className="h-full rounded-full bg-accent"
                                    style={{
                                        width: `${(contributor.commits / MAX_COMMITS) * 100}%`,
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
        </div>
    );
}
