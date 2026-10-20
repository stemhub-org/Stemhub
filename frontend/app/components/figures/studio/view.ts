import { boxCorners, fitProjection, iso, sideMatrix, type Plan, type Point3 } from "../../iso/iso-kit";

// A producer's desk: a monitor running the DAW with the StemHub plugin open, a MIDI keyboard,
// and a version tower with one spindle per branch. Units are about 1 px: x across the desk,
// y towards the reader, z up.

export const WIDTH = 600;
export const HEIGHT = 400;
export const AZIMUTH = 58;
export const ELEVATION = 22;

export const DESK: Plan = { x: -176, y: -66, w: 412, d: 134, r: 8 };
export const DESK_H = 8;
export const FOOT_H = 4;

export const MONITOR: Plan = { x: -152, y: -32, w: 262, d: 12, r: 3 };
export const MONITOR_Z0 = 32;
export const MONITOR_Z1 = 186;
export const FACE_Y = MONITOR.y + MONITOR.d;

// The screen, in its own flat coordinates: u to the right, v down, SCREEN_W × SCREEN_H.
export const SCREEN_X = -145;
export const SCREEN_TOP = 180;
export const SCREEN_W = 246;
export const SCREEN_H = 138;

export const FOOT: Plan = { x: -60, y: -62, w: 76, d: 40, r: 6 };
export const NECK: Plan = { x: -30, y: -46, w: 16, d: 12, r: 2 };

export const KEYBOARD: Plan = { x: -118, y: 8, w: 178, d: 46, r: 4 };
export const KEYBOARD_H = 7;

export interface Spindle {
  branch: "main" | "late-night-idea";
  x: number;
  y: number;
}

export const SPINDLES: Spindle[] = [
  { branch: "main", x: 150, y: -26 },
  { branch: "late-night-idea", x: 198, y: 18 },
];
export const SPINDLE_BASE_R = 22;
export const SPINDLE_BASE_H = 3;
export const POST_R = 2.4;
export const POST_TOP = 40;
export const DISC_R = 18;
export const DISC_H = 2;
export const DISC_PITCH = 2.5;
export const DROP = 46;

function fit() {
  const points: Point3[] = [
    ...boxCorners(DESK, -DESK_H - FOOT_H, 0),
    ...boxCorners({ x: MONITOR.x, y: MONITOR.y, w: MONITOR.w, d: MONITOR.d, r: 0 }, MONITOR_Z0, MONITOR_Z1),
    ...SPINDLES.map(({ x, y }): Point3 => [x, y, POST_TOP + DROP]),
  ];
  return fitProjection(points, WIDTH, HEIGHT, { pad: 14, azimuth: AZIMUTH, elevation: ELEVATION });
}

export const P = fit();
export const at = (point: Point3) => iso(point, P);

// Maps the screen's flat (u, v) coordinates onto the monitor's face.
export const SCREEN_MATRIX = sideMatrix([SCREEN_X, FACE_Y], SCREEN_TOP, P, "left");
