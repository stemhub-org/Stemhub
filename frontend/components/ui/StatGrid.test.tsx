import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { StatGrid } from "./StatGrid";

describe("StatGrid", () => {
    it("pairs each label with its figure, unit and hint", () => {
        render(
            <StatGrid
                stats={[
                    { label: "Projects", value: 6, hint: "2 public · 4 private" },
                    { label: "Storage", value: "18.2", unit: "GB" },
                ]}
            />
        );

        const terms = screen.getAllByRole("term").map((term) => term.textContent);
        expect(terms).toEqual(["Projects", "Storage"]);
        expect(screen.getByText("6")).toBeTruthy();
        expect(screen.getByText("2 public · 4 private")).toBeTruthy();
        expect(screen.getByText("GB")).toBeTruthy();
    });
});
