"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { useTheme } from "next-themes";
import {
    LayoutDashboard,
    Compass,
    Settings,
    FolderDot,
    Sun,
    Moon,
} from "lucide-react";

const navigation = [
    { name: "Overview", href: "/dashboard", icon: LayoutDashboard },
    { name: "Projects", href: "/dashboard/projects", icon: FolderDot },
    { name: "Explore", href: "/dashboard/explore", icon: Compass },
    { name: "Settings", href: "/dashboard/settings", icon: Settings },
];

type SidebarProps = {
    isOpen?: boolean;
    onToggleSidebar?: () => void;
};

export default function Sidebar({ isOpen = true, onToggleSidebar }: SidebarProps) {
    const pathname = usePathname();
    const { resolvedTheme, setTheme } = useTheme();
    const isDark = resolvedTheme === "dark";
    const isLight = resolvedTheme === "light";

    return (
        <div
            className={`h-full flex flex-col border-r border-border-subtle relative transition-[width,opacity] duration-300 ease-in-out ${
                isLight ? "bg-white" : "bg-background-tertiary"
            } ${isOpen ? "w-64 opacity-100" : "w-0 opacity-0 pointer-events-none overflow-hidden"}`}
            style={isLight ? { backgroundColor: "#ffffff" } : undefined}
        >
            {isOpen && (
                <>
                    <div className="h-20 flex items-center justify-between px-8 border-b border-border-subtle">
                        <Link href="/">
                            <span
                                className="text-2xl font-normal tracking-tight text-foreground"
                                style={{ fontFamily: "var(--font-syne)" }}
                            >
                                StemHub<span className="text-accent">.</span>
                            </span>
                        </Link>
                        {onToggleSidebar && (
                            <button
                                type="button"
                                onClick={onToggleSidebar}
                                aria-label="Close sidebar"
                                className="flex h-9 w-9 items-center justify-center rounded-lg hover:bg-foreground/5 transition-colors"
                            >
                                <span className="relative block h-4 w-4">
                                    <span className="absolute inset-x-0 top-1/2 h-[2px] min-h-[2px] w-full -translate-y-1/2 rotate-45 rounded-full bg-foreground shrink-0" />
                                    <span className="absolute inset-x-0 top-1/2 h-[2px] min-h-[2px] w-full -translate-y-1/2 -rotate-45 rounded-full bg-foreground shrink-0" />
                                </span>
                            </button>
                        )}
                    </div>

                    <nav className="flex-1 py-10 px-6 space-y-1.5 overflow-y-auto">
                        {navigation.map((item) => {
                            const isActive = pathname === item.href;
                            const Icon = item.icon;
                            return (
                                <Link
                                    key={item.name}
                                    href={item.href}
                                    className={`flex items-center gap-3 px-3 py-2.5 rounded-lg transition-all duration-300 group ${
                                        isActive
                                            ? "bg-accent/10 text-accent border border-accent/20 shadow-[0_0_15px_rgba(156,87,223,0.1)]"
                                            : "text-foreground-muted hover:text-foreground hover:bg-gradient-to-r hover:from-background-secondary hover:to-accent/5 border border-transparent hover:border-accent/20"
                                    }`}
                                >
                                    <Icon
                                        size={18}
                                        strokeWidth={isActive ? 2 : 1.5}
                                        className={`${
                                            isActive
                                                ? "text-accent"
                                                : "group-hover:text-accent transition-colors"
                                        }`}
                                    />
                                    <span className="text-sm font-medium">{item.name}</span>
                                </Link>
                            );
                        })}
                    </nav>

                    <div className="p-6 border-t border-border-subtle space-y-3">
                        <button
                            onClick={() => setTheme(isDark ? "light" : "dark")}
                            className="w-full flex items-center justify-between px-3 py-2 rounded-lg border border-border-subtle bg-background-secondary hover:border-accent/30 hover:bg-background-tertiary transition-all duration-300 group"
                            aria-label="Toggle theme"
                        >
                            <span className="text-xs font-medium text-foreground-muted group-hover:text-foreground transition-colors">
                                {isDark ? "Light Mode" : "Dark Mode"}
                            </span>
                            <div className="h-6 w-6 rounded-md bg-background border border-border-subtle flex items-center justify-center group-hover:border-accent/30 transition-colors">
                                {isDark ? (
                                    <Sun
                                        size={13}
                                        className="text-foreground-muted group-hover:text-accent transition-colors"
                                    />
                                ) : (
                                    <Moon
                                        size={13}
                                        className="text-foreground-muted group-hover:text-accent transition-colors"
                                    />
                                )}
                            </div>
                        </button>

                        <div className="rounded-lg bg-background-secondary border border-border-subtle p-4">
                            <p className="text-xs font-semibold text-foreground mb-1">Pro Plan</p>
                            <p className="text-xs text-foreground-muted mb-3">25GB / 100GB used</p>
                            <div className="w-full bg-background-tertiary rounded-full h-1 mb-4 overflow-hidden outline outline-1 outline-border-subtle">
                                <div
                                    className="bg-accent h-1 rounded-full"
                                    style={{ width: "25%" }}
                                ></div>
                            </div>
                            <button className="w-full text-xs font-semibold text-white bg-accent rounded-md py-2 transition-all duration-300 border border-transparent hover:bg-accent/90 hover:shadow-[0_0_15px_rgba(156,87,223,0.3)]">
                                Upgrade Storage
                            </button>
                        </div>
                    </div>
                </>
            )}
        </div>
    );
}
