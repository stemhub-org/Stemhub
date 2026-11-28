import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { StatusLed } from "./StatusLed";

describe("StatusLed", () => {
    it("always shows its label next to the light", () => {
        render(<StatusLed status="failed" label="Upload failed" />);

        expect(screen.getByText("Upload failed")).toBeTruthy();
    });

    it("only pulses the syncing light, and only when motion is allowed", () => {
        const { container } = render(
            <>
                <StatusLed status="syncing" label="Syncing" />
                <StatusLed status="active" label="Linked" />
            </>
        );

        expect(container.querySelector('[data-status="syncing"]')?.className).toContain("motion-safe:animate-pulse");
        expect(container.querySelector('[data-status="active"]')?.className).not.toContain("animate");
    });
});
