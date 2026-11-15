import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
import { queryKeys } from "@/lib/queries";
import LoginPage from "./page";

const push = vi.fn();
vi.mock("next/navigation", () => ({ useRouter: () => ({ push }) }));

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function submitLogin(client = new QueryClient(), { rememberMe = false } = {}) {
    const Wrapper = ({ children }: { children: ReactNode }) => (
        <QueryClientProvider client={client}>
            <ToastProvider>{children}</ToastProvider>
        </QueryClientProvider>
    );
    render(<LoginPage />, { wrapper: Wrapper });
    fireEvent.change(screen.getByPlaceholderText("Email"), { target: { value: "jb@stemhub.io" } });
    fireEvent.change(screen.getByPlaceholderText("Password"), { target: { value: "secret" } });
    if (rememberMe) fireEvent.click(screen.getByLabelText("Remember me for 30 days"));
    fireEvent.click(screen.getByRole("button", { name: "Log in" }));
    return client;
}

describe("LoginPage", () => {
    beforeEach(() => push.mockReset());

    it("stores the token, drops any cached user and opens the dashboard", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ access_token: "tok", token_type: "bearer" })));
        const client = new QueryClient();
        client.setQueryData(queryKeys.currentUser, { id: "previous-user" });

        submitLogin(client);

        await waitFor(() => expect(push).toHaveBeenCalledWith("/dashboard"));
        expect(client.getQueryData(queryKeys.currentUser)).toBeUndefined();
    });

    it("keeps the token for this browser session only by default", async () => {
        const fetchMock = vi.fn(async () => json({ access_token: "tok", token_type: "bearer" }));
        vi.stubGlobal("fetch", fetchMock);

        submitLogin();

        await waitFor(() => expect(push).toHaveBeenCalledWith("/dashboard"));
        expect(sessionStorage.getItem("access_token")).toBe("tok");
        expect(localStorage.getItem("access_token")).toBeNull();
        expect(fetchMock).toHaveBeenCalledWith(
            expect.stringContaining("/auth/login"),
            expect.objectContaining({
                body: JSON.stringify({ email: "jb@stemhub.io", password: "secret", remember_me: false }),
            }),
        );
    });

    it("remembers the sign-in when asked", async () => {
        const fetchMock = vi.fn(async () => json({ access_token: "tok", token_type: "bearer" }));
        vi.stubGlobal("fetch", fetchMock);

        submitLogin(new QueryClient(), { rememberMe: true });

        await waitFor(() => expect(push).toHaveBeenCalledWith("/dashboard"));
        expect(localStorage.getItem("access_token")).toBe("tok");
        expect(sessionStorage.getItem("access_token")).toBeNull();
        expect(fetchMock).toHaveBeenCalledWith(
            expect.stringContaining("/auth/login"),
            expect.objectContaining({
                body: JSON.stringify({ email: "jb@stemhub.io", password: "secret", remember_me: true }),
            }),
        );
    });

    it("shows the backend's message for wrong credentials without redirecting", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ detail: "Incorrect email or password" }, 401)));

        submitLogin();

        expect((await screen.findByRole("alert")).textContent).toBe("Incorrect email or password");
        expect(push).not.toHaveBeenCalled();
    });

    it("explains when the server can't be reached", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => { throw new TypeError("Failed to fetch"); }));

        submitLogin();

        expect((await screen.findByRole("alert")).textContent).toContain("Unable to reach the server");
    });

    it("says when signing in cancelled a scheduled account deletion", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ access_token: "tok", token_type: "bearer", account_restored: true })));

        submitLogin();

        expect(await screen.findByText("Welcome back! Your account deletion has been cancelled.")).toBeTruthy();
    });

    it("says nothing on a normal sign-in", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ access_token: "tok", token_type: "bearer", account_restored: false })));

        submitLogin();

        await waitFor(() => expect(push).toHaveBeenCalledWith("/dashboard"));
        expect(screen.queryByText(/account deletion has been cancelled/)).toBeNull();
    });
});
