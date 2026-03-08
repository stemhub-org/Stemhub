"use client";

import { useState, useEffect, useCallback } from "react";
import { Trash2, UserPlus, Users, AlertTriangle } from "lucide-react";
import { authFetch } from "@/lib/api";
import { useRouter } from "next/navigation";
import type { Collaborator } from "@/types/project";

interface ProjectSettingsProps {
    projectId: string;
    ownerId: string;
    currentUserId: string | null;
}

export function ProjectSettings({ projectId, ownerId, currentUserId }: ProjectSettingsProps) {
    const router = useRouter();
    const [collaborators, setCollaborators] = useState<Collaborator[]>([]);
    const [inviteUsername, setInviteUsername] = useState("");
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [successMsg, setSuccessMsg] = useState<string | null>(null);

    const isOwner = ownerId === currentUserId;

    const fetchCollaborators = useCallback(async () => {
        try {
            const data = await authFetch<Collaborator[]>(`/projects/${projectId}/collaborators/`);
            setCollaborators(data);
        } catch (err) {
            console.error("Failed to load collaborators", err);
        }
    }, [projectId]);

    useEffect(() => {
        if (isOwner) {
            fetchCollaborators();
        }
    }, [isOwner, fetchCollaborators]);

    const handleInvite = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!inviteUsername.trim()) return;
        setLoading(true);
        setError(null);
        setSuccessMsg(null);
        try {
            await authFetch(`/projects/${projectId}/collaborators/`, {
                method: "POST",
                body: JSON.stringify({ username: inviteUsername.trim() }),
            });
            setSuccessMsg(`User ${inviteUsername} added successfully!`);
            setInviteUsername("");
            fetchCollaborators();
        } catch (err) {
            setError(err instanceof Error ? err.message : "Failed to invite user");
        } finally {
            setLoading(false);
        }
    };

    const handleRemoveCollaborator = async (userId: string) => {
        if (!confirm("Are you sure you want to remove this collaborator?")) return;
        setLoading(true);
        setError(null);
        try {
            await authFetch(`/projects/${projectId}/collaborators/${userId}`, {
                method: "DELETE",
            });
            fetchCollaborators();
        } catch (err) {
            setError(err instanceof Error ? err.message : "Failed to remove collaborator");
        } finally {
            setLoading(false);
        }
    };

    const handleDeleteProject = async () => {
        if (!confirm("Are you absolutely sure you want to delete this project? This action cannot be undone.")) return;
        setLoading(true);
        setError(null);
        try {
            await authFetch(`/projects/${projectId}`, {
                method: "DELETE",
            });
            router.push("/dashboard");
        } catch (err) {
            setError(err instanceof Error ? err.message : "Failed to delete project");
            setLoading(false);
        }
    };

    if (!isOwner) return null;

    return (
        <div className="flex flex-col gap-6">
            <div className="border-b border-foreground/[0.08] bg-foreground/[0.02] px-6 py-3">
                <h3 className="text-sm font-medium text-foreground flex items-center gap-2" style={{ fontFamily: "var(--font-syne)" }}>
                    <Users className="size-4 shrink-0 text-accent" />
                    Manage Collaborators
                </h3>
            </div>
            
            <div className="px-6 flex flex-col gap-4">
                {error && <div className="text-red-500 text-sm bg-red-500/10 p-3 rounded-md">{error}</div>}
                {successMsg && <div className="text-green-500 text-sm bg-green-500/10 p-3 rounded-md">{successMsg}</div>}

                <form onSubmit={handleInvite} className="flex gap-3">
                    <input
                        type="text"
                        placeholder="Username to invite..."
                        value={inviteUsername}
                        onChange={(e) => setInviteUsername(e.target.value)}
                        className="flex-1 bg-background-tertiary border border-border-subtle rounded-md px-3 py-2 text-sm text-foreground focus:outline-none focus:border-accent"
                        disabled={loading}
                    />
                    <button
                        type="submit"
                        disabled={loading || !inviteUsername.trim()}
                        className="flex items-center gap-2 bg-accent hover:bg-accent/90 text-white px-4 py-2 rounded-md text-sm font-medium transition-colors disabled:opacity-50"
                    >
                        <UserPlus className="size-4" />
                        Invite
                    </button>
                </form>

                {collaborators.length > 0 ? (
                    <ul className="divide-y divide-border-subtle rounded-md border border-border-subtle">
                        {collaborators.map((c) => (
                            <li key={c.user_id} className="flex items-center justify-between p-3 flex-wrap gap-2 hover:bg-foreground/[0.02]">
                                <div className="flex flex-col min-w-0">
                                    <span className="text-sm font-medium text-foreground">{c.user?.username || 'Unknown'}</span>
                                    <span className="text-xs text-foreground/60">{c.user?.email}</span>
                                </div>
                                <button
                                    onClick={() => handleRemoveCollaborator(c.user_id)}
                                    disabled={loading}
                                    className="text-red-400 hover:text-red-500 text-xs font-medium px-2 py-1 rounded bg-red-500/10 hover:bg-red-500/20 transition-colors"
                                >
                                    Remove
                                </button>
                            </li>
                        ))}
                    </ul>
                ) : (
                    <p className="text-sm text-foreground/50 text-center py-4 border border-dashed border-border-subtle rounded-md">No collaborators yet.</p>
                )}
            </div>

            <div className="border-b border-foreground/[0.08] bg-foreground/[0.02] px-6 py-3 mt-4">
                <h3 className="text-sm font-medium text-red-500 flex items-center gap-2" style={{ fontFamily: "var(--font-syne)" }}>
                    <AlertTriangle className="size-4 shrink-0" />
                    Danger Zone
                </h3>
            </div>
            
            <div className="px-6 pb-6">
                <div className="border border-red-500/20 rounded-md p-4 flex flex-col md:flex-row items-start md:items-center justify-between gap-4">
                    <div>
                        <p className="text-sm font-medium text-foreground">Delete this project</p>
                        <p className="text-xs text-foreground/60 mt-1">Once you delete a project, there is no going back. Please be certain.</p>
                    </div>
                    <button
                        onClick={handleDeleteProject}
                        disabled={loading}
                        className="flex items-center gap-2 shrink-0 bg-red-500/10 hover:bg-red-500 text-red-500 hover:text-white border border-red-500/20 hover:border-red-500 px-4 py-2 rounded-md text-sm font-medium transition-all focus:outline-none"
                    >
                        <Trash2 className="size-4" />
                        Delete Project
                    </button>
                </div>
            </div>
        </div>
    );
}
