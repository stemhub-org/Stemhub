import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { loadPasswordStrength } from "@/lib/passwordStrength";
import { PasswordStrengthMeter } from "./PasswordStrengthMeter";

async function meterText(password: string, userInputs?: string[]) {
    render(<PasswordStrengthMeter password={password} userInputs={userInputs} />);
    return (await screen.findByText(/Password strength:/)).textContent;
}

describe("PasswordStrengthMeter", () => {
    it("shows nothing until something is typed", async () => {
        const { container } = render(<PasswordStrengthMeter password="" />);
        await loadPasswordStrength();
        expect(container.textContent).toBe("");
    });

    it("calls a common password very weak and says why", async () => {
        const text = await meterText("password");
        expect(text).toContain("Very weak");
        expect(text).toContain("This is a heavily used password.");
        expect(screen.getAllByTestId("strength-segment").filter((s) => s.className.includes("bg-red-500"))).toHaveLength(1);
    });

    it("sees through a common password dressed up with capitals and symbols", async () => {
        expect(await meterText("Password123!")).not.toMatch(/Good|Strong/);
    });

    it("calls a long passphrase strong", async () => {
        const text = await meterText("correct horse battery staple violin");
        expect(text).toContain("Strong");
        expect(screen.getAllByTestId("strength-segment").filter((s) => s.className.includes("bg-green-500"))).toHaveLength(4);
    });

    it("penalizes a password built from the username", async () => {
        const password = "jboshra2026";
        const alone = await loadPasswordStrength().then((check) => check(password).score);
        const withUsername = await loadPasswordStrength().then((check) => check(password, ["jboshra"]).score);
        expect(withUsername).toBeLessThan(alone);
    });
});
