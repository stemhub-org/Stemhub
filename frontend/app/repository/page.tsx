"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
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

export default function RepositoryPage() {
    const router = useRouter();
    const [isAuthenticated, setIsAuthenticated] = useState(false);

    useEffect(() => {
        const token = localStorage.getItem("token");
        if (!token) {
            router.push("/login");
        } else {
            setIsAuthenticated(true);
        }
    }, [router]);

    if (!isAuthenticated) return null;

    return (
        <div className="min-h-screen bg-background text-foreground">
            <RepositoryHeader />

            <div className="p-6 space-y-6">
                <div className="rounded-xl border border-foreground/[0.08] bg-white overflow-hidden">
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
                        <div className="rounded-xl border border-foreground/[0.08] bg-white">
                            <div className="p-8">
                                <RepositoryAudioPlayer />
                            </div>
                        </div>
                        <div className="rounded-xl border border-foreground/[0.08] bg-white p-6">
                            <QuickExport />
                        </div>
                        <div className="rounded-xl border border-foreground/[0.08] bg-white p-6">
                            <RecentChanges />
                        </div>
                    </section>

                    <aside className="w-[28rem] shrink-0 flex flex-col gap-6">
                        <div className="rounded-xl border border-foreground/[0.08] bg-white overflow-hidden">
                            <RepositoryFileList />
                        </div>
                        <div className="rounded-xl border border-foreground/[0.08] bg-white p-6 overflow-hidden">
                            <ContributionActivity />
                        </div>
                        <div className="rounded-xl border border-foreground/[0.08] bg-white p-6 overflow-hidden">
                            <TopContributors />
                        </div>
                    </aside>
                </motion.main>
            </div>
        </div>
    );
}
