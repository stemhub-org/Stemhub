export const API_URL =
    process.env.NEXT_PUBLIC_API_URL || "http://localhost:8000";

const SESSION_EXPIRED_MESSAGE = "Your sign-in has expired. Please sign in again.";
const NETWORK_ERROR_MESSAGE = "Can't reach the StemHub server. Check your connection and try again.";
const FALLBACK_ERROR_MESSAGE = "Something went wrong. Please try again.";

/**
 * Error thrown for every failed API call. `status` is the HTTP status, or 0
 * when the request never reached the server (offline, DNS, CORS).
 */
export class ApiError extends Error {
    readonly status: number;
    readonly detail: string | null;

    constructor(status: number, message: string, detail: string | null = null) {
        super(message);
        this.name = "ApiError";
        this.status = status;
        this.detail = detail;
    }

    get isUnauthorized(): boolean {
        return this.status === 401;
    }

    get isNetworkError(): boolean {
        return this.status === 0;
    }

    get isServerError(): boolean {
        return this.status >= 500;
    }
}

export interface ApiRequestInit extends RequestInit {
    /** Send the user to /login on 401. Turn off for the login form itself. Defaults to true. */
    redirectOnUnauthorized?: boolean;
}

/** A user-facing message for any thrown value. */
export function errorMessage(error: unknown, fallback = FALLBACK_ERROR_MESSAGE): string {
    if (error instanceof Error && error.message) return error.message;
    return fallback;
}

function buildHeaders(init: RequestInit): Headers {
    const headers = new Headers(init.headers);
    const hasJsonBody = init.body != null && !(init.body instanceof FormData);
    if (hasJsonBody && !headers.has("Content-Type")) {
        headers.set("Content-Type", "application/json");
    }

    const token = typeof window !== "undefined" ? localStorage.getItem("access_token") : null;
    if (token && !headers.has("Authorization")) {
        headers.set("Authorization", `Bearer ${token}`);
    }
    return headers;
}

// FastAPI sends `detail` as a string, or as a list of `{ msg }` for validation errors.
async function readErrorDetail(res: Response): Promise<string | null> {
    const body: unknown = await res.json().catch(() => null);
    const detail = body && typeof body === "object" && "detail" in body ? body.detail : null;

    if (typeof detail === "string") return detail;
    if (Array.isArray(detail)) {
        const messages = detail
            .map((item: unknown) =>
                item && typeof item === "object" && "msg" in item ? String(item.msg) : null,
            )
            .filter((msg): msg is string => Boolean(msg));
        return messages.length > 0 ? messages.join("; ") : null;
    }
    return null;
}

/**
 * Calls the StemHub API and returns the raw response, e.g. for file downloads.
 * Throws an ApiError for network failures and non-2xx responses.
 */
export async function apiFetchResponse(path: string, init: ApiRequestInit = {}): Promise<Response> {
    const { redirectOnUnauthorized = true, ...requestInit } = init;

    let res: Response;
    try {
        res = await fetch(`${API_URL}${path}`, {
            ...requestInit,
            credentials: "include",
            headers: buildHeaders(requestInit),
        });
    } catch {
        throw new ApiError(0, NETWORK_ERROR_MESSAGE);
    }

    if (res.ok) return res;

    const detail = await readErrorDetail(res);
    if (res.status === 401 && redirectOnUnauthorized) {
        if (typeof window !== "undefined") window.location.href = "/login";
        throw new ApiError(401, SESSION_EXPIRED_MESSAGE, detail);
    }
    throw new ApiError(
        res.status,
        detail ?? `API request failed: ${res.statusText || res.status}`,
        detail,
    );
}

/**
 * Calls the StemHub API and returns the JSON body as `T`.
 * Resolves to undefined for 204 and empty responses.
 */
export async function apiFetch<T>(path: string, init?: ApiRequestInit): Promise<T> {
    const res = await apiFetchResponse(path, init);
    if (res.status === 204) return undefined as T;

    const text = await res.text();
    return (text ? JSON.parse(text) : undefined) as T;
}

/** Alias of apiFetch, kept so existing callers keep working. */
export const authFetch = apiFetch;
