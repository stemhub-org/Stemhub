import {
  corners,
  cylinder,
  depthOf,
  insetPlan,
  lineOnTop,
  onTop,
  pathOf,
  planOutline,
  radialTicks,
  ring,
  ringOf,
  sideRing,
  sideSeam,
  slabOf,
  topTicks,
  type Plan,
  type Plane,
  type Point3,
  type SolidPaths,
} from "../../iso/iso-kit";
import { pointerOf } from "./frame";
import { openingClip, roundRect } from "../shapes";
import { ADDED, KEPT, REMOVED, STRIPS, WET_BEFORE, wetAngle, type Kind, type Module } from "./model";
import {
  BASE,
  BASE_H,
  BOX_H,
  BRIDGE,
  BRIDGE_FRONT,
  BRIDGE_TOP,
  CHASSIS,
  CHASSIS_Z,
  DECK_TOP,
  FADER_Y0,
  FADER_Y1,
  FOOT_H,
  MASTER_X,
  P,
  PITCH,
  PLATE_D,
  PLATE_H,
  PLATE_TOP,
  PLATE_W,
  STRIP_X0,
  at,
  slotPlan,
  stripX,
} from "./view";

const solid = (plan: Plan, z: number, h: number, steps = 4, bevel = 0) => slabOf(plan, z, h, P, steps, bevel);
const join = (paths: string[]) => paths.filter(Boolean).join("");
const dotsOn = (points: Point3[]) => points.map(at);
const onFaceY = (face: number, points: Plane[], closed = false) => pathOf(points.map(([x, z]) => at([x, face, z])), closed);
const depth = (point: Point3) => depthOf(point, P);

function knurlOf(cx: number, cy: number, r: number, z0: number, z1: number, count = 26) {
  const lines: string[] = [];
  const a = (P.azimuth! * Math.PI) / 180;
  for (let index = 0; index < count; index++) {
    const angle = (index / count) * Math.PI * 2;
    const nx = Math.cos(angle);
    const ny = Math.sin(angle);
    if (nx * Math.cos(a) + ny * Math.sin(a) < 0.2) continue;
    lines.push(pathOf([at([cx + r * nx, cy + r * ny, z0 + 0.5]), at([cx + r * nx, cy + r * ny, z1 - 0.7])]));
  }
  return join(lines);
}


export interface Knob {
  dial?: SolidPaths;
  minor?: string;
  major?: string;
  body: SolidPaths;
  knurl: string;
  pointer: string;
  top: number;
}

// A knob on a face at height z: an optional dial plate with a 270° scale, a knurled cap and
// a pointer line on its top.
function knobAt(cx: number, cy: number, z: number, r: number, h: number, angle: number, dialR = 0): Knob {
  const base = dialR ? z + 0.4 : z;
  const top = base + h;
  const ticks = dialR ? radialTicks(cx, cy, dialR - 0.2, 10, 5, base, [0.9, 1.7], P, -0.75 * Math.PI, 1.5 * Math.PI) : null;
  return {
    dial: dialR ? cylinder(cx, cy, dialR, z, 0.4, P, 40) : undefined,
    minor: ticks?.minor,
    major: ticks?.major,
    body: cylinder(cx, cy, r, base, h, P, 32, 0.35),
    knurl: knurlOf(cx, cy, r, base, top, r > 3 ? 28 : 20),
    pointer: pointerOf(cx, cy, r, top, angle),
    top,
  };
}

/* Bench plate. */

const BASE_SCREWS = corners(BASE, 9);

export const BASE_ART = {
  halo: solid(BASE, -BASE_H - FOOT_H, BASE_H).fill,
  feet: corners(BASE, 18).map(([x, y]) => [cylinder(x, y, 7, -BASE_H - FOOT_H, 1.6, P, 28), cylinder(x, y, 5.4, -BASE_H - FOOT_H + 1.6, FOOT_H - 1.6, P, 28)]),
  slab: solid(BASE, -BASE_H, BASE_H, 8, 1.6),
  groove: planOutline(insetPlan(BASE, 5), 0, P),
  screws: dotsOn(BASE_SCREWS.map(([x, y]) => [x, y, 0])),
  screwRings: join(BASE_SCREWS.map(([x, y]) => ring(x, y, 2.2, 0, P, 16))),
};

/* Chassis and meter bridge. */

const CX1 = CHASSIS.x + CHASSIS.w;
const CY1 = CHASSIS.y + CHASSIS.d;

export const DESK_ART = {
  halo: solid(insetPlan(CHASSIS, -3), 0, 1).fill,
  feet: corners(CHASSIS, 10).map(([x, y]) => cylinder(x, y, 3.6, 0, CHASSIS_Z, P, 20)),
  body: solid(CHASSIS, CHASSIS_Z, DECK_TOP - CHASSIS_Z, 6, 1.2),
  seam: sideSeam(CHASSIS, 9, P, 6),
  jack: join([sideRing([CX1 - 16, CY1, 11], 2.3, "y", P, 20), sideRing([CX1 - 16, CY1, 11], 1, "y", P, 12)]),
  frontScrews: dotsOn([
    [CHASSIS.x + 8, CY1, 13.6],
    [CX1 - 8, CY1, 13.6],
  ]),
  bridge: solid(BRIDGE, DECK_TOP, BRIDGE_TOP - DECK_TOP, 5, 1),
  bridgeSeam: sideSeam(BRIDGE, BRIDGE_TOP - 4, P, 5),
  scribbles: join(STRIPS.map((_, strip) => onFaceY(BRIDGE_FRONT, roundRect(stripX(strip) - 10, 21.5, stripX(strip) + 10, 26.5, 1), true))),
  scribbleRules: join(STRIPS.map((_, strip) => onFaceY(BRIDGE_FRONT, [[stripX(strip) - 7, 24], [stripX(strip) - 7 + [9, 12, 7, 8, 9][strip]!, 24]]))),
  separators: join([1, 2, 3, 4, 5].map((index) => lineOnTop([STRIP_X0 + index * PITCH, BRIDGE_FRONT], [STRIP_X0 + index * PITCH, CY1], DECK_TOP, P))),
  signal: STRIPS.map((_, strip) => cylinder(stripX(strip), BRIDGE_FRONT - 4.2, 1.1, BRIDGE_TOP, 0.5, P, 14)),
  masterPlate: solid({ x: MASTER_X + 6, y: -56, w: 34, d: 22, r: 1.5 }, BRIDGE_TOP, 0.8, 3),
  masterPlateLine: planOutline({ x: MASTER_X + 7.6, y: -54.4, w: 30.8, d: 18.8, r: 1 }, BRIDGE_TOP + 0.8, P, 3),
  masterPlateRules: join([0, 1, 2].map((row) => onTop([[MASTER_X + 11, -49 + row * 3.2], [MASTER_X + (row === 1 ? 26 : 34), -49 + row * 3.2]], BRIDGE_TOP + 0.8, P))),
  masterScrews: dotsOn([
    [MASTER_X + 9, -45, BRIDGE_TOP + 0.8],
    [MASTER_X + 37, -45, BRIDGE_TOP + 0.8],
  ]),
};

/* Insert slots: a dark opening with its two far walls showing. */

const SLOT_DEPTH = 6;

const slotArt = (strip: number, slot: number) => {
  const plan = slotPlan(strip, slot);
  const x1 = plan.x + plan.w;
  const y1 = plan.y + plan.d;
  const low = BRIDGE_TOP - SLOT_DEPTH;
  return {
    key: `slot-${strip}-${slot}`,
    hole: pathOf(ringOf(plan, 3).map(([x, y]) => at([x, y, BRIDGE_TOP])), true),
    back: pathOf([at([plan.x, plan.y, BRIDGE_TOP]), at([x1, plan.y, BRIDGE_TOP]), at([x1, plan.y, low]), at([plan.x, plan.y, low])], true),
    left: pathOf([at([plan.x, plan.y, BRIDGE_TOP]), at([plan.x, y1, BRIDGE_TOP]), at([plan.x, y1, low]), at([plan.x, plan.y, low])], true),
    rim: planOutline(plan, BRIDGE_TOP, P, 3),
    guides: join([plan.x + 3, x1 - 3].map((x) => pathOf([at([x, plan.y, BRIDGE_TOP]), at([x, plan.y, low])]))),
    clip: openingClip(plan, BRIDGE_TOP, 80, P),
  };
};

export const SLOTS = STRIPS.flatMap((_, strip) => [0, 1].map((slot) => slotArt(strip, slot)));

/* Modules. Each is built seated; added and removed ones are lifted by the live code. */

const KNOB_LAYOUT: Record<Kind, { at: Plane; r: number; dial?: number; angle: number }[]> = {
  comp: [
    { at: [-3.6, 7.6], r: 3.1, dial: 5.2, angle: wetAngle(WET_BEFORE) },
    { at: [6.4, 10.6], r: 1.8, angle: -0.6 },
  ],
  reverb: [
    { at: [-4.6, 7.6], r: 2.6, angle: 0.5 },
    { at: [4.6, 7.6], r: 2.6, angle: -0.9 },
  ],
  eq: [
    { at: [-6.2, 7.4], r: 1.9, angle: 0.4 },
    { at: [0, 7.4], r: 1.9, angle: -0.3 },
    { at: [6.2, 7.4], r: 1.9, angle: 0.9 },
  ],
  chorus: [{ at: [-4, 7.4], r: 2.6, angle: -0.5 }],
  delay: [{ at: [-4, 7.4], r: 2.6, angle: 1.1 }],
};

function moduleArt({ strip, slot, kind }: Module) {
  const slotAt = slotPlan(strip, slot);
  const cx = stripX(strip);
  const plate: Plan = { x: cx - PLATE_W / 2, y: slotAt.y - (PLATE_D - slotAt.d) / 2, w: PLATE_W, d: PLATE_D, r: 1.2 };
  const box: Plan = { x: cx - 8.4, y: slotAt.y + 1, w: 16.8, d: slotAt.d - 2, r: 0.6 };
  const knobs = KNOB_LAYOUT[kind].map(({ at: [dx, dy], r, dial, angle }) => knobAt(cx + dx, plate.y + dy, PLATE_TOP, r, r > 2.5 ? 3.4 : 2.4, angle, dial ?? 0));
  const switchPlan: Plan | null = kind === "chorus" || kind === "delay" ? { x: cx + 3.4, y: plate.y + 5.4, w: 3.6, d: 3.6, r: 0.6 } : null;
  return {
    key: `module-${strip}-${slot}`,
    strip,
    slot,
    kind,
    depth: depth([cx, slotAt.y + slotAt.d / 2, BRIDGE_TOP]),
    clip: `mx-slot-${strip}-${slot}`,
    box: solid(box, BRIDGE_TOP - BOX_H, BOX_H, 2),
    boxSeam: sideSeam(box, BRIDGE_TOP - BOX_H + 5, P, 2),
    plate: solid(plate, BRIDGE_TOP, PLATE_H, 3, 0.4),
    plateLine: planOutline(insetPlan(plate, 1.1), PLATE_TOP, P, 3),
    screws: dotsOn(corners(insetPlan(plate, 0.8), 1.2).map(([x, y]): Point3 => [x, y, PLATE_TOP])),
    rule: lineOnTop([plate.x + 3, plate.y + PLATE_D - 2.6], [plate.x + 3 + (kind === "comp" ? 8 : 6), plate.y + PLATE_D - 2.6], PLATE_TOP, P),
    led: at([plate.x + PLATE_W - 3.2, plate.y + PLATE_D - 2.6, PLATE_TOP]),
    knobs,
    button: switchPlan ? solid(switchPlan, PLATE_TOP, 1.4, 2, 0.3) : null,
    ghost: planOutline(plate, BRIDGE_TOP, P, 3),
  };
}

export const MODULES = KEPT.map(moduleArt);
export const ADDED_ART = moduleArt(ADDED);
export const REMOVED_ART = moduleArt(REMOVED);
/* Channel strips on the desk: gain, pan, mute and solo, a fader on a ruled strip. */

const FADER_AT = [44, 36, 40, 48, 42];

export interface DeskItem {
  key: string;
  depth: number;
  kind: "knob" | "button" | "fader" | "track";
  knob?: Knob;
  paths?: SolidPaths;
  line?: string;
  track?: { slot: string; rim: string; ruler: SolidPaths; minor: string; major: string };
}

function stripItems(strip: number): DeskItem[] {
  const cx = stripX(strip);
  const items: DeskItem[] = [];
  const gain = knobAt(cx, -12, DECK_TOP, 3.8, 4.4, [-0.4, 0.6, 0.2, -0.8, 0.9][strip]!, 6.2);
  const pan = knobAt(cx, 2.4, DECK_TOP, 3, 3.6, [0, -0.5, 0.4, 0.7, -0.3][strip]!, 5);
  items.push({ key: `gain-${strip}`, depth: depth([cx, -12, DECK_TOP]), kind: "knob", knob: gain });
  items.push({ key: `pan-${strip}`, depth: depth([cx, 2.4, DECK_TOP]), kind: "knob", knob: pan });
  [-5.2, 5.2].forEach((dx, index) => {
    const plan: Plan = { x: cx + dx - 3.6, y: 11, w: 7.2, d: 5.4, r: 0.8 };
    items.push({
      key: `button-${strip}-${index}`,
      depth: depth([cx + dx, 13.7, DECK_TOP]),
      kind: "button",
      paths: solid(plan, DECK_TOP, 2, 2, 0.35),
      line: lineOnTop([plan.x, plan.y + plan.d - 1.4], [plan.x + plan.w, plan.y + plan.d - 1.4], DECK_TOP + 2, P),
    });
  });
  const ruler: Plan = { x: cx + 4.2, y: FADER_Y0, w: 5, d: FADER_Y1 - FADER_Y0, r: 0.6 };
  const ticks = topTicks(FADER_Y0 + 2, FADER_Y1 - 2, 2.5, 4, ruler.x + 0.2, DECK_TOP + 0.4, [1.4, 2.8], P, "y");
  const slot: Plan = { x: cx - 1.3, y: FADER_Y0, w: 2.6, d: FADER_Y1 - FADER_Y0, r: 1.3 };
  items.push({
    key: `track-${strip}`,
    depth: depth([cx, FADER_Y0, DECK_TOP]) - 0.5,
    kind: "track",
    track: {
      slot: pathOf(ringOf(slot, 3).map(([x, y]) => at([x, y, DECK_TOP])), true),
      rim: planOutline(slot, DECK_TOP, P, 3),
      ruler: solid(ruler, DECK_TOP, 0.4, 2),
      minor: ticks.minor,
      major: ticks.major,
    },
  });
  const fy = FADER_AT[strip]!;
  const cap: Plan = { x: cx - 5, y: fy - 3, w: 10, d: 6, r: 1 };
  items.push({
    key: `fader-${strip}`,
    depth: depth([cx, fy, DECK_TOP]),
    kind: "fader",
    paths: solid(cap, DECK_TOP, 4.6, 3, 0.4),
    line: lineOnTop([cap.x, fy], [cap.x + cap.w, fy], DECK_TOP + 4.6, P),
  });
  return items;
}

function masterItems(): DeskItem[] {
  const items: DeskItem[] = [];
  const phones = knobAt(MASTER_X + 12, -10, DECK_TOP, 4.4, 4.8, 0.3, 7.2);
  const room = knobAt(MASTER_X + 34, -10, DECK_TOP, 5.2, 5.2, -0.2, 8.4);
  items.push({ key: "phones", depth: depth([MASTER_X + 12, -10, DECK_TOP]), kind: "knob", knob: phones });
  items.push({ key: "room", depth: depth([MASTER_X + 34, -10, DECK_TOP]), kind: "knob", knob: room });
  const cx = MASTER_X + 22;
  const ruler: Plan = { x: cx + 6.4, y: FADER_Y0, w: 5, d: FADER_Y1 - FADER_Y0, r: 0.6 };
  const ticks = topTicks(FADER_Y0 + 2, FADER_Y1 - 2, 2.5, 4, ruler.x + 0.2, DECK_TOP + 0.4, [1.4, 2.8], P, "y");
  const slot: Plan = { x: cx - 1.4, y: FADER_Y0, w: 2.8, d: FADER_Y1 - FADER_Y0, r: 1.4 };
  items.push({
    key: "master-track",
    depth: depth([cx, FADER_Y0, DECK_TOP]) - 0.5,
    kind: "track",
    track: {
      slot: pathOf(ringOf(slot, 3).map(([x, y]) => at([x, y, DECK_TOP])), true),
      rim: planOutline(slot, DECK_TOP, P, 3),
      ruler: solid(ruler, DECK_TOP, 0.4, 2),
      minor: ticks.minor,
      major: ticks.major,
    },
  });
  const cap: Plan = { x: cx - 7, y: 30, w: 14, d: 7, r: 1.2 };
  items.push({
    key: "master-fader",
    depth: depth([cx, 33.5, DECK_TOP]),
    kind: "fader",
    paths: solid(cap, DECK_TOP, 5, 3, 0.4),
    line: lineOnTop([cap.x, 33.5], [cap.x + cap.w, 33.5], DECK_TOP + 5, P),
  });
  [
    [MASTER_X + 12, 6],
    [MASTER_X + 34, 6],
  ].forEach(([x, y], index) => {
    const plan: Plan = { x: x! - 4, y: y!, w: 8, d: 6, r: 0.8 };
    items.push({
      key: `master-button-${index}`,
      depth: depth([x!, y! + 3, DECK_TOP]),
      kind: "button",
      paths: solid(plan, DECK_TOP, 2.2, 2, 0.35),
      line: lineOnTop([plan.x, plan.y + plan.d - 1.5], [plan.x + plan.w, plan.y + plan.d - 1.5], DECK_TOP + 2.2, P),
    });
  });
  return items;
}

export const DESK_ITEMS = [...STRIPS.flatMap((_, strip) => stripItems(strip)), ...masterItems()].sort((a, b) => a.depth - b.depth);
