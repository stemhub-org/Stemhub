import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import { Avatar } from "./Avatar";

describe("Avatar", () => {
    it("shows the picture when there is one", () => {
        render(<Avatar src="https://cdn.example/jb.png" alt="jb" className="h-8 w-8" fallback={<span>JB</span>} />);

        const img = screen.getByRole("img", { name: "jb" });
        expect(img.getAttribute("src")).toBe("https://cdn.example/jb.png");
        expect(img.className).toBe("h-8 w-8");
        expect(screen.queryByText("JB")).toBeNull();
    });

    it("shows the fallback without a picture", () => {
        render(<Avatar src={null} alt="jb" fallback={<span>JB</span>} />);

        expect(screen.getByText("JB")).toBeTruthy();
        expect(screen.queryByRole("img")).toBeNull();
    });

    it("falls back when the picture can't be loaded", () => {
        render(<Avatar src="https://cdn.example/gone.png" alt="jb" fallback={<span>JB</span>} />);

        fireEvent.error(screen.getByRole("img", { name: "jb" }));

        expect(screen.getByText("JB")).toBeTruthy();
        expect(screen.queryByRole("img")).toBeNull();
    });
});
