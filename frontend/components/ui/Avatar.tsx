"use client";

import { useState } from "react";
import type { ReactNode } from "react";
import { cn } from "@/lib/utils";

export interface AvatarProps {
    /** The user's picture: any URL or a data URI. Empty means no picture. */
    src?: string | null;
    alt: string;
    className?: string;
    /** Shown when there is no picture or it fails to load, e.g. the user's initials. */
    fallback?: ReactNode;
    name?: string | null;
}

// "night bus" -> "NB", "nightbus" -> "NI"
export function initialsOf(name: string | null | undefined): string {
    const words = (name ?? "").trim().split(/[\s._-]+/).filter(Boolean);
    if (words.length === 0) return "?";
    const letters = words.length > 1 ? words[0][0] + words[1][0] : words[0].slice(0, 2);
    return letters.toUpperCase();
}

/** A user's picture, or a fallback when there is none or it is broken. */
export function Avatar({ src, alt, className, fallback, name }: AvatarProps) {
    const [failedSrc, setFailedSrc] = useState<string | null>(null);

    if (!src || src === failedSrc) {
        if (fallback !== undefined) return <>{fallback}</>;
        return (
            // not Syne here, it's too wide for small squares
            <span
                role="img"
                aria-label={alt}
                className={cn(
                    "inline-flex shrink-0 select-none items-center justify-center overflow-hidden bg-foreground text-background font-extrabold leading-none tracking-tight tabular-nums",
                    className
                )}
                style={{ containerType: "size" }}
            >
                <span aria-hidden="true" style={{ fontSize: "max(9px, 38cqmin)" }}>
                    {initialsOf(name ?? alt)}
                </span>
            </span>
        );
    }

    return (
        // Avatars come from any host or are data URIs, so next/image would need every host allow-listed.
        // eslint-disable-next-line @next/next/no-img-element
        <img src={src} alt={alt} className={className} onError={() => setFailedSrc(src)} />
    );
}
