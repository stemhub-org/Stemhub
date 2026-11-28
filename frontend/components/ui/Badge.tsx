"use client";

import { forwardRef } from "react";
import type { HTMLAttributes } from "react";
import { cn } from "@/lib/utils";

type BadgeTone = "accent" | "neutral" | "inverse" | "danger";

export interface BadgeProps extends HTMLAttributes<HTMLSpanElement> {
    tone?: BadgeTone;
}

const toneClasses: Record<BadgeTone, string> = {
    accent: "bg-primary text-primary-foreground border-primary",
    neutral: "border-line text-foreground-muted",
    inverse: "bg-foreground text-background border-foreground",
    danger: "border-signal text-signal-text",
};

export const Badge = forwardRef<HTMLSpanElement, BadgeProps>(({ tone = "neutral", className, children, ...rest }, ref) => {
    return (
        <span
            ref={ref}
            className={cn(
                "inline-flex h-6 max-w-full shrink-0 items-center border px-2 caps-label whitespace-nowrap",
                toneClasses[tone],
                className
            )}
            {...rest}
        >
            <span className="truncate">{children}</span>
        </span>
    );
});

Badge.displayName = "Badge";
