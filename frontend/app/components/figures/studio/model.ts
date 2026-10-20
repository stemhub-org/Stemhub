// The StemHub plugin's state in a tiny DAW session, as a pure reducer: a step pattern being
// edited, two branches with their saved versions, and what the plugin panel shows.
// Wording follows the plugin itself (plugin/stemhub/Source/src/ui).

export const CHANNELS = ["Kick", "Snare", "Hats", "Bass", "Lead", "Pad"] as const;
export const STEPS = 16;
export const BRANCHES = ["main", "late-night-idea"] as const;
export type BranchName = (typeof BRANCHES)[number];
export type Pattern = boolean[][];

export interface Version {
  id: string;
  branch: BranchName;
  number: number;
  message: string;
  when: string;
  pattern: Pattern;
}

export type Activity = { kind: "idle" } | { kind: "saving"; startedAt: number } | { kind: "restoring"; startedAt: number };

export interface Studio {
  branch: BranchName;
  versions: Version[];
  pattern: Pattern;
  // The version the working copy was last saved as or restored from, on this branch.
  base: string;
  selected: string | null;
  file: string;
  status: string;
  activity: Activity;
  saves: number;
}

const row = (text: string) => Array.from(text, (char) => char === "x");
const patternOf = (rows: string[]): Pattern => rows.map(row);
const EMPTY = "................";

const KICK = "x...x...x...x...";
const SNARE = "....x.......x...";
const HATS = "..x...x...x...x.";
const BASS = "x..x..x...x..x..";
const LEAD = "x.....x.....x...";
const PAD = "x...............";
const HALF_KICK = "x.......x.......";
const HALF_SNARE = "........x.......";

const MAIN: [string, string, string[]][] = [
  ["First loop", "Mon 12 Oct, 22:41", [KICK, EMPTY, HATS, EMPTY, EMPTY, EMPTY]],
  ["Drums in", "Tue 13 Oct, 23:05", [KICK, SNARE, HATS, EMPTY, EMPTY, EMPTY]],
  ["Bassline", "Thu 15 Oct, 21:17", [KICK, SNARE, HATS, BASS, EMPTY, EMPTY]],
  ["Lead hook", "Sun 18 Oct, 00:52", [KICK, SNARE, HATS, BASS, LEAD, EMPTY]],
];
const IDEA: [string, string, string[]][] = [
  ["Half-time drums", "Fri 16 Oct, 02:13", [HALF_KICK, HALF_SNARE, HATS, BASS, EMPTY, EMPTY]],
  ["Pad swell", "Sat 17 Oct, 03:38", [HALF_KICK, HALF_SNARE, HATS, BASS, EMPTY, PAD]],
];

const versionsOf = (branch: BranchName, list: [string, string, string[]][]): Version[] =>
  list.map(([message, when, rows], index) => ({ id: `${branch}-${index + 1}`, branch, number: index + 1, message, when, pattern: patternOf(rows) }));

export const PROJECT = "Late Night";
export const FILE = `${PROJECT}.flp`;
export const SAVE_SECONDS = 0.9;
export const RESTORE_SECONDS = 0.7;
export const MAX_SAVES = 3;

export const clone = (pattern: Pattern): Pattern => pattern.map((line) => [...line]);
export const branchVersions = (studio: Studio, branch: BranchName = studio.branch) => studio.versions.filter((version) => version.branch === branch);
export const headOf = (studio: Studio, branch: BranchName = studio.branch) => branchVersions(studio, branch).at(-1)!;
export const versionById = (studio: Studio, id: string | null) => studio.versions.find((version) => version.id === id) ?? null;
export const label = (version: Version) => `V${String(version.number).padStart(2, "0")}`;

export function initialStudio(): Studio {
  const versions = [...versionsOf("main", MAIN), ...versionsOf("late-night-idea", IDEA)];
  const head = versions.find((version) => version.id === "main-4")!;
  return { branch: "main", versions, pattern: clone(head.pattern), base: head.id, selected: head.id, file: FILE, status: "Ready.", activity: { kind: "idle" }, saves: 0 };
}

// What changed in the pattern since the version it came from, worded for a save message:
// "Hats +2", "Kick +1, Snare −2".
export function changesOf(from: Pattern, to: Pattern) {
  const parts: string[] = [];
  CHANNELS.forEach((name, index) => {
    let added = 0;
    let removed = 0;
    to[index]!.forEach((on, step) => {
      const was = from[index]![step]!;
      if (on && !was) added++;
      if (!on && was) removed++;
    });
    if (added) parts.push(`${name} +${added}`);
    if (removed) parts.push(`${name} −${removed}`);
  });
  return parts;
}

export const changeCount = (studio: Studio) => {
  const base = versionById(studio, studio.base);
  if (!base) return 0;
  let count = 0;
  studio.pattern.forEach((line, index) => line.forEach((on, step) => (on !== base.pattern[index]![step] ? count++ : 0)));
  return count;
};

export const messageOf = (studio: Studio) => {
  const base = versionById(studio, studio.base);
  if (!base) return "";
  const parts = changesOf(base.pattern, studio.pattern);
  return parts.length > 2 ? `${parts.slice(0, 2).join(", ")}…` : parts.join(", ");
};

export type Action =
  | { type: "toggle"; channel: number; step: number }
  | { type: "save"; at: number }
  | { type: "saved" }
  | { type: "branch"; branch: BranchName }
  | { type: "select"; id: string }
  | { type: "restore"; at: number }
  | { type: "restored" }
  | { type: "reset" };

const busy = (studio: Studio) => studio.activity.kind !== "idle";

export function reduce(studio: Studio, action: Action): Studio {
  switch (action.type) {
    case "toggle": {
      if (busy(studio)) return studio;
      const pattern = clone(studio.pattern);
      pattern[action.channel]![action.step] = !pattern[action.channel]![action.step];
      const next = { ...studio, pattern };
      const count = changeCount(next);
      return { ...next, status: count ? `${count} unsaved ${count === 1 ? "change" : "changes"}` : "Ready." };
    }
    case "save": {
      if (busy(studio) || changeCount(studio) === 0) return studio;
      return { ...studio, activity: { kind: "saving", startedAt: action.at }, status: "Saving…" };
    }
    case "saved": {
      if (studio.activity.kind !== "saving") return studio;
      const list = branchVersions(studio);
      const number = list.at(-1)!.number + 1;
      const version: Version = { id: `${studio.branch}-${number}`, branch: studio.branch, number, message: messageOf(studio) || "Untitled", when: "Just now", pattern: clone(studio.pattern) };
      return { ...studio, versions: [...studio.versions, version], base: version.id, selected: version.id, activity: { kind: "idle" }, status: `Saved ${label(version)}.`, saves: studio.saves + 1 };
    }
    case "branch": {
      if (busy(studio) || action.branch === studio.branch) return studio;
      const head = headOf(studio, action.branch);
      return { ...studio, branch: action.branch, pattern: clone(head.pattern), base: head.id, selected: head.id, file: FILE, status: `On ${action.branch}.` };
    }
    case "select": {
      if (busy(studio)) return studio;
      const version = versionById(studio, action.id);
      if (!version) return studio;
      if (version.branch !== studio.branch) return reduce(reduce(studio, { type: "branch", branch: version.branch }), action);
      return { ...studio, selected: version.id };
    }
    case "restore": {
      const version = versionById(studio, studio.selected);
      if (busy(studio) || !version) return studio;
      return { ...studio, activity: { kind: "restoring", startedAt: action.at }, status: "Restoring…" };
    }
    case "restored": {
      const version = versionById(studio, studio.selected);
      if (studio.activity.kind !== "restoring" || !version) return studio;
      return {
        ...studio,
        pattern: clone(version.pattern),
        base: version.id,
        file: `${PROJECT} (${label(version)}).flp`,
        activity: { kind: "idle" },
        status: `Opened as a separate copy.`,
      };
    }
    case "reset":
      return initialStudio();
  }
}

export function readoutOf(studio: Studio) {
  const version = versionById(studio, studio.selected);
  const count = changeCount(studio);
  const where = `${studio.branch} · ${version ? `${label(version)} “${version.message}”` : "no version"}`;
  if (studio.activity.kind === "saving") return `${studio.branch} · saving…`;
  if (studio.activity.kind === "restoring") return `${studio.branch} · restoring ${version ? label(version) : ""}`;
  return count ? `${studio.branch} · ${count} unsaved ${count === 1 ? "change" : "changes"}` : where;
}
