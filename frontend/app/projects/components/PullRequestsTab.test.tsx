import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import type { Branch } from "@/types/project";
import { PullRequestsTab } from "./PullRequestsTab";

vi.mock("next/navigation", () => ({ useRouter: () => ({ push: vi.fn() }) }));

function branch(id: string, name: string): Branch {
    return { id, name, project_id: "p1", created_at: "2026-11-01T00:00:00Z", is_deleted: false, deleted_at: null };
}

function renderTab(currentUserId: string, collaborators: unknown[]) {
    vi.stubGlobal(
        "fetch",
        vi.fn(async (url: string) => {
            const body = url.endsWith("/collaborators/") ? collaborators : [];
            return new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } });
        }),
    );
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    render(
        <PullRequestsTab
            projectId="p1"
            ownerId="owner"
            currentUserId={currentUserId}
            branches={[branch("b-main", "main"), branch("b-feat", "drop-plus-fort")]}
        />,
        { wrapper: ({ children }: { children: ReactNode }) => <QueryClientProvider client={client}>{children}</QueryClientProvider> },
    );
}

describe("PullRequestsTab", () => {
    it("lets an Editor open the creation form", async () => {
        renderTab("u2", [{ project_id: "p1", user_id: "u2", role: "Editor", created_at: "", user: null }]);

        fireEvent.click(await screen.findByRole("button", { name: "New pull request" }));

        expect(screen.getByLabelText("Title")).toBeTruthy();
    });

    it("only lists pull requests for a Viewer", async () => {
        renderTab("u3", [{ project_id: "p1", user_id: "u3", role: "Viewer", created_at: "", user: null }]);

        expect(await screen.findByText("No pull requests yet.")).toBeTruthy();
        expect(screen.queryByRole("button", { name: "New pull request" })).toBeNull();
    });
});
