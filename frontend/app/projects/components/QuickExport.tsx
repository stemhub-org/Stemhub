"use client";

import { useState, useRef, useEffect } from "react";
import { Download, FileAudio, Package2, Archive, ChevronDown } from "lucide-react";
import { Track } from "@/types/project";
import { API_URL } from "@/lib/api";

const DAW_OPTIONS = [
    { id: "ableton", label: "Ableton Live" },
    { id: "flstudio", label: "FL Studio" },
    { id: "protools", label: "Pro Tools" },
];

export function QuickExport({ track }: { track?: Track }) {
    const [isZipOpen, setIsZipOpen] = useState(false);
    const dropdownRef = useRef<HTMLDivElement>(null);
    const [token, setToken] = useState<string | null>(null);

    useEffect(() => {
        setToken(localStorage.getItem("token"));
    }, []);

    useEffect(() => {
        function handleClickOutside(event: MouseEvent) {
            if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
                setIsZipOpen(false);
            }
        }
        document.addEventListener("mousedown", handleClickOutside);
        return () => document.removeEventListener("mousedown", handleClickOutside);
    }, []);

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
                {track && token ? (
                    <a
                        href={`${API_URL}/tracks/${track.id}/audio?token=${token}`}
                        download
                        className="flex w-full flex-col justify-between rounded-xl border border-border-subtle dark:border-accent/40 bg-background-tertiary/50 dark:bg-foreground/[0.01] px-4 py-3 text-left transition-colors dark:hover:bg-accent/5"
                    >
                        <div className="mb-2 flex items-center justify-between gap-2">
                            <div className="flex items-center gap-2">
                                <FileAudio className="size-4 text-accent" aria-hidden />
                                <span className="text-sm font-medium text-foreground">
                                    {track.name}
                                </span>
                            </div>
                            <span className="text-xs text-foreground/60"><Download className="size-3" /></span>
                        </div>
                        <p className="text-xs text-foreground/60">{track.file_type.toUpperCase().replace('.', '')}</p>
                    </a>
                ) : (
                    <button
                        type="button"
                        disabled
                        className="flex w-full flex-col justify-between rounded-xl border border-border-subtle bg-background-tertiary/20 px-4 py-3 text-left opacity-50 cursor-not-allowed"
                    >
                        <div className="mb-2 flex items-center justify-between gap-2">
                            <div className="flex items-center gap-2">
                                <FileAudio className="size-4" aria-hidden />
                                <span className="text-sm font-medium">No Track Available</span>
                            </div>
                        </div>
                        <p className="text-xs">Nothing to export</p>
                    </button>
                )}

                <div className="relative" ref={dropdownRef}>
                    <button
                        type="button"
                        onClick={() => setIsZipOpen((prev) => !prev)}
                        className="flex h-full w-full flex-col justify-between rounded-xl border border-border-subtle bg-background-tertiary/50 dark:bg-foreground/[0.01] px-4 py-3 text-left transition-colors dark:hover:bg-foreground/[0.03]"
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
                            <span className="text-xs text-foreground/60">234 MB</span>
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
                    {isZipOpen && (
                        <div className="absolute left-0 right-0 top-full z-20 mt-2 rounded-xl border border-border-subtle bg-background-secondary py-2 shadow-xl">
                            {DAW_OPTIONS.map((daw) => (
                                <button
                                    key={daw.id}
                                    type="button"
                                    onClick={() => {
                                        setIsZipOpen(false);
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
