"use client";
import { useEffect, Suspense } from "react";
import { useRouter, useSearchParams } from "next/navigation";

function AuthCallbackContent() {
    const router = useRouter();
    const searchParams = useSearchParams();

    useEffect(() => {
        router.push("/dashboard");
    }, [router]);

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
