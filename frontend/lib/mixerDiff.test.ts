import { describe, expect, it } from "vitest";
import type { MixerDiffChange } from "@/types/project";
import { changeLabel, formatChangeValue, insertLabel, mixerDiffSummaryLine } from "./mixerDiff";

function change(overrides: Partial<MixerDiffChange> = {}): MixerDiffChange {
    return {
        type: "insert_volume_changed",
        insert_index: 1,
        insert_name: "Kick",
        slot_index: null,
        before: 100,
        after: 120,
        message: "",
        ...overrides,
    };
}

describe("insertLabel", () => {
    it("names the insert the way FL Studio does", () => {
        expect(insertLabel(change({ insert_index: 0, insert_name: "Master" }))).toBe("Master");
        expect(insertLabel(change())).toBe('Insert 1 "Kick"');
        expect(insertLabel(change({ insert_index: 4, insert_name: null }))).toBe("Insert 4");
    });
});

describe("changeLabel", () => {
    it("says which property changed, and in which effect slot", () => {
        expect(changeLabel(change())).toBe("Volume");
        expect(changeLabel(change({ type: "insert_renamed" }))).toBe("Name");
        expect(changeLabel(change({ type: "insert_added" }))).toBe("Added");
        expect(changeLabel(change({ type: "slot_plugin_changed", slot_index: 1 }))).toBe("Slot 2 · Plugin");
        expect(changeLabel(change({ type: "slot_dry_wet_changed", slot_index: 0 }))).toBe("Slot 1 · Dry/wet");
    });

    it("still reads well for a change type added later on the backend", () => {
        expect(changeLabel(change({ type: "insert_stereo_separation_changed" }))).toBe("Stereo separation");
    });
});

describe("formatChangeValue", () => {
    it("formats each kind of value the diff returns", () => {
        expect(formatChangeValue(true)).toBe("On");
        expect(formatChangeValue(false)).toBe("Off");
        expect(formatChangeValue(120)).toBe("120");
        expect(formatChangeValue("Fruity Reeverb 2")).toBe("Fruity Reeverb 2");
        expect(formatChangeValue(null)).toBe("—");
    });

    it("names an added or removed insert or effect", () => {
        expect(formatChangeValue({ index: 2, plugin_name: "Fruity Limiter", name: "Limiter" })).toBe("Fruity Limiter");
        expect(formatChangeValue({ index: 3, name: "Lead" })).toBe("Lead");
        expect(formatChangeValue({ index: 3, name: null })).toBe("Present");
    });
});

describe("mixerDiffSummaryLine", () => {
    it("counts changes, inserts and effect slots", () => {
        const summary = { total_changes: 2, inserts_changed: 1, slots_changed: 1, parameter_changes: 1 };
        expect(mixerDiffSummaryLine(summary)).toBe("2 changes · 1 insert · 1 effect slot");
    });
});
