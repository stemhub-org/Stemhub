"use client";

import { useState, useRef, useEffect } from "react";
import { Download, FileAudio, Package2, Archive, ChevronDown } from "lucide-react";
import { API_URL, authFetch } from "@/lib/api";

const DAW_OPTIONS = [
    { id: "ableton", label: "Ableton Live" },
    { id: "flstudio", label: "FL Studio" },
    { id: "protools", label: "Pro Tools" },
];

interface QuickExportProps {
    projectId?: string | null;
    projectName?: string;
    branchName?: string;
    latestVersionId?: string | null;
    hasPreview?: boolean;
    hasArtifact?: boolean;
}

function sanitizeFilenamePart(value: string): string {
    return value
        .trim()
        .toLowerCase()
        .replace(/[^a-z0-9-_]+/g, "-")
        .replace(/-+/g, "-")
        .replace(/^-|-$/g, "") || "project";
}

export function QuickExport({
    projectId,
    projectName,
    branchName,
    latestVersionId,
    hasPreview,
    hasArtifact,
}: QuickExportProps) {
    const [isZipOpen, setIsZipOpen] = useState(false);
    const dropdownRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
        function handleClickOutside(event: MouseEvent) {
            if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
                setIsZipOpen(false);
            }
        }
        document.addEventListener("mousedown", handleClickOutside);
        return () => document.removeEventListener("mousedown", handleClickOutside);
    }, []);

    const handleDownloadPreview = async () => {
        if (!projectId || !hasPreview) return;
        try {
            const response = await authFetch<Response>(`/projects/${projectId}/preview`);
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement("a");
            a.href = url;
            a.download = "project-preview.wav";
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
        } catch (err) {
            console.error("Download failed:", err);
            alert("Failed to download preview");
        }
    };

    const handleDownloadArtifact = async () => {
        if (!latestVersionId || !hasArtifact) return;

        try {
            const response = await fetch(`${API_URL}/versions/${latestVersionId}/artifact`, {
                method: "GET",
                credentials: "include",
            });

            if (response.status === 401) {
                alert("Your session expired. Please sign in again.");
                return;
            }

            if (response.status === 404) {
                alert("Artifact ZIP not found for this version.");
                return;
            }

            if (!response.ok) {
                alert("Failed to download artifact ZIP. Please try again.");
                return;
            }

            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement("a");
            a.href = url;
            const safeProject = sanitizeFilenamePart(projectName || "project");
            const safeVersion = sanitizeFilenamePart(branchName || "version");
            const versionShort = latestVersionId.substring(0, 8);
            a.download = `${safeProject}-${safeVersion}-${versionShort}.zip`;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
        } catch (err) {
            console.error("Artifact download failed:", err);
            alert("Network error while downloading artifact ZIP.");
        }
    };

    return (
        <div className="relative flex flex-col gap-4">
            <div className="flex items-center gap-2">
                <Package2 className="size-4 text-accent" aria-hidden />
                <h3
                    className="text-sm font-medium text-foreground"
                    style={{ fontFamily: "var(--font-syne)" }}
                >
                    Quick Export
                </h3>
            </div>
            <div className="grid grid-cols-1 gap-3 md:grid-cols-2 md:[&>*]:h-full">
                {hasPreview ? (
                    <button
                        onClick={handleDownloadPreview}
                        className="flex w-full flex-col justify-between rounded-xl border border-border-subtle dark:border-accent/40 bg-background-secondary/50 dark:bg-background-tertiary/50 px-4 py-3 text-left transition-colors dark:hover:bg-accent/5"
                    >
                        <div className="mb-2 flex items-center justify-between gap-2">
                            <div className="flex items-center gap-2">
                                <FileAudio className="size-4 text-accent" aria-hidden />
                                <span className="text-sm font-medium text-foreground">
                                    Project Preview
                                </span>
                            </div>
                            <span className="text-xs text-foreground/60"><Download className="size-3" /></span>
                        </div>
                        <p className="text-xs text-foreground/60">Audio Preview</p>
                    </button>
                ) : (
                    <button
                        type="button"
                        disabled
                        className="flex w-full flex-col justify-between rounded-xl border border-border-subtle bg-background-secondary/20 dark:bg-background-tertiary/20 px-4 py-3 text-left opacity-50 cursor-not-allowed"
                    >
                        <div className="mb-2 flex items-center justify-between gap-2">
                            <div className="flex items-center gap-2">
                                <FileAudio className="size-4" aria-hidden />
                                <span className="text-sm font-medium">No Preview Available</span>
                            </div>
                        </div>
                        <p className="text-xs">Nothing to export</p>
                    </button>
                )}

                <div className="relative" ref={dropdownRef}>
                    <button
                        type="button"
                        disabled={!latestVersionId || !hasArtifact}
                        onClick={() => setIsZipOpen((prev) => !prev)}
                        className="flex h-full w-full flex-col justify-between rounded-xl border border-border-subtle bg-background-secondary/50 dark:bg-background-tertiary/50 px-4 py-3 text-left transition-colors dark:hover:bg-foreground/[0.03] disabled:opacity-50 disabled:cursor-not-allowed"
                        aria-expanded={isZipOpen}
                        aria-haspopup="true"
                    >
                        <div className="mb-2 flex items-center justify-between gap-2">
                            <div className="flex items-center gap-2">
                                <Archive className="size-4 text-accent" aria-hidden />
                                <span className="text-sm font-medium text-foreground">
                                    All Stems
                                </span>
                            </div>
                            <span className="text-xs text-foreground/60">ZIP</span>
                        </div>
                        <div className="flex items-center gap-2 text-xs text-foreground/60 mt-2">
                            <span>ZIP Archive</span>
                            <span className="h-[1px] flex-1 bg-foreground/10" />
                            <span className="flex items-center gap-1 text-foreground/70">
                                <span>Export for…</span>
                                <ChevronDown
                                    className={`size-3 transition-transform ${
                                        isZipOpen ? "rotate-180" : ""
                                    }`}
                                    aria-hidden
                                />
                            </span>
                        </div>
                    </button>
                    {isZipOpen && latestVersionId && hasArtifact && (
                        <div className="absolute left-0 right-0 top-full z-20 mt-2 rounded-xl border border-border-subtle bg-background-secondary dark:bg-background-tertiary py-2 shadow-xl">
                            {DAW_OPTIONS.map((daw) => (
                                <button
                                    key={daw.id}
                                    type="button"
                                    onClick={() => {
                                        setIsZipOpen(false);
                                        handleDownloadArtifact();
                                    }}
                                    className="flex w-full items-center justify-between px-4 py-2.5 text-left text-sm text-foreground transition-colors hover:bg-background-tertiary"
                                >
                                    <span className="flex items-center gap-2">
                                        <Download className="size-4 text-accent" aria-hidden />
                                        {daw.label}
                                    </span>
                                    <span className="text-xs text-foreground/50">ZIP</span>
                                </button>
                            ))}
                        </div>
                    )}
                </div>
            </div>
        </div>
    );
}
