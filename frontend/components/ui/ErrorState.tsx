"use client";

import { RotateCw } from "lucide-react";
import { errorMessage } from "@/lib/api";
import { cn } from "@/lib/utils";
import { Button } from "./Button";
import { StatusLed } from "./StatusLed";

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
            className={cn("flex flex-col items-start gap-3 border border-line p-6", className)}
        >
            <StatusLed status="failed" label="Error" />
            <p className="text-lg font-bold">{title}</p>
            <p className="max-w-md text-sm text-foreground-muted">{errorMessage(error)}</p>
            {onRetry && (
                <Button variant="secondary" size="sm" icon={<RotateCw size={14} aria-hidden="true" />} onClick={onRetry}>
                    Try again
                </Button>
            )}
        </div>
    );
}
