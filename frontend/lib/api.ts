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
        const body = await res.json().catch(() => ({}));
        throw new Error(body.detail || `API error ${res.status}`);
    }

    return res.json() as Promise<T>;
}
