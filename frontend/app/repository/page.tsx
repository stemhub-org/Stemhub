"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import { motion } from "framer-motion";
import { RepositoryHeader } from "./components/RepositoryHeader";

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

            <motion.main
                className="flex gap-6 p-6"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ duration: 0.3 }}
            >
                <section className="flex-1 min-w-0 rounded-xl border border-foreground/[0.08] bg-white overflow-hidden">
                    <div className="p-6 min-h-[400px] flex items-center justify-center text-foreground/70">
                        Contenu du repository (liste de fichiers)
                    </div>
                </section>

                <aside className="w-80 shrink-0 flex flex-col gap-6">
                    <div className="rounded-xl border border-foreground/[0.08] bg-white p-6 min-h-[280px] flex items-center justify-center text-foreground/70">
                        Lecteur audio & stats
                    </div>
                </aside>
            </motion.main>
        </div>
    );
}
