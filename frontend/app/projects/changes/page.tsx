"use client";

import type React from "react";
import Link from "next/link";
import { Suspense } from "react";
import { useRouter, useSearchParams } from "next/navigation";
import { useTheme } from "next-themes";
import { useState, useEffect } from "react";
import { RepositoryHeader } from "../components/RepositoryHeader";
import { ArrowLeft, Loader2, GitCompareArrows, Info } from "lucide-react";
import { authFetch } from "@/lib/api";
import type { ProjectSummaryResponse, VersionWithAuthor } from "@/types/project";
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

function buildVersionLabel(version: VersionWithAuthor): string {
    const message = version.commit_message || "No message";
    const timestamp = formatTimeAgo(version.created_at);
    const filename = version.source_project_filename ? ` • ${version.source_project_filename}` : "";
    return `${message} • ${timestamp}${filename}`;
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
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [selectedBranchId, setSelectedBranchId] = useState(branchIdFromQuery);
    const [baseVersionId, setBaseVersionId] = useState("");
    const [targetVersionId, setTargetVersionId] = useState("");
    const [compareLaunchMessage, setCompareLaunchMessage] = useState<string | null>(null);

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
                const summaryPath = selectedBranchId
                    ? `/projects/${projectId}/summary?branch_id=${selectedBranchId}`
                    : `/projects/${projectId}/summary`;
                const data = await authFetch<ProjectSummaryResponse>(summaryPath);

                if (cancelled) return;

                if (!selectedBranchId && data.branches.length > 0) {
                    const fallbackBranchId = data.branches[0].id;
                    setSelectedBranchId(fallbackBranchId);
                    router.replace(`/projects/changes?id=${projectId}&branch_id=${fallbackBranchId}`);
                    return;
                }

                setSummary(data);
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

    const versions = summary?.recent_versions || [];
    const branches = summary?.branches || [];
    const comparableVersions = versions.filter(
        (version) => version.has_artifact && version.source_daw === "FL Studio"
    );

    useEffect(() => {
        setCompareLaunchMessage(null);
        if (comparableVersions.length === 0) {
            setBaseVersionId("");
            setTargetVersionId("");
            return;
        }

        const versionIds = new Set(comparableVersions.map((version) => version.id));
        const nextBaseId = versionIds.has(baseVersionId) ? baseVersionId : comparableVersions[0].id;
        let nextTargetId = versionIds.has(targetVersionId) ? targetVersionId : "";

        if (!nextTargetId || nextTargetId === nextBaseId) {
            nextTargetId = comparableVersions.find((version) => version.id !== nextBaseId)?.id || "";
        }

        if (nextBaseId !== baseVersionId) {
            setBaseVersionId(nextBaseId);
        }
        if (nextTargetId !== targetVersionId) {
            setTargetVersionId(nextTargetId);
        }
    }, [baseVersionId, comparableVersions, targetVersionId]);

    const handleBranchChange = (branchId: string) => {
        setCompareLaunchMessage(null);
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
                                    Select a branch and two FL Studio versions to prepare a mixer diff.
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
                        {comparableVersions.length < 2 ? (
                            <div className="space-y-2">
                                <h2 className="text-sm font-medium text-foreground">Compare setup</h2>
                                <p className="text-sm text-foreground/60">
                                    This branch needs at least two FL Studio versions with artifacts before compare can be launched.
                                </p>
                            </div>
                        ) : (
                            <div className="space-y-4">
                                <div className="grid gap-4 lg:grid-cols-[1fr_1fr_auto] lg:items-end">
                                    <label className="flex flex-col gap-2 text-sm">
                                        <span className="font-medium text-foreground">Base version</span>
                                        <select
                                            value={baseVersionId}
                                            onChange={(event) => {
                                                setBaseVersionId(event.target.value);
                                                setCompareLaunchMessage(null);
                                            }}
                                            className="rounded-xl border border-border-subtle bg-background-secondary dark:bg-background-tertiary px-4 py-3 text-sm text-foreground outline-none transition-colors focus:border-accent"
                                        >
                                            {comparableVersions.map((version) => (
                                                <option key={version.id} value={version.id}>
                                                    {buildVersionLabel(version)}
                                                </option>
                                            ))}
                                        </select>
                                    </label>

                                    <label className="flex flex-col gap-2 text-sm">
                                        <span className="font-medium text-foreground">Target version</span>
                                        <select
                                            value={targetVersionId}
                                            onChange={(event) => {
                                                setTargetVersionId(event.target.value);
                                                setCompareLaunchMessage(null);
                                            }}
                                            className="rounded-xl border border-border-subtle bg-background-secondary dark:bg-background-tertiary px-4 py-3 text-sm text-foreground outline-none transition-colors focus:border-accent"
                                        >
                                            {comparableVersions.map((version) => (
                                                <option
                                                    key={version.id}
                                                    value={version.id}
                                                    disabled={version.id === baseVersionId}
                                                >
                                                    {buildVersionLabel(version)}
                                                </option>
                                            ))}
                                        </select>
                                    </label>

                                    <button
                                        type="button"
                                        onClick={() => {
                                            setCompareLaunchMessage(
                                                "Compare request is configured. Result fetching and rendering lands in the next iteration."
                                            );
                                        }}
                                        disabled={!baseVersionId || !targetVersionId || baseVersionId === targetVersionId}
                                        className="inline-flex h-[50px] items-center justify-center gap-2 rounded-xl bg-accent px-5 text-sm font-medium text-white transition-opacity hover:opacity-90 disabled:cursor-not-allowed disabled:opacity-50"
                                    >
                                        <GitCompareArrows className="size-4" aria-hidden />
                                        Compare versions
                                    </button>
                                </div>

                                {compareLaunchMessage && (
                                    <p className="text-sm text-foreground/60">{compareLaunchMessage}</p>
                                )}
                            </div>
                        )}
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
