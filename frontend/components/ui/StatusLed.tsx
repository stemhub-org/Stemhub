import { cn } from "@/lib/utils";

export type LedStatus = "active" | "syncing" | "failed" | "idle";

export interface StatusLedProps {
    status: LedStatus;
    label: string;
    className?: string;
}

const ledClasses: Record<LedStatus, string> = {
    active: "bg-accent",
    syncing: "bg-slate motion-safe:animate-pulse",
    failed: "bg-signal",
    idle: "bg-foreground-muted/40",
};

export function StatusLed({ status, label, className }: StatusLedProps) {
    return (
        <span className={cn("inline-flex min-w-0 items-center gap-2 caps-label text-foreground-muted", className)}>
            <span aria-hidden="true" data-status={status} className={cn("h-2 w-2 shrink-0", ledClasses[status])} />
            <span className="truncate">{label}</span>
        </span>
    );
}
