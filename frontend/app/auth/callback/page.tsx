"use client";
import { useEffect, useRef, Suspense } from "react";
import { useRouter, useSearchParams } from "next/navigation";
import { useToast } from "@/components/ToastProvider";
import { saveAccessToken } from "@/lib/authToken";

function AuthCallbackContent() {
    const router = useRouter();
    const searchParams = useSearchParams();
    const toast = useToast();
    // Effects run twice in development; the notice must show once.
    const handled = useRef(false);

    useEffect(() => {
        if (handled.current) return;
        handled.current = true;
        const token = searchParams.get("token");
        if (token) {
            // Google sign-in has no "remember me": a one-day session.
            saveAccessToken(token, { remember: false });
        }
        if (searchParams.get("account_restored") === "1") {
            toast.success("Welcome back! Your account deletion has been cancelled.");
        }
        router.push("/dashboard");
    }, [router, searchParams, toast]);

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
