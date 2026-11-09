import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { renderHook } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { API_URL, ApiError } from "./api";
import { useChangeEmail, useUpdateProfile } from "./account";
import { queryKeys } from "./queries";

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function renderWithClient<T>(hook: () => T) {
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const { result } = renderHook(hook, {
        wrapper: ({ children }: { children: ReactNode }) => (
            <QueryClientProvider client={client}>{children}</QueryClientProvider>
        ),
    });
    return { client, result };
}

describe("useUpdateProfile", () => {
    it("sends the changed fields and caches the updated user", async () => {
        const updated = { id: "u1", username: "jb", bio: "Beats" };
        const fetchMock = vi.fn(async () => json(updated));
        vi.stubGlobal("fetch", fetchMock);
        const { client, result } = renderWithClient(() => useUpdateProfile());
        client.setQueryData(queryKeys.currentUser, { id: "u1", username: "jb", bio: null });

        await result.current.mutateAsync({ bio: "Beats" });

        expect(fetchMock).toHaveBeenCalledWith(
            `${API_URL}/auth/me`,
            expect.objectContaining({ method: "PUT", body: JSON.stringify({ bio: "Beats" }) }),
        );
        expect(client.getQueryData(queryKeys.currentUser)).toEqual(updated);
    });

    it("keeps the cached user and reports the backend's message on failure", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ detail: "Username already taken" }, 400)));
        const { client, result } = renderWithClient(() => useUpdateProfile());
        const cached = { id: "u1", username: "jb" };
        client.setQueryData(queryKeys.currentUser, cached);

        const failure = result.current.mutateAsync({ username: "taken" });

        await expect(failure).rejects.toBeInstanceOf(ApiError);
        await expect(failure).rejects.toThrow("Username already taken");
        expect(client.getQueryData(queryKeys.currentUser)).toBe(cached);
    });
});

describe("useChangeEmail", () => {
    it("sends the new address and caches the updated user", async () => {
        const updated = { id: "u1", email: "new@stemhub.fr" };
        const fetchMock = vi.fn(async () => json(updated));
        vi.stubGlobal("fetch", fetchMock);
        const { client, result } = renderWithClient(() => useChangeEmail());

        await result.current.mutateAsync({ new_email: "new@stemhub.fr" });

        expect(fetchMock).toHaveBeenCalledWith(
            `${API_URL}/auth/me/email`,
            expect.objectContaining({ method: "PUT", body: JSON.stringify({ new_email: "new@stemhub.fr" }) }),
        );
        expect(client.getQueryData(queryKeys.currentUser)).toEqual(updated);
    });

    it("rejects with the backend's message", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ detail: "Email already in use" }, 400)));
        const { result } = renderWithClient(() => useChangeEmail());

        await expect(result.current.mutateAsync({ new_email: "taken@stemhub.fr" })).rejects.toThrow(
            "Email already in use",
        );
    });
});
