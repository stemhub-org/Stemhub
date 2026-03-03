"use client";
import { useEffect, Suspense } from "react";
import { useRouter, useSearchParams } from "next/navigation";

function AuthCallbackContent() {
    const router = useRouter();
    const searchParams = useSearchParams();

    useEffect(() => {
        const token = searchParams.get("token");
        if (token) {
            localStorage.setItem("token", token);
            router.push("/dashboard");
        } else {
            router.push("/login");
        }
    }, [router, searchParams]);

    return (
        <div className="flex min-h-screen items-center justify-center bg-background px-6">
            <p className="text-foreground/50" style={{ fontFamily: "var(--font-jakarta)" }}>
                Connexion en cours...
            </p>
        </div>
    );
}

export default function AuthCallback() {
    return (
        <Suspense fallback={<div className="flex min-h-screen items-center justify-center text-foreground/50">Connexion en cours...</div>}>
            <AuthCallbackContent />
        </Suspense>
    );
}
