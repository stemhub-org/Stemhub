import { describe, expect, it } from "vitest";
import { formatTimeAgo } from "./time";

const now = new Date("2026-11-08T12:00:00Z");

describe("formatTimeAgo", () => {
    it("says just now under a minute", () => {
        expect(formatTimeAgo("2026-11-08T11:59:30Z", now)).toBe("just now");
    });

    it("counts minutes, hours and days", () => {
        expect(formatTimeAgo("2026-11-08T11:15:00Z", now)).toBe("45m ago");
        expect(formatTimeAgo("2026-11-08T07:00:00Z", now)).toBe("5h ago");
        expect(formatTimeAgo("2026-11-05T12:00:00Z", now)).toBe("3d ago");
    });

    it("falls back to the date after a week", () => {
        expect(formatTimeAgo("2026-10-01T12:00:00Z", now)).toBe(new Date("2026-10-01T12:00:00Z").toLocaleDateString());
    });

    it("treats future dates (clock skew) as just now", () => {
        expect(formatTimeAgo("2026-11-08T12:05:00Z", now)).toBe("just now");
    });
});
