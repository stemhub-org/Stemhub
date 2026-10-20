// The save the landing page's diff card describes, as a small desk with five channel strips.
// Each strip has two insert slots on the meter bridge; a module in a slot is a plugin in that
// FL Studio insert slot. Dry/wet values are FL Studio's raw 0–12800 range.

export const STRIPS = ["Kick", "Drum Bus", "Lead", "Pad", "Hats"] as const;

export type Kind = "eq" | "comp" | "chorus" | "reverb" | "delay";

export interface Module {
  strip: number;
  slot: 0 | 1;
  kind: Kind;
}

// Modules that sit in both versions.
export const KEPT: Module[] = [
  { strip: 0, slot: 0, kind: "eq" },
  { strip: 1, slot: 0, kind: "comp" },
  { strip: 2, slot: 0, kind: "chorus" },
  { strip: 3, slot: 0, kind: "eq" },
  { strip: 3, slot: 1, kind: "delay" },
];

// The three changes, in the order the diff card lists them.
export const ADDED: Module = { strip: 2, slot: 1, kind: "reverb" };
export const REMOVED: Module = { strip: 4, slot: 0, kind: "delay" };
export const WET_STRIP = 1;
export const WET_BEFORE = 12800;
export const WET_AFTER = 6400;
export const WET_MAX = 12800;

export const CHANGES = 3;

// The knob sweeps 270°, from −135° at 0 to +135° at 12800.
export const wetAngle = (value: number) => ((-135 + (270 * value) / WET_MAX) * Math.PI) / 180;

export function readoutOf(applied: number, wet: number) {
  const value = Math.round(wet / 100) * 100;
  if (applied === 0) return `before save · dry/wet ${value}`;
  return `${applied} of ${CHANGES} changes · dry/wet ${value}`;
}

export function spokenOf(applied: number) {
  const steps = ["Reverb added on insert 3, Lead, slot 2", "Delay removed from insert 5, Hats, slot 1", "Drum Bus dry/wet from 12800 to 6400"];
  if (applied === 0) return "Before the save: no changes shown";
  return `${applied} of ${CHANGES} changes shown. Latest: ${steps[applied - 1]}`;
}
