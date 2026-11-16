import { describe, expect, it } from "vitest";
import type { Collaborator } from "@/types/project";
import { projectRole, pullRequestActions } from "./pullRequestPermissions";

function collaborator(userId: string, role: string): Collaborator {
    return { project_id: "p1", user_id: userId, role, created_at: "2026-11-01T00:00:00Z", user: null };
}

describe("pullRequestActions", () => {
    it("lets the owner and Admins close and accept an open pull request", () => {
        expect(pullRequestActions("owner", "OPEN")).toEqual({ canClose: true, canAccept: true });
        expect(pullRequestActions("Admin", "OPEN")).toEqual({ canClose: true, canAccept: true });
    });

    it("lets Editors close but not accept", () => {
        expect(pullRequestActions("Editor", "OPEN")).toEqual({ canClose: true, canAccept: false });
    });

    it("gives Viewers and strangers no action", () => {
        expect(pullRequestActions("Viewer", "OPEN")).toEqual({ canClose: false, canAccept: false });
        expect(pullRequestActions(null, "OPEN")).toEqual({ canClose: false, canAccept: false });
    });

    it("gives no action on a merged or closed pull request", () => {
        expect(pullRequestActions("owner", "MERGED")).toEqual({ canClose: false, canAccept: false });
        expect(pullRequestActions("owner", "CLOSED")).toEqual({ canClose: false, canAccept: false });
    });
});

describe("projectRole", () => {
    it("is owner for the project owner", () => {
        expect(projectRole("u1", "u1", [])).toBe("owner");
    });

    it("is the collaborator's role otherwise", () => {
        expect(projectRole("u1", "u2", [collaborator("u2", "Editor")])).toBe("Editor");
        expect(projectRole("u1", "u2", [collaborator("u2", "Admin")])).toBe("Admin");
    });

    it("is null for a stranger, a signed-out user or while collaborators load", () => {
        expect(projectRole("u1", "u3", [collaborator("u2", "Editor")])).toBeNull();
        expect(projectRole("u1", undefined, [])).toBeNull();
        expect(projectRole("u1", "u2", undefined)).toBeNull();
    });
});
