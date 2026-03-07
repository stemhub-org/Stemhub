"use client";

import { useEffect, useState, useCallback } from "react";
import { useRouter } from "next/navigation";
import { motion, AnimatePresence } from "framer-motion";
import {
    GitBranch,
    Clock,
    Plus,
    DownloadCloud,
    Users,
    HardDrive,
    ChevronRight,
    X,
    Loader2,
} from "lucide-react";
import { authFetch } from "@/lib/api";

interface ProjectItem {
    id: string;
    name: string;
    description: string | null;
    category: string;
    is_public: boolean;
    owner_id: string;
    created_at: string;
    is_deleted: boolean;
    deleted_at: string | null;
}

function formatTimeAgo(dateString: string): string {
    const now = new Date();
    const date = new Date(dateString);
    const diffMs = now.getTime() - date.getTime();
    const diffHours = Math.floor(diffMs / 3600000);
    const diffDays = Math.floor(diffHours / 24);
    if (diffHours < 1) return "just now";
    if (diffHours < 24) return `${diffHours}h`;
    if (diffDays < 7) return `${diffDays}d`;
    return date.toLocaleDateString();
}

export default function DashboardPage() {
    const router = useRouter();
    const [user, setUser] = useState<any>(null);
    const [projects, setProjects] = useState<ProjectItem[]>([]);
    const [loading, setLoading] = useState(true);
    const [showNewProject, setShowNewProject] = useState(false);
    const [newProjectName, setNewProjectName] = useState("");
    const [newProjectDesc, setNewProjectDesc] = useState("");
    const [newProjectCategory, setNewProjectCategory] = useState("General");
    const [creating, setCreating] = useState(false);

    const fetchData = useCallback(async () => {
        try {
            const [userData, projectsData] = await Promise.all([
                authFetch<any>("/auth/me"),
                authFetch<ProjectItem[]>("/projects/"),
            ]);
            setUser(userData);
            setProjects(projectsData);
        } catch {
            // If auth fails, user might need to login
        } finally {
            setLoading(false);
        }
    }, []);

    useEffect(() => {
        fetchData();
    }, [fetchData]);

    const handleCreateProject = async () => {
        if (!newProjectName.trim()) return;
        setCreating(true);
        try {
            const created = await authFetch<ProjectItem>("/projects/", {
                method: "POST",
                body: JSON.stringify({
                    name: newProjectName.trim(),
                    description: newProjectDesc.trim() || null,
                    category: newProjectCategory,
                }),
            });
            setProjects((prev) => [created, ...prev]);
            setShowNewProject(false);
            setNewProjectName("");
            setNewProjectDesc("");
            setNewProjectCategory("General");
        } catch (err) {
            alert(err instanceof Error ? err.message : "Failed to create project");
        } finally {
            setCreating(false);
        }
    };

    if (loading) {
        return (
            <div className="flex h-full min-h-[50vh] items-center justify-center">
                <div className="h-6 w-6 border-2 border-border-subtle border-t-accent rounded-full animate-spin" />
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
                            Welcome, <span className="text-accent">{user?.username || "Producer"}</span>
                        </h1>
                        <p className="text-foreground-muted text-sm">
                            System overview and recent studio activity.
                        </p>
                    </div>

                    <div className="flex gap-3">
                        <button className="flex items-center gap-2 px-4 py-2 rounded-md bg-accent/10 border border-accent/20 hover:bg-accent/20 hover:border-accent/40 transition-all duration-300 text-sm font-medium text-accent group">
                            <DownloadCloud size={16} className="text-accent group-hover:scale-110 transition-transform" />
                            <span>Import</span>
                        </button>
                        <button
                            onClick={() => setShowNewProject(true)}
                            className="flex items-center gap-2 px-4 py-2 rounded-md bg-accent text-white hover:bg-accent/90 hover:shadow-[0_0_15px_rgba(156,87,223,0.4)] transition-all duration-300 text-sm font-medium"
                        >
                            <Plus size={16} />
                            <span>New Project</span>
                        </button>
                    </div>
                </motion.div>

                {/* Stats Overview */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.1 }}
                    className="grid grid-cols-1 md:grid-cols-3 gap-4"
                >
                    <div className="bg-background-secondary border border-border-subtle rounded-xl p-6 hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.05)] transition-all duration-300 group">
                        <div className="flex items-center justify-between mb-8">
                            <div className="text-foreground transition-colors flex items-center gap-2 text-sm font-medium">
                                <HardDrive size={16} className="text-accent group-hover:scale-110 transition-transform" />
                                <span>Storage</span>
                            </div>
                            <span className="text-xs font-semibold px-2 py-1 rounded bg-accent/10 text-accent border border-accent/20">Active</span>
                        </div>
                        <div className="flex items-baseline gap-2">
                            <h3 className="text-3xl font-semibold tracking-tight text-foreground">{projects.length}</h3>
                            <span className="text-sm border-l border-border-subtle pl-2 text-foreground-muted">Projects</span>
                        </div>
                    </div>

                    <div className="bg-background-secondary border border-border-subtle rounded-xl p-6 hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.05)] transition-all duration-300 group">
                        <div className="flex items-center justify-between mb-8">
                            <div className="text-foreground transition-colors flex items-center gap-2 text-sm font-medium">
                                <GitBranch size={16} className="text-accent group-hover:scale-110 transition-transform" />
                                <span>Version Control</span>
                            </div>
                        </div>
                        <div className="flex items-baseline gap-2">
                            <h3 className="text-3xl font-semibold tracking-tight text-foreground">{projects.length}</h3>
                            <span className="text-sm border-l border-border-subtle pl-2 text-foreground-muted">Total Projects</span>
                        </div>
                    </div>

                    <div className="bg-background-secondary border border-border-subtle rounded-xl p-6 hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.05)] transition-all duration-300 group">
                        <div className="flex items-center justify-between mb-8">
                            <div className="text-foreground transition-colors flex items-center gap-2 text-sm font-medium">
                                <Users size={16} className="text-accent group-hover:scale-110 transition-transform" />
                                <span>Network</span>
                            </div>
                        </div>
                        <div className="flex items-baseline gap-2">
                            <h3 className="text-3xl font-semibold tracking-tight text-foreground">—</h3>
                            <span className="text-sm border-l border-border-subtle pl-2 text-foreground-muted">Collaborators</span>
                        </div>
                    </div>
                </motion.div>

                {/* Recent Projects — REAL DATA */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.2 }}
                    className="space-y-4"
                >
                    <div className="flex items-center justify-between">
                        <h2 className="text-sm font-semibold tracking-wide uppercase text-foreground-muted">
                            Your Projects
                        </h2>
                    </div>

                    {projects.length === 0 ? (
                        <div className="rounded-lg bg-background-secondary border border-border-subtle p-8 text-center">
                            <p className="text-foreground-muted text-sm mb-4">No projects yet. Create your first one!</p>
                            <button
                                onClick={() => setShowNewProject(true)}
                                className="inline-flex items-center gap-2 px-4 py-2 rounded-md bg-accent text-white hover:bg-accent/90 text-sm font-medium transition-all"
                            >
                                <Plus size={16} /> New Project
                            </button>
                        </div>
                    ) : (
                        <div className="flex flex-col gap-3">
                            {projects.map((project) => (
                                <div
                                    key={project.id}
                                    onClick={() => router.push(`/projects?id=${project.id}`)}
                                    className="group flex flex-col sm:flex-row sm:items-center justify-between p-4 rounded-lg bg-background-secondary border border-border-subtle hover:border-accent/40 hover:bg-gradient-to-r hover:from-background-secondary hover:to-accent/5 hover:shadow-[0_0_15px_rgba(156,87,223,0.05)] transition-all duration-300 cursor-pointer"
                                >
                                    <div className="flex items-center gap-4 mb-3 sm:mb-0">
                                        <div className="h-10 w-10 rounded-md bg-accent/10 flex items-center justify-center border border-accent/20 group-hover:border-accent/40 transition-colors">
                                            <span className="font-semibold text-accent text-sm group-hover:scale-110 transition-transform">
                                                {project.name.substring(0, 2).toUpperCase()}
                                            </span>
                                        </div>
                                        <div>
                                            <h3 className="font-medium text-foreground text-base group-hover:text-accent transition-colors">
                                                {project.name}
                                            </h3>
                                            <div className="flex gap-2 text-xs text-foreground-muted mt-1 items-center">
                                                <span className="flex items-center gap-1">
                                                    <Clock size={10} /> {formatTimeAgo(project.created_at)}
                                                </span>
                                                {project.description && (
                                                    <>
                                                        <span className="w-1 h-1 rounded-full bg-border-subtle" />
                                                        <span className="font-medium tracking-wide truncate max-w-[200px]">
                                                            {project.description}
                                                        </span>
                                                    </>
                                                )}
                                            </div>
                                        </div>
                                    </div>

                                    <div className="flex items-center gap-6 text-xs text-foreground-muted font-medium sm:ml-auto">
                                        <span className="px-2 py-0.5 rounded bg-accent/10 text-accent border border-accent/20 text-[10px]">
                                            {project.category}
                                        </span>
                                        <ChevronRight size={14} className="text-foreground-muted group-hover:text-accent transition-colors" />
                                    </div>
                                </div>
                            ))}
                        </div>
                    )}
                </motion.div>
            </div>

            {/* New Project Modal */}
            <AnimatePresence>
                {showNewProject && (
                    <>
                        <motion.div
                            initial={{ opacity: 0 }}
                            animate={{ opacity: 1 }}
                            exit={{ opacity: 0 }}
                            className="fixed inset-0 z-50 bg-black/50 backdrop-blur-sm"
                            onClick={() => setShowNewProject(false)}
                        />
                        <motion.div
                            initial={{ opacity: 0, scale: 0.95, y: 20 }}
                            animate={{ opacity: 1, scale: 1, y: 0 }}
                            exit={{ opacity: 0, scale: 0.95, y: 20 }}
                            transition={{ type: "spring", damping: 25, stiffness: 300 }}
                            className="fixed inset-0 z-50 flex items-center justify-center p-4"
                            onClick={(e) => e.stopPropagation()}
                        >
                            <div className="w-full max-w-md rounded-xl bg-background-secondary border border-border-subtle p-6 shadow-2xl">
                                <div className="flex items-center justify-between mb-6">
                                    <h2 className="text-lg font-medium text-foreground">New Project</h2>
                                    <button
                                        onClick={() => setShowNewProject(false)}
                                        className="text-foreground-muted hover:text-foreground transition-colors"
                                    >
                                        <X size={20} />
                                    </button>
                                </div>

                                <div className="space-y-4">
                                    <div>
                                        <label className="block text-sm font-medium text-foreground mb-1.5">
                                            Project Name *
                                        </label>
                                        <input
                                            type="text"
                                            value={newProjectName}
                                            onChange={(e) => setNewProjectName(e.target.value)}
                                            placeholder="My Awesome Track"
                                            className="w-full rounded-lg border border-border-subtle bg-background px-3 py-2.5 text-sm text-foreground placeholder:text-foreground-muted/50 focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent/50 transition-colors"
                                            autoFocus
                                        />
                                    </div>
                                    <div>
                                        <label className="block text-sm font-medium text-foreground mb-1.5">
                                            Description
                                        </label>
                                        <textarea
                                            value={newProjectDesc}
                                            onChange={(e) => setNewProjectDesc(e.target.value)}
                                            placeholder="A short description of your project"
                                            rows={3}
                                            className="w-full rounded-lg border border-border-subtle bg-background px-3 py-2.5 text-sm text-foreground placeholder:text-foreground-muted/50 focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent/50 transition-colors resize-none"
                                        />
                                    </div>
                                    <div>
                                        <label className="block text-sm font-medium text-foreground mb-1.5">
                                            Category
                                        </label>
                                        <select
                                            value={newProjectCategory}
                                            onChange={(e) => setNewProjectCategory(e.target.value)}
                                            className="w-full rounded-lg border border-border-subtle bg-background px-3 py-2.5 text-sm text-foreground focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent/50 transition-colors"
                                        >
                                            <option value="General">General</option>
                                            <option value="Electronic">Electronic</option>
                                            <option value="Hip-Hop">Hip-Hop</option>
                                            <option value="Pop">Pop</option>
                                            <option value="Rock">Rock</option>
                                            <option value="Jazz">Jazz</option>
                                            <option value="Classical">Classical</option>
                                            <option value="Other">Other</option>
                                        </select>
                                    </div>
                                </div>

                                <div className="flex justify-end gap-3 mt-6">
                                    <button
                                        onClick={() => setShowNewProject(false)}
                                        className="px-4 py-2 rounded-lg text-sm font-medium text-foreground-muted hover:text-foreground transition-colors"
                                    >
                                        Cancel
                                    </button>
                                    <button
                                        onClick={handleCreateProject}
                                        disabled={!newProjectName.trim() || creating}
                                        className="inline-flex items-center gap-2 px-4 py-2 rounded-lg bg-accent text-white text-sm font-medium hover:bg-accent/90 disabled:opacity-50 disabled:cursor-not-allowed transition-all"
                                    >
                                        {creating ? (
                                            <>
                                                <Loader2 size={16} className="animate-spin" />
                                                Creating…
                                            </>
                                        ) : (
                                            <>
                                                <Plus size={16} />
                                                Create Project
                                            </>
                                        )}
                                    </button>
                                </div>
                            </div>
                        </motion.div>
                    </>
                )}
            </AnimatePresence>
        </div>
    );
}
