"use client";

import Link from "next/link";
import { Bell, Sun, User } from "lucide-react";
import { useRouter } from "next/navigation";
import { useTheme } from "next-themes";

type TopNavProps = {
    onToggleSidebar?: () => void;
    sidebarOpen?: boolean;
};

export default function TopNav({ onToggleSidebar, sidebarOpen = true }: TopNavProps) {
    const router = useRouter();
    const { resolvedTheme, setTheme } = useTheme();
    const isDark = resolvedTheme === "dark";

    return (
        <header className="h-20 border-b border-foreground/[0.08] bg-background-secondary/30 backdrop-blur-xl flex items-center justify-between px-6 lg:px-8 z-10 sticky top-0">
            <div className="flex items-center gap-3">
                {onToggleSidebar && !sidebarOpen && (
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
                {onToggleSidebar && !sidebarOpen && (
                    <Link
                        href="/dashboard"
                        className="text-2xl font-medium tracking-tight text-foreground"
                        style={{ fontFamily: "var(--font-syne)" }}
                    >
                        StemHub<span className="text-accent">.</span>
                    </Link>
                )}
            </div>

            <div className="flex items-center gap-4">
                <button
                    className="p-2 rounded-full hover:bg-foreground/5 text-foreground/70 hover:text-foreground transition-colors"
                    aria-label="Toggle theme"
                    title={isDark ? "Light mode" : "Dark mode"}
                    onClick={() => setTheme(isDark ? "light" : "dark")}
                >
                    <Sun size={18} className="text-accent" />
                </button>

                <button className="relative p-2 rounded-full hover:bg-foreground/5 text-foreground/70 hover:text-foreground transition-colors">
                    <Bell size={20} />
                    <span className="absolute top-1.5 right-1.5 w-2 h-2 rounded-full bg-accent border-2 border-background"></span>
                </button>

                <div className="h-8 w-px bg-foreground/10 mx-2"></div>

                <div className="flex items-center gap-3">
                    <div className="flex flex-col items-end">
                        <span className="text-sm font-medium">Producer</span>
                        <span className="text-xs text-foreground/50">Free Plan</span>
                    </div>
                    <button
                        onClick={() => {
                            router.push("/dashboard/profile");
                        }}
                        className="h-10 w-10 rounded-full flex items-center justify-center text-white border-2 border-background shadow-sm hover:opacity-90 transition-opacity"
                        style={{ background: "linear-gradient(to top right, #9C57DF, #C28CF0)" }}
                        title="View profile"
                    >
                        <User size={18} />
                    </button>
                </div>
            </div>
        </header>
    );
}
