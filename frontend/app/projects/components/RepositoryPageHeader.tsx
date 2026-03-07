"use client";

import { useState } from "react";
import Link from "next/link";
import { Eye, Heart } from "lucide-react";

const PLACEHOLDER_USER = "Skrillex";
const PLACEHOLDER_REPO = "Dubstep-Track";
const PLACEHOLDER_BRANCH = "main";
const PLACEHOLDER_DESCRIPTION =
    "Experimental dubstep track • 140 BPM • C minor";

export function RepositoryPageHeader() {
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
                                    {PLACEHOLDER_USER}
                                </Link>
                            </li>
                            <span className="text-foreground/40">/</span>
                            <li>
                                <span className="font-medium text-foreground">
                                    {PLACEHOLDER_REPO}
                                </span>
                            </li>
                            <span className="text-foreground/40">/</span>
                            <li>
                                <span className="text-foreground/70">
                                    {PLACEHOLDER_BRANCH}
                                </span>
                            </li>
                        </ol>
                    </nav>
                    <h1
                        className="text-xl font-medium tracking-tight text-foreground"
                        style={{ fontFamily: "var(--font-syne)" }}
                    >
                        {PLACEHOLDER_REPO}
                    </h1>
                    <p className="mt-1 text-sm text-foreground/70">
                        {PLACEHOLDER_DESCRIPTION}
                    </p>
                </div>

                <div className="flex flex-wrap items-center gap-2">
                    <button
                        type="button"
                        className="inline-flex items-center gap-2 rounded-xl bg-accent px-4 py-2 text-sm font-medium text-white transition-opacity hover:opacity-90"
                    >
                        <Eye className="size-4" aria-hidden />
                        Watch
                    </button>
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
