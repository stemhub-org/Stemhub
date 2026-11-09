import { describe, expect, it } from "vitest";
import type { ActivityItem } from "@/types/activity";
import { describeActivity, groupActivityByDay } from "./activityText";

const main = { id: "b-main", name: "main", is_deleted: false };
const feat = { id: "b-feat", name: "feature-fast-tempo", is_deleted: false };
const jb = { id: "u1", username: "jb" };

function activity(overrides: Partial<ActivityItem>): ActivityItem {
    return {
        id: "version_saved:v1",
        type: "version_saved",
        occurred_at: "2026-11-08T10:00:00Z",
        actor: null,
        branch: null,
        version: null,
        pull_request: null,
        ...overrides,
    };
}

describe("describeActivity", () => {
    it("describes a saved version with its message and source", () => {
        const text = describeActivity(
            activity({
                actor: jb,
                branch: main,
                version: { id: "v1", message: "Faster hi-hats", source_daw: "FL Studio", source_project_filename: "beat.flp" },
            }),
        );

        expect(text).toEqual({
            title: "jb saved a version on main",
            detail: "Faster hi-hats",
            meta: "FL Studio · beat.flp",
        });
    });

    it("says someone when the author isn't recorded", () => {
        expect(describeActivity(activity({ branch: main, version: { id: "v1", message: null, source_daw: null, source_project_filename: null } }))).toEqual({
            title: "Someone saved a version on main",
            detail: null,
            meta: null,
        });
    });

    it("describes deleted versions and branch events", () => {
        expect(describeActivity(activity({ type: "version_deleted", branch: feat, version: { id: "v1", message: "Old take", source_daw: null, source_project_filename: null } })).title).toBe(
            "A version was deleted on feature-fast-tempo",
        );
        expect(describeActivity(activity({ type: "branch_created", branch: feat })).title).toBe("Branch feature-fast-tempo was created");
        expect(describeActivity(activity({ type: "branch_deleted", branch: feat })).title).toBe("Branch feature-fast-tempo was deleted");
    });

    it("describes pull requests with their branches", () => {
        const pullRequest = { id: "pr1", title: "Faster tempo", status: "MERGED", source_branch: feat, target_branch: main };

        expect(describeActivity(activity({ type: "pull_request_opened", actor: jb, pull_request: pullRequest }))).toEqual({
            title: "jb opened a pull request",
            detail: "Faster tempo",
            meta: "feature-fast-tempo → main",
        });
        expect(describeActivity(activity({ type: "pull_request_merged", actor: jb, pull_request: pullRequest })).title).toBe("jb accepted a pull request");
        expect(describeActivity(activity({ type: "pull_request_closed", actor: jb, pull_request: pullRequest })).title).toBe("jb closed a pull request");
    });
});

describe("groupActivityByDay", () => {
    it("groups items under Today, Yesterday and their date, keeping the order", () => {
        const now = new Date("2026-11-08T18:00:00");
        const items = [
            activity({ id: "a", occurred_at: new Date("2026-11-08T09:00:00").toISOString() }),
            activity({ id: "b", occurred_at: new Date("2026-11-07T22:00:00").toISOString() }),
            activity({ id: "c", occurred_at: new Date("2026-11-02T12:00:00").toISOString() }),
            activity({ id: "d", occurred_at: new Date("2026-11-02T08:00:00").toISOString() }),
        ];

        const groups = groupActivityByDay(items, now);

        expect(groups.map((group) => group.label)).toEqual([
            "Today",
            "Yesterday",
            new Date("2026-11-02T12:00:00").toLocaleDateString(undefined, { weekday: "long", day: "numeric", month: "long" }),
        ]);
        expect(groups.map((group) => group.items.map((item) => item.id))).toEqual([["a"], ["b"], ["c", "d"]]);
    });
});
