import type { Metadata } from "next";
import { LegalPage } from "@/components/LegalPage";
import { PRIVACY_POLICY_VERSION } from "@/lib/legal";

export const metadata: Metadata = { title: "Privacy Policy" };

export default function PrivacyPage() {
    return (
        <LegalPage title="Privacy Policy" version={PRIVACY_POLICY_VERSION}>
            {/* TODO(legal): replace with the Privacy Policy written by the team (GDPR). */}
            <p>
                The StemHub Privacy Policy is being written. This page will explain which personal data
                StemHub collects, why, how long it is kept, who it is shared with, and how to exercise your
                rights under the GDPR.
            </p>
        </LegalPage>
    );
}
