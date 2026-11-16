import type { MixerDiffChange, MixerDiffSummary } from "@/types/project";

// Wording for the few properties whose backend name doesn't read well as is.
const PROPERTY_LABELS: Record<string, string> = { renamed: "Name", dry_wet: "Dry/wet" };

/** "Master", `Insert 3`, or `Insert 3 "Kick"`: the wording of the backend's own messages. */
export function insertLabel(change: MixerDiffChange): string {
    const label = change.insert_index === 0 ? "Master" : `Insert ${change.insert_index}`;
    return change.insert_name && change.insert_name !== label ? `${label} "${change.insert_name}"` : label;
}

/**
 * What changed on the insert: "Volume", "Slot 2 · Plugin"…
 *
 * Derived from the change type (`insert_volume_changed` → "Volume") instead of
 * a list of known types, so a type added later on the backend still gets a label.
 */
export function changeLabel(change: MixerDiffChange): string {
    const property = change.type.replace(/^(insert|slot)_/, "").replace(/_changed$/, "");
    const words = property.replace(/_/g, " ");
    const label = PROPERTY_LABELS[property] ?? words.charAt(0).toUpperCase() + words.slice(1);
    // FL Studio shows effect slots from 1; slot_index is 0-based.
    return change.slot_index === null ? label : `Slot ${change.slot_index + 1} · ${label}`;
}

/** One side of a change (its `before` or `after`), as text. */
export function formatChangeValue(value: unknown): string {
    if (value === null || value === undefined) return "—";
    if (typeof value === "boolean") return value ? "On" : "Off";
    if (typeof value === "object") {
        // An added or removed insert or effect is sent whole: show its name.
        const { plugin_name, name } = value as { plugin_name?: unknown; name?: unknown };
        if (typeof plugin_name === "string" && plugin_name) return plugin_name;
        if (typeof name === "string" && name) return name;
        return "Present";
    }
    return String(value);
}

const plural = (count: number, word: string) => `${count} ${word}${count === 1 ? "" : "s"}`;

/** "3 changes · 2 inserts · 1 effect slot". */
export function mixerDiffSummaryLine(summary: MixerDiffSummary): string {
    return [
        plural(summary.total_changes, "change"),
        plural(summary.inserts_changed, "insert"),
        plural(summary.slots_changed, "effect slot"),
    ].join(" · ");
}
