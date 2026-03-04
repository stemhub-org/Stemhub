"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import { motion } from "framer-motion";
import {
    GitCommit,
    GitBranch,
    Clock,
    Plus,
    DownloadCloud,
    Users,
    HardDrive,
    Activity,
    ChevronRight,
    Search
} from "lucide-react";

const RECENT_REPOSITORIES = [
    {
        id: 1,
        name: "Synthwave Anthem",
        daw: "Ableton Live",
        updatedAt: "2h",
        branchCount: 3,
        contributors: 2,
        size: "1.2 GB"
    },
    {
        id: 2,
        name: "Acoustic Pop Session",
        daw: "Logic Pro",
        updatedAt: "1d",
        branchCount: 1,
        contributors: 1,
        size: "840 MB"
    },
    {
        id: 3,
        name: "Trap Beat 04",
        daw: "FL Studio",
        updatedAt: "3d",
        branchCount: 5,
        contributors: 3,
        size: "2.1 GB"
    }
];

const ACTIVITY_FEED = [
    {
        id: 1,
        user: "Erwan SEYTOR",
        action: "pushed to",
        target: "main",
        repository: "Synthwave Anthem",
        time: "2h",
        icon: GitCommit,
        color: "text-accent-blue"
    },
    {
        id: 2,
        user: "Gabin RUDIGOZ",
        action: "created branch",
        target: "feature/bass",
        repository: "Synthwave Anthem",
        time: "5h",
        icon: GitBranch,
        color: "text-foreground-muted"
    },
    {
        id: 3,
        user: "Dryss MARGUERITTE",
        action: "merged PR",
        target: "#12",
        repository: "Trap Beat 04",
        time: "1d",
        icon: GitBranch,
        color: "text-foreground-muted"
    }
];

export default function DashboardPage() {
    const router = useRouter();
    const [user, setUser] = useState<any>(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        const fetchUser = async () => {
            const token = localStorage.getItem("token");

            try {
                const apiUrl = process.env.NEXT_PUBLIC_API_URL || "http://localhost:8000";
                const response = await fetch(`${apiUrl}/auth/me`, {
                    headers: token ? {
                        "Authorization": `Bearer ${token}`
                    } : {},
                    credentials: "include"
                });

                if (!response.ok) {
                    throw new Error("Session expirée");
                }

                const data = await response.json();
                setUser(data);
            } catch (err) {
            } finally {
                setLoading(false);
            }
        };

        fetchUser();
    }, [router]);

    if (loading) {
        return (
            <div className="flex h-full min-h-[50vh] items-center justify-center">
                <div className="h-6 w-6 border-2 border-border-subtle border-t-accent-blue rounded-full animate-spin" />
            </div>
        );
    }

    return (
        <div className="min-h-full bg-background">
            <div className="max-w-7xl mx-auto space-y-8 pb-12 pt-8 px-4 lg:px-8">

                {/* Header Section */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4 }}
                    className="flex flex-col md:flex-row md:justify-between md:items-end gap-6 pb-6 border-b border-border-subtle"
                >
                    <div>
                        <h1 className="text-3xl font-medium tracking-tight text-foreground mb-1">
                            Welcome, <span className="text-accent-blue">{user?.username || "Producer"}</span>
                        </h1>
                        <p className="text-foreground-muted text-sm">
                            System overview and recent studio activity.
                        </p>
                    </div>

                    <div className="flex gap-3">
                        <button className="flex items-center gap-2 px-4 py-2 rounded-md bg-accent-blue/10 border border-accent-blue/20 hover:bg-accent-blue/20 hover:border-accent-blue/40 transition-all duration-300 text-sm font-medium text-accent-blue group">
                            <DownloadCloud size={16} className="text-accent-blue group-hover:scale-110 transition-transform" />
                            <span>Import</span>
                        </button>
                        <button className="flex items-center gap-2 px-4 py-2 rounded-md bg-accent-blue text-white hover:bg-accent-blue/90 hover:shadow-[0_0_15px_rgba(62,99,221,0.4)] transition-all duration-300 text-sm font-medium">
                            <Plus size={16} />
                            <span>New Repository</span>
                        </button>
                    </div>
                </motion.div>

                {/* Stats Overview - Minimalist Bento Grid */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.1 }}
                    className="grid grid-cols-1 md:grid-cols-3 gap-4"
                >
                    <div className="bg-background-secondary border border-border-subtle rounded-xl p-6 hover:border-accent-blue/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent-blue/5 hover:shadow-[0_0_20px_rgba(62,99,221,0.05)] transition-all duration-300 group">
                        <div className="flex items-center justify-between mb-8">
                            <div className="text-foreground transition-colors flex items-center gap-2 text-sm font-medium">
                                <HardDrive size={16} className="text-accent-blue group-hover:scale-110 transition-transform" />
                                <span>Storage</span>
                            </div>
                            <span className="text-xs font-semibold px-2 py-1 rounded bg-accent-blue/10 text-accent-blue border border-accent-blue/20">Active</span>
                        </div>
                        <div className="flex items-baseline gap-2">
                            <h3 className="text-3xl font-semibold tracking-tight text-foreground">18</h3>
                            <span className="text-sm border-l border-border-subtle pl-2 text-foreground-muted">Repositories</span>
                        </div>
                    </div>

                    <div className="bg-background-secondary border border-border-subtle rounded-xl p-6 hover:border-accent-blue/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent-blue/5 hover:shadow-[0_0_20px_rgba(62,99,221,0.05)] transition-all duration-300 group">
                        <div className="flex items-center justify-between mb-8">
                            <div className="text-foreground transition-colors flex items-center gap-2 text-sm font-medium">
                                <GitBranch size={16} className="text-accent-blue group-hover:scale-110 transition-transform" />
                                <span>Version Control</span>
                            </div>
                        </div>
                        <div className="flex items-baseline gap-2">
                            <h3 className="text-3xl font-semibold tracking-tight text-foreground">42</h3>
                            <span className="text-sm border-l border-border-subtle pl-2 text-foreground-muted">Total Branches</span>
                        </div>
                    </div>

                    <div className="bg-background-secondary border border-border-subtle rounded-xl p-6 hover:border-accent-blue/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent-blue/5 hover:shadow-[0_0_20px_rgba(62,99,221,0.05)] transition-all duration-300 group">
                        <div className="flex items-center justify-between mb-8">
                            <div className="text-foreground transition-colors flex items-center gap-2 text-sm font-medium">
                                <Users size={16} className="text-accent-blue group-hover:scale-110 transition-transform" />
                                <span>Network</span>
                            </div>
                            <div className="flex -space-x-1.5">
                                {[1, 2, 3].map((i) => (
                                    <div key={i} className="w-5 h-5 rounded-full bg-background-tertiary border border-border-subtle" />
                                ))}
                            </div>
                        </div>
                        <div className="flex items-baseline gap-2">
                            <h3 className="text-3xl font-semibold tracking-tight text-foreground">6</h3>
                            <span className="text-sm border-l border-border-subtle pl-2 text-foreground-muted">Collaborators</span>
                        </div>
                    </div>
                </motion.div>

                <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
                    {/* Recent Repositories */}
                    <motion.div
                        initial={{ opacity: 0, y: 10 }}
                        animate={{ opacity: 1, y: 0 }}
                        transition={{ duration: 0.4, delay: 0.2 }}
                        className="lg:col-span-2 space-y-4"
                    >
                        <div className="flex items-center justify-between">
                            <h2 className="text-sm font-semibold tracking-wide uppercase text-foreground-muted">
                                Recent Repositories
                            </h2>
                            <button className="text-xs text-foreground-muted hover:text-foreground transition-colors flex items-center gap-1">
                                View all <ChevronRight size={14} />
                            </button>
                        </div>

                        <div className="flex flex-col gap-3">
                            {RECENT_REPOSITORIES.map((repo, idx) => (
                                <div
                                    key={repo.id}
                                    className="group flex flex-col sm:flex-row sm:items-center justify-between p-4 rounded-lg bg-background-secondary border border-border-subtle hover:border-accent-blue/40 hover:bg-gradient-to-r hover:from-background-secondary hover:to-accent-blue/5 hover:shadow-[0_0_15px_rgba(62,99,221,0.05)] transition-all duration-300 cursor-pointer"
                                >
                                    <div className="flex items-center gap-4 mb-3 sm:mb-0">
                                        <div className="h-10 w-10 rounded-md bg-accent-blue/10 flex items-center justify-center border border-accent-blue/20 group-hover:border-accent-blue/40 transition-colors">
                                            <span className="font-semibold text-accent-blue text-sm group-hover:scale-110 transition-transform">
                                                {repo.name.substring(0, 2).toUpperCase()}
                                            </span>
                                        </div>
                                        <div>
                                            <h3 className="font-medium text-foreground text-base group-hover:text-accent-blue transition-colors">
                                                {repo.name}
                                            </h3>
                                            <div className="flex gap-2 text-xs text-foreground-muted mt-1 items-center">
                                                <span className="flex items-center gap-1">
                                                    <Clock size={10} /> {repo.updatedAt}
                                                </span>
                                                <span className="w-1 h-1 rounded-full bg-border-subtle" />
                                                <span className="font-medium tracking-wide">
                                                    {repo.daw}
                                                </span>
                                            </div>
                                        </div>
                                    </div>

                                    <div className="flex items-center gap-6 text-xs text-foreground-muted font-medium sm:ml-auto">
                                        <div className="flex items-center gap-1.5" title="Branches">
                                            <GitBranch size={14} /> {repo.branchCount}
                                        </div>
                                        <div className="flex items-center gap-1.5" title="Collaborators">
                                            <Users size={14} /> {repo.contributors}
                                        </div>
                                        <div className="flex items-center gap-1.5" title="Size">
                                            <HardDrive size={14} /> {repo.size}
                                        </div>
                                    </div>
                                </div>
                            ))}
                        </div>
                    </motion.div>

                    {/* Minimalist Activity Feed */}
                    <motion.div
                        initial={{ opacity: 0, y: 10 }}
                        animate={{ opacity: 1, y: 0 }}
                        transition={{ duration: 0.4, delay: 0.3 }}
                        className="space-y-4"
                    >
                        <div className="flex items-center justify-between">
                            <h2 className="text-sm font-semibold tracking-wide uppercase text-foreground-muted">
                                Activity
                            </h2>
                        </div>

                        <div className="rounded-lg bg-background-secondary border border-border-subtle p-5">
                            <div className="space-y-6">
                                {ACTIVITY_FEED.map((event, index) => {
                                    const Icon = event.icon;
                                    return (
                                        <div key={event.id} className="flex gap-4 group cursor-pointer">
                                            <div className="mt-0.5 relative flex flex-col items-center">
                                                <div className={`h-6 w-6 rounded-md flex items-center justify-center bg-accent-blue/10 border border-accent-blue/20 text-accent-blue group-hover:border-accent-blue/40 group-hover:shadow-[0_0_10px_rgba(62,99,221,0.2)] transition-all duration-300`}>
                                                    <Icon size={12} className="group-hover:scale-110 transition-transform duration-300" />
                                                </div>
                                                {index < ACTIVITY_FEED.length - 1 && (
                                                    <div className="w-px h-full bg-border-subtle mt-2 mb-[-16px] group-hover:bg-accent-blue/20 transition-colors duration-300" />
                                                )}
                                            </div>
                                            <div className="flex-1 pb-2">
                                                <p className="text-sm text-foreground-muted leading-snug">
                                                    <span className="font-medium text-foreground">{event.user}</span>{" "}
                                                    {event.action}{" "}
                                                    <span className="font-mono text-xs text-foreground">
                                                        {event.target}
                                                    </span>
                                                </p>
                                                <div className="flex justify-between items-center mt-1">
                                                    <p className="text-xs text-foreground-muted">
                                                        {event.repository}
                                                    </p>
                                                    <span className="text-xs text-foreground-muted opacity-50">{event.time}</span>
                                                </div>
                                            </div>
                                        </div>
                                    );
                                })}
                            </div>

                            <button className="w-full mt-6 py-2 text-xs font-semibold text-foreground-muted hover:text-accent-blue hover:bg-gradient-to-r hover:from-background hover:to-accent-blue/5 transition-all duration-300 rounded-md border border-transparent hover:border-accent-blue/20">
                                View All Logs
                            </button>
                        </div>
                    </motion.div>
                </div>
            </div>
        </div>
    );
}
