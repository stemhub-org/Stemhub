"use client";

import { useEffect, useState } from "react";
import { useRouter } from "next/navigation";
import { authFetch } from "@/lib/api";

interface UserMe {
    id: string;
    email: string;
    username: string;
    is_admin: boolean;
    is_active: boolean;
}

export default function AdminGuard({ children }: { children: React.ReactNode }) {
    const router = useRouter();
    const [ready, setReady] = useState(false);

    useEffect(() => {
        const token = localStorage.getItem("access_token");
        if (!token) {
            router.replace("/login");
            return;
        }

        authFetch<UserMe>("/auth/me")
            .then((user) => {
                if (!user.is_admin) {
                    router.replace("/dashboard");
                } else {
                    setReady(true);
                }
            })
            .catch(() => {
                router.replace("/login");
            });
    }, [router]);

    if (!ready) {
        return (
            <div className="flex h-screen items-center justify-center bg-[#F1F1F1]">
                <div className="w-6 h-6 rounded-full border-2 border-[#9C57DF] border-t-transparent animate-spin" />
            </div>
        );
    }

    return <>{children}</>;
}
