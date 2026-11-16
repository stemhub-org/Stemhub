import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import PrivacyPage from "@/app/privacy/page";
import TermsPage from "@/app/terms/page";
import { PRIVACY_POLICY_VERSION, TERMS_VERSION } from "@/lib/legal";

describe("legal pages", () => {
    it.each([
        [TermsPage, "Terms of Service", TERMS_VERSION],
        [PrivacyPage, "Privacy Policy", PRIVACY_POLICY_VERSION],
    ])("shows its title and the version users accept", (Page, title, version) => {
        render(<Page />);
        expect(screen.getByRole("heading", { level: 1 }).textContent).toBe(title);
        expect(screen.getByText(`Version of ${version}`)).toBeTruthy();
    });
});
