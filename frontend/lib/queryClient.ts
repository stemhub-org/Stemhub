import { QueryClient } from "@tanstack/react-query";
import { ApiError } from "./api";

const MAX_RETRIES = 2;

/**
 * Retry only failures a second try can fix: the server was unreachable or
 * returned a 5xx. 4xx answers (expired sign-in, not found, invalid input) and
 * bugs in our own code fail immediately.
 */
export function shouldRetry(failureCount: number, error: unknown): boolean {
    if (failureCount >= MAX_RETRIES) return false;
    return error instanceof ApiError && (error.isNetworkError || error.isServerError);
}

export function createQueryClient(): QueryClient {
    return new QueryClient({
        defaultOptions: {
            queries: {
                retry: shouldRetry,
                staleTime: 30_000,
                refetchOnWindowFocus: false,
            },
            mutations: {
                retry: false,
            },
        },
    });
}
