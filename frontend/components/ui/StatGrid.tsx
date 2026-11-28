import type { ReactNode } from "react";
import { cn } from "@/lib/utils";

export interface Stat {
    label: string;
    value: ReactNode;
    unit?: string;
    hint?: ReactNode;
}

export interface StatGridProps {
    stats: Stat[];
    className?: string;
}

export function StatGrid({ stats, className }: StatGridProps) {
    return (
        // borders on the cells, so a half-filled last row doesn't leave an empty box
        <dl className={cn("grid grid-cols-[repeat(auto-fit,minmax(11rem,1fr))] border-l border-t border-line", className)}>
            {stats.map((stat) => (
                <div key={stat.label} className="flex min-w-0 flex-col gap-3 border-b border-r border-line p-5 [container-type:inline-size]">
                    <dt className="caps-label truncate text-foreground-muted">{stat.label}</dt>
                    <dd className="flex min-w-0 items-baseline gap-1.5">
                        <span className="whitespace-nowrap font-display font-extrabold lining-nums leading-none tracking-[-0.02em] text-[min(var(--text-figure),20cqi)]">
                            {stat.value}
                        </span>
                        {stat.unit && <span className="caps-label text-foreground-muted">{stat.unit}</span>}
                    </dd>
                    {stat.hint && <dd className="text-sm text-foreground-muted">{stat.hint}</dd>}
                </div>
            ))}
        </dl>
    );
}
