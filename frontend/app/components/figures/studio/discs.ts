import { cylinder, ring, type SolidPaths } from "../../iso/iso-kit";
import { DISC_H, DISC_PITCH, DISC_R, P, POST_R, POST_TOP, SPINDLE_BASE_H } from "./view";

// One disc per saved version, stacked on its branch's spindle, oldest at the bottom.

export interface DiscPaths {
  body: SolidPaths;
  hole: string;
  label: string;
}

export const discZ = (index: number) => SPINDLE_BASE_H + 0.4 + index * DISC_PITCH;

export function discAt(x: number, y: number, index: number): DiscPaths {
  const z = discZ(index);
  return {
    body: cylinder(x, y, DISC_R, z, DISC_H, P, 72, 0.4),
    hole: ring(x, y, POST_R + 1, z + DISC_H, P, 24),
    label: ring(x, y, 8.4, z + DISC_H, P, 40),
  };
}

// The spindle's post above the top disc, and its cap.
export function postAbove(x: number, y: number, count: number) {
  const from = count ? discZ(count - 1) + DISC_H : SPINDLE_BASE_H;
  return {
    post: cylinder(x, y, POST_R, from, POST_TOP - from, P, 20),
    cap: cylinder(x, y, POST_R + 0.8, POST_TOP, 1.4, P, 20, 0.3),
  };
}
