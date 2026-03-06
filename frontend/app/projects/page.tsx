"use client";

import type React from "react";
import { useState } from "react";
import { useTheme } from "next-themes";
import { motion, AnimatePresence } from "framer-motion";
import Sidebar from "@/components/Sidebar";
import { RepositoryHeader } from "./components/RepositoryHeader";
import { RepositoryPageHeader } from "./components/RepositoryPageHeader";
import { RepositoryBranchBar } from "./components/RepositoryBranchBar";
import { RepositoryFileList } from "./components/RepositoryFileList";
import { RepositoryAudioPlayer } from "./components/RepositoryAudioPlayer";
import { QuickExport } from "./components/QuickExport";
import { RecentChanges } from "./components/RecentChanges";
import { ContributionActivity } from "./components/ContributionActivity";
import { TopContributors } from "./components/TopContributors";

const cardHoverDark =
    "hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.08)]";

export default function RepositoryPage() {
    const { resolvedTheme } = useTheme();
    const [sidebarOpen, setSidebarOpen] = useState(false);
    const isDark = resolvedTheme === "dark";
    const cardBase = isDark
        ? "rounded-xl bg-background-tertiary border border-border-subtle transition-all duration-300"
        : "rounded-xl bg-background-secondary border border-border-subtle transition-all duration-300";
    const cardClass = `${cardBase} ${isDark ? cardHoverDark : ""}`;

    return (
        <div
            className="min-h-screen bg-background text-foreground"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <RepositoryHeader
                onToggleSidebar={() => setSidebarOpen((prev) => !prev)}
                sidebarOpen={sidebarOpen}
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
                    <RepositoryPageHeader />
                </div>

                <div className="flex flex-wrap items-center gap-4">
                    <RepositoryBranchBar />
                </div>

                <motion.main
                    className="flex items-start gap-6"
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    transition={{ duration: 0.3 }}
                >
                    <section className="flex min-w-0 flex-1 flex-col gap-6 self-start">
                        <div className={cardClass}>
                            <div className="p-8">
                                <RepositoryAudioPlayer />
                            </div>
                        </div>
                        <div className={`${cardClass} p-6`}>
                            <QuickExport />
                        </div>
                        <div className={`${cardClass} p-6`}>
                            <RecentChanges />
                        </div>
                    </section>

                    <aside className="w-[28rem] shrink-0 flex flex-col gap-6">
                        <div className={`${cardClass} overflow-hidden`}>
                            <RepositoryFileList />
                        </div>
                        <div className={`${cardClass} p-6 overflow-hidden`}>
                            <ContributionActivity />
                        </div>
                        <div className={`${cardClass} p-6 overflow-hidden`}>
                            <TopContributors />
                        </div>
                    </aside>
                </motion.main>
            </div>
        </div>
    );
}
