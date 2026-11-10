import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { RecentVersions } from "./RecentVersions";

const version = {
    id: "v1",
    message: "Faster hi-hats",
    created_at: "2026-11-08T10:00:00Z",
    branch_name: "main",
    author: { id: "u1", username: "jb" },
    source_daw: "FL Studio",
    source_project_filename: "beat.flp",
};

describe("RecentVersions", () => {
    it("shows where each version was saved from", () => {
        render(<RecentVersions versions={[version]} projectId="p1" />);

        expect(screen.getByText("FL Studio")).toBeTruthy();
        expect(screen.getByText("beat.flp")).toBeTruthy();
    });

    it("leaves the source out when it isn't known", () => {
        render(<RecentVersions versions={[{ ...version, source_daw: null, source_project_filename: null }]} projectId="p1" />);

        expect(screen.queryByText("FL Studio")).toBeNull();
        expect(screen.queryByText("beat.flp")).toBeNull();
    });

    it("gives the exact save time on hover", () => {
        const { container } = render(<RecentVersions versions={[version]} projectId="p1" />);

        const time = container.querySelector("time");
        expect(time?.getAttribute("dateTime")).toBe("2026-11-08T10:00:00Z");
        expect(time?.getAttribute("title")).toBe(new Date("2026-11-08T10:00:00Z").toLocaleString());
    });
});
