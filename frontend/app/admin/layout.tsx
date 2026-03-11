"use client";

import type React from "react";
import Link from "next/link";
import { usePathname } from "next/navigation";
import { LayoutDashboard, Users, Settings, Shield } from "lucide-react";
import AdminGuard from "@/components/AdminGuard";

const NAV_ITEMS = [
    { href: "/admin", label: "Dashboard", icon: LayoutDashboard },
    { href: "/admin/users", label: "Utilisateurs", icon: Users },
    { href: "/admin/settings", label: "Paramètres", icon: Settings },
];

function AdminSidebar() {
    const pathname = usePathname();

    return (
        <aside className="w-56 shrink-0 h-screen flex flex-col border-r border-[#1A1A1A]/10 bg-[#F1F1F1]">
            <div className="flex items-center gap-2 px-5 py-6 border-b border-[#1A1A1A]/10">
                <Shield size={18} className="text-[#9C57DF]" />
                <span className="text-sm font-semibold tracking-tight text-[#1A1A1A]">Admin Panel</span>
            </div>

            <nav className="flex flex-col gap-1 p-3 flex-1">
                {NAV_ITEMS.map(({ href, label, icon: Icon }) => {
                    const isActive = href === "/admin" ? pathname === "/admin" : pathname.startsWith(href);
                    return (
                        <Link
                            key={href}
                            href={href}
                            className={`flex items-center gap-3 px-3 py-2 rounded-lg text-sm transition-colors ${
                                isActive
                                    ? "bg-[#9C57DF]/10 text-[#9C57DF] font-medium"
                                    : "text-[#1A1A1A]/60 hover:text-[#1A1A1A] hover:bg-[#1A1A1A]/5"
                            }`}
                        >
                            <Icon size={16} />
                            {label}
                        </Link>
                    );
                })}
            </nav>

            <div className="px-5 py-4 border-t border-[#1A1A1A]/10">
                <Link
                    href="/dashboard"
                    className="text-xs text-[#1A1A1A]/40 hover:text-[#1A1A1A]/60 transition-colors"
                >
                    ← Retour au dashboard
                </Link>
            </div>
        </aside>
    );
}

export default function AdminLayout({ children }: { children: React.ReactNode }) {
    return (
        <AdminGuard>
            <div className="flex h-screen bg-[#F1F1F1] text-[#1A1A1A] overflow-hidden">
                <AdminSidebar />
                <main className="flex-1 overflow-y-auto">
                    {children}
                </main>
            </div>
        </AdminGuard>
    );
}
