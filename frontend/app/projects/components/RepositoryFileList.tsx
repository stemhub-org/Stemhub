"use client";

import { Folder, FileAudio } from "lucide-react";
import type { TrackSummary } from "@/types/project";
import { Badge } from "@/components/ui/Badge";

function formatDuration(seconds: number | null): string {
    if (seconds == null) return "—";
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins}:${secs.toString().padStart(2, "0")}`;
}

function formatSize(bytes: number | null): string {
    if (bytes == null) return "—";
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

interface RepositoryFileListProps {
    tracks: TrackSummary[];
}

function ItemIcon({ fileType }: { fileType: string | null }) {
    const iconClass = "size-5 shrink-0 text-accent";
    if (fileType === "json" || fileType === "folder") {
        return <Folder className={iconClass} aria-hidden />;
    }
    return <FileAudio className={iconClass} aria-hidden />;
}

export function RepositoryFileList({ tracks }: RepositoryFileListProps) {
    return (
        <div>
            <div className="border-b border-foreground/[0.08] bg-foreground/[0.02] px-6 py-3">
                <h3
                    className="mb-3 text-sm font-medium text-foreground"
                    style={{ fontFamily: "var(--font-syne)" }}
                >
                    Stems
                </h3>
                <div className="grid grid-cols-[1fr_auto_auto_auto_auto] gap-4 text-xs font-medium uppercase tracking-wide text-foreground/60">
                    <span>Name</span>
                    <span>Key</span>
                    <span>BPM</span>
                    <span>Duration</span>
                    <span>Size</span>
                </div>
            </div>
            {tracks.length === 0 ? (
                <div className="px-6 py-8 text-center text-sm text-foreground/50">
                    No per-stem data available for this version.
                </div>
            ) : (
                <ul className="divide-y divide-foreground/[0.06]" role="list">
                    {tracks.map((track) => (
                        <li
                            key={track.id}
                            className="grid grid-cols-[1fr_auto_auto_auto_auto] items-center gap-4 px-6 py-4 transition-colors hover:bg-foreground/[0.03]"
                        >
                            <span className="flex min-w-0 items-center gap-3 truncate">
                                <ItemIcon fileType={track.file_type} />
                                <span className="truncate font-medium text-foreground">
                                    {track.name}
                                </span>
                                {track.source === "manifest" && <Badge size="sm">stored</Badge>}
                            </span>
                            <span className="text-sm text-foreground/70">{track.key ?? "—"}</span>
                            <span className="text-sm text-foreground/70">{track.bpm ?? "—"}</span>
                            <span className="text-sm text-foreground/70">{formatDuration(track.duration_seconds)}</span>
                            <span className="text-sm text-foreground/60">{formatSize(track.size_bytes)}</span>
                        </li>
                    ))}
                </ul>
            )}
        </div>
    );
}
