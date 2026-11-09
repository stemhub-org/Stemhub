import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { API_URL } from "@/lib/api";
import { queryKeys } from "@/lib/queries";
import Sidebar from "./Sidebar";

const push = vi.fn();
vi.mock("next/navigation", () => ({ useRouter: () => ({ push }), usePathname: () => "/dashboard" }));

function renderSidebar(client: QueryClient) {
    const Wrapper = ({ children }: { children: ReactNode }) => (
        <QueryClientProvider client={client}>{children}</QueryClientProvider>
    );
    return render(<Sidebar />, { wrapper: Wrapper });
}

describe("Sidebar logout", () => {
    it("signs out, forgets the cached user data and goes to the login page", async () => {
        const fetchMock = vi.fn(async () => new Response(null, { status: 204 }));
        vi.stubGlobal("fetch", fetchMock);
        localStorage.setItem("access_token", "tok");
        const client = new QueryClient();
        client.setQueryData(queryKeys.currentUser, { id: "u1", username: "jb" });

        renderSidebar(client);
        fireEvent.click(screen.getByRole("button", { name: "Log out" }));

        await waitFor(() => expect(push).toHaveBeenCalledWith("/login"));
        expect(fetchMock).toHaveBeenCalledWith(`${API_URL}/auth/logout`, expect.objectContaining({ method: "POST" }));
        expect(localStorage.getItem("access_token")).toBeNull();
        expect(client.getQueryData(queryKeys.currentUser)).toBeUndefined();
    });

    it("still signs out locally when the server can't be reached", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => { throw new TypeError("Failed to fetch"); }));
        localStorage.setItem("access_token", "tok");

        renderSidebar(new QueryClient());
        fireEvent.click(screen.getByRole("button", { name: "Log out" }));

        await waitFor(() => expect(push).toHaveBeenCalledWith("/login"));
        expect(localStorage.getItem("access_token")).toBeNull();
    });
});
