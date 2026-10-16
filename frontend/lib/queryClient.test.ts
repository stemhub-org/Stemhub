import { describe, expect, it } from "vitest";
import { ApiError } from "./api";
import { createQueryClient, shouldRetry } from "./queryClient";

describe("shouldRetry", () => {
    it("retries network and server errors at most twice", () => {
        const network = new ApiError(0, "offline");
        const server = new ApiError(503, "unavailable");

        expect(shouldRetry(0, network)).toBe(true);
        expect(shouldRetry(1, server)).toBe(true);
        expect(shouldRetry(2, server)).toBe(false);
    });

    it("never retries client errors", () => {
        expect(shouldRetry(0, new ApiError(401, "expired"))).toBe(false);
        expect(shouldRetry(0, new ApiError(404, "missing"))).toBe(false);
        expect(shouldRetry(0, new ApiError(422, "invalid"))).toBe(false);
    });

    it("never retries errors that are not ApiErrors", () => {
        expect(shouldRetry(0, new TypeError("bug in a queryFn"))).toBe(false);
    });
});

describe("createQueryClient", () => {
    it("applies the shared defaults", () => {
        const { queries, mutations } = createQueryClient().getDefaultOptions();

        expect(queries?.retry).toBe(shouldRetry);
        expect(queries?.staleTime).toBe(30_000);
        expect(queries?.refetchOnWindowFocus).toBe(false);
        expect(mutations?.retry).toBe(false);
    });
});
