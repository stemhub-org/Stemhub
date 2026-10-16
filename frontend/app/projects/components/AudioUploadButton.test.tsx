import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import { ToastProvider } from "@/components/ToastProvider";
import { API_URL } from "@/lib/api";
import { AudioUploadButton } from "./AudioUploadButton";

function pickFile(container: HTMLElement) {
    const input = container.querySelector('input[type="file"]') as HTMLInputElement;
    const file = new File(["audio"], "preview.mp3", { type: "audio/mpeg" });
    fireEvent.change(input, { target: { files: [file] } });
    return file;
}

describe("AudioUploadButton", () => {
    it("uploads the file as form data with the user's token", async () => {
        localStorage.setItem("access_token", "tok");
        const fetchMock = vi.fn(async (url: string, init: RequestInit) => {
            void url;
            void init;
            return new Response(JSON.stringify({ ok: true }), { headers: { "Content-Type": "application/json" } });
        });
        vi.stubGlobal("fetch", fetchMock);
        const onSuccess = vi.fn();

        const { container } = render(
            <ToastProvider>
                <AudioUploadButton projectId="p1" onSuccess={onSuccess} />
            </ToastProvider>,
        );
        const file = pickFile(container);

        await waitFor(() => expect(onSuccess).toHaveBeenCalledOnce());
        const [url, init] = fetchMock.mock.calls[0];
        expect(url).toBe(`${API_URL}/projects/p1/preview`);
        expect(init.method).toBe("POST");
        expect((init.body as FormData).get("preview")).toBe(file);
        const headers = new Headers(init.headers);
        expect(headers.get("Content-Type")).toBeNull();
        expect(headers.get("Authorization")).toBe("Bearer tok");
    });

    it("shows the backend's reason in a toast when the upload fails", async () => {
        vi.stubGlobal(
            "fetch",
            vi.fn(async () => new Response(JSON.stringify({ detail: "File too large" }), { status: 413 })),
        );
        const onSuccess = vi.fn();

        const { container } = render(
            <ToastProvider>
                <AudioUploadButton projectId="p1" onSuccess={onSuccess} />
            </ToastProvider>,
        );
        pickFile(container);

        expect(await screen.findByText("File too large")).toBeTruthy();
        expect(onSuccess).not.toHaveBeenCalled();
    });
});
