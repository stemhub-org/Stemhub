"use client";

import { forwardRef } from "react";
import type { HTMLAttributes } from "react";
import { cn } from "@/lib/utils";

type BadgeTone = "accent" | "neutral" | "success" | "danger";
type BadgeSize = "sm" | "md";

export interface BadgeProps extends HTMLAttributes<HTMLSpanElement> {
    tone?: BadgeTone;
    size?: BadgeSize;
}

const toneClasses: Record<BadgeTone, string> = {
    accent: "bg-accent/10 text-accent border border-accent/20",
    neutral: "bg-foreground/5 text-foreground-muted border border-border-subtle",
    success: "bg-green-500/10 text-green-500 border border-green-500/20",
    danger: "bg-red-500/10 text-red-500 border border-red-500/20",
};

const sizeClasses: Record<BadgeSize, string> = {
    sm: "text-[10px] px-2 py-0.5 rounded",
    md: "text-xs px-2 py-1 rounded",
};

export const Badge = forwardRef<HTMLSpanElement, BadgeProps>(
    ({ tone = "accent", size = "sm", className, children, ...rest }, ref) => {
        return (
            <span
                ref={ref}
                className={cn("inline-flex items-center font-semibold", toneClasses[tone], sizeClasses[size], className)}
                {...rest}
            >
                {children}
            </span>
        );
    }
);

Badge.displayName = "Badge";
