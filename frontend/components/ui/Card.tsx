"use client";

import { forwardRef } from "react";
import type { HTMLAttributes } from "react";
import { cn } from "@/lib/utils";

export interface CardProps extends HTMLAttributes<HTMLDivElement> {
    interactive?: boolean;
    padding?: "none" | "sm" | "md" | "lg";
}

const paddingClasses: Record<NonNullable<CardProps["padding"]>, string> = {
    none: "",
    sm: "p-4",
    md: "p-6",
    lg: "p-8",
};

export const Card = forwardRef<HTMLDivElement, CardProps>(
    ({ interactive = false, padding = "md", className, children, ...rest }, ref) => {
        // cursor-pointer is only warranted when the card is an actual click target —
        // `interactive` alone (hover border + group, for child group-hover effects) is
        // also used on purely decorative panels with no onClick.
        const isClickable = Boolean(rest.onClick);
        return (
            <div
                ref={ref}
                className={cn(
                    "border border-line bg-background-secondary",
                    paddingClasses[padding],
                    interactive && "group transition-colors duration-150 hover:border-border-hover",
                    isClickable && "cursor-pointer",
                    className
                )}
                {...rest}
            >
                {children}
            </div>
        );
    }
);

Card.displayName = "Card";
