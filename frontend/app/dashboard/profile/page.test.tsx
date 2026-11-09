import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
import ProfilePage from "./page";

const push = vi.fn();
vi.mock("next/navigation", () => ({ useRouter: () => ({ push }) }));
vi.mock("next-themes", () => ({ useTheme: () => ({ resolvedTheme: "dark" }) }));

const user = {
    id: "u1",
    email: "jb@stemhub.fr",
    username: "jb",
    avatar_url: null,
    bio: "Beats at night",
    location: "Paris, France",
    website: "jb.example",
    genres: ["house"],
    created_at: "2026-01-15T00:00:00Z",
    is_active: true,
    is_admin: false,
};

const projects = [
    { id: "p1", name: "Night Drive", owner_id: "u1", category: "Trap", is_public: false, is_deleted: false, created_at: "2026-02-01T00:00:00Z", description: null },
    { id: "p2", name: "Shared Loop", owner_id: "u2", category: "House", is_public: true, is_deleted: false, created_at: "2026-02-02T00:00:00Z", description: null },
    { id: "p3", name: "Trashed", owner_id: "u1", category: "Trap", is_public: false, is_deleted: true, created_at: "2026-02-03T00:00:00Z", description: null },
];

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function renderPage() {
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const Wrapper = ({ children }: { children: ReactNode }) => (
        <QueryClientProvider client={client}>
            <ToastProvider>{children}</ToastProvider>
        </QueryClientProvider>
    );
    return { client, ...render(<ProfilePage />, { wrapper: Wrapper }) };
}

function stubApi(overrides: { me?: () => Response; put?: (body: unknown) => Response } = {}) {
    const fetchMock = vi.fn(async (url: string, init?: RequestInit) => {
        if (init?.method === "PUT") return overrides.put ? overrides.put(JSON.parse(String(init.body))) : json(user);
        if (url.endsWith("/auth/me")) return overrides.me ? overrides.me() : json(user);
        if (url.endsWith("/projects/")) return json(projects);
        throw new Error(`Unexpected request ${url}`);
    });
    vi.stubGlobal("fetch", fetchMock);
    return fetchMock;
}

describe("ProfilePage", () => {
    it("shows the producer, their styles and live project counts", async () => {
        stubApi();

        renderPage();

        expect(await screen.findByRole("heading", { name: "jb" })).toBeTruthy();
        expect(screen.getByText("Beats at night")).toBeTruthy();
        expect(screen.getByText("house")).toBeTruthy();
        expect(await screen.findByText("Night Drive")).toBeTruthy();
        expect(screen.queryByText("Trashed")).toBeNull();
        expect(screen.getByText("Projects").nextElementSibling?.textContent).toBe("1");
        expect(screen.getByText("Collaborations").nextElementSibling?.textContent).toBe("1");
    });

    it("shows the error with a retry instead of sending the user to /login", async () => {
        let calls = 0;
        stubApi({
            me: () => {
                calls += 1;
                return calls === 1 ? json({ detail: "Database unavailable" }, 503) : json(user);
            },
        });

        renderPage();

        expect((await screen.findByRole("alert")).textContent).toContain("Database unavailable");
        expect(push).not.toHaveBeenCalled();

        fireEvent.click(screen.getByRole("button", { name: "Try again" }));

        expect(await screen.findByRole("heading", { name: "jb" })).toBeTruthy();
    });
});
