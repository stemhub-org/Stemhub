import { render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ProjectHeader } from "./ProjectHeader";

vi.mock("next/navigation", () => ({ useRouter: () => ({ push: vi.fn() }) }));

describe("ProjectHeader", () => {
    it("says beta instead of an invented plan", () => {
        render(<ProjectHeader username="jb" />);

        expect(screen.getByText("jb")).toBeTruthy();
        expect(screen.getByText("Beta")).toBeTruthy();
        expect(screen.queryByText("Free Plan")).toBeNull();
    });
});
