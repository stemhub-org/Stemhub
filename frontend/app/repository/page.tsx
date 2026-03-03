"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import { motion } from "framer-motion";
import { RepositoryHeader } from "./components/RepositoryHeader";
import { RepositoryPageHeader } from "./components/RepositoryPageHeader";
import { RepositoryFileList } from "./components/RepositoryFileList";

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

                <motion.main
                    className="flex gap-6"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ duration: 0.3 }}
            >
                <section className="min-w-0 flex-1 rounded-xl border border-foreground/[0.08] bg-white overflow-hidden">
                    <RepositoryFileList />
                </section>

                <aside className="w-[28rem] shrink-0 flex flex-col gap-6">
                    <div className="rounded-xl border border-foreground/[0.08] bg-white p-6 min-h-[280px] flex items-center justify-center text-foreground/70">
                        Lecteur audio & stats
                    </div>
                </aside>
                </motion.main>
            </div>
        </div>
    );
}
