import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import type { Branch } from "@/types/project";
import { NewPullRequestForm } from "./NewPullRequestForm";

function branch(id: string, name: string): Branch {
    return { id, name, project_id: "p1", created_at: "2026-11-01T00:00:00Z", is_deleted: false, deleted_at: null };
}

const main = branch("b-main", "main");
const feature = branch("b-feat", "drop-plus-fort");

function renderForm(branches: Branch[], respond: () => Response = () => new Response("{}", { status: 201 })) {
    const fetchMock = vi.fn(async () => respond());
    vi.stubGlobal("fetch", fetchMock);
    const onCreated = vi.fn();
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    render(<NewPullRequestForm projectId="p1" branches={branches} onCreated={onCreated} />, {
        wrapper: ({ children }: { children: ReactNode }) => <QueryClientProvider client={client}>{children}</QueryClientProvider>,
    });
    return { fetchMock, onCreated };
}

function json(body: unknown, status: number) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

const submit = () => screen.getByRole("button", { name: "Open pull request" }) as HTMLButtonElement;

describe("NewPullRequestForm", () => {
    it("asks for a second branch when the project has only one", () => {
        renderForm([main]);

        expect(screen.getByText("Create another branch to open a pull request.")).toBeTruthy();
        expect(screen.queryByRole("button", { name: "Open pull request" })).toBeNull();
    });

    it("targets main by default and cannot be sent while source and target are the same", () => {
        renderForm([main, feature]);
        fireEvent.change(screen.getByLabelText("Title"), { target: { value: "Louder drop" } });

        expect((screen.getByLabelText("Into") as HTMLSelectElement).value).toBe("b-main");
        expect(submit().disabled).toBe(false);

        fireEvent.change(screen.getByLabelText("From"), { target: { value: "b-main" } });
        expect(submit().disabled).toBe(true);
    });

    it("opens the pull request and hands it back", async () => {
        const { fetchMock, onCreated } = renderForm([main, feature], () => json({ id: "pr1", status: "OPEN" }, 201));
        fireEvent.change(screen.getByLabelText("Title"), { target: { value: "Louder drop" } });

        fireEvent.click(submit());

        await waitFor(() => expect(onCreated).toHaveBeenCalledWith({ id: "pr1", status: "OPEN" }));
        const [, init] = fetchMock.mock.calls[0] as unknown as [string, RequestInit];
        expect(JSON.parse(init.body as string)).toEqual({
            source_branch_id: "b-feat",
            target_branch_id: "b-main",
            title: "Louder drop",
        });
    });

    it("shows the server's refusal and keeps what was typed", async () => {
        const { onCreated } = renderForm([main, feature], () =>
            json({ detail: "An open pull request already exists for these branches" }, 409),
        );
        fireEvent.change(screen.getByLabelText("Title"), { target: { value: "Louder drop" } });

        fireEvent.click(submit());

        expect(await screen.findByText("An open pull request already exists for these branches")).toBeTruthy();
        expect((screen.getByLabelText("Title") as HTMLInputElement).value).toBe("Louder drop");
        expect(onCreated).not.toHaveBeenCalled();
    });
});
