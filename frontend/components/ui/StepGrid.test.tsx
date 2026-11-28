import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { StepGrid } from "./StepGrid";

describe("StepGrid", () => {
    it("describes itself to screen readers", () => {
        render(<StepGrid steps={[0, 1, 2, 3]} label="Saves on 3 of the last 4 days" />);

        expect(screen.getByRole("img", { name: "Saves on 3 of the last 4 days" })).toBeTruthy();
    });

    it("draws one square per step and clamps the levels", () => {
        const { container } = render(<StepGrid steps={[-2, 0, 1.4, 9]} label="Activity" />);

        const levels = [...container.querySelectorAll("[data-level]")].map((cell) => cell.getAttribute("data-level"));
        expect(levels).toEqual(["0", "0", "1", "3"]);
    });
});
