import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import HistoryPage from "./page";

let search = "";
const replace = vi.fn();
vi.mock("next/navigation", () => ({
    useSearchParams: () => new URLSearchParams(search),
    useRouter: () => ({ push: vi.fn(), replace }),
    usePathname: () => "/projects/history",
}));

const branches = [
    { id: "b-main", name: "main" },
    { id: "b-feat", name: "feature" },
];
const summary = {
    project: { id: "p1", name: "Night Drive", owner: { id: "u1", username: "jb" } },
    branches,
    recent_versions: [],
    latest_version_id: null,
    has_preview: false,
};

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function renderPage() {
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const Wrapper = ({ children }: { children: ReactNode }) => (
        <QueryClientProvider client={client}>{children}</QueryClientProvider>
    );
    return render(<HistoryPage />, { wrapper: Wrapper });
}

describe("HistoryPage", () => {
    beforeEach(() => replace.mockReset());

    it("opens the first branch and puts it in the URL when none is given", async () => {
        search = "id=p1";
        vi.stubGlobal("fetch", vi.fn(async (url: string) => (url.includes("diff-history") ? json([]) : json(summary))));

        renderPage();

        await waitFor(() => expect(replace).toHaveBeenCalledWith("/projects/history?id=p1&branch_id=b-main"));
        expect(await screen.findByText("This branch does not have any versions yet.")).toBeTruthy();
    });

    it("keeps the URL when the requested branch exists", async () => {
        search = "id=p1&branch_id=b-feat";
        vi.stubGlobal("fetch", vi.fn(async (url: string) => (url.includes("diff-history") ? json([]) : json(summary))));

        renderPage();

        expect(await screen.findByText("This branch does not have any versions yet.")).toBeTruthy();
        expect(replace).not.toHaveBeenCalled();
    });

    it("shows the error with a retry button when the history fails", async () => {
        search = "id=p1&branch_id=b-main";
        vi.stubGlobal(
            "fetch",
            vi.fn(async (url: string) =>
                url.includes("diff-history") ? json({ detail: "Parser crashed" }, 500) : json(summary),
            ),
        );

        renderPage();

        expect((await screen.findByRole("alert")).textContent).toContain("Parser crashed");
        expect(screen.getByRole("button", { name: "Try again" })).toBeTruthy();
    });
});
