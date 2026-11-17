"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { useTheme } from "next-themes";
import {
    LayoutDashboard,
    Compass,
    Settings,
    FolderDot,
    ChevronLeft,
    LogOut,
} from "lucide-react";
import { BrandFigure, Wordmark } from "@/app/components/BrandMark";
import { formatBytes } from "@/lib/format";
import { useStorageUsage } from "@/lib/queries";
import { useLogout } from "@/lib/useLogout";
import { useHydrated } from "@/lib/useHydrated";

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
    const { resolvedTheme } = useTheme();
    const hydrated = useHydrated();
    const isLight = hydrated && resolvedTheme === "light";

    const handleLogout = useLogout();

    return (
        <div
            className={`h-full flex flex-col border-r border-border-subtle relative transition-[width,opacity] duration-300 ease-in-out ${isLight ? "bg-white" : "bg-background-tertiary"
                } ${isOpen ? "w-64 opacity-100" : "w-0 opacity-0 pointer-events-none overflow-hidden"}`}
            style={isLight ? { backgroundColor: "#ffffff" } : undefined}
        >
            {isOpen && (
                <>
                    <div className="h-20 flex items-center justify-between px-8 border-b border-border-subtle">
                        <Link href="/dashboard" aria-label="StemHub home" className="flex items-center gap-2 text-foreground">
                            <BrandFigure className="h-7 w-auto" />
                            <Wordmark className="text-xl" />
                        </Link>
                    </div>
                    {/* On the sidebar's edge, not next to the logo: the wordmark
                        alone is wider than the space left between the paddings. */}
                    {onToggleSidebar && (
                        <button
                            type="button"
                            onClick={onToggleSidebar}
                            aria-label="Close sidebar"
                            className="absolute top-[4.25rem] -right-3 z-20 flex h-6 w-6 items-center justify-center rounded-full border border-border-subtle bg-background-secondary shadow-sm hover:bg-background-tertiary transition-colors"
                        >
                            <ChevronLeft size={14} className="text-foreground" />
                        </button>
                    )}

                    <nav className="flex-1 py-10 px-6 space-y-1.5 overflow-y-auto">
                        {navigation.map((item) => {
                            const isActive = pathname === item.href;
                            const Icon = item.icon;
                            return (
                                <Link
                                    key={item.name}
                                    href={item.href}
                                    className={`flex items-center gap-3 px-3 py-2.5 rounded-lg transition-all duration-300 group ${isActive
                                            ? "bg-accent/10 text-accent border border-accent/20 shadow-[var(--shadow-glow-accent)]"
                                            : "text-foreground-muted hover:text-foreground hover:bg-gradient-to-r hover:from-background-secondary hover:to-accent/5 border border-transparent hover:border-accent/20"
                                        }`}
                                >
                                    <Icon
                                        size={18}
                                        strokeWidth={isActive ? 2 : 1.5}
                                        className={`${isActive
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
                            onClick={handleLogout}
                            className="w-full flex items-center justify-between px-3 py-2 rounded-lg border border-border-subtle bg-background-secondary hover:border-red-500/40 hover:bg-red-500/5 transition-all duration-300 group"
                            aria-label="Log out"
                        >
                            <span className="text-xs font-medium text-foreground-muted group-hover:text-red-500 transition-colors">
                                Log out
                            </span>
                            <div className="h-6 w-6 rounded-md bg-background border border-border-subtle flex items-center justify-center group-hover:border-red-500/40 transition-colors">
                                <LogOut
                                    size={13}
                                    className="text-foreground-muted group-hover:text-red-500 transition-colors"
                                />
                            </div>
                        </button>

                        <SidebarStorage />
                    </div>
                </>
            )}
        </div>
    );
}

/** The real space the user's projects take (no plan or quota exists yet). */
function SidebarStorage() {
    const { data: usage } = useStorageUsage();

    return (
        <div className="rounded-lg bg-background-secondary border border-border-subtle p-4">
            <p className="text-xs font-semibold text-foreground mb-1">Free during the beta</p>
            {usage && <p className="text-xs text-foreground-muted">{formatBytes(usage.total_bytes)} used</p>}
        </div>
    );
}
