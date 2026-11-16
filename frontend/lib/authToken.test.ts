import { describe, expect, it } from "vitest";
import { clearAccessToken, getAccessToken, saveAccessToken } from "./authToken";

describe("authToken", () => {
    it("keeps a remembered token in localStorage", () => {
        saveAccessToken("tok", { remember: true });
        expect(localStorage.getItem("access_token")).toBe("tok");
        expect(sessionStorage.getItem("access_token")).toBeNull();
        expect(getAccessToken()).toBe("tok");
    });

    it("keeps any other token in sessionStorage", () => {
        saveAccessToken("tok", { remember: false });
        expect(sessionStorage.getItem("access_token")).toBe("tok");
        expect(localStorage.getItem("access_token")).toBeNull();
        expect(getAccessToken()).toBe("tok");
    });

    it("replaces the token of a previous sign-in wherever it was", () => {
        saveAccessToken("old", { remember: true });
        saveAccessToken("new", { remember: false });
        expect(localStorage.getItem("access_token")).toBeNull();
        expect(getAccessToken()).toBe("new");
    });

    it("clears both places", () => {
        localStorage.setItem("access_token", "a");
        sessionStorage.setItem("access_token", "b");
        clearAccessToken();
        expect(getAccessToken()).toBeNull();
    });
});
