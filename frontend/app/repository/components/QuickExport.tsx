"use client";

import { useState, useRef, useEffect } from "react";
import { Download, FileAudio, Archive, ChevronDown, Package2 } from "lucide-react";

const DAW_OPTIONS = [
    { id: "ableton", label: "Ableton Live" },
    { id: "flstudio", label: "FL Studio" },
    { id: "protools", label: "Pro Tools" },
];

export function QuickExport() {
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
                <button
                    type="button"
                    className="flex flex-col justify-between rounded-xl border border-accent/40 bg-foreground/[0.01] px-4 py-3 text-left transition-colors hover:bg-accent/5"
                >
                    <div className="mb-2 flex items-center justify-between gap-2">
                        <div className="flex items-center gap-2">
                            <FileAudio className="size-4 text-accent" aria-hidden />
                            <span className="text-sm font-medium text-foreground">
                                Master Track
                            </span>
                        </div>
                        <span className="text-xs text-foreground/60">48.3 MB</span>
                    </div>
                    <p className="text-xs text-foreground/60">WAV 24-bit</p>
                </button>

                <div className="relative" ref={dropdownRef}>
                    <button
                        type="button"
                        onClick={() => setIsZipOpen((prev) => !prev)}
                        className="flex h-full w-full flex-col justify-between rounded-xl border border-foreground/[0.08] bg-foreground/[0.01] px-4 py-3 text-left transition-colors hover:bg-foreground/[0.03]"
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
                        <div className="flex items-center gap-2 text-xs text-foreground/60">
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
                        <div className="absolute left-0 right-0 top-full z-20 mt-2 rounded-xl border border-foreground/[0.08] bg-white py-2 shadow-xl">
                            {DAW_OPTIONS.map((daw) => (
                                <button
                                    key={daw.id}
                                    type="button"
                                    onClick={() => {
                                        setIsZipOpen(false);
                                    }}
                                    className="flex w-full items-center justify-between px-4 py-2.5 text-left text-sm text-foreground transition-colors hover:bg-foreground/[0.04]"
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
