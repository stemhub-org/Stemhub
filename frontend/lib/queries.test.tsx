import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { renderHook, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { API_URL, ApiError } from "./api";
import { queryKeys, useCurrentUser, useProjects } from "./queries";

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
