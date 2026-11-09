import { describe, expect, it, vi } from "vitest";
import { API_URL } from "./api";
import { ACTIVITY_PAGE_SIZE, fetchProjectActivityPage } from "./activityFeed";

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
