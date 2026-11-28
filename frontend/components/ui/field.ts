import { cn } from "@/lib/utils";

export function fieldClasses(invalid: boolean): string {
    return cn(
        "w-full border bg-background px-3 text-sm text-foreground placeholder:text-foreground-muted",
        "transition-colors duration-150 focus:outline-none",
        invalid ? "border-signal focus:border-signal" : "border-line hover:border-border-hover focus:border-foreground"
    );
}
