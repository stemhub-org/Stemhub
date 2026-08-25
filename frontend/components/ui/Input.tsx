"use client";

import { forwardRef } from "react";
import type { InputHTMLAttributes } from "react";
import { cn } from "@/lib/utils";

export interface InputProps extends InputHTMLAttributes<HTMLInputElement> {
    invalid?: boolean;
}

export const Input = forwardRef<HTMLInputElement, InputProps>(
    ({ invalid = false, className, ...rest }, ref) => {
        return (
            <input
                ref={ref}
                aria-invalid={invalid || undefined}
                className={cn(
                    "w-full rounded-lg border bg-background px-3 py-2.5 text-sm text-foreground placeholder:text-foreground-muted/50 focus:outline-none focus:ring-1 transition-colors",
                    invalid
                        ? "border-red-500/40 focus:border-red-500/60 focus:ring-red-500/50"
                        : "border-border-subtle focus:border-accent focus:ring-accent/50",
                    className
                )}
                {...rest}
            />
        );
    }
);

Input.displayName = "Input";
