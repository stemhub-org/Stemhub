import { describe, expect, it } from "vitest";
import { newPasswordError, usernameError } from "./validation";

describe("usernameError", () => {
    it.each(["abc", "x".repeat(32), "Hubert Touraine", "  abc  "])("accepts %j", (username) => {
        expect(usernameError(username)).toBeNull();
    });

    it.each(["", "ab", "   ab   ", "x".repeat(33)])("refuses %j", (username) => {
        expect(usernameError(username)).toBe("Username must be between 3 and 32 characters");
    });
});

describe("newPasswordError", () => {
    it.each(["eight888", "x".repeat(72), "é".repeat(36)])("accepts %j", (password) => {
        expect(newPasswordError(password)).toBeNull();
    });

    it("refuses passwords shorter than 8 characters", () => {
        expect(newPasswordError("seven77")).toBe("Password must be at least 8 characters");
    });

    it("counts bytes, not characters, for the upper limit", () => {
        expect(newPasswordError("x".repeat(73))).toBe("Password must be at most 72 bytes");
        expect(newPasswordError("é".repeat(37))).toBe("Password must be at most 72 bytes");
    });
});
