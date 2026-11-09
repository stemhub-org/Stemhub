import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import Sidebar from "./Sidebar";

vi.mock("next/navigation", () => ({ usePathname: () => "/dashboard", useRouter: () => ({ push: vi.fn() }) }));

describe("Sidebar brand", () => {
    it("shows the StemHub mark and wordmark linking to the dashboard", () => {
        render(<Sidebar />, {
            wrapper: ({ children }: { children: ReactNode }) => (
                <QueryClientProvider client={new QueryClient()}>{children}</QueryClientProvider>
            ),
        });

        const home = screen.getByRole("link", { name: "StemHub home" });
        expect(home.getAttribute("href")).toBe("/dashboard");
        expect(home.querySelector("svg")).not.toBeNull();
        expect(home.textContent).toContain("StemHub");
    });
});
