import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ProjectSettings } from "./ProjectSettings";

const push = vi.hoisted(() => vi.fn());
vi.mock("next/navigation", () => ({ useRouter: () => ({ push }) }));

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

describe("ProjectSettings collaboration and deletion", () => {
    it("lets the owner choose Editor access and shows the resulting role", async () => {
        let added = false;
        const fetch = vi.fn(async (_: string, init?: RequestInit) => {
            if (init?.method === "POST") { added = true; return json({}); }
            return json(added ? [{ user_id: "editor", role: "Editor", user: { username: "producer", email: "p@example.com" } }] : []);
        });
        vi.stubGlobal("fetch", fetch);
        render(<ProjectSettings projectId="p1" ownerId="owner" currentUserId="owner" />);
        fireEvent.change(screen.getByPlaceholderText("Username to invite..."), { target: { value: "producer" } });
        fireEvent.change(screen.getByLabelText("Collaborator access"), { target: { value: "Editor" } });
        fireEvent.click(screen.getByRole("button", { name: "Invite" }));
        await screen.findByText("producer");
        expect(screen.getByText("Editor")).toBeTruthy();
        const sent = fetch.mock.calls.find(([, init]) => init?.method === "POST")?.[1];
        expect(JSON.parse(sent?.body as string)).toEqual({ username: "producer", role: "Editor" });
    });

    it("does not offer owner controls to a collaborator", () => {
        const fetch = vi.fn();
        vi.stubGlobal("fetch", fetch);
        render(<ProjectSettings projectId="p1" ownerId="owner" currentUserId="editor" />);
        expect(screen.queryByRole("button", { name: "Invite" })).toBeNull();
        expect(fetch).not.toHaveBeenCalled();
    });

    it("shows failures to load the collaborator list", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ detail: "Server unavailable" }, 503)));
        render(<ProjectSettings projectId="p1" ownerId="owner" currentUserId="owner" />);
        expect((await screen.findByRole("alert")).textContent).toContain("Server unavailable");
    });

    it("reports a refused invitation without claiming success", async () => {
        vi.stubGlobal("fetch", vi.fn(async (_: string, init?: RequestInit) =>
            init?.method === "POST" ? json({ detail: "User not found" }, 404) : json([])));
        render(<ProjectSettings projectId="p1" ownerId="owner" currentUserId="owner" />);
        fireEvent.change(screen.getByPlaceholderText("Username to invite..."), { target: { value: "unknown" } });
        fireEvent.click(screen.getByRole("button", { name: "Invite" }));
        expect((await screen.findByRole("alert")).textContent).toContain("User not found");
        expect(screen.queryByRole("status")).toBeNull();
    });

    it("removes a collaborator only after confirmation", async () => {
        let removed = false;
        const fetch = vi.fn(async (_: string, init?: RequestInit) => {
            if (init?.method === "DELETE") { removed = true; return new Response(null, { status: 204 }); }
            return json(removed ? [] : [{ user_id: "editor", role: "Editor", user: { username: "producer" } }]);
        });
        vi.stubGlobal("fetch", fetch);
        vi.spyOn(window, "confirm").mockReturnValue(true);
        render(<ProjectSettings projectId="p1" ownerId="owner" currentUserId="owner" />);
        fireEvent.click(await screen.findByRole("button", { name: "Remove" }));
        await waitFor(() => expect(screen.queryByText("producer")).toBeNull());
        expect(fetch.mock.calls.some(([url, init]) => url.endsWith("/collaborators/editor") && init?.method === "DELETE")).toBe(true);
    });

    it("redirects only after the project deletion succeeds", async () => {
        push.mockClear();
        vi.spyOn(window, "confirm").mockReturnValue(true);
        vi.stubGlobal("fetch", vi.fn(async (_: string, init?: RequestInit) =>
            init?.method === "DELETE" ? new Response(null, { status: 204 }) : json([])));
        render(<ProjectSettings projectId="p1" ownerId="owner" currentUserId="owner" />);
        fireEvent.click(screen.getByRole("button", { name: "Delete Project" }));
        await waitFor(() => expect(push).toHaveBeenCalledWith("/dashboard"));
    });
});
