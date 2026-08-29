"use client";

import { forwardRef } from "react";
import type { ButtonHTMLAttributes, ReactNode } from "react";
import { Loader2 } from "lucide-react";
import { cn } from "@/lib/utils";

type ButtonVariant = "solid" | "outline" | "ghost" | "icon";
type ButtonSize = "sm" | "md" | "lg";

export interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
    variant?: ButtonVariant;
    size?: ButtonSize;
    pill?: boolean;
    loading?: boolean;
    icon?: ReactNode;
}

const variantClasses: Record<ButtonVariant, string> = {
    solid: "bg-accent text-white hover:bg-accent/90 hover:shadow-[var(--shadow-glow-accent)]",
    outline:
        "border border-accent/20 bg-accent/10 text-accent hover:bg-accent/20 hover:border-accent/40",
    ghost: "text-foreground-muted hover:text-foreground hover:bg-foreground/5",
    icon: "text-foreground-muted hover:text-foreground hover:bg-foreground/5",
};

const sizeClasses: Record<ButtonVariant, Record<ButtonSize, string>> = {
    solid: { sm: "px-3 py-1.5 text-xs", md: "px-4 py-2 text-sm", lg: "px-8 py-4 text-sm" },
    outline: { sm: "px-3 py-1.5 text-xs", md: "px-4 py-2 text-sm", lg: "px-8 py-4 text-sm" },
    ghost: { sm: "px-3 py-1.5 text-xs", md: "px-4 py-2 text-sm", lg: "px-8 py-4 text-sm" },
    icon: { sm: "h-8 w-8", md: "h-10 w-10", lg: "h-12 w-12" },
};

export const Button = forwardRef<HTMLButtonElement, ButtonProps>(
    (
        {
            variant = "solid",
            size = "md",
            pill = false,
            loading = false,
            icon,
            disabled,
            className,
            children,
            ...rest
        },
        ref
    ) => {
        const isTransitionOnly = variant === "ghost" || variant === "icon";

        return (
            <button
                ref={ref}
                disabled={disabled || loading}
                aria-busy={loading || undefined}
                className={cn(
                    "inline-flex items-center justify-center gap-2 font-medium disabled:opacity-50 disabled:cursor-not-allowed",
                    pill ? "rounded-full" : variant === "icon" ? "rounded-full" : "rounded-lg",
                    isTransitionOnly ? "transition-colors duration-200" : "transition-all duration-200 ease-brand",
                    variantClasses[variant],
                    sizeClasses[variant][size],
                    variant === "icon" && "p-0",
                    className
                )}
                {...rest}
            >
                {loading ? <Loader2 size={16} className="animate-spin" /> : icon}
                {children}
            </button>
        );
    }
);

Button.displayName = "Button";
