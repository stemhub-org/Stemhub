"use client";

import Link from "next/link";
import { useRouter } from "next/navigation";

export function RepositoryHeader() {
    const router = useRouter();

    const handleLogout = () => {
        localStorage.removeItem("token");
        router.push("/login");
    };

    return (
        <header className="flex justify-between items-center py-4 px-6 border-b border-foreground/[0.08] bg-background">
            <Link href="/dashboard">
                <span
                    className="text-xl font-medium tracking-tight text-foreground"
                    style={{ fontFamily: "var(--font-syne)" }}
                >
                    StemHub<span className="text-[#9C57DF]">.</span>
                </span>
            </Link>
            <button
                type="button"
                onClick={handleLogout}
                className="rounded-xl bg-foreground/5 px-5 py-2 text-sm font-light text-foreground transition-colors hover:bg-foreground/10"
            >
                Déconnexion
            </button>
        </header>
    );
}
