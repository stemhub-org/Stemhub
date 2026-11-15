import { fireEvent, render, screen } from "@testing-library/react";
import { useState } from "react";
import { describe, expect, it } from "vitest";
import { PasswordVisibilityToggle } from "./PasswordVisibilityToggle";

function Field() {
    const [visible, setVisible] = useState(false);
    return (
        <div className="relative">
            <input aria-label="Password" type={visible ? "text" : "password"} defaultValue="secret" />
            <PasswordVisibilityToggle visible={visible} onToggle={() => setVisible(!visible)} />
        </div>
    );
}

describe("PasswordVisibilityToggle", () => {
    it("shows then hides the password, and says which it will do", () => {
        render(<Field />);
        const input = screen.getByLabelText("Password");
        expect(input.getAttribute("type")).toBe("password");

        fireEvent.click(screen.getByRole("button", { name: "Show password" }));
        expect(input.getAttribute("type")).toBe("text");
        expect(screen.getByRole("button", { name: "Hide password" }).getAttribute("aria-pressed")).toBe("true");

        fireEvent.click(screen.getByRole("button", { name: "Hide password" }));
        expect(input.getAttribute("type")).toBe("password");
    });

    it("never submits the form it sits in", () => {
        render(<form><PasswordVisibilityToggle visible={false} onToggle={() => undefined} /></form>);
        expect(screen.getByRole("button", { name: "Show password" }).getAttribute("type")).toBe("button");
    });
});
