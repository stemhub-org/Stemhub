"use client";

import { forwardRef } from "react";
import type { HTMLAttributes } from "react";
import { cn } from "@/lib/utils";

export interface CardProps extends HTMLAttributes<HTMLDivElement> {
    interactive?: boolean;
    padding?: "sm" | "md" | "lg";
}

const paddingClasses: Record<NonNullable<CardProps["padding"]>, string> = {
    sm: "p-4",
    md: "p-6",
    lg: "p-8",
};

export const Card = forwardRef<HTMLDivElement, CardProps>(
    ({ interactive = false, padding = "md", className, children, ...rest }, ref) => {
        return (
            <div
                ref={ref}
                className={cn(
                    "rounded-xl bg-background-secondary dark:bg-background-tertiary border border-border-subtle",
                    paddingClasses[padding],
                    interactive &&
                        "hover:border-accent/40 hover:bg-gradient-to-br hover:from-background-secondary dark:hover:from-background-tertiary hover:to-accent/5 hover:shadow-[0_0_20px_rgba(156,87,223,0.05)] transition-all duration-300 cursor-pointer group",
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
