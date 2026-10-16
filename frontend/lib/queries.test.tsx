import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { renderHook, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { API_URL, ApiError } from "./api";
import { fetchProjectSummary, queryKeys, useCreateProject, useCurrentUser, useProjects } from "./queries";

function wrapper() {
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    return function Wrapper({ children }: { children: ReactNode }) {
        return <QueryClientProvider client={client}>{children}</QueryClientProvider>;
    };
}

function mockJson(body: unknown, status = 200) {
    const fetchMock = vi.fn(async () =>
        new Response(JSON.stringify(body), {
            status,
            headers: { "Content-Type": "application/json" },
        }),
    );
    vi.stubGlobal("fetch", fetchMock);
    return fetchMock;
}

describe("queryKeys", () => {
    it("are stable so pages can invalidate them", () => {
        expect(queryKeys.currentUser).toEqual(["auth", "me"]);
        expect(queryKeys.projects).toEqual(["projects"]);
    });
});

describe("useCurrentUser", () => {
    it("loads the signed-in user from /auth/me", async () => {
        const fetchMock = mockJson({ id: "u1", username: "jb" });

        const { result } = renderHook(() => useCurrentUser(), { wrapper: wrapper() });

        await waitFor(() => expect(result.current.isSuccess).toBe(true));
        expect(result.current.data).toEqual({ id: "u1", username: "jb" });
        expect(fetchMock).toHaveBeenCalledWith(`${API_URL}/auth/me`, expect.anything());
    });

    it("exposes failures as ApiError", async () => {
        mockJson({ detail: "Server exploded" }, 500);

        const { result } = renderHook(() => useCurrentUser(), { wrapper: wrapper() });

        await waitFor(() => expect(result.current.isError).toBe(true));
        expect(result.current.error).toBeInstanceOf(ApiError);
        expect(result.current.error?.message).toBe("Server exploded");
    });
});

describe("fetchProjectSummary", () => {
    const branches = [{ id: "b-main" }, { id: "b-feat" }];

    function mockSummaries() {
        const fetchMock = vi.fn(async (url: string) => {
            const branchId = new URL(url).searchParams.get("branch_id");
            // Like the backend: an unknown branch_id is a 404.
            if (branchId && !branches.some((branch) => branch.id === branchId)) {
                return new Response(JSON.stringify({ detail: "Branch not found in project" }), { status: 404 });
            }
            return new Response(JSON.stringify({ branches, requested: branchId }), {
                headers: { "Content-Type": "application/json" },
            });
        });
        vi.stubGlobal("fetch", fetchMock);
        return fetchMock;
    }

    it("returns the requested branch when it exists", async () => {
        const fetchMock = mockSummaries();

        const result = await fetchProjectSummary("p1", "b-feat");

        expect(result.branchId).toBe("b-feat");
        expect(fetchMock).toHaveBeenCalledTimes(1);
    });

    it("falls back to the first branch when none or an unknown one is requested", async () => {
        for (const requested of [null, "b-deleted"]) {
            const fetchMock = mockSummaries();

            const result = await fetchProjectSummary("p1", requested);

            expect(result.branchId).toBe("b-main");
            expect(result.summary).toMatchObject({ requested: "b-main" });
            expect(fetchMock).toHaveBeenLastCalledWith(
                `${API_URL}/projects/p1/summary?branch_id=b-main`,
                expect.anything(),
            );
        }
    });

    it("returns the all-branches summary when the project has no branches", async () => {
        mockJson({ branches: [] });

        const result = await fetchProjectSummary("p1", null);

        expect(result.branchId).toBe("");
        expect(result.summary.branches).toEqual([]);
    });
});

describe("useCreateProject", () => {
    it("creates the project and puts it first in the cached list", async () => {
        const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
        client.setQueryData(queryKeys.projects, [{ id: "old", is_deleted: false }]);
        const fetchMock = mockJson({ id: "new", name: "Beat", is_deleted: false }, 201);

        const { result } = renderHook(() => useCreateProject(), {
            wrapper: ({ children }: { children: ReactNode }) => (
                <QueryClientProvider client={client}>{children}</QueryClientProvider>
            ),
        });
        const created = await result.current.mutateAsync({ name: "Beat", description: null, category: "General" });

        expect(created.id).toBe("new");
        expect(fetchMock).toHaveBeenCalledWith(
            `${API_URL}/projects/`,
            expect.objectContaining({
                method: "POST",
                body: JSON.stringify({ name: "Beat", description: null, category: "General" }),
            }),
        );
        expect(client.getQueryData<{ id: string }[]>(queryKeys.projects)?.map((p) => p.id)).toEqual(["new", "old"]);
    });
});

describe("useProjects", () => {
    it("lists the user's projects without the deleted ones", async () => {
        const fetchMock = mockJson([
            { id: "p1", name: "Live", is_deleted: false },
            { id: "p2", name: "Trashed", is_deleted: true },
        ]);

        const { result } = renderHook(() => useProjects(), { wrapper: wrapper() });

        await waitFor(() => expect(result.current.isSuccess).toBe(true));
        expect(result.current.data?.map((p) => p.id)).toEqual(["p1"]);
        expect(fetchMock).toHaveBeenCalledWith(`${API_URL}/projects/`, expect.anything());
    });
});
