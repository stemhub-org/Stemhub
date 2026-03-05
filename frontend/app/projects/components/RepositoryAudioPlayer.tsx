"use client";

import { Play } from "lucide-react";

const PLACEHOLDER_TRACK_LABEL = "Master Track Preview";
const PLACEHOLDER_TRACK_FILE = "Master_v3.wav";
const PLACEHOLDER_CURRENT_TIME = "1:34";
const PLACEHOLDER_DURATION = "4:27";
const PLACEHOLDER_SAMPLE_RATE = "48 kHz";
const PLACEHOLDER_BIT_DEPTH = "24 bit";

function WaveformPlaceholder() {
    const bars = [
        0.4, 0.7, 0.9, 0.6, 0.3, 0.5, 0.8, 0.9, 0.7, 0.4, 0.3, 0.5, 0.6, 0.8,
        0.9, 0.7, 0.5, 0.4, 0.6, 0.9, 0.8, 0.6, 0.3, 0.4, 0.5, 0.7, 0.9, 0.8,
        0.6, 0.4, 0.3, 0.5, 0.7, 0.9, 0.8, 0.5, 0.4, 0.6, 0.7, 0.9, 0.8, 0.6,
        0.4, 0.3, 0.5, 0.7, 0.9, 0.8, 0.6, 0.4,
    ];

    const progress = 0.4;
    const progressIndex = Math.floor(bars.length * progress);

    return (
        <div className="flex h-14 w-full items-center rounded-lg bg-background-tertiary px-4">
            <div className="flex h-full w-full items-end gap-[1px]">
                {bars.map((height, index) => {
                    const baseHeight = 18;
                    const extra = height * 26;
                    const barHeight = baseHeight + extra;
                    const isPlayed = index <= progressIndex;

                    return (
                        <div
                            key={index}
                            className="flex-1 rounded-[1px]"
                            style={{
                                height: `${barHeight}px`,
                                backgroundColor: isPlayed
                                    ? "var(--accent)"
                                    : "var(--border-subtle)",
                            }}
                        />
                    );
                })}
            </div>
        </div>
    );
}

export function RepositoryAudioPlayer() {
    return (
        <div className="flex flex-col gap-6">
            <div>
                <p className="text-base font-medium text-foreground">
                    {PLACEHOLDER_TRACK_LABEL}
                </p>
                <p className="mt-0.5 text-sm text-foreground/70">
                    {PLACEHOLDER_TRACK_FILE}
                </p>
            </div>
            <WaveformPlaceholder />
            <div className="flex items-center gap-4">
                <button
                    type="button"
                    className="flex size-12 shrink-0 items-center justify-center rounded-full bg-accent text-white transition-opacity hover:opacity-90"
                    aria-label="Play"
                >
                    <Play className="size-6 fill-current" aria-hidden />
                </button>
                <div className="min-w-0 flex-1">
                    <div className="h-1.5 w-full rounded-full bg-foreground/10">
                        <div className="h-full w-2/5 rounded-full bg-accent" />
                    </div>
                    <p className="mt-1 text-sm text-foreground/60">
                        {PLACEHOLDER_CURRENT_TIME} / {PLACEHOLDER_DURATION}
                    </p>
                </div>
            </div>
            <div className="flex flex-wrap gap-4 text-sm text-foreground/70">
                <span>Sample Rate {PLACEHOLDER_SAMPLE_RATE}</span>
                <span>Bit Depth {PLACEHOLDER_BIT_DEPTH}</span>
            </div>
        </div>
    );
}
