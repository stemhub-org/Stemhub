import type { Metadata } from "next";
import { LegalPage } from "@/components/LegalPage";
import { TERMS_VERSION } from "@/lib/legal";

export const metadata: Metadata = { title: "Terms of Service" };

export default function TermsPage() {
    return (
        <LegalPage title="Terms of Service" version={TERMS_VERSION}>
            {/* TODO(legal): replace with the Terms of Service written by the team. */}
            <p>
                The StemHub Terms of Service are being written. This page will hold the rules for using
                StemHub: accounts, the music you upload and share, and the responsibilities of StemHub and
                its users.
            </p>
        </LegalPage>
    );
}
