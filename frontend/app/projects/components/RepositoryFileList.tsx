"use client";

import { Folder, FileAudio } from "lucide-react";
import type { Track } from "@/types/project";

interface RepositoryFileListProps {
    tracks: Track[];
}

function ItemIcon({ fileType }: { fileType: string }) {
    const iconClass = "size-5 shrink-0 text-accent";
    if (fileType === ".json" || fileType === "folder") {
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
                    Files
                </h3>
                <div className="grid grid-cols-[1fr_2fr_auto] gap-4 text-xs font-medium uppercase tracking-wide text-foreground/60">
                    <span>Name</span>
                    <span>Type</span>
                    <span>Format</span>
                </div>
            </div>
            {tracks.length === 0 ? (
                <div className="px-6 py-8 text-center text-sm text-foreground/50">
                    No files yet
                </div>
            ) : (
                <ul className="divide-y divide-foreground/[0.06]" role="list">
                    {tracks.map((track) => (
                        <li key={track.id}>
                            <button
                                type="button"
                                className="grid w-full grid-cols-[1fr_2fr_auto] gap-4 px-6 py-4 text-left transition-colors hover:bg-foreground/[0.03]"
                            >
                                <span className="flex min-w-0 items-center gap-3 truncate">
                                    <ItemIcon fileType={track.file_type} />
                                    <span className="truncate font-medium text-foreground">
                                        {track.name}
                                    </span>
                                </span>
                                <span className="truncate text-sm text-foreground/70">
                                    Audio Track
                                </span>
                                <span className="shrink-0 text-sm text-foreground/60">
                                    {track.file_type}
                                </span>
                            </button>
                        </li>
                    ))}
                </ul>
            )}
        </div>
    );
}
