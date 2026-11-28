import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { Button } from "./Button";

describe("Button", () => {
    it("puts Ink text on the Lime primary button, never plain white", () => {
        render(<Button>Save</Button>);

        const button = screen.getByRole("button", { name: "Save" });
        expect(button.className).toContain("bg-primary");
        expect(button.className).toContain("text-primary-foreground");
        expect(button.className).not.toContain("text-white");
    });

    it("has square corners and no glow", () => {
        render(<Button variant="secondary">Import</Button>);

        const className = screen.getByRole("button", { name: "Import" }).className;
        expect(className).not.toMatch(/rounded|shadow/);
    });

    it("is busy and disabled while loading", () => {
        render(<Button loading>Create</Button>);

        const button = screen.getByRole("button", { name: "Create" });
        expect(button.hasAttribute("disabled")).toBe(true);
        expect(button.getAttribute("aria-busy")).toBe("true");
    });
});
