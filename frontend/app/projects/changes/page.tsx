"use client";

import type React from "react";
import Link from "next/link";
import { Suspense } from "react";
import { useRouter, useSearchParams } from "next/navigation";
import { useTheme } from "next-themes";
import { useState, useEffect } from "react";
import { RepositoryHeader } from "../components/RepositoryHeader";
import { ArrowLeft, Loader2, Info } from "lucide-react";
import { authFetch } from "@/lib/api";
import type { ProjectSummaryResponse, VersionDiffHistoryEntry } from "@/types/project";
import { RepositoryBranchBar } from "../components/RepositoryBranchBar";

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

function formatSummaryLine(entry: VersionDiffHistoryEntry): string {
    if (!entry.summary) return "";
    const parts = [
        `${entry.summary.total_changes} change${entry.summary.total_changes === 1 ? "" : "s"}`,
        `${entry.summary.inserts_changed} insert${entry.summary.inserts_changed === 1 ? "" : "s"}`,
        `${entry.summary.slots_changed} slot${entry.summary.slots_changed === 1 ? "" : "s"}`,
    ];
    return parts.join(" • ");
}

const cardBase =
    "rounded-xl bg-background-secondary dark:bg-background-tertiary border border-border-subtle p-6 transition-all duration-300";
const cardHoverDark =
    "hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary dark:hover:from-background-tertiary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.08)]";

function ProjectChangesContent() {
    const { resolvedTheme } = useTheme();
    const router = useRouter();
    const searchParams = useSearchParams();
    const projectId = searchParams.get("id");
    const branchIdFromQuery = searchParams.get("branch_id") || "";
    const isDark = resolvedTheme === "dark";
    const cardClass = `${cardBase} ${isDark ? cardHoverDark : ""}`;

    const [summary, setSummary] = useState<ProjectSummaryResponse | null>(null);
    const [historyEntries, setHistoryEntries] = useState<VersionDiffHistoryEntry[]>([]);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [selectedBranchId, setSelectedBranchId] = useState(branchIdFromQuery);

    useEffect(() => {
        setSelectedBranchId(branchIdFromQuery);
    }, [branchIdFromQuery]);

    useEffect(() => {
        if (!projectId) {
            setLoading(false);
            setError("No project ID provided.");
            return;
        }

        let cancelled = false;

        (async () => {
            setLoading(true);
            setError(null);
            try {
                if (!selectedBranchId) {
                    const data = await authFetch<ProjectSummaryResponse>(`/projects/${projectId}/summary`);

                    if (cancelled) return;

                    if (data.branches.length > 0) {
                        const fallbackBranchId = data.branches[0].id;
                        setSelectedBranchId(fallbackBranchId);
                        router.replace(`/projects/changes?id=${projectId}&branch_id=${fallbackBranchId}`);
                        return;
                    }

                    setSummary(data);
                    setHistoryEntries([]);
                    return;
                }

                const [summaryData, historyData] = await Promise.all([
                    authFetch<ProjectSummaryResponse>(`/projects/${projectId}/summary?branch_id=${selectedBranchId}`),
                    authFetch<VersionDiffHistoryEntry[]>(`/branches/${selectedBranchId}/versions/diff-history`),
                ]);

                if (cancelled) return;

                const branchExists = summaryData.branches.some((branch) => branch.id === selectedBranchId);
                if (!branchExists && summaryData.branches.length > 0) {
                    const fallbackBranchId = summaryData.branches[0].id;
                    setSelectedBranchId(fallbackBranchId);
                    router.replace(`/projects/changes?id=${projectId}&branch_id=${fallbackBranchId}`);
                    return;
                }

                setHistoryEntries(historyData);
                setSummary(summaryData);
            } catch (err) {
                if (cancelled) return;
                setError(err instanceof Error ? err.message : "Failed to load changes");
            } finally {
                if (!cancelled) {
                    setLoading(false);
                }
            }
        })();

        return () => {
            cancelled = true;
        };
    }, [projectId, router, selectedBranchId]);

    const branches = summary?.branches || [];

    const handleBranchChange = (branchId: string) => {
        setSelectedBranchId(branchId);
        if (projectId) {
            router.replace(`/projects/changes?id=${projectId}&branch_id=${branchId}`);
        }
    };

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
                    href={`/projects?id=${projectId}${selectedBranchId ? `&branch_id=${selectedBranchId}` : ""}`}
                    className="inline-flex items-center gap-2 text-sm text-foreground/70 transition-colors hover:text-foreground"
                >
                    <ArrowLeft className="size-4" aria-hidden />
                    Back to project
                </Link>
                <div className={cardClass}>
                    <div className="mb-6 flex flex-col gap-4">
                        <div className="flex flex-wrap items-center justify-between gap-3">
                            <div>
                                <h1
                                    className="pb-1 text-lg font-medium leading-relaxed text-foreground"
                                    style={{ fontFamily: "var(--font-syne)" }}
                                >
                                    Changes
                                </h1>
                                <p className="text-sm text-foreground/60">
                                    Each version is automatically compared against its previous version on the active branch.
                                </p>
                            </div>
                            <div className="inline-flex items-center gap-2 rounded-xl border border-accent/20 bg-accent/10 px-3 py-2 text-xs font-medium text-accent">
                                <Info className="size-3.5" aria-hidden />
                                FL Studio mixer diff only
                            </div>
                        </div>

                        <RepositoryBranchBar
                            branches={branches}
                            selectedBranchId={selectedBranchId}
                            onBranchChange={handleBranchChange}
                        />
                    </div>

                    <div className="mb-6 rounded-xl border border-foreground/[0.08] bg-foreground/[0.02] p-5">
                        {historyEntries.length === 0 ? (
                            <div className="space-y-2">
                                <h2 className="text-sm font-medium text-foreground">Branch diff timeline</h2>
                                <p className="text-sm text-foreground/60">
                                    This branch does not have any version history yet.
                                </p>
                            </div>
                        ) : (
                            <div className="space-y-4">
                                <p className="text-sm text-foreground/60">
                                    The latest versions are shown first. Each card summarizes mixer changes against the previous branch version.
                                </p>
                            </div>
                        )}
                    </div>

                    {historyEntries.length === 0 ? (
                        <div className="rounded-xl border border-foreground/[0.08] bg-foreground/[0.02] p-6 text-center">
                            <p className="text-sm text-foreground/50">No changes yet</p>
                        </div>
                    ) : (
                        <ul className="flex flex-col gap-3" role="list">
                            {historyEntries.map((entry) => {
                                const version = entry.version;
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
                                                    {version.source_daw && (
                                                        <p className="mt-0.5 text-xs text-foreground/50">
                                                            {version.source_daw}
                                                            {version.source_project_filename ? ` • ${version.source_project_filename}` : ""}
                                                        </p>
                                                    )}

                                                    <div className="mt-3 rounded-xl border border-foreground/[0.08] bg-background/40 p-3">
                                                        {entry.status === "initial" && (
                                                            <p className="text-sm text-foreground/65">
                                                                {entry.status_message || "Initial snapshot on this branch."}
                                                            </p>
                                                        )}

                                                        {entry.status === "unsupported" && (
                                                            <p className="text-sm text-foreground/65">
                                                                {entry.status_message || "Automatic mixer diff unavailable for this version."}
                                                            </p>
                                                        )}

                                                        {entry.status === "compared" && entry.summary && (
                                                            <div className="space-y-3">
                                                                <div className="flex flex-wrap items-center gap-2 text-xs">
                                                                    <span className="rounded-md border border-accent/30 bg-accent/15 px-2.5 py-1 font-medium text-accent">
                                                                        {formatSummaryLine(entry)}
                                                                    </span>
                                                                    <span className="text-foreground/50">
                                                                        Compared to previous branch version
                                                                    </span>
                                                                </div>

                                                                {entry.summary.total_changes === 0 ? (
                                                                    <p className="text-sm text-foreground/65">
                                                                        No mixer changes detected.
                                                                    </p>
                                                                ) : (
                                                                    <ul className="space-y-2">
                                                                        {entry.changes.map((change, index) => (
                                                                            <li
                                                                                key={`${version.id}-${change.type}-${change.slot_index ?? "insert"}-${index}`}
                                                                                className="text-sm text-foreground/72"
                                                                            >
                                                                                {change.message}
                                                                            </li>
                                                                        ))}
                                                                    </ul>
                                                                )}
                                                            </div>
                                                        )}
                                                    </div>
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
