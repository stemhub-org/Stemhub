import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import type { Branch } from "@/types/project";
import { PullRequestsTab } from "./PullRequestsTab";

vi.mock("next/navigation", () => ({ useRouter: () => ({ push: vi.fn() }) }));

function branch(id: string, name: string): Branch {
    return { id, name, project_id: "p1", created_at: "2026-11-01T00:00:00Z", is_deleted: false, deleted_at: null };
}

function renderTab(currentUserId: string, collaborators: unknown[], pullRequests: unknown[] = []) {
    vi.stubGlobal(
        "fetch",
        vi.fn(async (url: string) => {
            const body = url.endsWith("/collaborators/") ? collaborators : pullRequests;
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

function pr(id: string, status: string) {
    const main = { id: "b-main", name: "main", is_deleted: false };
    const feature = { id: "b-feat", name: "drop-plus-fort", is_deleted: false };
    return {
        id, project_id: "p1", source_branch_id: feature.id, target_branch_id: main.id, title: `PR ${id}`,
        description: null, status, source_head_version_id: null, target_head_version_id: null, created_by: null,
        created_at: "2026-11-01T00:00:00Z", closed_by: null, closed_at: null, is_deleted: false, deleted_at: null,
        author: null, source_branch: feature, target_branch: main,
    };
}

describe("PullRequestsTab", () => {
    it("counts the pull requests by status above the list", async () => {
        renderTab("u3", [], [pr("1", "OPEN"), pr("2", "OPEN"), pr("3", "MERGED"), pr("4", "CLOSED")]);

        expect(await screen.findByText("2 open · 1 accepted · 1 closed")).toBeTruthy();
    });

    it("shows an Editor the creation form next to the list", async () => {
        renderTab("u2", [{ project_id: "p1", user_id: "u2", role: "Editor", created_at: "", user: null }]);

        expect(await screen.findByLabelText("Title")).toBeTruthy();
        expect(screen.getByRole("heading", { name: "Pull requests" })).toBeTruthy();
    });

    it("only lists pull requests for a Viewer", async () => {
        renderTab("u3", [{ project_id: "p1", user_id: "u3", role: "Viewer", created_at: "", user: null }]);

        expect(await screen.findByText("No pull requests yet.")).toBeTruthy();
        expect(screen.queryByLabelText("Title")).toBeNull();
    });
});
