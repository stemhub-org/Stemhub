import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { renderHook, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { API_URL } from "./api";
import { ACTIVITY_PAGE_SIZE, fetchProjectActivityPage, useProjectActivityFeed } from "./activityFeed";
import { queryKeys } from "./queries";

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function item(id: string) {
    return {
        id: `version_saved:${id}`,
        type: "version_saved",
        occurred_at: "2026-11-08T10:00:00Z",
        actor: null,
        branch: null,
        version: null,
        pull_request: null,
    };
}

describe("fetchProjectActivityPage", () => {
    it("asks for one page with the page size", async () => {
        const fetchMock = vi.fn(async (_url: string) => json({ items: [], next_cursor: null }));
        vi.stubGlobal("fetch", fetchMock);

        await fetchProjectActivityPage("p1", {});

        expect(fetchMock.mock.calls[0][0]).toBe(`${API_URL}/projects/p1/activity?limit=${ACTIVITY_PAGE_SIZE}`);
    });

    it("passes the cursor and repeats types", async () => {
        const fetchMock = vi.fn(async (_url: string) => json({ items: [], next_cursor: null }));
        vi.stubGlobal("fetch", fetchMock);

        await fetchProjectActivityPage("p1", { cursor: "abc_-", types: ["branch_created", "branch_deleted"] });

        const url = new URL(fetchMock.mock.calls[0][0]);
        expect(url.searchParams.get("cursor")).toBe("abc_-");
        expect(url.searchParams.getAll("types")).toEqual(["branch_created", "branch_deleted"]);
    });
});

describe("useProjectActivityFeed", () => {
    it("loads pages one after the other with the previous next_cursor", async () => {
        const fetchMock = vi.fn(async (url: string) =>
            new URL(url).searchParams.get("cursor") === "page-2"
                ? json({ items: [item("b")], next_cursor: null })
                : json({ items: [item("a")], next_cursor: "page-2" }),
        );
        vi.stubGlobal("fetch", fetchMock);
        const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
        const { result } = renderHook(() => useProjectActivityFeed("p1", null), {
            wrapper: ({ children }: { children: ReactNode }) => (
                <QueryClientProvider client={client}>{children}</QueryClientProvider>
            ),
        });

        await waitFor(() => expect(result.current.isSuccess).toBe(true));
        expect(result.current.hasNextPage).toBe(true);

        await result.current.fetchNextPage();

        await waitFor(() => expect(result.current.data?.pages).toHaveLength(2));
        expect(result.current.data?.pages.flatMap((page) => page.items).map((i) => i.id)).toEqual([
            "version_saved:a",
            "version_saved:b",
        ]);
        expect(result.current.hasNextPage).toBe(false);
    });

    it("is cached under the project so project refreshes reload it", () => {
        expect(queryKeys.projectActivityFeed("p1", null).slice(0, 2)).toEqual(queryKeys.project("p1"));
        expect(queryKeys.projectActivityFeed("p1", ["branch_created"])).not.toEqual(
            queryKeys.projectActivityFeed("p1", null),
        );
    });
});
