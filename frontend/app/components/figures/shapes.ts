import { iso, pathOf, ringOf, type Flat, type Plan, type Plane, type Point3, type Projection } from "../iso/iso-kit";

// Small 2D helpers shared by the landing page's figures.

// A rounded rectangle as a closed list of 2D points, for slots and windows drawn in a face.
export function roundRect(u0: number, v0: number, u1: number, v1: number, r: number, steps = 4): Plane[] {
  const points: Plane[] = [];
  const arcs: [number, number, number][] = [
    [u1 - r, v0 + r, -Math.PI / 2],
    [u1 - r, v1 - r, 0],
    [u0 + r, v1 - r, Math.PI / 2],
    [u0 + r, v0 + r, Math.PI],
  ];
  for (const [cu, cv, start] of arcs) {
    for (let step = 0; step <= steps; step++) {
      const angle = start + (step / steps) * (Math.PI / 2);
      points.push([cu + r * Math.cos(angle), cv + r * Math.sin(angle)]);
    }
  }
  return points;
}

// Convex hull of screen points (monotone chain), for clip paths.
export function hullOf(points: Flat[]): Flat[] {
  const sorted = [...points].sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const cross = (o: Flat, a: Flat, b: Flat) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  const lower: Flat[] = [];
  for (const point of sorted) {
    while (lower.length >= 2 && cross(lower[lower.length - 2]!, lower[lower.length - 1]!, point) <= 0) lower.pop();
    lower.push(point);
  }
  const upper: Flat[] = [];
  for (const point of [...sorted].reverse()) {
    while (upper.length >= 2 && cross(upper[upper.length - 2]!, upper[upper.length - 1]!, point) <= 0) upper.pop();
    upper.push(point);
  }
  return [...lower.slice(0, -1), ...upper.slice(0, -1)];
}

// The opening of a recess at height z, extruded upwards: whatever sits in the recess is only
// ever seen through this region.
export const openingClip = (plan: Plan, z: number, up: number, P: Projection) =>
  pathOf(hullOf(ringOf(plan, 3).flatMap(([x, y]) => [iso([x, y, z], P), iso([x, y, z + up], P)])), true);

// A closed polygon of world points, projected.
export const polygonOf = (points: Point3[], P: Projection) => pathOf(points.map((point) => iso(point, P)), true);
