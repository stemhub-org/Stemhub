import { cn } from "@/lib/utils";

export interface StepGridProps {
    /** 0 (nothing) to 3 (a lot) per step */
    steps: number[];
    label: string;
    columns?: number;
    className?: string;
}

const levelClasses = ["bg-foreground/[0.06]", "bg-accent/35", "bg-accent/65", "bg-accent"] as const;

export function StepGrid({ steps, label, columns, className }: StepGridProps) {
    const perRow = columns ?? steps.length;
    return (
        <div
            role="img"
            aria-label={label}
            className={cn("grid gap-1", className)}
            style={{ gridTemplateColumns: `repeat(${perRow}, minmax(0, 1fr))` }}
        >
            {steps.map((value, index) => {
                const level = Math.max(0, Math.min(3, Math.round(value)));
                return <span key={index} data-level={level} className={cn("aspect-square", levelClasses[level])} />;
            })}
        </div>
    );
}
