import type { ReactNode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { render, screen, within } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { PullRequestDiff } from "./PullRequestDiff";

function renderDiff(body: unknown, status = 200) {
    vi.stubGlobal(
        "fetch",
        vi.fn(async () => new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } })),
    );
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    render(<PullRequestDiff pullRequestId="pr1" targetName="main" sourceName="drop-plus-fort" />, {
        wrapper: ({ children }: { children: ReactNode }) => <QueryClientProvider client={client}>{children}</QueryClientProvider>,
    });
}

function diff(overrides: Record<string, unknown> = {}) {
    return { status: "compared", status_message: null, base_version_id: "v1", head_version_id: "v2", summary: null, changes: [], ...overrides };
}

const volume = {
    type: "insert_volume_changed", insert_index: 1, insert_name: "Kick", slot_index: null,
    before: 100, after: 120, message: 'Insert 1 "Kick" volume changed: 100 -> 120',
};
const plugin = {
    type: "slot_added", insert_index: 1, insert_name: "Kick", slot_index: 1,
    before: null, after: { index: 1, plugin_name: "Fruity Reeverb 2" }, message: "",
};

describe("PullRequestDiff", () => {
    it("shows each change with the target on the left and the source on the right", async () => {
        renderDiff(
            diff({
                summary: { total_changes: 2, inserts_changed: 1, slots_changed: 1, parameter_changes: 1 },
                changes: [volume, plugin],
            }),
        );

        expect(await screen.findByText("2 changes · 1 insert · 1 effect slot")).toBeTruthy();
        const headers = screen.getAllByRole("columnheader").map((cell) => cell.textContent);
        expect(headers).toEqual(["Change", "main", "drop-plus-fort"]);
        expect(screen.getByText('Insert 1 "Kick"')).toBeTruthy();

        const volumeRow = screen.getByRole("row", { name: /Volume/ });
        expect(within(volumeRow).getAllByRole("cell").map((cell) => cell.textContent)).toEqual(["100", "120"]);
        const slotRow = screen.getByRole("row", { name: /Slot 2 · Added/ });
        expect(within(slotRow).getAllByRole("cell").map((cell) => cell.textContent)).toEqual(["—", "Fruity Reeverb 2"]);
    });

    it("shows a project-level change as a sentence", async () => {
        const message = "The FL Studio project file changed, but its mixer could not be read, so no mixer diff is available.";
        renderDiff(
            diff({
                summary: { total_changes: 1, inserts_changed: 0, slots_changed: 0, parameter_changes: 0 },
                changes: [{ type: "project_file_changed", insert_index: null, insert_name: null, slot_index: null, before: {}, after: {}, message }],
            }),
        );

        expect(await screen.findByText(message)).toBeTruthy();
    });

    it("says so when the mixers are identical", async () => {
        renderDiff(diff({ summary: { total_changes: 0, inserts_changed: 0, slots_changed: 0, parameter_changes: 0 } }));

        expect(await screen.findByText("No mixer changes.")).toBeTruthy();
        expect(screen.queryByRole("table")).toBeNull();
    });

    it("explains why there is nothing to compare", async () => {
        renderDiff(diff({ status: "unsupported", status_message: "Only FL Studio projects can be compared" }));
        expect(await screen.findByText("Only FL Studio projects can be compared")).toBeTruthy();
    });

    it("warns when the target branch moved since the pull request was opened", async () => {
        renderDiff(diff({ status: "target_moved" }));
        expect(await screen.findByText(/main has new versions since this pull request was opened/)).toBeTruthy();
    });

    it("says which branch has no version", async () => {
        renderDiff(diff({ status: "no_source_version" }));
        expect(await screen.findByText(/drop-plus-fort has no version yet/)).toBeTruthy();
    });

    it("says accepting starts the history of an empty target", async () => {
        renderDiff(diff({ status: "no_target_version" }));
        expect(await screen.findByText(/main has no version yet/)).toBeTruthy();
    });

    it("shows an error it can retry when the diff cannot be loaded", async () => {
        renderDiff({ detail: "Server error" }, 500);
        expect(await screen.findByText("Couldn't load the changes")).toBeTruthy();
    });
});
