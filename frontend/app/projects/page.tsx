"use client";

import type React from "react";
import { useState, useEffect, useCallback, Suspense } from "react";
import { useTheme } from "next-themes";
import { useSearchParams } from "next/navigation";
import { motion, AnimatePresence } from "framer-motion";
import { Loader2, Settings, FileText } from "lucide-react";
import Sidebar from "@/components/Sidebar";
import { RepositoryHeader } from "./components/RepositoryHeader";
import { RepositoryPageHeader } from "./components/RepositoryPageHeader";
import { RepositoryBranchBar } from "./components/RepositoryBranchBar";
import { RepositoryAudioPlayer } from "./components/RepositoryAudioPlayer";
import { QuickExport } from "./components/QuickExport";
import { RecentChanges } from "./components/RecentChanges";
import { ContributionActivity } from "./components/ContributionActivity";
import { TopContributors } from "./components/TopContributors";
import { authFetch } from "@/lib/api";
import type {
    ProjectSummaryResponse,
    ActivityStatsResponse,
    TopContributorsResponse,
} from "@/types/project";
import { ProjectSettings } from "./components/ProjectSettings";

const cardHoverDark =
    "hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary dark:hover:from-background-tertiary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.08)]";

function RepositoryPageContent() {
    const { resolvedTheme } = useTheme();
    const searchParams = useSearchParams();
    const projectId = searchParams.get("id");
    const [sidebarOpen, setSidebarOpen] = useState(false);
    const isDark = resolvedTheme === "dark";
    const cardBase =
        "rounded-xl bg-background-secondary dark:bg-background-tertiary border border-border-subtle transition-all duration-300";
    const cardClass = `${cardBase} ${isDark ? cardHoverDark : ""}`;

    // ── Data state ──
    const [summary, setSummary] = useState<ProjectSummaryResponse | null>(null);
    const [activity, setActivity] = useState<ActivityStatsResponse | null>(null);
    const [contributors, setContributors] = useState<TopContributorsResponse | null>(null);
    const [isLoading, setIsLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [selectedBranchId, setSelectedBranchId] = useState<string>("");
    const [currentUserId, setCurrentUserId] = useState<string | null>(null);
    const [currentUserAvatar, setCurrentUserAvatar] = useState<string | null>(null);
    const [currentUsername, setCurrentUsername] = useState<string | null>(null);
    const [activeTab, setActiveTab] = useState<"Project" | "Settings">("Project");

    const fetchData = useCallback(async (projectId: string, branchId?: string) => {
        setIsLoading(true);
        setError(null);
        try {
            const summaryPath = branchId
                ? `/projects/${projectId}/summary?branch_id=${branchId}`
                : `/projects/${projectId}/summary`;
            const [summaryData, activityData, contributorsData, userData] = await Promise.all([
                authFetch<ProjectSummaryResponse>(summaryPath),
                authFetch<ActivityStatsResponse>(`/projects/${projectId}/stats/activity`),
                authFetch<TopContributorsResponse>(`/projects/${projectId}/stats/top-contributors`),
                authFetch<any>(`/auth/me`),
            ]);
            setSummary(summaryData);
            setActivity(activityData);
            setContributors(contributorsData);
            setCurrentUserId(userData.id);
            setCurrentUserAvatar(userData.avatar_url || null);
            setCurrentUsername(userData.username || null);
            setSelectedBranchId((current) => {
                if (summaryData.branches.length === 0) return "";
                if (current && summaryData.branches.some((branch) => branch.id === current)) return current;
                if (branchId && summaryData.branches.some((branch) => branch.id === branchId)) return branchId;
                return summaryData.branches[0].id;
            });
        } catch (err) {
            setError(err instanceof Error ? err.message : "Failed to load project data");
        } finally {
            setIsLoading(false);
        }
    }, []);

    const handleDeleteBranch = async (branchId: string) => {
        try {
            await authFetch(`/branches/${branchId}`, { method: "DELETE" });
            if (projectId) fetchData(projectId);
        } catch (err) {
            alert(err instanceof Error ? err.message : "Failed to delete branch");
        }
    };

    const handleCreateBranch = async (branchName: string) => {
        if (!projectId) return;
        try {
            const createdBranch = await authFetch<{ id: string }>(`/projects/${projectId}/branches/`, {
                method: "POST",
                body: JSON.stringify({ name: branchName }),
            });
            setSelectedBranchId(createdBranch.id);
            fetchData(projectId, createdBranch.id);
        } catch (err) {
            alert(err instanceof Error ? err.message : "Failed to create branch");
            throw err;
        }
    };

    useEffect(() => {
        if (projectId) {
            fetchData(projectId, selectedBranchId || undefined);
        } else {
            setIsLoading(false);
            setError("No project ID provided. Add ?id=<project-uuid> to the URL.");
        }
    }, [fetchData, projectId, selectedBranchId]);

    if (isLoading) {
        return (
            <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                <div className="flex flex-col items-center gap-3">
                    <Loader2 className="size-8 text-accent animate-spin" />
                    <p className="text-sm text-foreground/60">Loading project…</p>
                </div>
            </div>
        );
    }

    if (error) {
        return (
            <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                <div className="flex flex-col items-center gap-3 text-center max-w-md">
                    <p className="text-lg font-medium text-red-500">Error</p>
                    <p className="text-sm text-foreground/60">{error}</p>
                </div>
            </div>
        );
    }

    if (!summary) return null;
    const selectedBranchName = summary.branches.find((branch) => branch.id === selectedBranchId)?.name || "main";
    const latestVersion = summary.recent_versions?.[0];

    return (
        <div
            className="min-h-screen bg-background text-foreground"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <RepositoryHeader
                onToggleSidebar={() => setSidebarOpen((prev) => !prev)}
                sidebarOpen={sidebarOpen}
                userAvatarUrl={currentUserAvatar}
                username={currentUsername || "Producer"}
            />

            <AnimatePresence>
                {sidebarOpen && (
                    <>
                        <motion.div
                            key="backdrop"
                            initial={{ opacity: 0 }}
                            animate={{ opacity: 1 }}
                            exit={{ opacity: 0 }}
                            transition={{ duration: 0.25, ease: "easeInOut" }}
                            className="fixed inset-0 z-30 bg-black/40 lg:bg-black/30 backdrop-blur-sm"
                            onClick={() => setSidebarOpen(false)}
                        />
                        <motion.div
                            key="sidebar"
                            initial={{ x: "-100%" }}
                            animate={{ x: 0 }}
                            exit={{ x: "-100%" }}
                            transition={{ type: "spring", damping: 25, stiffness: 200 }}
                            className="fixed inset-y-0 left-0 z-40 w-64 shadow-xl"
                        >
                            <Sidebar isOpen onToggleSidebar={() => setSidebarOpen(false)} />
                        </motion.div>
                    </>
                )}
            </AnimatePresence>

            <div className="relative z-10 p-6 space-y-6">
                <div className={`${cardClass} overflow-hidden`}>
                    <RepositoryPageHeader
                        ownerUsername={summary.project.owner.username}
                        projectName={summary.project.name}
                        branchName={selectedBranchName}
                        description={summary.project.description || ""}
                        projectId={projectId || undefined}
                        onUploadSuccess={() => {
                            if (projectId) {
                                fetchData(projectId, selectedBranchId || undefined);
                            }
                        }}
                    />
                </div>

                <div className="flex flex-wrap items-center justify-end gap-4 mt-4 mb-6 px-2">
                    <div className="flex items-center gap-4 border-b border-border-subtle flex-1 justify-end">
                        <button
                            type="button"
                            onClick={() => setActiveTab("Project")}
                            className={`flex items-center gap-2 pb-3 px-1 text-sm font-medium border-b-2 transition-colors ${
                                activeTab === "Project"
                                    ? "border-accent text-foreground"
                                    : "border-transparent text-foreground/60 hover:text-foreground hover:border-border-subtle"
                            }`}
                        >
                            <FileText className="size-4" />
                            Project
                        </button>
                        {currentUserId && summary.project.owner.id === currentUserId && (
                        <button
                            type="button"
                            onClick={() => setActiveTab("Settings")}
                            className={`flex items-center gap-2 pb-3 px-1 text-sm font-medium border-b-2 transition-colors ${
                                activeTab === "Settings"
                                    ? "border-accent text-foreground"
                                    : "border-transparent text-foreground/60 hover:text-foreground hover:border-border-subtle"
                            }`}
                        >
                            <Settings className="size-4" />
                            Settings
                        </button>
                        )}
                    </div>
                </div>

                {activeTab === "Project" && (
                    <>
                        <motion.main
                            className="flex items-start gap-6"
                            initial={{ opacity: 0 }}
                            animate={{ opacity: 1 }}
                            transition={{ duration: 0.3 }}
                        >
                            <section className="flex min-w-0 flex-1 flex-col gap-6 self-start">
                                <RepositoryBranchBar
                                    branches={summary.branches}
                                    selectedBranchId={selectedBranchId}
                                    onBranchChange={setSelectedBranchId}
                                    isOwner={currentUserId === summary.project.owner.id}
                                    onDelete={handleDeleteBranch}
                                    onCreate={currentUserId === summary.project.owner.id ? handleCreateBranch : undefined}
                                />
                                <div className={cardClass}>
                                    <div className="p-8">
                                        <RepositoryAudioPlayer
                                            projectId={projectId}
                                            hasPreview={summary.has_preview}
                                        />
                                    </div>
                                </div>
                                <div className={`${cardClass} p-6`}>
                                    <QuickExport
                                        projectId={projectId}
                                        projectName={summary.project.name}
                                        branchName={selectedBranchName}
                                        latestVersionId={summary.latest_version_id}
                                        hasPreview={summary.has_preview}
                                        hasArtifact={Boolean(latestVersion?.has_artifact)}
                                    />
                                </div>
                                <div className={`${cardClass} p-6`}>
                                    <RecentChanges versions={summary.recent_versions} projectId={projectId} />
                                </div>
                            </section>

                            <aside className="w-[28rem] shrink-0 flex flex-col gap-6">
                                <div className={`${cardClass} p-6 overflow-hidden`}>
                                    <ContributionActivity
                                        dailyActivity={activity?.daily_activity || []}
                                        totalCommits={activity?.total_commits || 0}
                                        totalContributors={activity?.total_contributors || 0}
                                    />
                                </div>
                                <div className={`${cardClass} p-6 overflow-hidden`}>
                                    <TopContributors
                                        contributors={contributors?.contributors || []}
                                    />
                                </div>
                            </aside>
                        </motion.main>
                    </>
                )}

                {activeTab === "Settings" && currentUserId && summary.project.owner.id === currentUserId && (
                    <motion.div
                        initial={{ opacity: 0, y: 10 }}
                        animate={{ opacity: 1, y: 0 }}
                        transition={{ duration: 0.3 }}
                        className={`${cardClass} py-6`}
                    >
                        <ProjectSettings projectId={projectId!} ownerId={summary.project.owner.id} currentUserId={currentUserId} />
                    </motion.div>
                )}
            </div>
        </div>
    );
}

export default function RepositoryPage() {
    return (
        <Suspense
            fallback={
                <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                    <div className="h-6 w-6 border-2 border-border-subtle border-t-accent rounded-full animate-spin" />
                </div>
            }
        >
            <RepositoryPageContent />
        </Suspense>
    );
}
