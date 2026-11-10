import { describe, expect, it, vi } from "vitest";
import { searchPlaces } from "./places";

describe("searchPlaces", () => {
    it("asks Nominatim for five places matching the query", async () => {
        const fetchMock = vi.fn<(url: URL) => Promise<Response>>(async () =>
            new Response(JSON.stringify([{ place_id: 1, display_name: "Paris, France", lat: "48.8" }])),
        );
        vi.stubGlobal("fetch", fetchMock);

        const places = await searchPlaces("Paris & co");

        expect(places).toEqual([{ place_id: 1, display_name: "Paris, France" }]);
        const url = new URL(String(fetchMock.mock.calls[0][0]));
        expect(url.origin).toBe("https://nominatim.openstreetmap.org");
        expect(url.searchParams.get("q")).toBe("Paris & co");
        expect(url.searchParams.get("limit")).toBe("5");
        expect(url.searchParams.get("format")).toBe("json");
    });

    it("skips queries shorter than three characters", async () => {
        const fetchMock = vi.fn();
        vi.stubGlobal("fetch", fetchMock);

        expect(await searchPlaces("Pa")).toEqual([]);
        expect(fetchMock).not.toHaveBeenCalled();
    });

    it("returns no places when the service fails or answers garbage", async () => {
        vi.stubGlobal("fetch", vi.fn(async () => new Response("rate limited", { status: 429 })));
        expect(await searchPlaces("Paris")).toEqual([]);

        vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ error: "nope" }))));
        expect(await searchPlaces("Paris")).toEqual([]);

        vi.stubGlobal("fetch", vi.fn(async () => { throw new TypeError("Failed to fetch"); }));
        expect(await searchPlaces("Paris")).toEqual([]);
    });
});
