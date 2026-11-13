import { corners, cylinder, insetPlan, lineOnTop, pathOf, planOutline, ring, sideSeam, slabOf, topMatrix, topTicks, type Plan, type Point3 } from "../../iso/iso-kit";
import { polygonOf } from "../shapes";
import {
  DESK,
  DESK_H,
  FACE_Y,
  FOOT,
  FOOT_H,
  KEYBOARD,
  KEYBOARD_H,
  MONITOR,
  MONITOR_Z0,
  MONITOR_Z1,
  NECK,
  P,
  POST_R,
  POST_TOP,
  SCREEN_H,
  SCREEN_TOP,
  SCREEN_W,
  SCREEN_X,
  SPINDLES,
  SPINDLE_BASE_H,
  SPINDLE_BASE_R,
  at,
} from "./view";

const solid = (plan: Plan, z: number, h: number, steps = 3, bevel = 0) => slabOf(plan, z, h, P, steps, bevel);
const join = (paths: string[]) => paths.filter(Boolean).join("");
const dotsOn = (points: Point3[]) => points.map(at);

/* The desk. */

const DESK_SCREWS = corners(DESK, 9);
const STRIP: Plan = { x: -150, y: DESK.y + DESK.d - 10, w: 120, d: 5.4, r: 1 };

export const DESK_ART = {
  halo: solid(DESK, -DESK_H - FOOT_H, DESK_H).fill,
  feet: corners(DESK, 20).map(([x, y]) => [cylinder(x, y, 7, -DESK_H - FOOT_H, 1.6, P, 28), cylinder(x, y, 5.4, -DESK_H - FOOT_H + 1.6, FOOT_H - 1.6, P, 28)]),
  slab: solid(DESK, -DESK_H, DESK_H, 8, 1.6),
  groove: planOutline(insetPlan(DESK, 5), 0, P),
  screws: dotsOn(DESK_SCREWS.map(([x, y]) => [x, y, 0])),
  screwRings: join(DESK_SCREWS.map(([x, y]) => ring(x, y, 2.2, 0, P, 16))),
  strip: solid(STRIP, 0, 0.6, 2),
  ticks: topTicks(STRIP.x + 4, STRIP.x + STRIP.w - 4, 4, 5, STRIP.y + 0.2, 0.6, [1.6, 3.2], P, "x"),
};

/* The monitor on its stand. */

const SX1 = SCREEN_X + SCREEN_W;
const SZ0 = SCREEN_TOP - SCREEN_H;

export const MONITOR_ART = {
  halo: solid(insetPlan(FOOT, -3), 0, 1).fill,
  foot: solid(FOOT, 0, 5, 6, 0.8),
  footLine: planOutline(insetPlan(FOOT, 3), 5, P, 6),
  neck: solid(NECK, 5, 92, 3, 0.6),
  neckSeam: sideSeam(NECK, 40, P, 3),
  body: solid(MONITOR, MONITOR_Z0, MONITOR_Z1 - MONITOR_Z0, 4, 1.2),
  bodySeam: sideSeam(MONITOR, SCREEN_TOP - SCREEN_H - 3, P, 4),
  screenRim: polygonOf(
    [
      [SCREEN_X, FACE_Y, SCREEN_TOP],
      [SX1, FACE_Y, SCREEN_TOP],
      [SX1, FACE_Y, SZ0],
      [SCREEN_X, FACE_Y, SZ0],
    ],
    P,
  ),
  power: at([SX1 + 3, FACE_Y, SZ0 - 6]),
  buttons: dotsOn([SX1 - 12, SX1 - 18, SX1 - 24].map((x): Point3 => [x, FACE_Y, SZ0 - 6.5])),
};

/* A 25-key MIDI keyboard. */

const KEY_W = 10.8;
const KEY_X0 = KEYBOARD.x + 10;
const KEY_Y = KEYBOARD.y + 15;
const KEY_D = KEYBOARD.d - 18;
const WHITE = 15;
const BLACK_AFTER = [0, 1, 3, 4, 5];

export const KEYBOARD_ART = {
  halo: solid(insetPlan(KEYBOARD, -2), 0, 1).fill,
  base: solid(KEYBOARD, 0, KEYBOARD_H, 4, 0.8),
  seam: sideSeam(KEYBOARD, 3, P, 4),
  bed: planOutline({ x: KEY_X0 - 1.2, y: KEY_Y - 1.2, w: KEY_W * WHITE + 2.4, d: KEY_D + 1.2, r: 0.8 }, KEYBOARD_H, P, 2),
  whites: Array.from({ length: WHITE }, (_, index) => solid({ x: KEY_X0 + index * KEY_W + 0.2, y: KEY_Y, w: KEY_W - 0.4, d: KEY_D, r: 0.6 }, KEYBOARD_H - 1.6, 3, 2, 0.3)),
  blacks: Array.from({ length: WHITE - 1 }, (_, index) => index)
    .filter((index) => BLACK_AFTER.includes(index % 7))
    .map((index) => solid({ x: KEY_X0 + (index + 1) * KEY_W - 3, y: KEY_Y, w: 6, d: KEY_D * 0.6, r: 0.5 }, KEYBOARD_H + 1.4, 2.6, 2, 0.3)),
  knobs: [0, 1, 2, 3].map((index) => {
    const cx = KEYBOARD.x + 18 + index * 14;
    const cy = KEYBOARD.y + 7.4;
    return {
      body: cylinder(cx, cy, 3.4, KEYBOARD_H, 3.2, P, 24, 0.3),
      pointer: lineOnTop([cx, cy], [cx + Math.sin(index * 0.9 - 1.2) * 2.8, cy - Math.cos(index * 0.9 - 1.2) * 2.8], KEYBOARD_H + 3.2, P),
    };
  }),
  pads: [0, 1, 2, 3].map((index) => solid({ x: KEYBOARD.x + 84 + index * 11, y: KEYBOARD.y + 3.4, w: 9, d: 8, r: 1 }, KEYBOARD_H, 1.4, 2, 0.3)),
  display: planOutline({ x: KEYBOARD.x + 136, y: KEYBOARD.y + 3.6, w: 30, d: 7.6, r: 0.8 }, KEYBOARD_H, P, 2),
  displayFill: pathOf(
    [
      [KEYBOARD.x + 136, KEYBOARD.y + 3.6],
      [KEYBOARD.x + 166, KEYBOARD.y + 3.6],
      [KEYBOARD.x + 166, KEYBOARD.y + 11.2],
      [KEYBOARD.x + 136, KEYBOARD.y + 11.2],
    ].map(([x, y]) => at([x!, y!, KEYBOARD_H])),
    true,
  ),
  displayText: topMatrix([KEYBOARD.x + 151, KEYBOARD.y + 9.4], KEYBOARD_H, P, "x"),
};

/* The version tower: one spindle per branch, on a turned base with a name plate. */

export const SPINDLE_ART = SPINDLES.map(({ branch, x, y }) => {
  const plate: Plan = { x: x - 17, y: y + SPINDLE_BASE_R + 3, w: 34, d: 7, r: 1.2 };
  return {
    branch,
    halo: cylinder(x, y, SPINDLE_BASE_R + 2, 0, 1, P, 48).fill,
    base: cylinder(x, y, SPINDLE_BASE_R, 0, SPINDLE_BASE_H, P, 64, 0.6),
    screws: dotsOn([-0.5, 0.25, 1].map((angle): Point3 => [x + Math.cos(angle + 0.5) * (SPINDLE_BASE_R - 1.6), y + Math.sin(angle + 0.5) * (SPINDLE_BASE_R - 1.6), SPINDLE_BASE_H])),
    post: cylinder(x, y, POST_R, SPINDLE_BASE_H, POST_TOP - SPINDLE_BASE_H, P, 20),
    plate: solid(plate, 0, 0.8, 3),
    plateLine: planOutline(insetPlan(plate, 1.2), 0.8, P, 3),
    plateText: topMatrix([x, plate.y + 4.9], 0.8, P, "x"),
  };
});

