"use client";

import { useState } from "react";
import type { ReactNode } from "react";

export interface AvatarProps {
    /** The user's picture: any URL or a data URI. Empty means no picture. */
    src?: string | null;
    alt: string;
    className?: string;
    /** Shown when there is no picture or it fails to load, e.g. the user's initials. */
    fallback: ReactNode;
}

/** A user's picture, or a fallback when there is none or it is broken. */
export function Avatar({ src, alt, className, fallback }: AvatarProps) {
    const [failedSrc, setFailedSrc] = useState<string | null>(null);

    if (!src || src === failedSrc) return <>{fallback}</>;

    return (
        // Avatars come from any host or are data URIs, so next/image would need every host allow-listed.
        // eslint-disable-next-line @next/next/no-img-element
        <img src={src} alt={alt} className={className} onError={() => setFailedSrc(src)} />
    );
}
