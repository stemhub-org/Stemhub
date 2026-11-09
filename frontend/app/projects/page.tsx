"use client";

import type React from "react";
import { useState, Suspense } from "react";
import { useSearchParams } from "next/navigation";
import { motion, AnimatePresence } from "framer-motion";
import { Loader2, Settings, FileText } from "lucide-react";
import Sidebar from "@/components/Sidebar";
import { ProjectHeader } from "./components/ProjectHeader";
import { ProjectPageHeader } from "./components/ProjectPageHeader";
import { BranchPicker } from "./components/BranchPicker";
import { ProjectAudioPlayer } from "./components/ProjectAudioPlayer";
import { PreviewDownload } from "./components/PreviewDownload";
import { RecentVersions } from "./components/RecentVersions";
import { ContributionActivity } from "./components/ContributionActivity";
import { TopContributors } from "./components/TopContributors";
import { ProjectFileList } from "./components/ProjectFileList";
import { useQueryClient } from "@tanstack/react-query";
import { apiFetch, errorMessage } from "@/lib/api";
import {
    queryKeys,
    useCurrentUser,
    useProjectActivity,
    useProjectSummary,
    useTopContributors,
    useVersionAssets,
} from "@/lib/queries";
import { ProjectSettings } from "./components/ProjectSettings";
import { Card } from "@/components/ui/Card";
import { Badge } from "@/components/ui/Badge";
import { useToast } from "@/components/ToastProvider";
import { ErrorState } from "@/components/ui/ErrorState";

function formatFileCount(hasVersion: boolean, count: number, isLoading: boolean, loadError: string | null): string {
    if (!hasVersion) return "No versions yet";
    if (isLoading) return "Loading files…";
    if (loadError) return "File list unavailable";
    return `${count} audio & MIDI ${count === 1 ? "file" : "files"}`;
}

function FullPageError({ error, onRetry }: { error: unknown; onRetry?: () => void }) {
    return (
        <div className="min-h-screen bg-background text-foreground flex items-center justify-center p-6">
            <ErrorState title="Couldn't load this project" error={error} onRetry={onRetry} className="max-w-md" />
        </div>
    );
}

function ProjectPageContent() {
    const projectId = useSearchParams().get("id");
    if (!projectId) {
        return <FullPageError error={new Error("No project ID provided. Add ?id=<project-uuid> to the URL.")} />;
    }
    // Keyed so that opening another project starts from a clean state (tab, branch).
    return <ProjectOverview key={projectId} projectId={projectId} />;
}

function ProjectOverview({ projectId }: { projectId: string }) {
    const [sidebarOpen, setSidebarOpen] = useState(false);
    const [activeTab, setActiveTab] = useState<"Project" | "Settings">("Project");
    // null until the user picks a branch: the summary then shows the first branch.
    const [requestedBranchId, setRequestedBranchId] = useState<string | null>(null);
    const toast = useToast();
    const queryClient = useQueryClient();

    const summaryQuery = useProjectSummary(projectId, requestedBranchId);
    const { data: activity } = useProjectActivity(projectId);
    const { data: contributors } = useTopContributors(projectId);
    const { data: currentUser } = useCurrentUser();
    // The latest version's audio & MIDI files, read from its manifest. Loaded
    // separately so a failure here never blocks the rest of the project overview.
    const assetsQuery = useVersionAssets(summaryQuery.data?.summary.latest_version_id ?? null);

    const refreshProject = () => queryClient.invalidateQueries({ queryKey: queryKeys.project(projectId) });

    const handleDeleteBranch = async (branchId: string) => {
        try {
            await apiFetch(`/branches/${branchId}`, { method: "DELETE" });
            if (branchId === summaryQuery.data?.branchId) setRequestedBranchId(null);
            await refreshProject();
        } catch (err) {
            toast.error(errorMessage(err, "Failed to delete branch"));
        }
    };

    const handleCreateBranch = async (branchName: string) => {
        try {
            const createdBranch = await apiFetch<{ id: string }>(`/projects/${projectId}/branches/`, {
                method: "POST",
                body: JSON.stringify({ name: branchName }),
            });
            setRequestedBranchId(createdBranch.id);
            void refreshProject();
        } catch (err) {
            toast.error(errorMessage(err, "Failed to create branch"));
            throw err;
        }
    };

    if (summaryQuery.isPending) {
        return (
            <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                <div className="flex flex-col items-center gap-3">
                    <Loader2 className="size-8 text-accent animate-spin" />
                    <p className="text-sm text-foreground/60">Loading project…</p>
                </div>
            </div>
        );
    }

    if (summaryQuery.isError) {
        return <FullPageError error={summaryQuery.error} onRetry={() => summaryQuery.refetch()} />;
    }

    const { summary, branchId: selectedBranchId } = summaryQuery.data;
    const isRefreshing = summaryQuery.isFetching;
    const currentUserId = currentUser?.id ?? null;
    const selectedBranchName = summary.branches.find((branch) => branch.id === selectedBranchId)?.name || "main";
    const latestVersion = summary.recent_versions?.[0];
    const hasVersion = Boolean(summary.latest_version_id);
    // While the summary refreshes (e.g. after a branch switch) the latest version
    // may change too, so the file list shows as loading rather than stale.
    const assetsLoading = hasVersion && (isRefreshing || assetsQuery.isPending);
    const assets = assetsLoading ? [] : (assetsQuery.data ?? []);
    const assetsError =
        assetsLoading || !assetsQuery.isError ? null : errorMessage(assetsQuery.error, "Failed to load the file list");

    return (
        <div
            className="min-h-screen bg-background text-foreground"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <ProjectHeader
                onToggleSidebar={() => setSidebarOpen((prev) => !prev)}
                sidebarOpen={sidebarOpen}
                userAvatarUrl={currentUser?.avatar_url ?? null}
                username={currentUser?.username || "Producer"}
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

            <div className="relative z-0 p-6 space-y-6">
                <Card interactive padding="none" className="overflow-hidden">
                    <ProjectPageHeader
                        ownerUsername={summary.project.owner.username}
                        projectName={summary.project.name}
                        branchName={selectedBranchName}
                        description={summary.project.description || ""}
                        projectId={projectId}
                        onUploadSuccess={() => void refreshProject()}
                    />
                </Card>

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
                                <div className="flex items-center gap-3">
                                    <BranchPicker
                                        branches={summary.branches}
                                        selectedBranchId={selectedBranchId}
                                        onBranchChange={setRequestedBranchId}
                                        isOwner={currentUserId === summary.project.owner.id}
                                        onDelete={handleDeleteBranch}
                                        onCreate={currentUserId === summary.project.owner.id ? handleCreateBranch : undefined}
                                    />
                                    {isRefreshing && (
                                        <Loader2 className="size-4 text-accent animate-spin shrink-0" aria-label="Refreshing…" />
                                    )}
                                </div>
                                <Card interactive padding="none">
                                    <div className="p-8">
                                        <ProjectAudioPlayer
                                            projectId={projectId}
                                            hasPreview={summary.has_preview}
                                        />
                                    </div>
                                </Card>
                                <Card className="flex flex-wrap items-center gap-3">
                                    <Badge tone={assets.length > 0 ? "success" : "neutral"}>
                                        {formatFileCount(hasVersion, assets.length, assetsLoading, assetsError)}
                                    </Badge>
                                    {latestVersion?.source_daw && (
                                        <Badge tone="accent">{latestVersion.source_daw}</Badge>
                                    )}
                                    {latestVersion?.source_project_filename && (
                                        <span className="text-sm text-foreground-muted truncate">
                                            {latestVersion.source_project_filename}
                                        </span>
                                    )}
                                </Card>
                                <Card interactive padding="none">
                                    <ProjectFileList
                                        assets={assets}
                                        hasVersion={hasVersion}
                                        isLoading={assetsLoading}
                                        loadError={assetsError}
                                    />
                                </Card>
                                <Card interactive>
                                    <PreviewDownload
                                        projectId={projectId}
                                        projectName={summary.project.name}
                                        hasPreview={summary.has_preview}
                                    />
                                </Card>
                                <Card interactive>
                                    <RecentVersions
                                        versions={summary.recent_versions}
                                        projectId={projectId}
                                        branchId={selectedBranchId || undefined}
                                    />
                                </Card>
                            </section>

                            <aside className="w-[28rem] shrink-0 flex flex-col gap-6">
                                <Card interactive className="overflow-hidden">
                                    <ContributionActivity
                                        dailyActivity={activity?.daily_activity || []}
                                        totalVersions={activity?.total_versions || 0}
                                        totalContributors={activity?.total_contributors || 0}
                                    />
                                </Card>
                                <Card interactive className="overflow-hidden">
                                    <TopContributors
                                        contributors={contributors?.contributors || []}
                                    />
                                </Card>
                            </aside>
                        </motion.main>
                    </>
                )}

                {activeTab === "Settings" && currentUserId && summary.project.owner.id === currentUserId && (
                    <Card interactive padding="none">
                        <motion.div
                            initial={{ opacity: 0, y: 10 }}
                            animate={{ opacity: 1, y: 0 }}
                            transition={{ duration: 0.3 }}
                            className="py-6"
                        >
                            <ProjectSettings projectId={projectId} ownerId={summary.project.owner.id} currentUserId={currentUserId} />
                        </motion.div>
                    </Card>
                )}
            </div>
        </div>
    );
}

export default function ProjectPage() {
    return (
        <Suspense
            fallback={
                <div className="min-h-screen bg-background text-foreground flex items-center justify-center">
                    <div className="h-6 w-6 border-2 border-border-subtle border-t-accent rounded-full animate-spin" />
                </div>
            }
        >
            <ProjectPageContent />
        </Suspense>
    );
}
