"use client";

import { Folder, FileAudio, Trash2 } from "lucide-react";
import type { Track } from "@/types/project";

function formatTimeAgo(dateString: string | null) {
    if (!dateString) return "Unknown date";
    const date = new Date(dateString);
    const now = new Date();
    const seconds = Math.floor((now.getTime() - date.getTime()) / 1000);
    
    if (seconds < 60) return "Just now";
    
    const minutes = Math.floor(seconds / 60);
    if (minutes < 60) return `${minutes} minute${minutes > 1 ? 's' : ''} ago`;
    
    const hours = Math.floor(minutes / 60);
    if (hours < 24) return `${hours} hour${hours > 1 ? 's' : ''} ago`;
    
    const days = Math.floor(hours / 24);
    if (days < 30) return `${days} day${days > 1 ? 's' : ''} ago`;
    
    const months = Math.floor(days / 30);
    if (months < 12) return `${months} month${months > 1 ? 's' : ''} ago`;
    
    const years = Math.floor(months / 12);
    return `${years} year${years > 1 ? 's' : ''} ago`;
}

interface RepositoryFileListProps {
    tracks: Track[];
    isOwner?: boolean;
    onDeleteTrack?: (trackId: string) => void;
}

function ItemIcon({ fileType }: { fileType: string }) {
    const iconClass = "size-5 shrink-0 text-accent";
    if (fileType === ".json" || fileType === "folder") {
        return <Folder className={iconClass} aria-hidden />;
    }
    return <FileAudio className={iconClass} aria-hidden />;
}

export function RepositoryFileList({ tracks, isOwner, onDeleteTrack }: RepositoryFileListProps) {
    return (
        <div>
            <div className="border-b border-foreground/[0.08] bg-foreground/[0.02] px-6 py-3">
                <h3
                    className="mb-3 text-sm font-medium text-foreground"
                    style={{ fontFamily: "var(--font-syne)" }}
                >
                    Files
                </h3>
                <div className={`grid gap-4 text-xs font-medium uppercase tracking-wide text-foreground/60 ${isOwner ? 'grid-cols-[1fr_2fr_1fr_auto_auto]' : 'grid-cols-[1fr_2fr_1fr_auto]'}`}>
                    <span>Name</span>
                    <span>Type</span>
                    <span>Added</span>
                    <span>Format</span>
                    {isOwner && <span></span>}
                </div>
            </div>
            {tracks.length === 0 ? (
                <div className="px-6 py-8 text-center text-sm text-foreground/50">
                    No files yet
                </div>
            ) : (
                <ul className="divide-y divide-foreground/[0.06]" role="list">
                    {tracks.map((track) => (
                        <li key={track.id} className={`grid gap-4 px-6 py-4 transition-colors hover:bg-foreground/[0.03] items-center ${isOwner ? 'grid-cols-[1fr_2fr_1fr_auto_auto]' : 'grid-cols-[1fr_2fr_1fr_auto]'}`}>
                            <span className="flex min-w-0 items-center gap-3 truncate">
                                <ItemIcon fileType={track.file_type} />
                                <span className="truncate font-medium text-foreground">
                                    {track.name}
                                </span>
                            </span>
                            <span className="truncate text-sm text-foreground/70">
                                Audio Track
                            </span>
                            <span className="truncate text-sm text-foreground/50">
                                {track.created_at ? formatTimeAgo(track.created_at) : "Unknown"}
                            </span>
                            <span className="shrink-0 text-sm text-foreground/60">
                                {track.file_type}
                            </span>
                            {isOwner && (
                                <button
                                    onClick={(e) => {
                                        e.stopPropagation();
                                        if (onDeleteTrack && window.confirm(`Are you sure you want to delete track '${track.name}'?`)) {
                                            onDeleteTrack(track.id);
                                        }
                                    }}
                                    className="text-foreground/40 hover:text-red-500 transition-colors shrink-0 flex items-center justify-center p-1"
                                    title="Delete Track"
                                >
                                    <Trash2 className="size-4" />
                                </button>
                            )}
                        </li>
                    ))}
                </ul>
            )}
        </div>
    );
}
