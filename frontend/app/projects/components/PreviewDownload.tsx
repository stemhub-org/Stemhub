"use client";

import { useState } from "react";
import { Download, FileAudio, Loader2 } from "lucide-react";
import { authFetch } from "@/lib/api";
import { previewFilename, saveBlob } from "@/lib/download";
import { useToast } from "@/components/ToastProvider";

interface PreviewDownloadProps {
    projectId?: string | null;
    projectName?: string;
    hasPreview?: boolean;
}

export function PreviewDownload({ projectId, projectName, hasPreview }: PreviewDownloadProps) {
    const [isDownloading, setIsDownloading] = useState(false);
    const toast = useToast();

    const handleDownloadPreview = async () => {
        if (!projectId || !hasPreview || isDownloading) return;
        setIsDownloading(true);
        try {
            const response = await authFetch<Response>(`/projects/${projectId}/preview`);
            const blob = await response.blob();
            const filename = previewFilename(
                response.headers.get("content-disposition"),
                response.headers.get("content-type") || blob.type,
                projectName || "project",
            );
            saveBlob(blob, filename);
        } catch (err) {
            toast.error(err instanceof Error ? err.message : "Failed to download the preview");
        } finally {
            setIsDownloading(false);
        }
    };

    return (
        <div className="relative flex flex-col gap-4">
            <div className="flex items-center gap-2">
                <Download className="size-4 text-accent" aria-hidden />
                <h3
                    className="text-sm font-medium text-foreground"
                    style={{ fontFamily: "var(--font-syne)" }}
                >
                    Download
                </h3>
            </div>
            {hasPreview ? (
                <button
                    type="button"
                    onClick={handleDownloadPreview}
                    disabled={isDownloading}
                    className="flex w-full flex-col justify-between rounded-xl border border-border-subtle dark:border-accent/40 bg-background-secondary/50 dark:bg-background-tertiary/50 px-4 py-3 text-left transition-colors dark:hover:bg-accent/5 disabled:opacity-70 disabled:cursor-wait"
                >
                    <div className="mb-2 flex items-center justify-between gap-2">
                        <div className="flex items-center gap-2">
                            <FileAudio className="size-4 text-accent" aria-hidden />
                            <span className="text-sm font-medium text-foreground">
                                Download preview
                            </span>
                        </div>
                        <span className="text-xs text-foreground/60">
                            {isDownloading ? (
                                <Loader2 className="size-3 animate-spin" aria-label="Downloading…" />
                            ) : (
                                <Download className="size-3" aria-hidden />
                            )}
                        </span>
                    </div>
                    <p className="text-xs text-foreground/60">Audio file, as uploaded</p>
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
                    <p className="text-xs">Nothing to download</p>
                </button>
            )}
        </div>
    );
}
