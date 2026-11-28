"use client";

import { forwardRef } from "react";
import type { ButtonHTMLAttributes, ReactNode } from "react";
import { Loader2 } from "lucide-react";
import { cn } from "@/lib/utils";

type ButtonVariant = "primary" | "secondary" | "ghost" | "danger" | "icon";
type ButtonSize = "sm" | "md" | "lg";

export interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
    variant?: ButtonVariant;
    size?: ButtonSize;
    loading?: boolean;
    icon?: ReactNode;
}

const variantClasses: Record<ButtonVariant, string> = {
    primary: "bg-primary text-primary-foreground hover:bg-primary/85",
    secondary: "border border-foreground text-foreground hover:bg-foreground hover:text-background",
    ghost: "text-foreground hover:bg-foreground/5",
    danger: "text-signal-text hover:bg-signal hover:text-ink",
    icon: "text-foreground-muted hover:text-foreground hover:bg-foreground/5",
};

const sizeClasses: Record<ButtonSize, string> = {
    sm: "h-9 px-3",
    md: "h-11 px-4",
    lg: "h-12 px-6",
};

const iconSizeClasses: Record<ButtonSize, string> = {
    sm: "h-9 w-9",
    md: "h-11 w-11",
    lg: "h-12 w-12",
};

export const Button = forwardRef<HTMLButtonElement, ButtonProps>(
    ({ variant = "primary", size = "md", loading = false, icon, disabled, className, children, ...rest }, ref) => {
        return (
            <button
                ref={ref}
                disabled={disabled || loading}
                aria-busy={loading || undefined}
                className={cn(
                    "inline-flex shrink-0 items-center justify-center gap-2 whitespace-nowrap caps-label",
                    "transition-colors duration-150 active:translate-y-px",
                    "focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-foreground",
                    "disabled:cursor-not-allowed disabled:active:translate-y-0",
                    !loading && "disabled:opacity-40",
                    variantClasses[variant],
                    variant === "icon" ? iconSizeClasses[size] : sizeClasses[size],
                    className
                )}
                {...rest}
            >
                {loading ? <Loader2 size={16} className="animate-spin" aria-hidden="true" /> : icon}
                {children}
            </button>
        );
    }
);

Button.displayName = "Button";
