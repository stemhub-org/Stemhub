"use client";

import { Eye, EyeOff } from "lucide-react";

interface Props {
    visible: boolean;
    onToggle: () => void;
    /** Positions the button inside a `relative` field wrapper. */
    className?: string;
}

/** The eye button that shows or hides a password field's text. */
export function PasswordVisibilityToggle({ visible, onToggle, className = "" }: Props) {
    return (
        <button
            type="button"
            onClick={onToggle}
            aria-label={visible ? "Hide password" : "Show password"}
            aria-pressed={visible}
            className={`absolute top-1/2 right-4 -translate-y-1/2 text-foreground/30 transition-colors hover:text-foreground/60 ${className}`}
        >
            {visible ? <EyeOff size={16} strokeWidth={1.5} /> : <Eye size={16} strokeWidth={1.5} />}
        </button>
    );
}
