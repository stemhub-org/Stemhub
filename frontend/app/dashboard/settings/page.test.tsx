import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
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
