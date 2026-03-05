"use client";

<<<<<<< HEAD
import type React from "react";
import { useState } from "react";
=======
import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
>>>>>>> origin/dev
import Sidebar from "@/components/Sidebar";
import TopNav from "@/components/TopNav";

export default function DashboardLayout({
    children,
}: {
    children: React.ReactNode;
}) {
<<<<<<< HEAD
    const [isAuthenticated, setIsAuthenticated] = useState(true);
    const [sidebarOpen, setSidebarOpen] = useState(false);
=======
    const router = useRouter();
    const [isAuthenticated, setIsAuthenticated] = useState(true); // TODO: Revert back to false after testing

    useEffect(() => {
        // TODO: Recoment this block after testing
        // const token = localStorage.getItem("token");
        // if (!token) {
        //     router.push("/login");
        // } else {
        //     setIsAuthenticated(true);
        // }
    }, [router]);
>>>>>>> origin/dev

    if (!isAuthenticated) return null;

    return (
<<<<<<< HEAD
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
=======
        <div className="flex h-screen bg-background text-foreground overflow-hidden">
            <Sidebar />

            <div className="flex-1 flex flex-col h-full overflow-hidden relative">
                <TopNav />
>>>>>>> origin/dev

                <main className="flex-1 overflow-y-auto p-8 relative scroll-smooth">
                    {children}
                </main>
            </div>
        </div>
    );
}
