"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import Sidebar from "@/components/Sidebar";
import TopNav from "@/components/TopNav";

export default function DashboardLayout({
    children,
}: {
    children: React.ReactNode;
}) {
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

    if (!isAuthenticated) return null;

    return (
        <div className="flex h-screen bg-background text-foreground overflow-hidden">
            <Sidebar />

            <div className="flex-1 flex flex-col h-full overflow-hidden relative">
                <TopNav />

                <main className="flex-1 overflow-y-auto p-8 relative scroll-smooth">
                    {children}
                </main>
            </div>
        </div>
    );
}
