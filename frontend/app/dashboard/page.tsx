"use client";

import { useEffect, useState, useCallback } from "react";
import { useRouter } from "next/navigation";
import { motion } from "framer-motion";
import {
    GitBranch,
    Clock,
    Plus,
    DownloadCloud,
    Users,
    HardDrive,
    ChevronRight,
} from "lucide-react";
import { authFetch } from "@/lib/api";
import { Button } from "@/components/ui/Button";
import { Card } from "@/components/ui/Card";
import { Badge } from "@/components/ui/Badge";
import NewProjectModal from "@/components/NewProjectModal";
import { useToast } from "@/components/ToastProvider";

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
    const toast = useToast();
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
            toast.error(err instanceof Error ? err.message : "Failed to create project");
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
                        <Button variant="outline" icon={<DownloadCloud size={16} />}>
                            Import
                        </Button>
                        <Button variant="solid" icon={<Plus size={16} />} onClick={() => setShowNewProject(true)}>
                            New Project
                        </Button>
                    </div>
                </motion.div>

                {/* Stats Overview */}
                <motion.div
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.4, delay: 0.1 }}
                    className="grid grid-cols-1 md:grid-cols-3 gap-4"
                >
                    <Card interactive>
                        <div className="flex items-center justify-between mb-8">
                            <div className="text-foreground transition-colors flex items-center gap-2 text-sm font-medium">
                                <HardDrive size={16} className="text-accent group-hover:scale-110 transition-transform" />
                                <span>Storage</span>
                            </div>
                            <Badge size="md">Active</Badge>
                        </div>
                        <div className="flex items-baseline gap-2">
                            <h3 className="text-3xl font-semibold tracking-tight text-foreground">{projects.length}</h3>
                            <span className="text-sm border-l border-border-subtle pl-2 text-foreground-muted">Projects</span>
                        </div>
                    </Card>

                    <Card interactive>
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
                    </Card>

                    <Card interactive>
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
                    </Card>
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
                        <Card padding="lg" className="text-center">
                            <p className="text-foreground-muted text-sm mb-4">No projects yet. Create your first one!</p>
                            <Button variant="solid" icon={<Plus size={16} />} onClick={() => setShowNewProject(true)}>
                                New Project
                            </Button>
                        </Card>
                    ) : (
                        <div className="flex flex-col gap-3">
                            {projects.map((project) => (
                                <Card
                                    key={project.id}
                                    interactive
                                    padding="sm"
                                    onClick={() => router.push(`/projects?id=${project.id}`)}
                                    className="flex flex-col sm:flex-row sm:items-center justify-between"
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
                                        <Badge>{project.category}</Badge>
                                        <ChevronRight size={14} className="text-foreground-muted group-hover:text-accent transition-colors" />
                                    </div>
                                </Card>
                            ))}
                        </div>
                    )}
                </motion.div>
            </div>

            <NewProjectModal
                open={showNewProject}
                onClose={() => setShowNewProject(false)}
                name={newProjectName}
                onNameChange={setNewProjectName}
                description={newProjectDesc}
                onDescriptionChange={setNewProjectDesc}
                category={newProjectCategory}
                onCategoryChange={setNewProjectCategory}
                creating={creating}
                onCreate={handleCreateProject}
            />
        </div>
    );
}
