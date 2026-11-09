import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ActivityFeed } from "./ActivityFeed";

const main = { id: "b-main", name: "main", is_deleted: false };

function saved(id: string, message: string, occurredAt = new Date().toISOString()) {
    return {
        id: `version_saved:${id}`,
        type: "version_saved",
        occurred_at: occurredAt,
        actor: { id: "u1", username: "jb" },
        branch: main,
        version: { id, message, source_daw: "FL Studio", source_project_filename: "beat.flp" },
        pull_request: null,
    };
}

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function renderFeed(respond: (url: URL) => Response) {
    const fetchMock = vi.fn(async (url: string) => respond(new URL(url)));
    vi.stubGlobal("fetch", fetchMock);
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    render(<ActivityFeed projectId="p1" />, {
        wrapper: ({ children }: { children: ReactNode }) => (
            <QueryClientProvider client={client}>{children}</QueryClientProvider>
        ),
    });
    return fetchMock;
}

describe("ActivityFeed", () => {
    it("lists what happened, grouped by day", async () => {
        renderFeed(() => json({ items: [saved("v1", "Faster hi-hats")], next_cursor: null }));

        expect(await screen.findByText("jb saved a version on main")).toBeTruthy();
        expect(screen.getByText("Faster hi-hats")).toBeTruthy();
        expect(screen.getByText("FL Studio · beat.flp")).toBeTruthy();
        expect(screen.getByText("Today")).toBeTruthy();
        expect(screen.queryByRole("button", { name: "Load older activity" })).toBeNull();
    });

    it("loads older activity on demand", async () => {
        renderFeed((url) =>
            url.searchParams.get("cursor") === "older"
                ? json({ items: [saved("v0", "First beat")], next_cursor: null })
                : json({ items: [saved("v1", "Faster hi-hats")], next_cursor: "older" }),
        );

        fireEvent.click(await screen.findByRole("button", { name: "Load older activity" }));

        expect(await screen.findByText("First beat")).toBeTruthy();
        expect(screen.getByText("Faster hi-hats")).toBeTruthy();
        await waitFor(() => expect(screen.queryByRole("button", { name: "Load older activity" })).toBeNull());
    });

    it("shows the error with a retry", async () => {
        let calls = 0;
        renderFeed(() => {
            calls += 1;
            return calls === 1
                ? json({ detail: "Database unavailable" }, 503)
                : json({ items: [saved("v1", "Faster hi-hats")], next_cursor: null });
        });

        expect((await screen.findByRole("alert")).textContent).toContain("Database unavailable");
        fireEvent.click(screen.getByRole("button", { name: "Try again" }));

        expect(await screen.findByText("Faster hi-hats")).toBeTruthy();
    });
});
