"use client";

import { TrendingUp } from "lucide-react";

const PLACEHOLDER_TOTAL_COMMITS = 282;
const PLACEHOLDER_CONTRIBUTORS = 3;

const GRID_COLS = 26;
const GRID_ROWS = 7;
const CELLS = GRID_COLS * GRID_ROWS;

function seeded(i: number, row: number): number {
    const n = (i + 1) * 12.9898 + row * 78.233;
    return ((Math.sin(n) * 43758.5453) % 1 + 1) % 1;
}

const INTENSITIES = Array.from({ length: CELLS }, (_, i) => {
    const row = Math.floor(i / GRID_COLS);
    const r = seeded(i, row);
    if (r < 0.45) return 0;
    if (r < 0.65) return 0.25;
    if (r < 0.8) return 0.5;
    if (r < 0.92) return 0.75;
    return 1;
});

const LEGEND_LEVELS = [0, 0.2, 0.45, 0.7, 1];

export function ContributionActivity() {
    return (
        <div className="flex flex-col gap-3">
            <div className="flex items-center gap-2">
                <TrendingUp className="size-4 text-foreground/60" aria-hidden />
                <h3
                    className="text-sm font-medium text-foreground"
                    style={{ fontFamily: "var(--font-syne)" }}
                >
                    Contribution Activity
                </h3>
            </div>
            <div
                className="grid w-full gap-0.5"
                style={{ gridTemplateColumns: `repeat(${GRID_COLS}, minmax(0, 1fr))` }}
            >
                {Array.from({ length: CELLS }, (_, i) => {
                    const intensity = INTENSITIES[i] ?? 0;
                    return (
                        <div
                            key={i}
                            className="aspect-square min-w-0 rounded-[3px] bg-accent"
                            style={{
                                opacity: intensity === 0 ? 0.08 : 0.2 + intensity * 0.8,
                            }}
                            title={`${Math.round(intensity * 100)}%`}
                        />
                    );
                })}
            </div>
            <div className="flex items-center justify-between gap-2 text-[10px] text-foreground/50">
                <span>Less</span>
                <div className="flex items-center gap-0.5" aria-hidden>
                    {LEGEND_LEVELS.map((level, idx) => (
                        <div
                            key={idx}
                            className="size-2.5 rounded-[3px] bg-accent"
                            style={{
                                opacity: level === 0 ? 0.08 : 0.2 + level * 0.8,
                            }}
                        />
                    ))}
                </div>
                <span>More</span>
            </div>
            <div className="mt-1 flex gap-4">
                <div className="rounded-lg bg-foreground/[0.04] px-3 py-2">
                    <p className="text-[10px] uppercase tracking-wide text-foreground/50">
                        Total Changes
                    </p>
                    <p className="text-lg font-medium text-foreground">
                        {PLACEHOLDER_TOTAL_COMMITS}
                    </p>
                </div>
                <div className="rounded-lg bg-foreground/[0.04] px-3 py-2">
                    <p className="text-[10px] uppercase tracking-wide text-foreground/50">
                        Contributors
                    </p>
                    <p className="text-lg font-medium text-foreground">
                        {PLACEHOLDER_CONTRIBUTORS}
                    </p>
                </div>
            </div>
        </div>
    );
}
