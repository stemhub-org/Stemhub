import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
import ProjectPage from "./page";

let search = "id=p1";
vi.mock("next/navigation", () => ({
    useSearchParams: () => new URLSearchParams(search),
    useRouter: () => ({ push: vi.fn(), replace: vi.fn() }),
    usePathname: () => "/projects",
}));

function renderPage() {
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const Wrapper = ({ children }: { children: ReactNode }) => (
        <QueryClientProvider client={client}>
            <ToastProvider>{children}</ToastProvider>
        </QueryClientProvider>
    );
    return render(<ProjectPage />, { wrapper: Wrapper });
}

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

describe("ProjectPage", () => {
    beforeEach(() => {
        search = "id=p1";
    });

    it("explains a missing project id", async () => {
        search = "";
        vi.stubGlobal("fetch", vi.fn());

        renderPage();

        expect((await screen.findByRole("alert")).textContent).toContain("No project ID provided");
    });

    it("shows why the project failed to load and retries it", async () => {
        let summaryCalls = 0;
        vi.stubGlobal(
            "fetch",
            vi.fn(async (url: string) => {
                if (url.includes("/summary")) {
                    summaryCalls += 1;
                    return json({ detail: "Project not found" }, 404);
                }
                return json({});
            }),
        );

        renderPage();

        expect((await screen.findByRole("alert")).textContent).toContain("Project not found");

        fireEvent.click(screen.getByRole("button", { name: "Try again" }));

        await waitFor(() => expect(summaryCalls).toBe(2));
    });

    it("shows the project's activity feed next to the overview", async () => {
        const summary = {
            project: { id: "p1", name: "Night Drive", description: null, owner: { id: "u1", username: "jb" } },
            branches: [{ id: "b-main", name: "main" }],
            recent_versions: [],
            latest_version_id: null,
            has_preview: false,
        };
        vi.stubGlobal(
            "fetch",
            vi.fn(async (url: string) => {
                if (url.includes("/summary")) return json(summary);
                if (url.includes("/activity?")) {
                    return json({
                        items: [
                            {
                                id: "branch_created:b-main",
                                type: "branch_created",
                                occurred_at: new Date().toISOString(),
                                actor: null,
                                branch: { id: "b-main", name: "main", is_deleted: false },
                                version: null,
                                pull_request: null,
                            },
                        ],
                        next_cursor: null,
                    });
                }
                if (url.endsWith("/auth/me")) return json({ id: "u1", username: "jb" });
                return json({});
            }),
        );

        renderPage();

        expect(await screen.findByRole("heading", { name: "Activity" })).toBeTruthy();
        expect(await screen.findByText("Branch main was created")).toBeTruthy();
    });
});
