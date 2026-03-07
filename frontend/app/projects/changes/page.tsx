"use client";

import type React from "react";
import Link from "next/link";
import { useTheme } from "next-themes";
import { RepositoryHeader } from "../components/RepositoryHeader";
import { ArrowLeft } from "lucide-react";

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
        Updated: "bg-accent/15 text-accent border border-accent/30",
        Added: "bg-emerald-500/15 text-emerald-700 border border-emerald-500/30",
        Merged: "bg-amber-500/15 text-amber-700 border border-amber-500/30",
    };
    return (
        <span
            className={`rounded-md border px-2.5 py-0.5 text-[10px] font-medium ${styles[type]}`}
        >
            {type}
        </span>
    );
}

const cardBase =
    "rounded-xl bg-background-tertiary border border-border-subtle p-6 transition-all duration-300";
const cardHoverDark =
    "hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.08)]";

export default function ProjectChangesPage() {
    const { resolvedTheme } = useTheme();
    const isDark = resolvedTheme === "dark";
    const cardClass = `${cardBase} ${isDark ? cardHoverDark : ""}`;

    return (
        <div
            className="min-h-screen bg-background text-foreground"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <RepositoryHeader />
            <div className="p-6 space-y-6">
                <Link
                    href="/projects"
                    className="inline-flex items-center gap-2 text-sm text-foreground/70 transition-colors hover:text-foreground"
                >
                    <ArrowLeft className="size-4" aria-hidden />
                    Back to projects
                </Link>
                <div className={cardClass}>
                    <h1
                        className="mb-6 pb-1 text-lg font-medium leading-relaxed text-foreground"
                        style={{ fontFamily: "var(--font-syne)" }}
                    >
                        Changes
                    </h1>
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
            </div>
        </div>
    );
}
