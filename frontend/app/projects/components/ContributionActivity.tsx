"use client";

import { TrendingUp } from "lucide-react";
import type { DailyActivity } from "@/types/project";

interface ContributionActivityProps {
    dailyActivity: DailyActivity[];
    totalCommits: number;
    totalContributors: number;
}

const GRID_COLS = 26;
const GRID_ROWS = 7;
const CELLS = GRID_COLS * GRID_ROWS;

const LEGEND_LEVELS = [0, 0.2, 0.45, 0.7, 1];

export function ContributionActivity({
    dailyActivity,
    totalCommits,
    totalContributors,
}: ContributionActivityProps) {
    // Compute max count for intensity normalization
    const maxCount = Math.max(1, ...dailyActivity.map((d) => d.count));

    // Map daily activity data to grid cells (last CELLS days)
    const recentDays = dailyActivity.slice(-CELLS);
    const intensities = Array.from({ length: CELLS }, (_, i) => {
        const day = recentDays[i];
        if (!day || day.count === 0) return 0;
        return day.count / maxCount;
    });

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
                    const intensity = intensities[i] ?? 0;
                    return (
                        <div
                            key={i}
                            className="aspect-square min-w-0 rounded-[3px] bg-accent"
                            style={{
                                opacity: intensity === 0 ? 0.08 : 0.2 + intensity * 0.8,
                            }}
                            title={
                                recentDays[i]
                                    ? `${recentDays[i].date}: ${recentDays[i].count} changes`
                                    : "No data"
                            }
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
                        {totalCommits}
                    </p>
                </div>
                <div className="rounded-lg bg-foreground/[0.04] px-3 py-2">
                    <p className="text-[10px] uppercase tracking-wide text-foreground/50">
                        Contributors
                    </p>
                    <p className="text-lg font-medium text-foreground">
                        {totalContributors}
                    </p>
                </div>
            </div>
        </div>
    );
}
