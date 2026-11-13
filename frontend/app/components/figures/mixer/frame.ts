import { lineOnTop } from "../../iso/iso-kit";
import { WET_STRIP } from "./model";
import { P, PLATE_D, PLATE_TOP, SLOT_D, slotPlan, stripX } from "./view";

// The dry/wet knob on the Drum Bus compressor: the one part the live code redraws.
export const WET_R = 3.1;
export const WET_H = 3.4;
export const WET_DIAL = 0.4;
export const WET_CENTRE: [number, number] = [stripX(WET_STRIP) - 3.6, slotPlan(WET_STRIP, 0).y - (PLATE_D - SLOT_D) / 2 + 7.6];
export const WET_TOP = PLATE_TOP + WET_DIAL + WET_H;

export const pointerOf = (cx: number, cy: number, r: number, z: number, angle: number) =>
  lineOnTop([cx + Math.sin(angle) * r * 0.25, cy - Math.cos(angle) * r * 0.25], [cx + Math.sin(angle) * (r - 0.5), cy - Math.cos(angle) * (r - 0.5)], z, P);

export const wetPointer = (angle: number) => pointerOf(WET_CENTRE[0], WET_CENTRE[1], WET_R, WET_TOP, angle);
