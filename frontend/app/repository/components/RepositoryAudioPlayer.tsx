"use client";

import { Play, Download } from "lucide-react";

const PLACEHOLDER_TRACK_LABEL = "Master Track Preview";
const PLACEHOLDER_TRACK_FILE = "Master_v3.wav";
const PLACEHOLDER_CURRENT_TIME = "0:00";
const PLACEHOLDER_DURATION = "4:27";
const PLACEHOLDER_SAMPLE_RATE = "48 kHz";
const PLACEHOLDER_BIT_DEPTH = "24 bit";

export function RepositoryAudioPlayer() {
    return (
        <div className="flex flex-col gap-4">
            <div>
                <p className="text-sm font-medium text-foreground">
                    {PLACEHOLDER_TRACK_LABEL}
                </p>
                <p className="text-xs text-foreground/70">
                    {PLACEHOLDER_TRACK_FILE}
                </p>
            </div>
            <div className="flex items-center gap-3">
                <button
                    type="button"
                    className="flex size-10 shrink-0 items-center justify-center rounded-full bg-accent text-white transition-opacity hover:opacity-90"
                    aria-label="Play"
                >
                    <Play className="size-5 fill-current" aria-hidden />
                </button>
                <div className="min-w-0 flex-1">
                    <div className="h-2 w-full rounded-full bg-foreground/10" />
                    <p className="mt-1 text-xs text-foreground/60">
                        {PLACEHOLDER_CURRENT_TIME} / {PLACEHOLDER_DURATION}
                    </p>
                </div>
            </div>
            <div className="flex flex-wrap gap-4 text-xs text-foreground/70">
                <span>Sample Rate {PLACEHOLDER_SAMPLE_RATE}</span>
                <span>Bit Depth {PLACEHOLDER_BIT_DEPTH}</span>
            </div>
            <button
                type="button"
                className="flex w-full items-center justify-center gap-2 rounded-xl bg-accent px-4 py-3 text-sm font-medium text-white transition-opacity hover:opacity-90"
            >
                <Download className="size-4" aria-hidden />
                Download Master
            </button>
        </div>
    );
}
