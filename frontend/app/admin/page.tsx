"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { Users, FolderGit2, ShieldCheck, Activity, Globe, Lock } from "lucide-react";
import {
    AreaChart,
    Area,
    XAxis,
    YAxis,
    Tooltip,
    ResponsiveContainer,
} from "recharts";
import { authFetch } from "@/lib/api";

interface DailySignup {
    date: string;
    count: number;
}

interface AdminStats {
    total_users: number;
    total_projects: number;
    active_users: number;
    admin_users: number;
    public_projects: number;
    private_projects: number;
    signups_last_30_days: DailySignup[];
}

interface RecentUser {
    id: string;
    username: string;
    email: string;
    avatar_url: string | null;
    created_at: string;
    is_admin: boolean;
}

function StatCard({
    label,
    value,
    icon,
    accent = false,
    delay = 0,
}: {
    label: string;
    value: number | null;
    icon: React.ReactNode;
    accent?: boolean;
    delay?: number;
}) {
    return (
        <motion.div
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.4, delay, ease: "easeOut" }}
            className={`rounded-2xl border p-5 flex flex-col gap-3 ${
                accent ? "border-[#9C57DF]/30 bg-[#9C57DF]/5" : "border-[#1A1A1A]/10 bg-white"
            }`}
            style={{ boxShadow: "0 1px 4px 0 rgba(26,26,26,0.06)" }}
        >
            <div className="flex items-center justify-between">
                <span className="text-xs font-medium text-[#1A1A1A]/50 uppercase tracking-wider">{label}</span>
                <span className={`p-1.5 rounded-lg ${accent ? "bg-[#9C57DF]/10 text-[#9C57DF]" : "bg-[#1A1A1A]/5 text-[#1A1A1A]/40"}`}>
                    {icon}
                </span>
            </div>
            <span className="text-3xl font-semibold tracking-tight text-[#1A1A1A]">
                {value === null ? (
                    <span className="inline-block w-16 h-8 bg-[#1A1A1A]/8 rounded animate-pulse" />
                ) : (
                    value.toLocaleString("fr-FR")
                )}
            </span>
        </motion.div>
    );
}

function formatDateShort(iso: string) {
    const d = new Date(iso);
    return d.toLocaleDateString("fr-FR", { day: "2-digit", month: "short" });
}

function formatDateFull(iso: string) {
    return new Date(iso).toLocaleDateString("fr-FR", {
        day: "2-digit",
        month: "short",
        year: "numeric",
    });
}

function UserAvatar({ user }: { user: RecentUser }) {
    if (user.avatar_url) {
        return <img src={user.avatar_url} alt={user.username} className="w-7 h-7 rounded-full object-cover" />;
    }
    return (
        <div className="w-7 h-7 rounded-full bg-[#9C57DF]/15 text-[#9C57DF] text-xs font-semibold flex items-center justify-center">
            {user.username.slice(0, 2).toUpperCase()}
        </div>
    );
}

export default function AdminDashboard() {
    const [stats, setStats] = useState<AdminStats | null>(null);
    const [recentUsers, setRecentUsers] = useState<RecentUser[] | null>(null);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        authFetch<AdminStats>("/api/admin/stats").then(setStats).catch((e) => setError(e.message));
        authFetch<RecentUser[]>("/api/admin/recent-users").then(setRecentUsers).catch(() => {});
    }, []);

    const chartData = stats?.signups_last_30_days.map((d) => ({
        date: formatDateShort(d.date),
        Inscriptions: d.count,
    })) ?? [];

    return (
        <div className="p-8 max-w-5xl space-y-6">
            <motion.div
                initial={{ opacity: 0, y: -8 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.35 }}
            >
                <h1 className="text-2xl font-semibold tracking-tight text-[#1A1A1A]">Vue d'ensemble</h1>
                <p className="text-sm text-[#1A1A1A]/50 mt-1">Statistiques globales de la plateforme</p>
            </motion.div>

            {error && (
                <div className="px-4 py-3 rounded-xl border border-red-200 bg-red-50 text-sm text-red-600">{error}</div>
            )}

            {/* Stats grid */}
            <div className="grid grid-cols-3 gap-4">
                <StatCard label="Utilisateurs" value={stats?.total_users ?? null} icon={<Users size={15} />} accent delay={0} />
                <StatCard label="Projets actifs" value={stats?.total_projects ?? null} icon={<FolderGit2 size={15} />} delay={0.06} />
                <StatCard label="Comptes actifs" value={stats?.active_users ?? null} icon={<Activity size={15} />} delay={0.12} />
                <StatCard label="Projets publics" value={stats?.public_projects ?? null} icon={<Globe size={15} />} delay={0.18} />
                <StatCard label="Projets privés" value={stats?.private_projects ?? null} icon={<Lock size={15} />} delay={0.24} />
                <StatCard label="Administrateurs" value={stats?.admin_users ?? null} icon={<ShieldCheck size={15} />} delay={0.30} />
            </div>

            {/* Chart + Recent users */}
            <div className="grid grid-cols-5 gap-4">
                {/* Signups chart */}
                <motion.div
                    initial={{ opacity: 0, y: 12 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.36 }}
                    className="col-span-3 rounded-2xl border border-[#1A1A1A]/10 bg-white p-5"
                    style={{ boxShadow: "0 1px 4px 0 rgba(26,26,26,0.06)" }}
                >
                    <p className="text-xs font-medium text-[#1A1A1A]/50 uppercase tracking-wider mb-4">
                        Inscriptions — 30 derniers jours
                    </p>
                    {stats ? (
                        <ResponsiveContainer width="100%" height={160}>
                            <AreaChart data={chartData} margin={{ top: 0, right: 0, left: -28, bottom: 0 }}>
                                <defs>
                                    <linearGradient id="grad" x1="0" y1="0" x2="0" y2="1">
                                        <stop offset="5%" stopColor="#9C57DF" stopOpacity={0.2} />
                                        <stop offset="95%" stopColor="#9C57DF" stopOpacity={0} />
                                    </linearGradient>
                                </defs>
                                <XAxis
                                    dataKey="date"
                                    tick={{ fontSize: 10, fill: "#1A1A1A60" }}
                                    tickLine={false}
                                    axisLine={false}
                                    interval={6}
                                />
                                <YAxis
                                    tick={{ fontSize: 10, fill: "#1A1A1A60" }}
                                    tickLine={false}
                                    axisLine={false}
                                    allowDecimals={false}
                                />
                                <Tooltip
                                    contentStyle={{
                                        background: "white",
                                        border: "1px solid rgba(26,26,26,0.1)",
                                        borderRadius: 10,
                                        fontSize: 12,
                                        boxShadow: "0 4px 12px rgba(0,0,0,0.08)",
                                    }}
                                    labelStyle={{ color: "#1A1A1A80", marginBottom: 2 }}
                                    itemStyle={{ color: "#9C57DF" }}
                                />
                                <Area
                                    type="monotone"
                                    dataKey="Inscriptions"
                                    stroke="#9C57DF"
                                    strokeWidth={2}
                                    fill="url(#grad)"
                                    dot={false}
                                    activeDot={{ r: 4, fill: "#9C57DF" }}
                                />
                            </AreaChart>
                        </ResponsiveContainer>
                    ) : (
                        <div className="h-40 flex items-center justify-center">
                            <div className="w-5 h-5 rounded-full border-2 border-[#9C57DF] border-t-transparent animate-spin" />
                        </div>
                    )}
                </motion.div>

                {/* Recent users */}
                <motion.div
                    initial={{ opacity: 0, y: 12 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.42 }}
                    className="col-span-2 rounded-2xl border border-[#1A1A1A]/10 bg-white p-5"
                    style={{ boxShadow: "0 1px 4px 0 rgba(26,26,26,0.06)" }}
                >
                    <p className="text-xs font-medium text-[#1A1A1A]/50 uppercase tracking-wider mb-4">
                        Derniers inscrits
                    </p>
                    <div className="space-y-3">
                        {recentUsers === null
                            ? Array.from({ length: 5 }).map((_, i) => (
                                  <div key={i} className="flex items-center gap-2.5">
                                      <div className="w-7 h-7 rounded-full bg-[#1A1A1A]/8 animate-pulse shrink-0" />
                                      <div className="flex-1 space-y-1">
                                          <div className="w-20 h-2.5 bg-[#1A1A1A]/8 rounded animate-pulse" />
                                          <div className="w-14 h-2 bg-[#1A1A1A]/5 rounded animate-pulse" />
                                      </div>
                                  </div>
                              ))
                            : recentUsers.map((user) => (
                                  <div key={user.id} className="flex items-center gap-2.5">
                                      <UserAvatar user={user} />
                                      <div className="flex-1 min-w-0">
                                          <p className="text-sm font-medium text-[#1A1A1A] truncate">{user.username}</p>
                                          <p className="text-xs text-[#1A1A1A]/40">{formatDateFull(user.created_at)}</p>
                                      </div>
                                      {user.is_admin && <ShieldCheck size={12} className="text-[#9C57DF] shrink-0" />}
                                  </div>
                              ))}
                    </div>
                </motion.div>
            </div>
        </div>
    );
}
