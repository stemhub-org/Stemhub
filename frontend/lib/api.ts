export const API_URL =
    process.env.NEXT_PUBLIC_API_URL || "http://localhost:8000";

/**
 * Wrapper around fetch that handles authentication via HttpOnly cookies.
 * Throws on non-ok responses with the API error detail.
 */
export async function authFetch<T>(path: string, init?: RequestInit): Promise<T> {
    const res = await fetch(`${API_URL}${path}`, {
        ...init,
        credentials: "include",
        headers: {
            "Content-Type": "application/json",
            ...init?.headers,
        },
    });

    if (!res.ok) {
        if (res.status === 401 && typeof window !== "undefined") {
            // Token is expired or invalid, redirect to login
            window.location.href = "/login";
            throw new Error("Session expired. Please log in again.");
        }

        const errorData = await res.json().catch(() => ({}));
        throw new Error(errorData.detail || `API request failed: ${res.statusText}`);
    }

    if (res.status === 204) {
        return {} as T;
    }

    const contentType = res.headers.get("content-type");
    if (contentType && !contentType.includes("application/json")) {
        return res as unknown as T;
    }

    try {
        return await res.json() as T;
    } catch {
        return {} as T;
    }
}
