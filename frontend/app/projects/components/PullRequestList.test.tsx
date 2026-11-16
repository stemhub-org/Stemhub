import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { PullRequestList } from "./PullRequestList";

const main = { id: "b-main", name: "main", is_deleted: false };
const feature = { id: "b-feat", name: "drop-plus-fort", is_deleted: false };

function pullRequest(overrides: Record<string, unknown> = {}) {
    return {
        id: "pr1",
        project_id: "p1",
        source_branch_id: feature.id,
        target_branch_id: main.id,
        title: "Louder drop",
        description: null,
        status: "OPEN",
        source_head_version_id: null,
        target_head_version_id: null,
        created_by: "u1",
        created_at: new Date().toISOString(),
        closed_by: null,
        closed_at: null,
        is_deleted: false,
        deleted_at: null,
        author: { id: "u1", username: "jb" },
        source_branch: feature,
        target_branch: main,
        ...overrides,
    };
}

function renderList(body: unknown) {
    vi.stubGlobal(
        "fetch",
        vi.fn(async () => new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } })),
    );
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    render(<PullRequestList projectId="p1" />, {
        wrapper: ({ children }: { children: ReactNode }) => <QueryClientProvider client={client}>{children}</QueryClientProvider>,
    });
}

describe("PullRequestList", () => {
    it("lists each pull request with its branches, author and status, linking to its page", async () => {
        renderList([pullRequest()]);

        const link = await screen.findByRole("link", { name: /Louder drop/ });
        expect(link.getAttribute("href")).toBe("/projects/pull-requests?id=p1&pr=pr1");
        expect(screen.getByText("drop-plus-fort → main")).toBeTruthy();
        expect(screen.getByText(/jb/)).toBeTruthy();
        expect(screen.getByText("OPEN")).toBeTruthy();
    });

    it("shows Unknown without an author and marks deleted branches", async () => {
        renderList([pullRequest({ author: null, source_branch: { ...feature, is_deleted: true } })]);

        expect(await screen.findByText("drop-plus-fort (deleted) → main")).toBeTruthy();
        expect(screen.getByText(/Unknown/)).toBeTruthy();
    });

    it("says so when there is none", async () => {
        renderList([]);

        expect(await screen.findByText("No pull requests yet.")).toBeTruthy();
    });
});
