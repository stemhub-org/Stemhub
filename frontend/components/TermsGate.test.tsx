import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { API_URL } from "@/lib/api";
import { TermsGate } from "./TermsGate";

const push = vi.fn();
vi.mock("next/navigation", () => ({ useRouter: () => ({ push }) }));

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

const user = { id: "u1", email: "jb@stemhub.fr", username: "jb", is_active: true, is_admin: false };

function renderGate() {
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const Wrapper = ({ children }: { children: ReactNode }) => (
        <QueryClientProvider client={client}>{children}</QueryClientProvider>
    );
    render(<TermsGate><p>Dashboard</p></TermsGate>, { wrapper: Wrapper });
}

describe("TermsGate", () => {
    beforeEach(() => push.mockReset());

    it("shows the page once the current terms are accepted", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ ...user, terms_accepted: true })));
        renderGate();
        expect(await screen.findByText("Dashboard")).toBeTruthy();
    });

    it("asks first when they are not, and only continues once ticked", async () => {
        const fetchMock = vi.fn(async (url: string) =>
            url.endsWith("/auth/me/terms") ? json({ ...user, terms_accepted: true }) : json({ ...user, terms_accepted: false }),
        );
        vi.stubGlobal("fetch", fetchMock);
        renderGate();

        expect(await screen.findByRole("heading", { name: "Before you continue" })).toBeTruthy();
        expect(screen.queryByText("Dashboard")).toBeNull();
        const continueButton = screen.getByRole("button", { name: "Continue" }) as HTMLButtonElement;
        expect(continueButton.disabled).toBe(true);

        fireEvent.click(screen.getByRole("checkbox", { name: /I accept the Terms of Service/ }));
        fireEvent.click(continueButton);

        expect(await screen.findByText("Dashboard")).toBeTruthy();
        expect(fetchMock).toHaveBeenCalledWith(
            `${API_URL}/auth/me/terms`,
            expect.objectContaining({ method: "POST", body: JSON.stringify({ accept_terms: true }) }),
        );
    });

    it("explains when the acceptance can't be saved", async () => {
        vi.stubGlobal("fetch", vi.fn(async (url: string) =>
            url.endsWith("/auth/me/terms") ? json({ detail: "Server error" }, 500) : json({ ...user, terms_accepted: false }),
        ));
        renderGate();

        fireEvent.click(await screen.findByRole("checkbox", { name: /I accept the Terms of Service/ }));
        fireEvent.click(screen.getByRole("button", { name: "Continue" }));

        expect((await screen.findByRole("alert")).textContent).toBe("Server error");
        expect(screen.queryByText("Dashboard")).toBeNull();
    });

    it("lets the user log out instead", async () => {
        const fetchMock = vi.fn(async (url: string) =>
            url.endsWith("/auth/logout") ? new Response(null, { status: 204 }) : json({ ...user, terms_accepted: false }),
        );
        vi.stubGlobal("fetch", fetchMock);
        renderGate();

        fireEvent.click(await screen.findByRole("button", { name: "Log out instead" }));

        await waitFor(() => expect(push).toHaveBeenCalledWith("/login"));
        expect(fetchMock).toHaveBeenCalledWith(`${API_URL}/auth/logout`, expect.objectContaining({ method: "POST" }));
    });
});
