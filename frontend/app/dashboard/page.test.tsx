import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
import DashboardPage from "./page";

vi.mock("next/navigation", () => ({ useRouter: () => ({ push: vi.fn() }) }));

function renderPage() {
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const Wrapper = ({ children }: { children: ReactNode }) => (
        <QueryClientProvider client={client}>
            <ToastProvider>{children}</ToastProvider>
        </QueryClientProvider>
    );
    return render(<DashboardPage />, { wrapper: Wrapper });
}

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

describe("DashboardPage", () => {
    it("shows the user and their projects", async () => {
        vi.stubGlobal(
            "fetch",
            vi.fn(async (url: string) =>
                url.endsWith("/auth/me")
                    ? json({ id: "u1", username: "jb" })
                    : json([{ id: "p1", name: "Night Drive", category: "Trap", created_at: new Date().toISOString(), is_deleted: false }]),
            ),
        );

        renderPage();

        expect(await screen.findByText("Night Drive")).toBeTruthy();
        expect(screen.getByText("jb")).toBeTruthy();
    });

    it("shows the error and retries instead of an empty dashboard", async () => {
        let projectsCalls = 0;
        vi.stubGlobal(
            "fetch",
            vi.fn(async (url: string) => {
                if (url.endsWith("/auth/me")) return json({ id: "u1", username: "jb" });
                projectsCalls += 1;
                return projectsCalls === 1
                    ? json({ detail: "Database unavailable" }, 503)
                    : json([{ id: "p1", name: "Night Drive", category: "Trap", created_at: new Date().toISOString(), is_deleted: false }]);
            }),
        );

        renderPage();

        const alert = await screen.findByRole("alert");
        expect(alert.textContent).toContain("Database unavailable");

        fireEvent.click(screen.getByRole("button", { name: "Try again" }));

        expect(await screen.findByText("Night Drive")).toBeTruthy();
    });
});
