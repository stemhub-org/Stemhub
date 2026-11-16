import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import PullRequestPage from "./page";

let search = "id=p1&pr=pr1";
vi.mock("next/navigation", () => ({
    useSearchParams: () => new URLSearchParams(search),
    useRouter: () => ({ push: vi.fn(), replace: vi.fn() }),
    usePathname: () => "/projects/pull-requests",
}));

const main = { id: "b-main", name: "main", is_deleted: false };
const feature = { id: "b-feat", name: "drop-plus-fort", is_deleted: false };

function pullRequest(overrides: Record<string, unknown> = {}) {
    return {
        id: "pr1",
        project_id: "p1",
        source_branch_id: feature.id,
        target_branch_id: main.id,
        title: "Louder drop",
        description: "Kick +2 dB",
        status: "OPEN",
        source_head_version_id: null,
        target_head_version_id: null,
        created_by: "jb",
        created_at: new Date().toISOString(),
        closed_by: null,
        closed_at: null,
        is_deleted: false,
        deleted_at: null,
        author: { id: "jb", username: "jb" },
        source_branch: feature,
        target_branch: main,
        ...overrides,
    };
}

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

interface Setup {
    me?: string;
    collaborators?: unknown[];
    pr?: Record<string, unknown>;
    accept?: () => Response;
}

const mixerDiff = {
    status: "compared",
    status_message: null,
    base_version_id: "v-main",
    head_version_id: "v-feat",
    summary: { total_changes: 1, inserts_changed: 1, slots_changed: 0, parameter_changes: 1 },
    changes: [
        {
            type: "insert_volume_changed", insert_index: 1, insert_name: "Kick", slot_index: null,
            before: 100, after: 120, message: 'Insert 1 "Kick" volume changed: 100 -> 120',
        },
    ],
};

const diffRequests = (fetchMock: ReturnType<typeof renderPage>) =>
    fetchMock.mock.calls.filter(([url]) => String(url).endsWith("/pull-requests/pr1/diff"));

function renderPage({ me = "owner", collaborators = [], pr = pullRequest(), accept }: Setup = {}) {
    const fetchMock = vi.fn(async (url: string, init?: RequestInit) => {
        const path = new URL(url).pathname;
        if (path === "/auth/me") return json({ id: me, username: me });
        if (path === "/projects/p1/summary") {
            return json({
                project: { id: "p1", name: "Demo", owner: { id: "owner", username: "owner" } },
                branches: [],
                recent_versions: [],
                latest_version_id: null,
                has_preview: false,
            });
        }
        if (path === "/projects/p1/collaborators/") return json(collaborators);
        if (path === "/pull-requests/pr1/accept" && init?.method === "POST") {
            return accept ? accept() : json(pullRequest({ status: "MERGED", closed_by: me, closed_at: new Date().toISOString() }));
        }
        if (path === "/pull-requests/pr1/diff") return json(mixerDiff);
        if (path === "/pull-requests/pr1") return json(pr);
        return json({ detail: "Not found" }, 404);
    });
    vi.stubGlobal("fetch", fetchMock);
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    render(<PullRequestPage />, {
        wrapper: ({ children }: { children: ReactNode }) => <QueryClientProvider client={client}>{children}</QueryClientProvider>,
    });
    return fetchMock;
}

const editor = { project_id: "p1", user_id: "ed", role: "Editor", created_at: "", user: null };
const viewer = { project_id: "p1", user_id: "vi", role: "Viewer", created_at: "", user: null };

describe("PullRequestPage", () => {
    beforeEach(() => {
        search = "id=p1&pr=pr1";
    });

    it("shows the pull request with its author, branches and description", async () => {
        renderPage();

        expect(await screen.findByRole("heading", { name: "Louder drop" })).toBeTruthy();
        expect(screen.getByText("drop-plus-fort → main")).toBeTruthy();
        expect(screen.getByText(/jb opened this/)).toBeTruthy();
        expect(screen.getByText("Kick +2 dB")).toBeTruthy();
        expect(screen.getByRole("link", { name: /Back to project/ }).getAttribute("href")).toBe("/projects?id=p1");
    });

    it("shows the signed-in user in the header", async () => {
        renderPage({ me: "raprap" });

        expect(await screen.findByText("raprap")).toBeTruthy();
        expect(screen.queryByText("Producer")).toBeNull();
    });

    it("gives the owner Close and Accept", async () => {
        renderPage();

        expect(await screen.findByRole("button", { name: "Accept" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "Close" })).toBeTruthy();
    });

    it("gives an Editor Close only, and a Viewer nothing", async () => {
        renderPage({ me: "ed", collaborators: [editor] });
        expect(await screen.findByRole("button", { name: "Close" })).toBeTruthy();
        expect(screen.queryByRole("button", { name: "Accept" })).toBeNull();
    });

    it("gives a Viewer no button", async () => {
        renderPage({ me: "vi", collaborators: [viewer] });
        await screen.findByRole("heading", { name: "Louder drop" });
        expect(screen.queryByRole("button", { name: "Close" })).toBeNull();
        expect(screen.queryByRole("button", { name: "Accept" })).toBeNull();
    });

    it("shows no button on a merged pull request", async () => {
        renderPage({ pr: pullRequest({ status: "MERGED", closed_by: "owner", closed_at: new Date().toISOString() }) });
        expect(await screen.findByText("MERGED")).toBeTruthy();
        expect(screen.queryByRole("button", { name: "Accept" })).toBeNull();
    });

    it("accepts the pull request and shows it merged", async () => {
        const fetchMock = renderPage();

        fireEvent.click(await screen.findByRole("button", { name: "Accept" }));

        expect(await screen.findByText("MERGED")).toBeTruthy();
        expect(fetchMock).toHaveBeenCalledWith(
            "http://localhost:8000/pull-requests/pr1/accept",
            expect.objectContaining({ method: "POST" }),
        );
    });

    it("shows what accepting an open pull request would change in the mixer", async () => {
        renderPage();

        expect(await screen.findByRole("heading", { name: "Changes" })).toBeTruthy();
        expect(await screen.findByRole("row", { name: /Volume/ })).toBeTruthy();
        expect(screen.getByRole("columnheader", { name: "drop-plus-fort" })).toBeTruthy();
    });

    it("removes the changes once the pull request is accepted", async () => {
        renderPage();
        await screen.findByRole("row", { name: /Volume/ });

        fireEvent.click(await screen.findByRole("button", { name: "Accept" }));

        expect(await screen.findByText("MERGED")).toBeTruthy();
        expect(screen.queryByRole("table")).toBeNull();
        expect(screen.getByRole("link", { name: "View history" })).toBeTruthy();
    });

    it("accepts the source version whose changes are on screen", async () => {
        const fetchMock = renderPage();
        await screen.findByRole("row", { name: /Volume/ });

        fireEvent.click(await screen.findByRole("button", { name: "Accept" }));

        await screen.findByText("MERGED");
        const accept = fetchMock.mock.calls.find(([url]) => String(url).endsWith("/pull-requests/pr1/accept"));
        expect(JSON.parse(accept?.[1]?.body as string)).toEqual({ expected_source_head_version_id: "v-feat" });
    });

    it("reloads the changes when the source branch got a new version before accepting", async () => {
        const refusal = "The source branch has new versions since you reviewed the changes: review them before accepting";
        const fetchMock = renderPage({ accept: () => json({ detail: refusal }, 409) });
        await screen.findByRole("row", { name: /Volume/ });

        fireEvent.click(await screen.findByRole("button", { name: "Accept" }));

        expect(await screen.findByText(refusal)).toBeTruthy();
        await waitFor(() => expect(diffRequests(fetchMock)).toHaveLength(2));
    });

    it("points an accepted pull request to the target branch's history instead of a diff", async () => {
        const fetchMock = renderPage({ pr: pullRequest({ status: "MERGED", closed_at: new Date().toISOString() }) });

        const link = await screen.findByRole("link", { name: "View history" });
        expect(link.getAttribute("href")).toBe("/projects/history?id=p1&branch_id=b-main");
        expect(screen.getByText(/The accepted changes are in the history of main/)).toBeTruthy();
        expect(screen.queryByRole("heading", { name: "Changes" })).toBeNull();
        expect(diffRequests(fetchMock)).toHaveLength(0);
    });

    it("says a closed pull request applied nothing", async () => {
        const fetchMock = renderPage({ pr: pullRequest({ status: "CLOSED", closed_at: new Date().toISOString() }) });

        expect(await screen.findByText("This pull request was closed: nothing was applied.")).toBeTruthy();
        expect(screen.queryByRole("link", { name: "View history" })).toBeNull();
        expect(diffRequests(fetchMock)).toHaveLength(0);
    });

    it("shows the server's refusal and keeps the pull request open", async () => {
        renderPage({ accept: () => json({ detail: "The target branch has new versions since this pull request was opened" }, 409) });

        fireEvent.click(await screen.findByRole("button", { name: "Accept" }));

        expect(await screen.findByText("The target branch has new versions since this pull request was opened")).toBeTruthy();
        expect(screen.getByText("OPEN")).toBeTruthy();
    });

    it("reloads the pull request when someone else already closed it", async () => {
        let closedElsewhere = false;
        const closed = pullRequest({ status: "CLOSED", closed_by: "ed", closed_at: new Date().toISOString() });
        renderPage({
            accept: () => {
                closedElsewhere = true;
                return json({ detail: "Pull request is already CLOSED" }, 409);
            },
        });
        // The next read of the pull request sees the other user's close.
        const original = vi.mocked(fetch).getMockImplementation()!;
        vi.mocked(fetch).mockImplementation(async (url, init) =>
            closedElsewhere && new URL(String(url)).pathname === "/pull-requests/pr1" ? json(closed) : original(url, init),
        );

        fireEvent.click(await screen.findByRole("button", { name: "Accept" }));

        expect(await screen.findByText("CLOSED")).toBeTruthy();
        expect(screen.queryByRole("button", { name: "Accept" })).toBeNull();
    });

    it("marks a deleted source branch", async () => {
        renderPage({ pr: pullRequest({ source_branch: { ...feature, is_deleted: true } }) });
        expect(await screen.findByText("drop-plus-fort (deleted) → main")).toBeTruthy();
    });

    it("says so when the pull request id is missing", async () => {
        search = "id=p1";
        renderPage();
        expect(await screen.findByText("Pull request not found.")).toBeTruthy();
    });
});
