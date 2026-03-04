"use client";

import { Bell, Search, User } from "lucide-react";
import { useRouter } from "next/navigation";

export default function TopNav() {
    const router = useRouter();

    return (
        <header className="h-20 border-b border-foreground/[0.08] bg-background-secondary/30 backdrop-blur-xl flex items-center justify-between px-8 z-10 sticky top-0">
            <div className="flex-1 flex items-center max-w-xl">
                <div className="relative w-full">
                    <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-foreground/40" size={18} />
                    <input
                        type="text"
                        placeholder="Search projects, branches, or users..."
                        className="w-full bg-background/50 border border-foreground/10 rounded-full py-2.5 pl-10 pr-4 text-sm focus:outline-none focus:border-accent/50 focus:ring-1 focus:ring-accent/50 transition-all"
                    />
                </div>
            </div>

            <div className="flex items-center gap-4 ml-6">
                <button className="relative p-2 rounded-full hover:bg-foreground/5 text-foreground/70 hover:text-foreground transition-colors">
                    <Bell size={20} />
                    <span className="absolute top-1.5 right-1.5 w-2 h-2 rounded-full bg-accent-blue border-2 border-background"></span>
                </button>

                <div className="h-8 w-px bg-foreground/10 mx-2"></div>

                <div className="flex items-center gap-3">
                    <div className="flex flex-col items-end">
                        <span className="text-sm font-medium">Producer</span>
                        <span className="text-xs text-foreground/50">Free Plan</span>
                    </div>
                    <button
                        onClick={() => {
                            localStorage.removeItem("token");
                            router.push("/login");
                        }}
                        className="h-10 w-10 rounded-full flex items-center justify-center text-white border-2 border-background shadow-sm hover:opacity-90 transition-opacity"
                        style={{ background: "linear-gradient(to top right, #3E63DD, #6B8CEE)" }}
                        title="Sign out"
                    >
                        <User size={18} />
                    </button>
                </div>
            </div>
        </header>
    );
}
