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
    HardDrive
} from "lucide-react";

const RECENT_PROJECTS = [
    {
        id: 1,
        name: "Synthwave Anthem",
        daw: "Ableton Live",
        updatedAt: "2 hours ago",
        branchCount: 3,
        contributors: 2,
        size: "1.2 GB"
    },
    {
        id: 2,
        name: "Acoustic Pop Session",
        daw: "Logic Pro",
        updatedAt: "Yesterday",
        branchCount: 1,
        contributors: 1,
        size: "840 MB"
    },
    {
        id: 3,
        name: "Trap Beat 04",
        daw: "FL Studio",
        updatedAt: "3 days ago",
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
        project: "Synthwave Anthem",
        time: "2 hours ago",
        icon: GitCommit,
        color: "text-accent"
    },
    {
        id: 2,
        user: "Gabin RUDIGOZ",
        action: "created branch",
        target: "feature/new-bassline",
        project: "Synthwave Anthem",
        time: "5 hours ago",
        icon: GitBranch,
        color: "text-blue-400"
    },
    {
        id: 3,
        user: "Dryss MARGUERITTE",
        action: "merged pull request",
        target: "#12",
        project: "Trap Beat 04",
        time: "1 day ago",
        icon: GitBranch,
        color: "text-green-400"
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
            <div className="flex h-full min-h-[50vh] items-center justify-center">
                <div className="h-8 w-8 animate-spin rounded-full border-2 border-accent border-t-transparent" />
            </div>
        );
    }

    return (
        <div className="max-w-7xl mx-auto space-y-10 pb-12">
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4 }}
                className="flex justify-between items-end"
            >
                <div>
                    <h1 className="text-3xl font-light mb-2 tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>
                        Welcome back, <span className="font-medium text-accent">{user?.username || "Producer"}</span>
                    </h1>
                    <p className="text-foreground/60 font-light">
                        Here's what's happening with your sessions today.
                    </p>
                </div>

                <div className="flex gap-4">
                    <button className="flex items-center gap-2 px-5 py-2.5 rounded-xl bg-foreground/5 hover:bg-foreground/10 text-sm font-medium transition-all border border-foreground/10">
                        <DownloadCloud size={18} />
                        <span>Import Project</span>
                    </button>
                    <button className="flex items-center gap-2 px-5 py-2.5 rounded-xl bg-accent hover:bg-accent/90 text-white text-sm font-medium transition-all shadow-lg shadow-accent/20">
                        <Plus size={18} />
                        <span>New Repository</span>
                    </button>
                </div>
            </motion.div>

            {/* Stats Overview */}
            <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: 0.4, delay: 0.1 }}
                className="grid grid-cols-1 md:grid-cols-3 gap-6"
            >
                <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/30 p-6 backdrop-blur-xl flex flex-col gap-2">
                    <div className="flex items-center justify-between mb-2">
                        <div className="p-2.5 rounded-lg bg-accent/20 text-accent">
                            <HardDrive size={22} />
                        </div>
                    </div>
                    <h3 className="text-3xl font-medium tracking-tight">18</h3>
                    <p className="text-sm font-light text-foreground/60">Active Projects</p>
                </div>

                <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/30 p-6 backdrop-blur-xl flex flex-col gap-2">
                    <div className="flex items-center justify-between mb-2">
                        <div className="p-2.5 rounded-lg bg-blue-500/20 text-blue-500">
                            <GitBranch size={22} />
                        </div>
                    </div>
                    <h3 className="text-3xl font-medium tracking-tight">42</h3>
                    <p className="text-sm font-light text-foreground/60">Total Branches</p>
                </div>

                <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/30 p-6 backdrop-blur-xl flex flex-col gap-2">
                    <div className="flex items-center justify-between mb-2">
                        <div className="p-2.5 rounded-lg bg-green-500/20 text-green-500">
                            <Users size={22} />
                        </div>
                    </div>
                    <h3 className="text-3xl font-medium tracking-tight">6</h3>
                    <p className="text-sm font-light text-foreground/60">Collaborators</p>
                </div>
            </motion.div>

            <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
                {/* Recent Projects Section */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.2 }}
                    className="lg:col-span-2 space-y-6"
                >
                    <div className="flex items-center justify-between">
                        <h2 className="text-xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>Recent Projects</h2>
                        <button className="text-sm text-accent hover:underline font-light">View all</button>
                    </div>

                    <div className="flex flex-col gap-4">
                        {RECENT_PROJECTS.map((project) => (
                            <div
                                key={project.id}
                                className="group flex flex-col md:flex-row md:items-center justify-between p-5 rounded-2xl border border-foreground/[0.08] bg-background-secondary/10 hover:bg-background-secondary/30 backdrop-blur-xl transition-all cursor-pointer"
                            >
                                <div className="flex items-center gap-4 mb-4 md:mb-0">
                                    <div className="h-12 w-12 rounded-xl bg-gradient-to-br from-foreground/10 to-foreground/5 flex items-center justify-center border border-foreground/10">
                                        <span className="font-bold text-foreground/80 tracking-tighter text-lg">{project.name.substring(0, 2).toUpperCase()}</span>
                                    </div>
                                    <div>
                                        <h3 className="font-medium text-lg leading-tight group-hover:text-accent transition-colors">
                                            {project.name}
                                        </h3>
                                        <div className="flex gap-3 text-xs text-foreground/50 mt-1 font-light">
                                            <span className="flex items-center gap-1">
                                                <Clock size={12} /> {project.updatedAt}
                                            </span>
                                            <span className="flex items-center gap-1 px-1.5 py-0.5 rounded-md text-[10px] font-medium tracking-wider" style={{ background: 'var(--foreground)', color: 'var(--background)' }}>
                                                {project.daw}
                                            </span>
                                        </div>
                                    </div>
                                </div>

                                <div className="flex items-center gap-6 text-sm text-foreground/60 font-light ml-[64px] md:ml-0 border-t border-foreground/5 pt-3 md:border-t-0 md:pt-0">
                                    <div className="flex items-center gap-1.5" title="Branches">
                                        <GitBranch size={16} /> {project.branchCount}
                                    </div>
                                    <div className="flex items-center gap-1.5" title="Collaborators">
                                        <Users size={16} /> {project.contributors}
                                    </div>
                                    <div className="flex items-center gap-1.5" title="Project Size">
                                        <HardDrive size={16} /> {project.size}
                                    </div>
                                </div>
                            </div>
                        ))}
                    </div>
                </motion.div>

                {/* Activity Feed Section */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.3 }}
                    className="space-y-6"
                >
                    <div className="flex items-center justify-between">
                        <h2 className="text-xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>Activity</h2>
                    </div>

                    <div className="rounded-2xl border border-foreground/[0.08] bg-background-secondary/30 p-6 backdrop-blur-xl h-[calc(100%-3rem)] overflow-hidden relative">
                        {/* Timeline line */}
                        <div className="absolute left-[39px] top-8 bottom-8 w-px bg-foreground/10"></div>

                        <div className="space-y-8 relative">
                            {ACTIVITY_FEED.map((event, index) => {
                                const Icon = event.icon;
                                return (
                                    <div key={event.id} className="flex gap-4 relative z-10">
                                        <div className="mt-1">
                                            <div className={`h-8 w-8 rounded-full flex items-center justify-center bg-background border-2 border-background-secondary ${event.color} shadow-sm`}>
                                                <Icon size={14} />
                                            </div>
                                        </div>
                                        <div>
                                            <p className="text-sm font-light leading-snug">
                                                <span className="font-medium text-foreground">{event.user}</span>{" "}
                                                {event.action}{" "}
                                                <span className="font-medium text-foreground/80 font-mono text-xs bg-foreground/5 px-1 py-0.5 rounded">
                                                    {event.target}
                                                </span>
                                            </p>
                                            <p className="text-xs text-foreground/50 mt-1">
                                                on <span className="text-accent">{event.project}</span> • {event.time}
                                            </p>
                                        </div>
                                    </div>
                                );
                            })}
                        </div>

                        <div className="mt-8 pt-4 border-t border-foreground/5 text-center">
                            <button className="text-xs font-medium text-foreground/50 hover:text-accent transition-colors">
                                View full history
                            </button>
                        </div>
                    </div>
                </motion.div>
            </div>
        </div>
    );
}
