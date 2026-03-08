"use client";

import { useState } from "react";
import Link from "next/link";
import { Heart } from "lucide-react";
import { AudioUploadButton } from "./AudioUploadButton";

interface RepositoryPageHeaderProps {
    ownerUsername: string;
    projectName: string;
    branchName: string;
    description: string;
    latestVersionId?: string;
    onUploadSuccess?: () => void;
}

export function RepositoryPageHeader({
    ownerUsername,
    projectName,
    branchName,
    description,
    latestVersionId,
    onUploadSuccess,
}: RepositoryPageHeaderProps) {
    const [isFavorite, setIsFavorite] = useState(false);

    return (
        <div className="px-6 py-5">
            <div className="flex flex-wrap items-start justify-between gap-4">
                <div className="min-w-0 flex-1">
                    <nav
                        className="mb-1 text-sm text-foreground/70"
                        aria-label="Breadcrumb"
                    >
                        <ol className="flex flex-wrap items-center gap-1">
                            <li>
                                <Link
                                    href="/dashboard"
                                    className="transition-colors hover:text-foreground"
                                >
                                    {ownerUsername}
                                </Link>
                            </li>
                            <span className="text-foreground/40">/</span>
                            <li>
                                <span className="font-medium text-foreground">
                                    {projectName}
                                </span>
                            </li>
                            <span className="text-foreground/40">/</span>
                            <li>
                                <span className="text-foreground/70">
                                    {branchName}
                                </span>
                            </li>
                        </ol>
                    </nav>
                    <h1
                        className="text-xl font-medium tracking-tight text-foreground"
                        style={{ fontFamily: "var(--font-syne)" }}
                    >
                        {projectName}
                    </h1>
                    <p className="mt-1 text-sm text-foreground/70">
                        {description}
                    </p>
                </div>

                <div className="flex flex-wrap items-center gap-2">
                    <AudioUploadButton versionId={latestVersionId} onSuccess={onUploadSuccess} />
                    <button
                        type="button"
                        onClick={() => setIsFavorite((prev) => !prev)}
                        className="inline-flex items-center gap-2 rounded-xl bg-accent px-4 py-2 text-sm font-medium text-white border border-transparent hover:opacity-90 transition-opacity"
                    >
                        <Heart
                            className={`size-4 ${
                                isFavorite ? "text-red-500 fill-red-500" : "text-white"
                            }`}
                            aria-hidden
                        />
                        Favorite
                    </button>
                </div>
            </div>
        </div>
    );
}
