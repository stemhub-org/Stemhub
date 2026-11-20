import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { API_URL } from "@/lib/api";
import RegisterPage from "./page";

const push = vi.fn();
vi.mock("next/navigation", () => ({ useRouter: () => ({ push }) }));

function json(body: unknown, status = 200) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

function submitRegister({
    username = "jboshra",
    password = "secret123",
    confirmPassword = password,
    acceptTerms = true,
}: { username?: string; password?: string; confirmPassword?: string; acceptTerms?: boolean } = {}) {
    render(<RegisterPage />);
    fireEvent.change(screen.getByPlaceholderText("Username"), { target: { value: username } });
    fireEvent.change(screen.getByPlaceholderText("Email"), { target: { value: "jb@stemhub.io" } });
    fireEvent.change(screen.getByPlaceholderText("Password"), { target: { value: password } });
    fireEvent.change(screen.getByPlaceholderText("Confirm password"), { target: { value: confirmPassword } });
    if (acceptTerms) fireEvent.click(screen.getByRole("checkbox", { name: /I accept the Terms of Service/ }));
    fireEvent.click(screen.getByRole("button", { name: "Create my account" }));
}

describe("RegisterPage", () => {
    beforeEach(() => push.mockReset());

    it("creates the account and goes to the login page", async () => {
        const fetchMock = vi.fn(async () => json({ id: "u1" }, 201));
        vi.stubGlobal("fetch", fetchMock);

        submitRegister();

        await waitFor(() => expect(push).toHaveBeenCalledWith("/login"));
        expect(fetchMock).toHaveBeenCalledWith(
            `${API_URL}/auth/register`,
            expect.objectContaining({
                method: "POST",
                body: JSON.stringify({ username: "jboshra", email: "jb@stemhub.io", password: "secret123", accept_terms: true }),
            }),
        );
    });

    it("shows the backend's validation message", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => json({ detail: [{ msg: "value is not a valid email address" }] }, 422)));

        submitRegister();

        expect((await screen.findByRole("alert")).textContent).toBe("value is not a valid email address");
        expect(push).not.toHaveBeenCalled();
    });

    it("checks that the passwords match before calling the server", async () => {
        const fetchMock = vi.fn();
        vi.stubGlobal("fetch", fetchMock);

        submitRegister({ confirmPassword: "different" });

        expect((await screen.findByRole("alert")).textContent).toBe("Passwords do not match");
        expect(fetchMock).not.toHaveBeenCalled();
    });

    it.each([
        [{ password: "short" }, "Password must be at least 8 characters"],
        [{ password: "x".repeat(73) }, "Password must be at most 72 bytes"],
        [{ username: "jb" }, "Username must be between 3 and 32 characters"],
        [{ acceptTerms: false }, "Please accept the Terms of Service and the Privacy Policy."],
    ])("refuses %o before calling the server", async (fields, message) => {
        const fetchMock = vi.fn();
        vi.stubGlobal("fetch", fetchMock);

        submitRegister(fields);

        expect((await screen.findByRole("alert")).textContent).toBe(message);
        expect(fetchMock).not.toHaveBeenCalled();
    });

    it("lets password managers fill and save the account", () => {
        render(<RegisterPage />);
        expect(screen.getByPlaceholderText("Username").getAttribute("autocomplete")).toBe("username");
        expect(screen.getByPlaceholderText("Email").getAttribute("autocomplete")).toBe("email");
        expect(screen.getByPlaceholderText("Password").getAttribute("autocomplete")).toBe("new-password");
        expect(screen.getByPlaceholderText("Confirm password").getAttribute("autocomplete")).toBe("new-password");
    });

    it("links the terms and privacy policy in a new tab, so the form is kept", () => {
        render(<RegisterPage />);
        for (const [name, href] of [["Terms of Service", "/terms"], ["Privacy Policy", "/privacy"]]) {
            const link = screen.getByRole("link", { name });
            expect(link.getAttribute("href")).toBe(href);
            expect(link.getAttribute("target")).toBe("_blank");
        }
    });
});
