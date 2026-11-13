import type { ReactNode } from "react";
import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
import ExplorePage from "./page";

const challenge = {
    id: "c1",
    title: "Flip this sample",
    description: "One sample, one week.",
    level: "Open",
    prize: "Studio time",
    ends_at: "2026-12-01T00:00:00Z",
    created_at: "2026-11-01T00:00:00Z",
};
const event = {
    id: "e1",
    type: "Workshop",
    title: "Mixing live",
    host_name: "jb",
    event_date: "2026-12-05T18:00:00Z",
    created_at: "2026-11-01T00:00:00Z",
};

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function stubApi(post: (url: string) => Response) {
    vi.stubGlobal(
        "fetch",
        vi.fn(async (url: string, init?: RequestInit) => {
            if (init?.method === "POST") return post(url);
            if (url.includes("/community/challenges")) return json([challenge]);
            if (url.includes("/community/events")) return json([event]);
            return json([]);
        }),
    );
}

async function openCommunityTab() {
    render(<ExplorePage />, { wrapper: ({ children }: { children: ReactNode }) => <ToastProvider>{children}</ToastProvider> });
    fireEvent.click(await screen.findByRole("button", { name: /Community/ }));
}

describe("ExplorePage community", () => {
    it("confirms joining a challenge in a toast", async () => {
        stubApi(() => json({ ok: true }));
        await openCommunityTab();

        fireEvent.click(await screen.findByRole("button", { name: "Join Now" }));

        expect(await screen.findByText("You joined “Flip this sample”.")).toBeTruthy();
    });

    it("shows why joining a challenge failed", async () => {
        stubApi(() => json({ detail: "Already joined" }, 400));
        await openCommunityTab();

        fireEvent.click(await screen.findByRole("button", { name: "Join Now" }));

        expect(await screen.findByText("Already joined")).toBeTruthy();
    });

    it("confirms registering for an event in a toast", async () => {
        stubApi(() => json({ ok: true }));
        await openCommunityTab();

        fireEvent.click(await screen.findByRole("button", { name: "Register for Event" }));

        expect(await screen.findByText("You're registered for “Mixing live”.")).toBeTruthy();
    });
});
