"use client";

import { AlertCircle, RotateCw } from "lucide-react";
import { errorMessage } from "@/lib/api";
import { cn } from "@/lib/utils";
import { Button } from "./Button";

export interface ErrorStateProps {
    error: unknown;
    /** Shows a "Try again" button when set, e.g. a query's `refetch`. */
    onRetry?: () => void;
    title?: string;
    className?: string;
}

export function ErrorState({ error, onRetry, title = "Couldn't load this", className }: ErrorStateProps) {
    return (
        <div
            role="alert"
            className={cn(
                "flex flex-col items-center justify-center gap-3 rounded-2xl border border-foreground/[0.08] p-8 text-center",
                className
            )}
        >
            <AlertCircle size={28} className="text-red-500" aria-hidden="true" />
            <p className="font-medium">{title}</p>
            <p className="max-w-md text-sm text-foreground-muted">{errorMessage(error)}</p>
            {onRetry && (
                <Button variant="outline" size="sm" icon={<RotateCw size={14} aria-hidden="true" />} onClick={onRetry}>
                    Try again
                </Button>
            )}
        </div>
    );
}
