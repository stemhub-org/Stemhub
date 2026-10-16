import { beforeEach, describe, expect, it, vi } from "vitest";
import { API_URL, ApiError, apiFetch, apiFetchResponse, authFetch, errorMessage } from "./api";

function jsonResponse(body: unknown, init: ResponseInit = {}): Response {
    return new Response(JSON.stringify(body), {
        status: 200,
        headers: { "Content-Type": "application/json" },
        ...init,
    });
}

function mockFetch(response: Response | Error) {
    const fetchMock = vi.fn(async () => {
        if (response instanceof Error) throw response;
        return response;
    });
    vi.stubGlobal("fetch", fetchMock);
    return fetchMock;
}

function sentHeaders(fetchMock: ReturnType<typeof mockFetch>): Headers {
    const init = (fetchMock.mock.calls[0] as unknown as [string, RequestInit])[1];
    return new Headers(init.headers);
}

describe("apiFetch", () => {
    beforeEach(() => {
        vi.stubGlobal("location", { href: "http://localhost/dashboard" });
    });

    it("returns the parsed JSON body", async () => {
        const fetchMock = mockFetch(jsonResponse({ id: "p1" }));

        await expect(apiFetch<{ id: string }>("/projects/p1")).resolves.toEqual({ id: "p1" });
        expect(fetchMock).toHaveBeenCalledWith(
            `${API_URL}/projects/p1`,
            expect.objectContaining({ credentials: "include" }),
        );
    });

    it("returns undefined for 204 and empty bodies", async () => {
        mockFetch(new Response(null, { status: 204 }));
        await expect(apiFetch("/projects/p1", { method: "DELETE" })).resolves.toBeUndefined();

        mockFetch(new Response("", { status: 200 }));
        await expect(apiFetch("/projects/p1")).resolves.toBeUndefined();
    });

    it("sends a JSON content type only for non-FormData bodies", async () => {
        let fetchMock = mockFetch(jsonResponse({}));
        await apiFetch("/projects/", { method: "POST", body: JSON.stringify({ name: "x" }) });
        expect(sentHeaders(fetchMock).get("Content-Type")).toBe("application/json");

        fetchMock = mockFetch(jsonResponse({}));
        await apiFetch("/projects/p1/preview", { method: "POST", body: new FormData() });
        expect(sentHeaders(fetchMock).get("Content-Type")).toBeNull();

        fetchMock = mockFetch(jsonResponse({}));
        await apiFetch("/projects/");
        expect(sentHeaders(fetchMock).get("Content-Type")).toBeNull();
    });

    it("sends the stored access token as a bearer token", async () => {
        localStorage.setItem("access_token", "tok");
        const fetchMock = mockFetch(jsonResponse({}));

        await apiFetch("/auth/me");

        expect(sentHeaders(fetchMock).get("Authorization")).toBe("Bearer tok");
    });

    it("throws an ApiError with the backend detail", async () => {
        mockFetch(jsonResponse({ detail: "Project not found" }, { status: 404, statusText: "Not Found" }));

        const error = await apiFetch("/projects/x").catch((e: unknown) => e);

        expect(error).toBeInstanceOf(ApiError);
        expect(error).toMatchObject({ status: 404, detail: "Project not found", message: "Project not found" });
        expect((error as ApiError).isServerError).toBe(false);
    });

    it("joins FastAPI validation errors into one message", async () => {
        mockFetch(
            jsonResponse(
                { detail: [{ msg: "Field required" }, { msg: "Value is not a valid email" }] },
                { status: 422 },
            ),
        );

        await expect(apiFetch("/auth/register")).rejects.toMatchObject({
            status: 422,
            message: "Field required; Value is not a valid email",
        });
    });

    it("falls back to the status text when there is no detail", async () => {
        mockFetch(new Response("oops", { status: 500, statusText: "Internal Server Error" }));

        const error = (await apiFetch("/projects/").catch((e: unknown) => e)) as ApiError;

        expect(error.message).toBe("API request failed: Internal Server Error");
        expect(error.detail).toBeNull();
        expect(error.isServerError).toBe(true);
    });

    it("redirects to the login page on 401", async () => {
        mockFetch(jsonResponse({ detail: "Not authenticated" }, { status: 401 }));

        const error = (await apiFetch("/auth/me").catch((e: unknown) => e)) as ApiError;

        expect(error.isUnauthorized).toBe(true);
        expect(error.message).toBe("Your sign-in has expired. Please sign in again.");
        expect(window.location.href).toBe("/login");
    });

    it("can skip the login redirect and keep the backend detail on 401", async () => {
        mockFetch(jsonResponse({ detail: "Incorrect email or password" }, { status: 401 }));

        await expect(
            apiFetch("/auth/login", { method: "POST", redirectOnUnauthorized: false }),
        ).rejects.toMatchObject({ status: 401, message: "Incorrect email or password" });
        expect(window.location.href).toBe("http://localhost/dashboard");
    });

    it("turns network failures into an ApiError with status 0", async () => {
        mockFetch(new TypeError("Failed to fetch"));

        const error = (await apiFetch("/projects/").catch((e: unknown) => e)) as ApiError;

        expect(error).toBeInstanceOf(ApiError);
        expect(error.status).toBe(0);
        expect(error.isNetworkError).toBe(true);
    });

    it("keeps authFetch as an alias for existing callers", () => {
        expect(authFetch).toBe(apiFetch);
    });
});

describe("apiFetchResponse", () => {
    it("returns the raw response for downloads", async () => {
        // Bytes, not a Blob: on Node 20 jsdom's Blob can't back a Node Response.
        const response = new Response(new TextEncoder().encode("audio"), {
            status: 200,
            headers: { "Content-Type": "audio/mpeg" },
        });
        mockFetch(response);

        await expect(apiFetchResponse("/projects/p1/preview")).resolves.toBe(response);
    });

    it("throws ApiError on failure like apiFetch", async () => {
        mockFetch(jsonResponse({ detail: "No preview" }, { status: 404 }));

        await expect(apiFetchResponse("/projects/p1/preview")).rejects.toMatchObject({
            status: 404,
            message: "No preview",
        });
    });
});

describe("errorMessage", () => {
    it("uses the error message when there is one", () => {
        expect(errorMessage(new ApiError(404, "Project not found"))).toBe("Project not found");
    });

    it("falls back for non-errors and empty messages", () => {
        expect(errorMessage("boom")).toBe("Something went wrong. Please try again.");
        expect(errorMessage(new Error(""), "Could not save")).toBe("Could not save");
    });
});
