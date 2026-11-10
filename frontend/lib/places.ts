/** A place suggested for the profile's location field. */
export interface PlaceSuggestion {
    place_id: number;
    display_name: string;
}

const NOMINATIM_SEARCH_URL = "https://nominatim.openstreetmap.org/search";
const MIN_QUERY_LENGTH = 3;
const MAX_RESULTS = 5;

function isPlaceSuggestion(value: unknown): value is PlaceSuggestion {
    return (
        typeof value === "object" &&
        value !== null &&
        "place_id" in value &&
        typeof value.place_id === "number" &&
        "display_name" in value &&
        typeof value.display_name === "string"
    );
}

/**
 * Looks up places on OpenStreetMap's Nominatim (1 request per second, so debounce
 * the calls). Never throws: a failure means no suggestions. No custom headers, so
 * browsers don't send a CORS preflight.
 */
export async function searchPlaces(query: string, signal?: AbortSignal): Promise<PlaceSuggestion[]> {
    const trimmed = query.trim();
    if (trimmed.length < MIN_QUERY_LENGTH) return [];

    const url = new URL(NOMINATIM_SEARCH_URL);
    url.searchParams.set("format", "json");
    url.searchParams.set("addressdetails", "1");
    url.searchParams.set("limit", String(MAX_RESULTS));
    url.searchParams.set("q", trimmed);

    try {
        const res = await fetch(url, { signal });
        if (!res.ok) return [];
        const body: unknown = await res.json();
        if (!Array.isArray(body)) return [];
        return body
            .filter(isPlaceSuggestion)
            .map(({ place_id, display_name }) => ({ place_id, display_name }));
    } catch {
        return [];
    }
}
