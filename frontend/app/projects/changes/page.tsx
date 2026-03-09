"use client";

import type React from "react";
import Link from "next/link";
import { Suspense } from "react";
import { useSearchParams } from "next/navigation";
import { useTheme } from "next-themes";
import { useState, useEffect } from "react";
import { RepositoryHeader } from "../components/RepositoryHeader";
import { ArrowLeft, Loader2 } from "lucide-react";
import { authFetch } from "@/lib/api";
import type { VersionWithAuthor } from "@/types/project";

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

const cardBase =
    "rounded-xl bg-background-tertiary border border-border-subtle p-6 transition-all duration-300";
const cardHoverDark =
    "hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.08)]";

function ProjectChangesContent() {
    const { resolvedTheme } = useTheme();
    const searchParams = useSearchParams();
    const projectId = searchParams.get("id");
    const isDark = resolvedTheme === "dark";
    const cardClass = `${cardBase} ${isDark ? cardHoverDark : ""}`;

    const [versions, setVersions] = useState<VersionWithAuthor[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        if (!projectId) {
            setLoading(false);
            setError("No project ID provided.");
            return;
        }
        (async () => {
            try {
                const data = await authFetch<{ recent_versions: VersionWithAuthor[] }>(
                    `/projects/${projectId}`
                );
                setVersions(data.recent_versions);
            } catch (err) {
                setError(err instanceof Error ? err.message : "Failed to load changes");
            } finally {
                setLoading(false);
            }
        })();
    }, [projectId]);

    if (loading) {
        return (
            <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                <Loader2 className="size-8 text-accent animate-spin" />
            </div>
        );
    }

    if (error) {
        return (
            <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                <p className="text-red-500">{error}</p>
            </div>
        );
    }

    return (
        <div
            className="min-h-screen bg-background text-foreground"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <RepositoryHeader />
            <div className="p-6 space-y-6">
                <Link
                    href={`/projects?id=${projectId}`}
                    className="inline-flex items-center gap-2 text-sm text-foreground/70 transition-colors hover:text-foreground"
                >
                    <ArrowLeft className="size-4" aria-hidden />
                    Back to project
                </Link>
                <div className={cardClass}>
                    <h1
                        className="mb-6 pb-1 text-lg font-medium leading-relaxed text-foreground"
                        style={{ fontFamily: "var(--font-syne)" }}
                    >
                        Changes
                    </h1>
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
                                                            Commit
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
            </div>
        </div>
    );
}

export default function ProjectChangesPage() {
    return (
        <Suspense
            fallback={
                <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                    <div className="h-6 w-6 border-2 border-border-subtle border-t-accent rounded-full animate-spin" />
                </div>
            }
        >
            <ProjectChangesContent />
        </Suspense>
    );
}
