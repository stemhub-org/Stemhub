"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { Users, FolderGit2, ShieldCheck, Activity } from "lucide-react";
import { authFetch } from "@/lib/api";

interface AdminStats {
    total_users: number;
    total_projects: number;
    active_users: number;
    admin_users: number;
}

interface StatCardProps {
    label: string;
    value: number | null;
    icon: React.ReactNode;
    accent?: boolean;
    delay?: number;
}

function StatCard({ label, value, icon, accent = false, delay = 0 }: StatCardProps) {
    return (
        <motion.div
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.4, delay, ease: "easeOut" }}
            className={`rounded-2xl border p-6 flex flex-col gap-4 ${
                accent
                    ? "border-[#9C57DF]/30 bg-[#9C57DF]/5"
                    : "border-[#1A1A1A]/10 bg-white"
            }`}
            style={{ boxShadow: "0 1px 4px 0 rgba(26,26,26,0.06)" }}
        >
            <div className="flex items-center justify-between">
                <span className="text-xs font-medium text-[#1A1A1A]/50 uppercase tracking-wider">{label}</span>
                <span className={`p-2 rounded-lg ${accent ? "bg-[#9C57DF]/10 text-[#9C57DF]" : "bg-[#1A1A1A]/5 text-[#1A1A1A]/40"}`}>
                    {icon}
                </span>
            </div>
            <span className="text-4xl font-semibold tracking-tight text-[#1A1A1A]">
                {value === null ? (
                    <span className="inline-block w-16 h-9 bg-[#1A1A1A]/8 rounded animate-pulse" />
                ) : (
                    value.toLocaleString("fr-FR")
                )}
            </span>
        </motion.div>
    );
}

export default function AdminDashboard() {
    const [stats, setStats] = useState<AdminStats | null>(null);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        authFetch<AdminStats>("/api/admin/stats")
            .then(setStats)
            .catch((err) => setError(err.message));
    }, []);

    return (
        <div className="p-8 max-w-4xl">
            <motion.div
                initial={{ opacity: 0, y: -8 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.35 }}
                className="mb-8"
            >
                <h1 className="text-2xl font-semibold tracking-tight text-[#1A1A1A]">Vue d'ensemble</h1>
                <p className="text-sm text-[#1A1A1A]/50 mt-1">Statistiques globales de la plateforme</p>
            </motion.div>

            {error && (
                <div className="mb-6 px-4 py-3 rounded-xl border border-red-200 bg-red-50 text-sm text-red-600">
                    {error}
                </div>
            )}

            <div className="grid grid-cols-2 gap-4">
                <StatCard
                    label="Utilisateurs total"
                    value={stats?.total_users ?? null}
                    icon={<Users size={16} />}
                    accent
                    delay={0}
                />
                <StatCard
                    label="Projets actifs"
                    value={stats?.total_projects ?? null}
                    icon={<FolderGit2 size={16} />}
                    delay={0.08}
                />
                <StatCard
                    label="Comptes actifs"
                    value={stats?.active_users ?? null}
                    icon={<Activity size={16} />}
                    delay={0.16}
                />
                <StatCard
                    label="Administrateurs"
                    value={stats?.admin_users ?? null}
                    icon={<ShieldCheck size={16} />}
                    delay={0.24}
                />
            </div>
        </div>
    );
}
