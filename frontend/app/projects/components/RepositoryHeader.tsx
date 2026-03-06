"use client";

import Link from "next/link";
import { useRouter } from "next/navigation";
import { Bell, Sun, User } from "lucide-react";
import { useTheme } from "next-themes";

const REPO_ACCENT = "#9C57DF";

type RepositoryHeaderProps = {
    onToggleSidebar?: () => void;
    sidebarOpen?: boolean;
};

export function RepositoryHeader({ onToggleSidebar, sidebarOpen = false }: RepositoryHeaderProps) {
    const router = useRouter();
    const { resolvedTheme, setTheme } = useTheme();
    const isDark = resolvedTheme === "dark";

    const handleOpenProfile = () => {
        router.push("/dashboard/profile");
    };

    const toggleTheme = () => {
        setTheme(isDark ? "light" : "dark");
    };

    return (
        <header className="flex items-center justify-between py-4 px-6 border-b border-border-subtle bg-background">
            <div className="flex items-center gap-3">
                {onToggleSidebar && (
                    <button
                        type="button"
                        onClick={onToggleSidebar}
                        aria-label={sidebarOpen ? "Hide sidebar" : "Show sidebar"}
                        className="flex h-10 w-10 items-center justify-center rounded-xl bg-transparent hover:bg-foreground/5 transition-colors"
                    >
                        <span className="flex flex-col justify-center gap-1 items-center">
                            <span
                                className={`block h-[2px] min-h-[2px] w-4 shrink-0 rounded-full bg-foreground transition-transform duration-200 ease-out ${
                                    sidebarOpen ? "translate-y-[2px] rotate-45" : ""
                                }`}
                            />
                            <span
                                className={`block h-[2px] min-h-[2px] w-4 shrink-0 rounded-full bg-foreground transition-transform duration-200 ease-out ${
                                    sidebarOpen ? "-translate-y-[2px] -rotate-45" : ""
                                }`}
                            />
                        </span>
                    </button>
                )}
                {(!onToggleSidebar || !sidebarOpen) && (
                    <Link href="/dashboard">
                        <span
                            className="text-2xl font-medium tracking-tight text-foreground"
                            style={{ fontFamily: "var(--font-syne)" }}
                        >
                            StemHub<span style={{ color: REPO_ACCENT }}>.</span>
                        </span>
                    </Link>
                )}
            </div>

            <div className="flex items-center gap-4">
                <button
                    type="button"
                    onClick={toggleTheme}
                    className="p-2 rounded-full hover:bg-foreground/5 text-foreground/70 hover:text-foreground transition-colors"
                    aria-label="Toggle theme"
                    title={isDark ? "Light mode" : "Dark mode"}
                >
                    <Sun size={18} style={{ color: REPO_ACCENT }} />
                </button>
                <button
                    type="button"
                    className="relative p-2 rounded-full hover:bg-foreground/5 text-foreground/70 hover:text-foreground transition-colors"
                    aria-label="Notifications"
                >
                    <Bell size={20} />
                    <span
                        className="absolute top-1.5 right-1.5 w-2 h-2 rounded-full border-2 border-background"
                        style={{ backgroundColor: REPO_ACCENT }}
                    />
                </button>
                <div className="h-8 w-px bg-foreground/10" />
                <div className="flex items-center gap-3">
                    <div className="flex flex-col items-end">
                        <span className="text-sm font-medium text-foreground">Producer</span>
                        <span className="text-xs text-foreground/50">Free Plan</span>
                    </div>
                    <button
                        type="button"
                        onClick={handleOpenProfile}
                        className="h-10 w-10 rounded-full flex items-center justify-center text-white border-2 border-background shadow-sm hover:opacity-90 transition-opacity"
                        style={{ background: `linear-gradient(to top right, ${REPO_ACCENT}, #C28CF0)` }}
                        title="View profile"
                        aria-label="Open profile"
                    >
                        <User size={18} />
                    </button>
                </div>
            </div>
        </header>
    );
}
