"use client";

import Link from "next/link";
import { Clock, ChevronRight } from "lucide-react";

type ChangeType = "Updated" | "Added" | "Merged";

type ChangeEntry = {
    id: string;
    author: string;
    initials: string;
    timeAgo: string;
    type: ChangeType;
    description: string;
    file: string;
    tags: string[];
};

const PLACEHOLDER_CHANGES: ChangeEntry[] = [
    {
        id: "1",
        author: "Skrillex",
        initials: "SK",
        timeAgo: "1h ago",
        type: "Updated",
        description: "Updated master with new limiter settings",
        file: "Master_v3.wav",
        tags: ["Master", "Limiting", "Dynamics"],
    },
    {
        id: "2",
        author: "Metro Boomin",
        initials: "MB",
        timeAgo: "2h ago",
        type: "Added",
        description: "Added sub bass layer to bassline",
        file: "Bass_Bus.wav",
        tags: ["Bass", "Sub", "Low End"],
    },
    {
        id: "3",
        author: "Skrillex",
        initials: "SK",
        timeAgo: "2h ago",
        type: "Updated",
        description: "Updated kick pattern with sidechain",
        file: "Drums/Kick.wav",
        tags: ["Drums", "Kick", "Sidechain"],
    },
    {
        id: "4",
        author: "deadmau5",
        initials: "D5",
        timeAgo: "5h ago",
        type: "Merged",
        description: "Merged vocal harmonies",
        file: "Vocals/Harmony_Stack.wav",
        tags: ["Vocals", "Harmonies"],
    },
    {
        id: "5",
        author: "Skrillex",
        initials: "SK",
        timeAgo: "1d ago",
        type: "Updated",
        description: "Reorganized mixer tracks",
        file: "Main_Project.als",
        tags: ["Mix", "Project", "Mixer", "Organization"],
    },
];

function TypeBadge({ type }: { type: ChangeType }) {
    const styles: Record<ChangeType, string> = {
        Updated:
            "bg-accent/15 text-accent border border-accent/30",
        Added:
            "bg-emerald-500/15 text-emerald-700 border border-emerald-500/30",
        Merged:
            "bg-amber-500/15 text-amber-700 border border-amber-500/30",
    };
    return (
        <span
            className={`rounded-md border px-2.5 py-0.5 text-[10px] font-medium ${styles[type]}`}
        >
            {type}
        </span>
    );
}

export function RecentChanges() {
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
                        • Last 24 hours
                    </span>
                </div>
                <Link
                    href="/repository/commits"
                    className="flex items-center gap-1 rounded-lg px-2 py-1 text-xs font-medium text-foreground/70 transition-colors hover:bg-foreground/5 hover:text-foreground"
                    aria-label="View all changes"
                >
                    View all
                    <ChevronRight className="size-3.5" aria-hidden />
                </Link>
            </div>
            <ul className="flex flex-col gap-3" role="list">
                {PLACEHOLDER_CHANGES.map((entry) => (
                    <li key={entry.id}>
                        <div className="rounded-xl border border-foreground/[0.08] bg-foreground/[0.02] p-4">
                            <div className="flex gap-3">
                                <div
                                    className="flex size-8 shrink-0 items-center justify-center rounded-full bg-accent/80 text-xs font-medium text-white"
                                    aria-hidden
                                >
                                    {entry.initials}
                                </div>
                                <div className="min-w-0 flex-1">
                                    <div className="flex flex-wrap items-center gap-2 text-sm">
                                        <span className="font-medium text-foreground">
                                            {entry.author}
                                        </span>
                                        <span className="text-foreground/50">
                                            {entry.timeAgo}
                                        </span>
                                        <TypeBadge type={entry.type} />
                                    </div>
                                    <p className="mt-1 text-sm text-foreground/80">
                                        {entry.description}
                                    </p>
                                    <p className="mt-0.5 font-mono text-xs text-foreground/60">
                                        {entry.file}
                                    </p>
                                    <div className="mt-2 flex flex-wrap gap-1.5">
                                        {entry.tags.map((tag) => (
                                            <span
                                                key={tag}
                                                className="rounded-md bg-foreground/[0.06] px-2 py-0.5 text-[10px] text-foreground/70"
                                            >
                                                {tag}
                                            </span>
                                        ))}
                                    </div>
                                </div>
                            </div>
                        </div>
                    </li>
                ))}
            </ul>
        </div>
    );
}
