"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import { motion } from "framer-motion";
import Link from "next/link";

export default function DashboardPage() {
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
        <div className="min-h-screen bg-background text-foreground p-8">
            <motion.div
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.5 }}
            >
                <header className="flex justify-between items-center mb-12">
                    <Link href="/">
                        <span
                            className="text-2xl font-medium tracking-tight"
                            style={{ fontFamily: "var(--font-syne)" }}
                        >
                            stemhub<span className="text-accent">.</span>
                        </span>
                    </Link>
                    <button
                        onClick={() => {
                            localStorage.removeItem("token");
                            router.push("/login");
                        }}
                        className="rounded-xl bg-foreground/10 px-6 py-2 text-sm font-light transition-colors hover:bg-foreground/20"
                    >
                        Déconnexion
                    </button>
                </header>

                <main>
                    <h1
                        className="text-4xl font-extralight mb-6"
                        style={{ fontFamily: "var(--font-syne)" }}
                    >
                        Dashboard
                    </h1>
                    <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/30 p-8 backdrop-blur-xl">
                        <p className="font-light text-foreground/70">
                            Connexion réussie ! Bienvenue dans votre nouvel espace de travail StemHub.
                        </p>
                    </div>
                </main>
            </motion.div>
        </div>
    );
}
