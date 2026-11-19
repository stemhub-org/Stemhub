import { describe, expect, it } from "vitest";
import { formatBytes } from "./format";

describe("formatBytes", () => {
    it("shows a dash when the size is unknown", () => {
        expect(formatBytes(null)).toBe("—");
    });

    it("picks the unit that keeps the number short", () => {
        expect(formatBytes(512)).toBe("512 B");
        expect(formatBytes(1536)).toBe("1.5 KB");
        expect(formatBytes(5 * 1024 ** 2)).toBe("5.0 MB");
        expect(formatBytes(1.5 * 1024 ** 3)).toBe("1.5 GB");
    });
});
