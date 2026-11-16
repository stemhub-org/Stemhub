"use client";

import Link from "next/link";
import { useState, type ReactNode } from "react";
import { useAcceptTerms } from "@/lib/account";
import { errorMessage } from "@/lib/api";
import { useCurrentUser } from "@/lib/queries";
import { useLogout } from "@/lib/useLogout";

/**
 * Shows the signed-in pages only once the current Terms of Service and
 * Privacy Policy are accepted. Users who have not (Google sign-up, accounts
 * from before, a new version) are asked first.
 */
export function TermsGate({ children }: { children: ReactNode }) {
    const { data: user } = useCurrentUser();
    if (user?.terms_accepted === false) return <TermsAcceptance />;
    return <>{children}</>;
}

function TermsAcceptance() {
    const [accepted, setAccepted] = useState(false);
    const acceptTerms = useAcceptTerms();
    const logout = useLogout();

    return (
        <div className="flex min-h-screen items-center justify-center bg-background px-6 text-foreground">
            <div className="w-full max-w-md" style={{ fontFamily: "var(--font-jakarta)" }}>
                <h1 className="mb-3 text-2xl font-extralight tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>
                    Before you continue
                </h1>
                <p className="mb-8 text-sm font-light leading-relaxed text-foreground/60">
                    To use StemHub, please read and accept our Terms of Service and Privacy Policy.
                </p>

                <label className="mb-6 flex cursor-pointer items-start gap-2.5 text-sm font-light leading-relaxed text-foreground/70">
                    <input
                        type="checkbox"
                        checked={accepted}
                        onChange={(e) => setAccepted(e.target.checked)}
                        className="mt-1 h-3.5 w-3.5 shrink-0 cursor-pointer accent-current"
                    />
                    <span>
                        I accept the{" "}
                        <Link href="/terms" target="_blank" className="underline underline-offset-2 hover:text-accent">
                            Terms of Service
                        </Link>{" "}
                        and the{" "}
                        <Link href="/privacy" target="_blank" className="underline underline-offset-2 hover:text-accent">
                            Privacy Policy
                        </Link>
                        .
                    </span>
                </label>

                {acceptTerms.error && (
                    <p role="alert" className="mb-4 text-sm font-light text-red-500">
                        {errorMessage(acceptTerms.error, "Couldn't save your acceptance. Please try again.")}
                    </p>
                )}

                <button
                    type="button"
                    disabled={!accepted || acceptTerms.isPending}
                    onClick={() => acceptTerms.mutate()}
                    className="w-full rounded-xl bg-foreground py-3.5 text-sm font-light tracking-wide text-background transition-all duration-300 hover:bg-accent disabled:cursor-not-allowed disabled:opacity-40"
                >
                    Continue
                </button>
                <button
                    type="button"
                    onClick={logout}
                    className="mt-4 w-full text-center text-xs font-light text-foreground/40 transition-colors hover:text-foreground/70"
                >
                    Log out instead
                </button>
            </div>
        </div>
    );
}
