import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
import { queryKeys } from "@/lib/queries";
import SettingsPage from "./page";

const push = vi.fn();
vi.mock("next/navigation", () => ({ useRouter: () => ({ push }) }));
const saveBlob = vi.fn();
vi.mock("@/lib/download", async (importOriginal) => ({
    ...(await importOriginal<typeof import("@/lib/download")>()),
    saveBlob: (...args: unknown[]) => saveBlob(...args),
}));

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

    it("can show the current password, and the new one with its confirmation", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json(user)));

        renderPage();
        fireEvent.click(await screen.findByRole("button", { name: "Account" }));
        const type = (label: string) => screen.getByLabelText(label).getAttribute("type");
        await screen.findByLabelText("Current Password");
        const [showCurrent, showNew] = screen.getAllByRole("button", { name: "Show password" });

        fireEvent.click(showCurrent);
        expect([type("Current Password"), type("New Password"), type("Confirm New Password")]).toEqual(["text", "password", "password"]);

        fireEvent.click(showNew);
        expect([type("New Password"), type("Confirm New Password")]).toEqual(["text", "text"]);
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

    function fillPasswords(current: string, next: string, confirm: string) {
        fireEvent.change(screen.getByLabelText("Current Password"), { target: { value: current } });
        fireEvent.change(screen.getByLabelText("New Password"), { target: { value: next } });
        fireEvent.change(screen.getByLabelText("Confirm New Password"), { target: { value: confirm } });
        fireEvent.click(screen.getByRole("button", { name: "Update Password" }));
    }

    it("refuses mismatched new passwords without calling the API", async () => {
        const fetchMock = vi.fn<(url: string) => Promise<Response>>(async () => json(user));
        vi.stubGlobal("fetch", fetchMock);
        await openAccountTab();
        await screen.findByLabelText("Current Password");

        fillPasswords("old-secret", "new-secret", "typo-secret");

        expect((await screen.findByRole("alert")).textContent).toBe("The new passwords do not match.");
        expect(fetchMock.mock.calls.some(([url]) => String(url).endsWith("/auth/me/password"))).toBe(false);
    });

    it("changes the password and clears the fields", async () => {
        const fetchMock = vi.fn(async (_url: string, init?: RequestInit) =>
            init?.method === "PUT" ? json({ message: "ok" }) : json(user),
        );
        vi.stubGlobal("fetch", fetchMock);
        await openAccountTab();
        await screen.findByLabelText("Current Password");

        fillPasswords("old-secret", "new-secret", "new-secret");

        expect((await screen.findByRole("status")).textContent).toBe("Password updated.");
        expect((screen.getByLabelText("Current Password") as HTMLInputElement).value).toBe("");
        expect(fetchMock).toHaveBeenCalledWith(
            expect.stringMatching(/\/auth\/me\/password$/),
            expect.objectContaining({
                method: "PUT",
                body: JSON.stringify({ current_password: "old-secret", new_password: "new-secret" }),
            }),
        );
    });

    it("shows the backend's message for a wrong current password", async () => {
        vi.stubGlobal(
            "fetch",
            vi.fn(async (_url: string, init?: RequestInit) =>
                init?.method === "PUT" ? json({ detail: "Current password is incorrect" }, 400) : json(user),
            ),
        );
        await openAccountTab();
        await screen.findByLabelText("Current Password");

        fillPasswords("wrong", "new-secret", "new-secret");

        expect((await screen.findByRole("alert")).textContent).toBe("Current password is incorrect");
    });
});

describe("SettingsPage account deletion", () => {
    beforeEach(() => push.mockReset());

    async function openDeletion() {
        renderPage();
        fireEvent.click(await screen.findByRole("button", { name: "Account" }));
        fireEvent.click(await screen.findByRole("button", { name: "Delete Account" }));
    }

    it("only deletes once the username is typed, then signs out with the date", async () => {
        const fetchMock = vi.fn(async (url: string) => {
            if (url.endsWith("/auth/me/delete")) return json({ erased_at: "2026-12-15T14:00:00Z" });
            if (url.endsWith("/auth/logout")) return new Response(null, { status: 204 });
            return json(user);
        });
        vi.stubGlobal("fetch", fetchMock);
        await openDeletion();

        const deleteButton = screen.getByRole("button", { name: "Delete my account" }) as HTMLButtonElement;
        expect(deleteButton.disabled).toBe(true);
        fireEvent.change(screen.getByLabelText(/Type your username/), { target: { value: user.username } });
        fireEvent.change(screen.getByLabelText("Password"), { target: { value: "secret123" } });
        expect(deleteButton.disabled).toBe(false);
        fireEvent.click(deleteButton);

        expect(await screen.findByText("Your account will be deleted on December 15, 2026. Sign in before then to cancel.")).toBeTruthy();
        await waitFor(() => expect(push).toHaveBeenCalledWith("/login"));
        expect(fetchMock).toHaveBeenCalledWith(
            expect.stringContaining("/auth/me/delete"),
            expect.objectContaining({
                method: "POST",
                body: JSON.stringify({ confirm_username: user.username, password: "secret123" }),
            }),
        );
    });

    it("shows why the deletion was refused and stays signed in", async () => {
        vi.stubGlobal("fetch", vi.fn(async (url: string) =>
            url.endsWith("/auth/me/delete") ? json({ detail: "Incorrect password" }, 400) : json(user),
        ));
        await openDeletion();

        fireEvent.change(screen.getByLabelText(/Type your username/), { target: { value: user.username } });
        fireEvent.click(screen.getByRole("button", { name: "Delete my account" }));

        expect((await screen.findByRole("alert")).textContent).toBe("Incorrect password");
        expect(push).not.toHaveBeenCalled();
    });

    it("can be cancelled", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json(user)));
        await openDeletion();

        fireEvent.click(screen.getByRole("button", { name: "Cancel" }));

        expect(screen.queryByRole("button", { name: "Delete my account" })).toBeNull();
        expect(screen.getByRole("button", { name: "Delete Account" })).toBeTruthy();
    });
});

describe("SettingsPage data export", () => {
    beforeEach(() => saveBlob.mockReset());

    it("downloads the export under the server's filename", async () => {
        const fetchMock = vi.fn(async (url: string) =>
            url.endsWith("/auth/me/export")
                ? new Response(JSON.stringify({ account: { username: "jb" } }), {
                      headers: {
                          "Content-Type": "application/json",
                          "Content-Disposition": 'attachment; filename="stemhub-data-jb-2026-11-15.json"',
                      },
                  })
                : json(user),
        );
        vi.stubGlobal("fetch", fetchMock);
        renderPage();
        fireEvent.click(await screen.findByRole("button", { name: "Account" }));

        fireEvent.click(await screen.findByRole("button", { name: "Download my data" }));

        await waitFor(() => expect(saveBlob).toHaveBeenCalledTimes(1));
        const [blob, filename] = saveBlob.mock.calls[0];
        expect(filename).toBe("stemhub-data-jb-2026-11-15.json");
        expect(JSON.parse(await (blob as Blob).text())).toEqual({ account: { username: "jb" } });
    });

    it("explains a refused export", async () => {
        vi.stubGlobal("fetch", vi.fn(async (url: string) =>
            url.endsWith("/auth/me/export")
                ? json({ detail: "Too many data exports. Try again in 40 minutes." }, 429)
                : json(user),
        ));
        renderPage();
        fireEvent.click(await screen.findByRole("button", { name: "Account" }));

        fireEvent.click(await screen.findByRole("button", { name: "Download my data" }));

        expect(await screen.findByText("Too many data exports. Try again in 40 minutes.")).toBeTruthy();
        expect(saveBlob).not.toHaveBeenCalled();
    });
});

