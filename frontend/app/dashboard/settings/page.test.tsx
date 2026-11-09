import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
import { queryKeys } from "@/lib/queries";
import SettingsPage from "./page";

const push = vi.fn();
vi.mock("next/navigation", () => ({ useRouter: () => ({ push }) }));

const user = {
    id: "u1",
    email: "jb@stemhub.fr",
    username: "jb",
    avatar_url: null,
    bio: "Beats at night",
    location: "Paris, France",
    website: "https://jb.example",
    genres: [],
    created_at: "2026-01-01T00:00:00Z",
    is_active: true,
    is_admin: false,
};

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function renderPage() {
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const Wrapper = ({ children }: { children: ReactNode }) => (
        <QueryClientProvider client={client}>
            <ToastProvider>{children}</ToastProvider>
        </QueryClientProvider>
    );
    return { client, ...render(<SettingsPage />, { wrapper: Wrapper }) };
}

describe("SettingsPage", () => {
    it("shows the error with a retry instead of sending the user to /login", async () => {
        let calls = 0;
        vi.stubGlobal(
            "fetch",
            vi.fn(async () => {
                calls += 1;
                return calls === 1 ? json({ detail: "Database unavailable" }, 503) : json(user);
            }),
        );

        renderPage();

        expect((await screen.findByRole("alert")).textContent).toContain("Database unavailable");
        expect(push).not.toHaveBeenCalled();

        fireEvent.click(screen.getByRole("button", { name: "Try again" }));

        expect(((await screen.findByLabelText("Username")) as HTMLInputElement).value).toBe("jb");
    });

    it("fills the profile form with the signed-in user", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json(user)));

        renderPage();

        expect(((await screen.findByLabelText("Username")) as HTMLInputElement).value).toBe("jb");
        expect((screen.getByLabelText("Location") as HTMLInputElement).value).toBe("Paris, France");
        expect((screen.getByLabelText("Website") as HTMLInputElement).value).toBe("https://jb.example");
        expect((screen.getByLabelText("Bio") as HTMLTextAreaElement).value).toBe("Beats at night");
    });

    it("labels every account field", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json(user)));

        renderPage();
        fireEvent.click(await screen.findByRole("button", { name: "Account" }));

        expect(((await screen.findByLabelText("Email Address")) as HTMLInputElement).value).toBe("jb@stemhub.fr");
        expect(screen.getByLabelText("Current Password")).toBeTruthy();
        expect(screen.getByLabelText("New Password")).toBeTruthy();
        expect(screen.getByLabelText("Confirm New Password")).toBeTruthy();
    });
});

describe("SettingsPage profile tab", () => {
    it("saves the profile and confirms it", async () => {
        const fetchMock = vi.fn(async (_url: string, init?: RequestInit) =>
            init?.method === "PUT" ? json({ ...user, bio: "New bio" }) : json(user),
        );
        vi.stubGlobal("fetch", fetchMock);
        const { client } = renderPage();

        fireEvent.change(await screen.findByLabelText("Bio"), { target: { value: "New bio" } });
        fireEvent.click(screen.getByRole("button", { name: "Save Changes" }));

        expect(await screen.findByText("Profile saved.")).toBeTruthy();
        const [, init] = fetchMock.mock.calls.find(([, init]) => init?.method === "PUT")!;
        expect(JSON.parse(String(init?.body))).toEqual({
            username: "jb",
            avatar_url: null,
            bio: "New bio",
            location: "Paris, France",
            website: "https://jb.example",
        });
        await waitFor(() => expect(client.getQueryData(queryKeys.currentUser)).toMatchObject({ bio: "New bio" }));
    });

    it("shows the backend's message when the save fails", async () => {
        vi.stubGlobal(
            "fetch",
            vi.fn(async (_url: string, init?: RequestInit) =>
                init?.method === "PUT" ? json({ detail: "Username already taken" }, 400) : json(user),
            ),
        );
        renderPage();

        fireEvent.click(await screen.findByRole("button", { name: "Save Changes" }));

        expect(await screen.findByText("Username already taken")).toBeTruthy();
    });
});

describe("SettingsPage account tab", () => {
    async function openAccountTab() {
        renderPage();
        fireEvent.click(await screen.findByRole("button", { name: "Account" }));
    }

    it("changes the email and confirms it", async () => {
        const fetchMock = vi.fn(async (url: string, init?: RequestInit) =>
            init?.method === "PUT" ? json({ ...user, email: "new@stemhub.fr" }) : json(user),
        );
        vi.stubGlobal("fetch", fetchMock);
        await openAccountTab();

        fireEvent.change(await screen.findByLabelText("Email Address"), { target: { value: "new@stemhub.fr" } });
        fireEvent.click(screen.getByRole("button", { name: "Change" }));

        expect(await screen.findByText("Email updated.")).toBeTruthy();
        expect(fetchMock).toHaveBeenCalledWith(
            expect.stringMatching(/\/auth\/me\/email$/),
            expect.objectContaining({ method: "PUT", body: JSON.stringify({ new_email: "new@stemhub.fr" }) }),
        );
    });

    it("shows the backend's message when the email change fails", async () => {
        vi.stubGlobal(
            "fetch",
            vi.fn(async (_url: string, init?: RequestInit) =>
                init?.method === "PUT" ? json({ detail: "Email already in use" }, 400) : json(user),
            ),
        );
        await openAccountTab();

        fireEvent.change(await screen.findByLabelText("Email Address"), { target: { value: "taken@stemhub.fr" } });
        fireEvent.click(screen.getByRole("button", { name: "Change" }));

        expect(await screen.findByText("Email already in use")).toBeTruthy();
    });
});
