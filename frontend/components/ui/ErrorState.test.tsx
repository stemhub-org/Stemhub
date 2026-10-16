import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ApiError } from "@/lib/api";
import { ErrorState } from "./ErrorState";

describe("ErrorState", () => {
    it("announces the title and the error message", () => {
        render(<ErrorState error={new ApiError(500, "Database unavailable")} title="Couldn't load projects" />);

        const alert = screen.getByRole("alert");
        expect(alert.textContent).toContain("Couldn't load projects");
        expect(alert.textContent).toContain("Database unavailable");
    });

    it("falls back to a generic message for unknown errors", () => {
        render(<ErrorState error={null} />);

        expect(screen.getByRole("alert").textContent).toContain("Something went wrong. Please try again.");
    });

    it("calls onRetry from the retry button", () => {
        const onRetry = vi.fn();
        render(<ErrorState error={new Error("offline")} onRetry={onRetry} />);

        fireEvent.click(screen.getByRole("button", { name: "Try again" }));

        expect(onRetry).toHaveBeenCalledOnce();
    });

    it("has no retry button without onRetry", () => {
        render(<ErrorState error={new Error("offline")} />);

        expect(screen.queryByRole("button")).toBeNull();
    });
});
