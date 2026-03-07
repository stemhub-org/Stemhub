"use client";

import type React from "react";
import { useState } from "react";
import Sidebar from "@/components/Sidebar";
import TopNav from "@/components/TopNav";

export default function DashboardLayout({
    children,
}: {
    children: React.ReactNode;
}) {
    const [isAuthenticated, setIsAuthenticated] = useState(true);
    const [sidebarOpen, setSidebarOpen] = useState(false);

    if (!isAuthenticated) return null;

    return (
        <div
            className="flex h-screen bg-background text-foreground overflow-hidden"
            style={{ "--accent": "#9C57DF" } as React.CSSProperties}
        >
            <Sidebar isOpen={sidebarOpen} onToggleSidebar={() => setSidebarOpen(false)} />

            <div className="flex-1 flex flex-col h-full overflow-hidden relative transition-[margin] duration-300 ease-in-out">
                <TopNav
                    onToggleSidebar={!sidebarOpen ? () => setSidebarOpen(true) : undefined}
                    sidebarOpen={sidebarOpen}
                />

                <main className="flex-1 overflow-y-auto p-8 relative scroll-smooth">
                    {children}
                </main>
            </div>
        </div>
    );
}
