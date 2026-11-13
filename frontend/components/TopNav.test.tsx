import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { act, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { queryKeys } from "@/lib/queries";
import TopNav from "./TopNav";

vi.mock("next/navigation", () => ({ useRouter: () => ({ push: vi.fn() }) }));

describe("TopNav", () => {
    it("shows the signed-in user and follows updates to the cached user", async () => {
        vi.stubGlobal(
            "fetch",
            vi.fn(async () => new Response(JSON.stringify({ id: "u1", username: "jb", avatar_url: null }), {
                headers: { "Content-Type": "application/json" },
            })),
        );
        const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
        const Wrapper = ({ children }: { children: ReactNode }) => (
            <QueryClientProvider client={client}>{children}</QueryClientProvider>
        );

        render(<TopNav />, { wrapper: Wrapper });

        expect(await screen.findByText("jb")).toBeTruthy();

        // What the profile page does after a successful save.
        act(() => {
            client.setQueryData(queryKeys.currentUser, { id: "u1", username: "jb-renamed", avatar_url: null });
        });

        expect(await screen.findByText("jb-renamed")).toBeTruthy();
    });
});
