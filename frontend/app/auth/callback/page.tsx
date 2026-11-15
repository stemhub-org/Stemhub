"use client";
import { useEffect, Suspense } from "react";
import { useRouter, useSearchParams } from "next/navigation";
import { saveAccessToken } from "@/lib/authToken";

function AuthCallbackContent() {
    const router = useRouter();
    const searchParams = useSearchParams();

    useEffect(() => {
        const token = searchParams.get("token");
        if (token) {
            // Google sign-in has no "remember me": a one-day session.
            saveAccessToken(token, { remember: false });
        }
        router.push("/dashboard");
    }, [router, searchParams]);

    return (
        <div className="flex min-h-screen items-center justify-center bg-background px-6">
            <p className="text-foreground/50" style={{ fontFamily: "var(--font-jakarta)" }}>
                Connection in progress...
            </p>
        </div>
    );
}

export default function AuthCallback() {
    return (
        <Suspense fallback={<div className="flex min-h-screen items-center justify-center text-foreground/50">Connection in progress...</div>}>
            <AuthCallbackContent />
        </Suspense>
    );
}
