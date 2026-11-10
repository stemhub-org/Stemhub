import { boxCorners, fitProjection, iso, type Plan, type Point3 } from "../../iso/iso-kit";

// World layout of the desk, in units of about 1 px: x across the strips, y towards the
// reader, z up.

export const WIDTH = 600;
export const HEIGHT = 360;
export const AZIMUTH = 52;
export const ELEVATION = 38;

export const BASE: Plan = { x: -110, y: -76, w: 216, d: 152, r: 7 };
export const BASE_H = 8;
export const FOOT_H = 4;

export const CHASSIS: Plan = { x: -96, y: -64, w: 188, d: 128, r: 5 };
export const CHASSIS_Z = 3;
export const DECK_TOP = 18;

// The meter bridge across the back carries the insert slots.
export const BRIDGE: Plan = { x: -96, y: -64, w: 188, d: 41, r: 4 };
export const BRIDGE_TOP = 34;
export const BRIDGE_FRONT = BRIDGE.y + BRIDGE.d;

export const PITCH = 26;
export const STRIP_X0 = -92;
export const stripX = (strip: number) => STRIP_X0 + PITCH / 2 + strip * PITCH;
export const MASTER_X = STRIP_X0 + PITCH * 5 + 4;

export const SLOT_W = 19;
export const SLOT_D = 12;
export const SLOT_Y = [-58, -41];
export const slotPlan = (strip: number, slot: number): Plan => ({ x: stripX(strip) - SLOT_W / 2, y: SLOT_Y[slot]!, w: SLOT_W, d: SLOT_D, r: 1 });

// A module: a faceplate resting on the bridge over its slot, its box hanging in the slot.
export const PLATE_W = 22.6;
export const PLATE_D = 15;
export const PLATE_H = 1.6;
export const PLATE_TOP = BRIDGE_TOP + PLATE_H;
export const BOX_H = 16;
export const LIFT = 26;

export const FADER_Y0 = 21;
export const FADER_Y1 = 56;

function fit() {
  const lifted: Plan = { x: CHASSIS.x, y: BRIDGE.y, w: CHASSIS.w, d: BRIDGE.d, r: 0 };
  const points: Point3[] = [...boxCorners(BASE, -BASE_H - FOOT_H, 0), ...boxCorners(lifted, BRIDGE_TOP, PLATE_TOP + LIFT + 6)];
  return fitProjection(points, WIDTH, HEIGHT, { pad: 18, azimuth: AZIMUTH, elevation: ELEVATION });
}

export const P = fit();
export const at = (point: Point3) => iso(point, P);
