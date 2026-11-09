"use client";

import type React from "react";
import Link from "next/link";
import { Suspense } from "react";
import { useRouter, useSearchParams } from "next/navigation";
import { useEffect } from "react";
import { ProjectHeader } from "../components/ProjectHeader";
import { ArrowLeft, Loader2, Info } from "lucide-react";
import { useBranchDiffHistory, useProjectSummary } from "@/lib/queries";
import type { VersionDiffHistoryEntry } from "@/types/project";
import { BranchPicker } from "../components/BranchPicker";
import { Card } from "@/components/ui/Card";
import { ErrorState } from "@/components/ui/ErrorState";

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
        `${entry.summary.slots_changed} effect slot${entry.summary.slots_changed === 1 ? "" : "s"}`,
    ];
    return parts.join(" • ");
}

function FullPageError({ error, onRetry }: { error: unknown; onRetry?: () => void }) {
    return (
        <div className="min-h-screen bg-background text-foreground flex items-center justify-center p-6">
            <ErrorState title="Couldn't load the history" error={error} onRetry={onRetry} className="max-w-md" />
        </div>
    );
}

function ProjectHistoryContent() {
    const searchParams = useSearchParams();
    const projectId = searchParams.get("id");
    if (!projectId) return <FullPageError error={new Error("No project ID provided.")} />;
    return <ProjectHistory projectId={projectId} requestedBranchId={searchParams.get("branch_id") || null} />;
}

function ProjectHistory({ projectId, requestedBranchId }: { projectId: string; requestedBranchId: string | null }) {
    const router = useRouter();
    const summaryQuery = useProjectSummary(projectId, requestedBranchId);
    // The branch actually shown: the requested one, or the first branch when it is missing or gone.
    const selectedBranchId = summaryQuery.data?.branchId ?? "";
    const historyQuery = useBranchDiffHistory(selectedBranchId);

    // Put the branch we fell back to in the URL. Skipped while the previous
    // branch is still shown as placeholder data, or it would undo a branch switch.
    const fellBack = !summaryQuery.isPlaceholderData && selectedBranchId !== "" && selectedBranchId !== requestedBranchId;
    useEffect(() => {
        if (fellBack) router.replace(`/projects/history?id=${projectId}&branch_id=${selectedBranchId}`);
    }, [fellBack, projectId, router, selectedBranchId]);

    const handleBranchChange = (branchId: string) => {
        router.replace(`/projects/history?id=${projectId}&branch_id=${branchId}`);
    };

    if (summaryQuery.isPending || (selectedBranchId && historyQuery.isPending)) {
        return (
            <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                <Loader2 className="size-8 text-accent animate-spin" />
            </div>
        );
    }

    if (summaryQuery.isError || historyQuery.isError) {
        return (
            <FullPageError
                error={summaryQuery.error ?? historyQuery.error}
                onRetry={() => {
                    summaryQuery.refetch();
                    if (selectedBranchId) historyQuery.refetch();
                }}
            />
        );
    }

    const branches = summaryQuery.data.summary.branches;
    const historyEntries = historyQuery.data ?? [];

    return (
        <div
            className="min-h-screen bg-background text-foreground"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <ProjectHeader />
            <div className="p-6 space-y-6">
                <Link
                    href={`/projects?id=${projectId}${selectedBranchId ? `&branch_id=${selectedBranchId}` : ""}`}
                    className="inline-flex items-center gap-2 text-sm text-foreground/70 transition-colors hover:text-foreground"
                >
                    <ArrowLeft className="size-4" aria-hidden />
                    Back to project
                </Link>
                <Card interactive>
                    <div className="mb-6 flex flex-col gap-4">
                        <div className="flex flex-wrap items-center justify-between gap-3">
                            <div>
                                <h1
                                    className="pb-1 text-lg font-medium leading-relaxed text-foreground"
                                    style={{ fontFamily: "var(--font-syne)" }}
                                >
                                    History
                                </h1>
                                <p className="text-sm text-foreground/60">
                                    Each version is automatically compared with the version it was saved from, or with the previous version on this branch when that one is gone.
                                </p>
                            </div>
                            <div className="inline-flex items-center gap-2 rounded-xl border border-accent/20 bg-accent/10 px-3 py-2 text-xs font-medium text-accent">
                                <Info className="size-3.5" aria-hidden />
                                FL Studio mixer diff only
                            </div>
                        </div>

                        <BranchPicker
                            branches={branches}
                            selectedBranchId={selectedBranchId}
                            onBranchChange={handleBranchChange}
                        />
                    </div>

                    <div className="mb-6 rounded-xl border border-foreground/[0.08] bg-foreground/[0.02] p-5">
                        {historyEntries.length === 0 ? (
                            <div className="space-y-2">
                                <h2 className="text-sm font-medium text-foreground">Branch history</h2>
                                <p className="text-sm text-foreground/60">
                                    This branch does not have any versions yet.
                                </p>
                            </div>
                        ) : (
                            <div className="space-y-4">
                                <p className="text-sm text-foreground/60">
                                    The newest versions are shown first. Each card summarizes the mixer changes compared with the version before it.
                                </p>
                            </div>
                        )}
                    </div>

                    {historyEntries.length === 0 ? (
                        <div className="rounded-xl border border-foreground/[0.08] bg-foreground/[0.02] p-6 text-center">
                            <p className="text-sm text-foreground/50">No versions yet</p>
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
                                                    </div>
                                                    <p className="mt-1 text-sm text-foreground/80">
                                                        {version.message || "No message"}
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
                                                                {entry.status_message || "First version on this branch."}
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
                                                                        Compared with the version before it
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
                </Card>
            </div>
        </div>
    );
}

export default function ProjectHistoryPage() {
    return (
        <Suspense
            fallback={
                <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                    <div className="h-6 w-6 border-2 border-border-subtle border-t-accent rounded-full animate-spin" />
                </div>
            }
        >
            <ProjectHistoryContent />
        </Suspense>
    );
}
