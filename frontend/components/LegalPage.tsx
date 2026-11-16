import Link from "next/link";
import type { ReactNode } from "react";

interface Props {
    title: string;
    version: string;
    children: ReactNode;
}

/** Layout shared by the Terms of Service and Privacy Policy pages. */
export function LegalPage({ title, version, children }: Props) {
    return (
        <main className="mx-auto max-w-2xl px-6 py-24" style={{ fontFamily: "var(--font-jakarta)" }}>
            <Link href="/" className="mb-12 block">
                <span className="text-2xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>
                    stemhub<span className="text-accent">.</span>
                </span>
            </Link>
            <h1 className="mb-2 text-3xl font-extralight tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>
                {title}
            </h1>
            <p className="mb-10 text-sm font-light text-foreground/40">Version of {version}</p>
            <div className="space-y-4 text-sm font-light leading-relaxed text-foreground/70">{children}</div>
        </main>
    );
}
