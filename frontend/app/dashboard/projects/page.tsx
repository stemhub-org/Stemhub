"use client";

import type React from "react";
import { useState, useEffect } from "react";
import Link from "next/link";
import { motion, AnimatePresence } from "framer-motion";
import { useTheme } from "next-themes";
import { Heart, GitBranch, Clock, Activity, Folder, Search, Plus, User, Loader2, X } from "lucide-react";
import { authFetch } from "@/lib/api";

interface UserProfile {
    id: string;
    email: string;
    username: string;
    avatar_url?: string | null;
    bio?: string | null;
    location?: string | null;
    website?: string | null;
    genres?: string[] | null;
    created_at: string;
    is_active: boolean;
}

interface Project {
    id: string;
    owner_id: string;
    name: string;
    description?: string | null;
    category?: string;
    is_public: boolean;
    created_at: string;
    is_deleted: boolean;
    deleted_at?: string | null;
}

export default function DashboardProjectsPage() {
    const [user, setUser] = useState<UserProfile | null>(null);
    const [projects, setProjects] = useState<Project[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [searchQuery, setSearchQuery] = useState("");
    const [favorites, setFavorites] = useState<Record<string, boolean>>({});
    const [showNewProject, setShowNewProject] = useState(false);
    const [newProjectName, setNewProjectName] = useState("");
    const [newProjectDesc, setNewProjectDesc] = useState("");
    const [newProjectCategory, setNewProjectCategory] = useState("General");
    const [creating, setCreating] = useState(false);

    const { resolvedTheme } = useTheme();
    const isDark = resolvedTheme === "dark";

    useEffect(() => {
        const fetchData = async () => {
            setIsLoading(true);
            setError(null);

            try {
                const [userRes, projectsRes] = await Promise.all([
                    authFetch<UserProfile>("/auth/me"),
                    authFetch<Project[]>("/projects/")
                ]);

                setUser(userRes);

                const projectsData = Array.isArray(projectsRes)
                    ? projectsRes.filter((p: Project) => !p.is_deleted)
                    : [];

                projectsData.sort(
                    (a, b) =>
                        new Date(b.created_at).getTime() - new Date(a.created_at).getTime()
                );

                setProjects(projectsData);
            } catch (err: any) {
                console.error("Error fetching dashboard data:", err);
                setError(err.message || "An error occurred while fetching data.");
            } finally {
                setIsLoading(false);
            }
        };

        fetchData();
    }, []);

    const handleCreateProject = async () => {
        if (!newProjectName.trim()) return;
        setCreating(true);
        try {
            const created = await authFetch<Project>("/projects/", {
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
        } catch (err: any) {
            alert(err.message || "Failed to create project");
        } finally {
            setCreating(false);
        }
    };

    const filteredProjects = projects.filter(p => 
        p.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        (p.description && p.description.toLowerCase().includes(searchQuery.toLowerCase()))
    );

    const formatDate = (dateString: string) => {
        const date = new Date(dateString);
        const now = new Date();
        const diffInMs = now.getTime() - date.getTime();
        const diffInDays = Math.floor(diffInMs / (1000 * 60 * 60 * 24));
        
        if (diffInDays === 0) return "Updated today";
        if (diffInDays === 1) return "Updated 1d ago";
        if (diffInDays < 30) return `Updated ${diffInDays}d ago`;
        
        return new Intl.DateTimeFormat("en-US", {
            month: "short",
            day: "numeric",
            year: date.getFullYear() !== now.getFullYear() ? "numeric" : undefined
        }).format(date);
    };

    if (isLoading) {
        return (
            <div className="flex justify-center items-center min-h-[60vh]">
                <Loader2 className="size-10 animate-spin text-accent" />
            </div>
        );
    }
    
    if (error) {
        return (
            <div className="flex flex-col justify-center items-center min-h-[60vh] gap-4">
                <p className="text-red-500 font-medium">Error loading dashboard</p>
                <p className="text-foreground-muted text-sm">{error}</p>
                <Link href="/login" className="px-4 py-2 bg-accent/20 text-accent rounded-md hover:bg-accent/30 transition-colors">
                    Go to Login
                </Link>
            </div>
        );
    }

    return (
        <div
            className="max-w-6xl mx-auto pt-6 pb-10"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <div className="flex flex-col gap-8 md:flex-row md:items-start md:gap-10">
                <aside className="w-full md:w-62 flex-shrink-0">
                    <div className="flex flex-col items-center md:items-start gap-4">
                        <div className={`h-60 w-60 overflow-hidden rounded-full bg-gradient-to-tr from-accent to-accent/40 flex items-center justify-center border-2 ${isDark ? "border-transparent" : "border-white"}`}>
                            {user?.avatar_url ? (
                                <img src={user.avatar_url} alt={`${user.username}'s avatar`} className="h-full w-full object-cover rounded-full" />
                            ) : (
                                <span className="flex h-24 w-24 items-center justify-center rounded-full bg-black/15">
                                    <User className="h-14 w-14 text-white/90" aria-hidden />
                                </span>
                            )}
                        </div>
                        <div className="space-y-2 text-center md:text-left">
                            <h1
                                className="text-3xl font-medium tracking-tight"
                                style={{ fontFamily: "var(--font-syne)" }}
                            >
                                {user?.username || "Unknown User"}
                            </h1>
                            <p className="text-base text-foreground-muted">@{user?.username}</p>
                            <p className="text-sm text-foreground-muted">
                                <span className="font-medium text-foreground">0</span> followers ·{" "}
                                <span className="font-medium text-foreground">0</span> following
                            </p>
                            <Link
                                href="/dashboard/profile"
                                className={`mt-2 inline-block w-full rounded-lg border border-border-subtle px-4 py-2 text-center text-sm font-medium transition-colors ${
                                    "bg-background-secondary dark:bg-background-tertiary text-foreground hover:bg-background-tertiary dark:hover:bg-background-tertiary/80 hover:border-accent/40"
                                }`}
                            >
                                View profile
                            </Link>
                        </div>
                    </div>
                </aside>

                <section className="flex-1 space-y-4">
                    <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:gap-4">
                        <div className="relative w-full sm:flex-1">
                            <span className="pointer-events-none absolute inset-y-0 left-3 flex items-center text-foreground-muted">
                                <Search className="size-4" aria-hidden />
                            </span>
                            <input
                                type="text"
                                placeholder="Find a project..."
                                value={searchQuery}
                                onChange={(e) => setSearchQuery(e.target.value)}
                                className="w-full rounded-md border border-border-subtle bg-background-secondary/80 dark:bg-background-tertiary/80 py-2.5 pl-9 pr-3 text-sm text-foreground placeholder:text-foreground-muted focus:outline-none focus:border-border-hover"
                            />
                        </div>
                        <button
                            type="button"
                            onClick={() => setShowNewProject(true)}
                            className="inline-flex items-center justify-center gap-2 rounded-lg px-5 py-2 text-base font-medium text-foreground dark:text-white bg-accent/25 border border-accent/35 backdrop-blur-xl shadow-[0_0_24px_rgba(156,87,223,0.18)] hover:bg-accent/35 hover:border-accent/50 transition-all duration-300"
                        >
                            <Plus className="size-4" aria-hidden />
                            New
                        </button>
                    </div>

                    <ul
                        className="divide-y divide-border-subtle border border-border-subtle rounded-md bg-background-secondary/60 dark:bg-background-tertiary/60"
                        role="list"
                    >
                        {filteredProjects.length === 0 ? (
                            <li className="px-4 py-8 text-center text-foreground-muted">
                                {searchQuery ? "No projects found matching your search." : "You don't have any projects yet."}
                            </li>
                        ) : (
                            filteredProjects.map((project) => {
                                const isFavorite = !!favorites[project.id];
    
                                return (
                                    <li
                                        key={project.id}
                                        className="px-4 py-3 hover:bg-background-tertiary/70 transition-colors"
                                    >
                                        <div className="flex items-center justify-between gap-3">
                                            <div className="min-w-0 flex-1 space-y-0.5">
                                                <div className="flex items-baseline gap-2">
                                                    <Folder className="size-4 text-accent" aria-hidden />
                                                    <Link
                                                        href={`/projects?id=${project.id}`}
                                                        className="text-lg font-medium text-foreground hover:text-accent transition-colors"
                                                    >
                                                        {project.name}
                                                    </Link>
                                                    <span className="text-xs uppercase tracking-wide text-foreground-muted">
                                                        {project.is_public ? "Public" : "Private"}
                                                    </span>
                                                </div>
                                                <p className="mt-0.5 text-sm text-foreground-muted truncate">
                                                    {project.description || "No description provided."}
                                                </p>
                                                <div className="mt-1 flex flex-wrap items-center gap-3 text-sm text-foreground-muted">
                                                    <span className="inline-flex items-center gap-1">
                                                        <Clock className="size-3.5" aria-hidden />
                                                        {formatDate(project.created_at)}
                                                    </span>
                                                    <span className="inline-flex items-center gap-1">
                                                        <GitBranch className="size-3.5" aria-hidden />
                                                        0 versions
                                                    </span>
                                                    <span className="inline-flex items-center gap-1">
                                                        <Activity className="size-3.5" aria-hidden />
                                                        0 stars
                                                    </span>
                                                </div>
                                            </div>
                                            <button
                                                type="button"
                                                className="inline-flex items-center gap-1 rounded-md border border-border-subtle bg-background px-3 py-1 text-sm font-medium text-foreground-muted hover:border-accent/40 hover:text-foreground transition-colors"
                                                onClick={() =>
                                                    setFavorites((prev) => ({
                                                        ...prev,
                                                        [project.id]: !isFavorite,
                                                    }))
                                                }
                                            >
                                                <Heart
                                                    className={`size-3.5 ${
                                                        isFavorite
                                                            ? "text-red-500 fill-red-500"
                                                            : "text-foreground-muted"
                                                    }`}
                                                    aria-hidden
                                                />
                                                Favorite
                                            </button>
                                        </div>
                                    </li>
                                );
                            })
                        )}
                    </ul>
                </section>
            </div>

            {/* New Project Modal — shared design with dashboard */}
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
                            <div className="w-full max-w-md rounded-xl bg-background-secondary dark:bg-background-tertiary border border-border-subtle p-6 shadow-2xl">
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

