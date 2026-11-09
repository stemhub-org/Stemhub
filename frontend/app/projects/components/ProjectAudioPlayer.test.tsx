import { render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { ProjectAudioPlayer } from "./ProjectAudioPlayer";

describe("ProjectAudioPlayer", () => {
    beforeEach(() => {
        vi.stubGlobal("URL", Object.assign(URL, {
            createObjectURL: vi.fn(() => "blob:preview"),
            revokeObjectURL: vi.fn(),
        }));
    });

    it("explains there is no preview and keeps Play disabled", () => {
        const fetchMock = vi.fn();
        vi.stubGlobal("fetch", fetchMock);

        render(<ProjectAudioPlayer projectId="p1" hasPreview={false} />);

        expect(screen.getByText("No Preview Available")).toBeTruthy();
        expect((screen.getByRole("button", { name: "Play" }) as HTMLButtonElement).disabled).toBe(true);
        expect(fetchMock).not.toHaveBeenCalled();
    });

    it("loads the preview and enables Play", async () => {
        const fetchMock = vi.fn(async () => new Response(new Blob(["RIFF"], { type: "audio/wav" })));
        vi.stubGlobal("fetch", fetchMock);

        const { container } = render(<ProjectAudioPlayer projectId="p1" hasPreview />);

        await waitFor(() =>
            expect((screen.getByRole("button", { name: "Play" }) as HTMLButtonElement).disabled).toBe(false),
        );
        expect(container.querySelector("audio")?.getAttribute("src")).toBe("blob:preview");
        expect(fetchMock).toHaveBeenCalledWith(expect.stringMatching(/\/projects\/p1\/preview$/), expect.anything());
    });

    it("drops the loaded preview when the project loses it", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => new Response(new Blob(["RIFF"]))));
        const { container, rerender } = render(<ProjectAudioPlayer projectId="p1" hasPreview />);
        await waitFor(() => expect(container.querySelector("audio")).not.toBeNull());

        rerender(<ProjectAudioPlayer projectId="p1" hasPreview={false} />);

        expect(container.querySelector("audio")).toBeNull();
        expect(screen.getByText("No Preview Available")).toBeTruthy();
        expect(URL.revokeObjectURL).toHaveBeenCalledWith("blob:preview");
    });
});
