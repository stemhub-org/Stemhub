"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import Link from "next/link";
import { RepositoryHeader } from "../components/RepositoryHeader";
import { GitCommit, ArrowLeft } from "lucide-react";

const PLACEHOLDER_COMMITS = [
    { hash: "a1b2c3d", message: "Final master with limiting", author: "Skrillex", date: "1d ago" },
    { hash: "e4f5g6h", message: "Exported with new compression", author: "Metro Boomin", date: "2h ago" },
    { hash: "i7j8k9l", message: "Reorganized mixer tracks", author: "Skrillex", date: "1h ago" },
    { hash: "m0n1o2p", message: "Merged vocal harmonies", author: "deadmau5", date: "5h ago" },
    { hash: "q3r4s5t", message: "Added sub bass layer", author: "Skrillex", date: "3h ago" },
    { hash: "u6v7w8x", message: "Updated kick pattern with sidechain", author: "Metro Boomin", date: "2h ago" },
];

export default function RepositoryCommitsPage() {
    const router = useRouter();
    const [isAuthenticated, setIsAuthenticated] = useState(false);

    useEffect(() => {
        const token = localStorage.getItem("token");
        if (!token) {
            router.push("/login");
        } else {
            setIsAuthenticated(true);
        }
    }, [router]);

    if (!isAuthenticated) return null;

    return (
        <div className="min-h-screen bg-background text-foreground">
            <RepositoryHeader />
            <div className="p-6 space-y-6">
                <Link
                    href="/repository"
                    className="inline-flex items-center gap-2 text-sm text-foreground/70 transition-colors hover:text-foreground"
                >
                    <ArrowLeft className="size-4" aria-hidden />
                    Retour au repository
                </Link>
                <div className="rounded-xl border border-foreground/[0.08] bg-white overflow-hidden">
                    <div className="border-b border-foreground/[0.08] bg-foreground/[0.02] px-6 py-3">
                        <h1
                            className="flex items-center gap-2 text-lg font-medium text-foreground"
                            style={{ fontFamily: "var(--font-syne)" }}
                        >
                            <GitCommit className="size-5 text-accent" aria-hidden />
                            Historique des commits
                        </h1>
                    </div>
                    <ul className="divide-y divide-foreground/[0.06]" role="list">
                        {PLACEHOLDER_COMMITS.map((commit) => (
                            <li key={commit.hash}>
                                <div className="grid grid-cols-[auto_1fr_auto] gap-4 px-6 py-4 text-left">
                                    <code className="text-xs font-mono text-accent">
                                        {commit.hash}
                                    </code>
                                    <div className="min-w-0">
                                        <p className="font-medium text-foreground">
                                            {commit.message}
                                        </p>
                                        <p className="text-sm text-foreground/60">
                                            {commit.author} · {commit.date}
                                        </p>
                                    </div>
                                    <span className="text-sm text-foreground/50">
                                        {commit.date}
                                    </span>
                                </div>
                            </li>
                        ))}
                    </ul>
                </div>
            </div>
        </div>
    );
}
