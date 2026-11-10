import { renderHook } from "@testing-library/react";
import { renderToString } from "react-dom/server";
import { describe, expect, it } from "vitest";
import { useHydrated } from "./useHydrated";

function Probe() {
    return <span>{useHydrated() ? "client" : "server"}</span>;
}

describe("useHydrated", () => {
    it("is false while rendering on the server", () => {
        expect(renderToString(<Probe />)).toContain("server");
    });

    it("is true once rendered in the browser", () => {
        const { result } = renderHook(() => useHydrated());
        expect(result.current).toBe(true);
    });
});
