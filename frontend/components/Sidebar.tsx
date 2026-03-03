"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import {
    LayoutDashboard,
    GitBranch,
    GitPullRequest,
    Compass,
    Settings,
    FolderDot
} from "lucide-react";

const navigation = [
    { name: "Overview", href: "/dashboard", icon: LayoutDashboard },
    { name: "Projects", href: "/dashboard/projects", icon: FolderDot },
    { name: "Branches", href: "/dashboard/branches", icon: GitBranch },
    { name: "Pull Requests", href: "/dashboard/pulls", icon: GitPullRequest },
    { name: "Explore", href: "/dashboard/explore", icon: Compass },
    { name: "Settings", href: "/dashboard/settings", icon: Settings },
];

export default function Sidebar() {
    const pathname = usePathname();

    return (
        <div className="w-64 border-r border-foreground/[0.08] bg-background-secondary/30 backdrop-blur-xl h-full flex flex-col">
            <div className="h-20 flex items-center px-8 border-b border-foreground/[0.08]">
                <Link href="/">
                    <span
                        className="text-2xl font-medium tracking-tight"
                        style={{ fontFamily: "var(--font-syne)" }}
                    >
                        stemhub<span className="text-accent">.</span>
                    </span>
                </Link>
            </div>

            <nav className="flex-1 py-8 px-4 space-y-2 overflow-y-auto">
                {navigation.map((item) => {
                    const isActive = pathname === item.href;
                    const Icon = item.icon;

                    return (
                        <Link
                            key={item.name}
                            href={item.href}
                            className={`flex items-center gap-3 px-4 py-3 rounded-xl transition-all duration-200 ${isActive
                                    ? "bg-accent/10 text-accent font-medium"
                                    : "text-foreground/70 hover:bg-foreground/5 hover:text-foreground"
                                }`}
                        >
                            <Icon size={20} strokeWidth={isActive ? 2.5 : 2} />
                            <span className="text-sm">{item.name}</span>
                        </Link>
                    );
                })}
            </nav>

            <div className="p-4 border-t border-foreground/[0.08]">
                <div className="rounded-xl bg-accent/10 p-4 border border-accent/20">
                    <p className="text-xs font-medium text-accent mb-1">Pro Plan</p>
                    <p className="text-xs text-foreground/70 mb-3">25GB / 100GB used</p>
                    <div className="w-full bg-foreground/10 rounded-full h-1.5 mb-3">
                        <div className="bg-accent h-1.5 rounded-full" style={{ width: "25%" }}></div>
                    </div>
                    <button className="w-full text-xs font-medium bg-background rounded-lg py-2 hover:bg-foreground/5 transition-colors border border-foreground/10">
                        Upgrade Storage
                    </button>
                </div>
            </div>
        </div>
    );
}
