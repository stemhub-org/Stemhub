export const API_URL =
    process.env.NEXT_PUBLIC_API_URL || "http://localhost:8000";

/**
 * Wrapper around fetch that injects the JWT stored in localStorage.
 * Throws on non-ok responses with the API error detail.
 */
export async function authFetch<T>(path: string, init?: RequestInit): Promise<T> {
    const token = typeof window !== "undefined" ? localStorage.getItem("token") : null;

    const res = await fetch(`${API_URL}${path}`, {
        ...init,
        headers: {
            "Content-Type": "application/json",
            ...(token ? { Authorization: `Bearer ${token}` } : {}),
            ...init?.headers,
        },
    });

    if (!res.ok) {
        if (res.status === 401 && typeof window !== "undefined") {
            // Token is expired or invalid, clear it and redirect to login
            localStorage.removeItem("token");
            window.location.href = "/login";
            throw new Error("Session expired. Please log in again.");
        }

        const errorData = await res.json().catch(() => ({}));
        throw new Error(errorData.detail || `API request failed: ${res.statusText}`);
    }

    return res.json() as Promise<T>;
}
