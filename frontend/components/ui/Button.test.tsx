import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { Button } from "./Button";

describe("Button", () => {
    it("puts the accent foreground on solid accent buttons, never plain white", () => {
        render(<Button variant="solid">Save</Button>);

        const button = screen.getByRole("button", { name: "Save" });
        expect(button.className).toContain("bg-accent");
        expect(button.className).toContain("text-accent-foreground");
        expect(button.className).not.toContain("text-white");
    });
});
