"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import { motion } from "framer-motion";
import Link from "next/link";

export default function DashboardPage() {
    const router = useRouter();
    const [user, setUser] = useState<any>(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        const fetchUser = async () => {
            const token = localStorage.getItem("token");
            if (!token) {
                router.push("/login");
                return;
            }

            try {
                const response = await fetch("http://localhost:8000/auth/me", {
                    headers: {
                        "Authorization": `Bearer ${token}`
                    }
                });

                if (!response.ok) {
                    throw new Error("Session expirée");
                }

                const data = await response.json();
                setUser(data);
            } catch (err) {
                localStorage.removeItem("token");
                router.push("/login");
            } finally {
                setLoading(false);
            }
        };

        fetchUser();
    }, [router]);

    if (loading) {
        return (
            <div className="flex min-h-screen items-center justify-center bg-background">
                <div className="h-8 w-8 animate-spin rounded-full border-2 border-accent border-t-transparent" />
            </div>
        );
    }

    return (
        <div className="min-h-screen bg-background text-foreground selection:bg-accent/30">
            <div className="pointer-events-none fixed top-0 left-0 h-full w-full opacity-40">
                <div className="absolute top-[-10%] left-[-10%] h-[500px] w-[500px] rounded-full bg-accent/20 blur-[120px]" />
                <div className="absolute bottom-[-10%] right-[-10%] h-[500px] w-[500px] rounded-full bg-blue-500/10 blur-[120px]" />
            </div>

            <div className="relative z-10 mx-auto max-w-6xl px-6 py-10">
                <header className="mb-16 flex items-center justify-between">
                    <Link href="/" className="group flex items-center gap-2">
                        <span className="text-2xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>
                            stemhub<span className="text-accent group-hover:animate-pulse">.</span>
                        </span>
                    </Link>

                    <div className="flex items-center gap-6">
                        <div className="flex items-center gap-3 rounded-full border border-foreground/[0.08] bg-background-secondary/50 px-4 py-2 backdrop-blur-md">
                            <div className="h-8 w-8 overflow-hidden rounded-full bg-accent/20 flex items-center justify-center text-[10px] font-medium text-accent">
                                {user?.avatar_url ? (
                                    <img src={user.avatar_url} alt={user.username} className="h-full w-full object-cover" />
                                ) : (
                                    user?.username?.charAt(0).toUpperCase()
                                )}
                            </div>
                            <span className="text-sm font-light text-foreground/80">{user?.username}</span>
                        </div>
                        <button
                            onClick={() => {
                                localStorage.removeItem("token");
                                router.push("/login");
                            }}
                            className="text-sm font-light text-foreground/40 transition-colors hover:text-red-400"
                        >
                            Déconnexion
                        </button>
                    </div>
                </header>

                <main className="grid gap-8 md:grid-cols-12">
                    <div className="md:col-span-12">
                        <motion.h1
                            initial={{ opacity: 0, x: -20 }}
                            animate={{ opacity: 1, x: 0 }}
                            className="text-5xl font-extralight tracking-tight mb-2"
                            style={{ fontFamily: "var(--font-syne)" }}
                        >
                            Bienvenue, {user?.username}
                        </motion.h1>
                        <p className="text-foreground/50 font-light" style={{ fontFamily: "var(--font-jakarta)" }}>
                            Gérez votre profil et vos projets musicaux depuis votre centre de contrôle.
                        </p>
                    </div>

                    <div className="md:col-span-4">
                        <motion.div
                            initial={{ opacity: 0, y: 20 }}
                            animate={{ opacity: 1, y: 0 }}
                            transition={{ delay: 0.1 }}
                            className="flex flex-col gap-6 rounded-3xl border border-foreground/[0.08] bg-background-secondary/30 p-8 backdrop-blur-xl"
                        >
                            <div className="flex flex-col items-center gap-4 mb-4">
                                <div className="h-24 w-24 rounded-[2rem] bg-accent/10 p-1 ring-1 ring-foreground/[0.08]">
                                    <div className="h-full w-full overflow-hidden rounded-[1.8rem] bg-background flex items-center justify-center">
                                        {user?.avatar_url ? (
                                            <img src={user.avatar_url} alt={user.username} className="h-full w-full object-cover" />
                                        ) : (
                                            <span className="text-3xl font-light text-accent uppercase">
                                                {user?.username?.charAt(0)}
                                            </span>
                                        )}
                                    </div>
                                </div>
                                <div className="text-center">
                                    <h2 className="text-xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>
                                        {user?.username}
                                    </h2>
                                    <p className="text-xs font-light text-accent/70 uppercase tracking-widest mt-1">
                                        Membre Producteur
                                    </p>
                                </div>
                            </div>

                            <div className="space-y-4">
                                <InfoRow label="Email" value={user?.email} />
                                <InfoRow label="ID Compte" value={user?.id?.substring(0, 18) + "..."} />
                                <InfoRow label="Date d'inscription" value={new Date(user?.created_at).toLocaleDateString()} />
                                <InfoRow label="Statut" value={user?.is_active ? "Actif" : "Inactif"} />
                            </div>

                            <button className="w-full rounded-xl border border-accent/20 bg-accent/10 py-3 text-sm font-light text-accent transition-all hover:bg-accent hover:text-background font-jakarta">
                                Modifier le profil
                            </button>
                        </motion.div>
                    </div>

                    <div className="md:col-span-8">
                        <motion.div
                            initial={{ opacity: 0, y: 20 }}
                            animate={{ opacity: 1, y: 0 }}
                            transition={{ delay: 0.2 }}
                            className="rounded-3xl border border-foreground/[0.08] bg-background-secondary/30 p-8 backdrop-blur-xl h-full flex flex-col justify-center items-center text-center"
                        >
                            <div className="mb-6 h-16 w-16 rounded-full bg-foreground/[0.03] flex items-center justify-center">
                                <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" className="text-foreground/20">
                                    <path d="M12 5v14M5 12h14" strokeLinecap="round" strokeLinejoin="round" />
                                </svg>
                            </div>
                            <h3 className="text-xl font-light mb-2" style={{ fontFamily: "var(--font-syne)" }}>Aucun projet récent</h3>
                            <p className="max-w-xs text-sm font-light text-foreground/40 mb-8" style={{ fontFamily: "var(--font-jakarta)" }}>
                                Commencez à collaborer en créant votre premier projet musical sur StemHub.
                            </p>
                            <button className="rounded-xl bg-foreground px-8 py-3 text-sm font-light text-background transition-all hover:bg-accent hover:scale-[1.02] active:scale-[0.98]">
                                Créer un projet
                            </button>
                        </motion.div>
                    </div>
                </main>
            </div>
        </div>
    );
}

function InfoRow({ label, value }: { label: string; value: string }) {
    return (
        <div className="flex flex-col gap-1 border-b border-foreground/[0.04] pb-3 last:border-0 last:pb-0">
            <span className="text-[10px] uppercase tracking-wider text-foreground/30 font-medium">
                {label}
            </span>
            <span className="text-sm font-light text-foreground/80 break-all">
                {value}
            </span>
        </div>
    );
}
