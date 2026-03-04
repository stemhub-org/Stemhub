"use client";

import type React from "react";
import { useTheme } from "next-themes";
import { motion } from "framer-motion";
import { RepositoryHeader } from "./components/RepositoryHeader";
import { RepositoryPageHeader } from "./components/RepositoryPageHeader";
import { RepositoryBranchBar } from "./components/RepositoryBranchBar";
import { RepositoryFileList } from "./components/RepositoryFileList";
import { RepositoryAudioPlayer } from "./components/RepositoryAudioPlayer";
import { QuickExport } from "./components/QuickExport";
import { RecentChanges } from "./components/RecentChanges";
import { ContributionActivity } from "./components/ContributionActivity";
import { TopContributors } from "./components/TopContributors";

const cardBase =
    "rounded-xl bg-background-secondary border border-border-subtle transition-all duration-300";
const cardHoverDark =
    "hover:border-accent-blue/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent-blue/5 hover:shadow-[0_0_20px_rgba(62,99,221,0.05)]";

export default function RepositoryPage() {
    const { resolvedTheme } = useTheme();
    const isDark = resolvedTheme === "dark";
    const cardClass = `${cardBase} ${isDark ? cardHoverDark : ""}`;

    return (
        <div
            className="min-h-screen bg-background text-foreground"
            style={{ "--accent": "#3E63DD" } as React.CSSProperties}
        >
            <RepositoryHeader />

            <div className="p-6 space-y-6">
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
