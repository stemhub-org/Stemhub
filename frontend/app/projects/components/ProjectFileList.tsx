"use client";

import { FileAudio } from "lucide-react";
import type { AssetSummary } from "@/types/project";

function formatSize(bytes: number | null): string {
    if (bytes == null) return "—";
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

// Splits an asset's path so its folder can be shown dimmed before its file
// name. Uses only `path`: `name` is a display name (no extension), not the
// last path segment, so it can't be used to split the path.
function pathParts(asset: AssetSummary): { folder: string; name: string } {
    if (!asset.path) return { folder: "", name: asset.name };
    const lastSlash = asset.path.lastIndexOf("/");
    return {
        folder: asset.path.slice(0, lastSlash + 1),
        name: asset.path.slice(lastSlash + 1),
    };
}

interface ProjectFileListProps {
    assets: AssetSummary[];
    hasVersion: boolean;
    isLoading?: boolean;
    loadError?: string | null;
}

function emptyMessage(hasVersion: boolean, isLoading?: boolean, loadError?: string | null): string {
    if (!hasVersion) return "No versions on this branch yet.";
    if (isLoading) return "Loading audio & MIDI files…";
    if (loadError) return "Couldn't load the file list for this version.";
    return "No audio & MIDI files in this version.";
}

export function ProjectFileList({ assets, hasVersion, isLoading, loadError }: ProjectFileListProps) {
    return (
        <div>
            <div className="border-b border-foreground/[0.08] bg-foreground/[0.02] px-6 py-3">
                <h3
                    className="mb-3 text-sm font-medium text-foreground"
                    style={{ fontFamily: "var(--font-syne)" }}
                >
                    Audio &amp; MIDI files
                </h3>
                <div className="grid grid-cols-[1fr_auto_auto] gap-4 text-xs font-medium uppercase tracking-wide text-foreground/60">
                    <span>Path</span>
                    <span>Type</span>
                    <span>Size</span>
                </div>
            </div>
            {isLoading || assets.length === 0 ? (
                <div className="px-6 py-8 text-center text-sm text-foreground/50" aria-live="polite">
                    {emptyMessage(hasVersion, isLoading, loadError)}
                </div>
            ) : (
                <ul className="divide-y divide-foreground/[0.06]" role="list">
                    {assets.map((asset) => {
                        const { folder, name } = pathParts(asset);
                        return (
                            <li
                                key={asset.id}
                                className="grid grid-cols-[1fr_auto_auto] items-center gap-4 px-6 py-4 transition-colors hover:bg-foreground/[0.03]"
                            >
                                <span className="flex min-w-0 items-center gap-3" title={asset.path}>
                                    <FileAudio className="size-5 shrink-0 text-accent" aria-hidden />
                                    <span className="truncate">
                                        <span className="text-foreground/50">{folder}</span>
                                        <span className="font-medium text-foreground">{name}</span>
                                    </span>
                                </span>
                                <span className="text-sm uppercase text-foreground/70">{asset.file_type ?? "—"}</span>
                                <span className="text-sm text-foreground/60">{formatSize(asset.size_bytes)}</span>
                            </li>
                        );
                    })}
                </ul>
            )}
        </div>
    );
}
