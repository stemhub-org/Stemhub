"use client";

import type React from "react";
import { useState } from "react";
import Link from "next/link";
import { useTheme } from "next-themes";
import { Heart, GitBranch, Clock, Activity, Folder, Search, Plus, User } from "lucide-react";

const PROJECTS = [
    {
        id: "main",
        name: "Dubstep-Track",
        visibility: "Private",
        description: "Experimental dubstep track · 140 BPM · C minor",
        updatedAt: "Last updated 2h ago",
        href: "/projects", // existing project detail page
        languages: ["Ableton Live"],
        stars: 12,
        branches: 4,
    },
    {
        id: "placeholder-1",
        name: "Synthwave Anthem",
        visibility: "Private",
        description: "Retro synthwave session with analog leads and wide pads.",
        updatedAt: "Updated 1d ago",
        href: "#",
        languages: ["FL Studio"],
        stars: 8,
        branches: 3,
    },
    {
        id: "placeholder-2",
        name: "Trap Beat 04",
        visibility: "Private",
        description: "Hard-hitting trap beat with 808s and fast hats.",
        updatedAt: "Updated 3d ago",
        href: "#",
        languages: ["Logic Pro"],
        stars: 5,
        branches: 2,
    },
    {
        id: "placeholder-3",
        name: "Lo-Fi Study Session",
        visibility: "Private",
        description: "Lo-fi beats with warm keys and vinyl textures.",
        updatedAt: "Updated 5d ago",
        href: "#",
        languages: ["Ableton Live"],
        stars: 4,
        branches: 3,
    },
    {
        id: "placeholder-4",
        name: "Afrobeat Groove",
        visibility: "Private",
        description: "Afrobeat drums, percussions and bright guitars.",
        updatedAt: "Updated 1w ago",
        href: "#",
        languages: ["FL Studio"],
        stars: 6,
        branches: 4,
    },
    {
        id: "placeholder-5",
        name: "House Banger 2026",
        visibility: "Private",
        description: "Club-ready house track with punchy kick and bass.",
        updatedAt: "Updated 2w ago",
        href: "#",
        languages: ["Logic Pro"],
        stars: 9,
        branches: 5,
    },
];

export default function DashboardProjectsPage() {
    const [favorites, setFavorites] = useState<Record<string, boolean>>({});
    const { resolvedTheme } = useTheme();
    const isDark = resolvedTheme === "dark";

    return (
        <div
            className="max-w-6xl mx-auto pt-6 pb-10"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <div className="flex flex-col gap-8 md:flex-row md:items-start md:gap-10">
                <aside className="w-full md:w-62 flex-shrink-0">
                    <div className="flex flex-col items-center md:items-start gap-4">
                        <div className="h-60 w-60 rounded-full bg-gradient-to-tr from-accent to-accent/40 border border-border-subtle flex items-center justify-center">
                            <span className="flex h-24 w-24 items-center justify-center rounded-full bg-black/15">
                                <User className="h-14 w-14 text-white/90" aria-hidden />
                            </span>
                        </div>
                        <div className="space-y-2 text-center md:text-left">
                            <h1
                                className="text-3xl font-medium tracking-tight"
                                style={{ fontFamily: "var(--font-syne)" }}
                            >
                                Dryss Margueritte
                            </h1>
                            <p className="text-base text-foreground-muted">Dryss10</p>
                            <p className="text-sm text-foreground-muted">
                                <span className="font-medium text-foreground">2</span> followers ·{" "}
                                <span className="font-medium text-foreground">10</span> following
                            </p>
                            <button
                                className={`mt-2 w-full rounded-lg border border-border-subtle px-4 py-2 text-sm font-medium transition-colors ${
                                    isDark
                                        ? "bg-background-tertiary text-foreground hover:bg-background-tertiary/80 hover:border-accent/40"
                                        : "bg-background-secondary text-foreground hover:bg-background-tertiary hover:border-accent/40"
                                }`}
                            >
                                View profile
                            </button>
                        </div>
                    </div>
                </aside>

                <section className="flex-1 space-y-4">
                    <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:gap-4">
                        <div className="relative w-full sm:flex-1">
                            <span className="pointer-events-none absolute inset-y-0 left-3 flex items-center text-foreground-muted">
                                <Search className="size-4" aria-hidden />
                            </span>
                            <input
                                type="text"
                                placeholder="Find a project..."
                                className="w-full rounded-md border border-border-subtle bg-background-secondary/80 py-2.5 pl-9 pr-3 text-sm text-foreground placeholder:text-foreground-muted focus:outline-none focus:border-border-hover"
                            />
                        </div>
                        <button
                            type="button"
                            className="inline-flex items-center justify-center gap-2 rounded-lg px-5 py-2 text-base font-medium text-foreground dark:text-white bg-accent/25 border border-accent/35 backdrop-blur-xl shadow-[0_0_24px_rgba(156,87,223,0.18)] hover:bg-accent/35 hover:border-accent/50 transition-all duration-300"
                        >
                            <Plus className="size-4" aria-hidden />
                            New
                        </button>
                    </div>

                    <ul
                        className="divide-y divide-border-subtle border border-border-subtle rounded-md bg-background-secondary/60"
                        role="list"
                    >
                        {PROJECTS.map((project, index) => {
                            const isClickable = index === 0 && project.href !== "#";
                            const isFavorite = !!favorites[project.id];

                            return (
                                <li
                                    key={project.id}
                                    className="px-4 py-3 hover:bg-background-tertiary/70 transition-colors"
                                >
                                    <div className="flex items-center justify-between gap-3">
                                        <div className="min-w-0 flex-1 space-y-0.5">
                                            <div className="flex items-baseline gap-2">
                                                <Folder className="size-4 text-accent" aria-hidden />
                                                {isClickable ? (
                                                    <Link
                                                        href={project.href}
                                                        className="text-lg font-medium text-foreground hover:text-accent transition-colors"
                                                    >
                                                        {project.name}
                                                    </Link>
                                                ) : (
                                                    <span className="text-lg font-medium text-foreground">
                                                        {project.name}
                                                    </span>
                                                )}
                                                <span className="text-xs uppercase tracking-wide text-foreground-muted">
                                                    {project.visibility}
                                                </span>
                                            </div>
                                            <p className="mt-0.5 text-sm text-foreground-muted truncate">
                                                {project.description}
                                            </p>
                                            <div className="mt-1 flex flex-wrap items-center gap-3 text-sm text-foreground-muted">
                                                <span className="inline-flex items-center gap-1">
                                                    <Clock className="size-3.5" aria-hidden />
                                                    {project.updatedAt}
                                                </span>
                                                <span className="inline-flex items-center gap-1">
                                                    <GitBranch className="size-3.5" aria-hidden />
                                                    {project.branches} versions
                                                </span>
                                                <span className="inline-flex items-center gap-1">
                                                    <Activity className="size-3.5" aria-hidden />
                                                    {project.stars} stars
                                                </span>
                                            </div>
                                        </div>
                                        <button
                                            type="button"
                                            className="inline-flex items-center gap-1 rounded-md border border-border-subtle bg-background px-3 py-1 text-sm font-medium text-foreground-muted hover:border-accent/40 hover:text-foreground transition-colors"
                                            onClick={() =>
                                                setFavorites((prev) => ({
                                                    ...prev,
                                                    [project.id]: !isFavorite,
                                                }))
                                            }
                                        >
                                            <Heart
                                                className={`size-3.5 ${
                                                    isFavorite
                                                        ? "text-red-500 fill-red-500"
                                                        : "text-foreground-muted"
                                                }`}
                                                aria-hidden
                                            />
                                            Favorite
                                        </button>
                                    </div>
                                </li>
                            );
                        })}
                    </ul>
                </section>
            </div>
        </div>
    );
}

