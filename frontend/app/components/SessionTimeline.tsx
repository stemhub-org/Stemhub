"use client";

import { motion } from "framer-motion";
import { EASE_OUT, LABEL } from "./landing";

// The whole Midnight Drive session from the walkthrough, drawn as a branch graph:
// one lane per branch, one square per save, a hollow square for the restore.
const LANES = [
  { name: "main", line: "bg-lime", dot: "bg-lime" },
  { name: "vocal-ideas", line: "bg-paper", dot: "bg-paper" },
  { name: "kai-topline", line: "bg-paper-soft", dot: "bg-paper-soft" },
] as const;

type LaneName = (typeof LANES)[number]["name"];

const events: { when: string; lane: LaneName; tag: string; text: string; who: string; restore?: boolean }[] = [
  { when: "Fri 16 Oct, 21:40", lane: "main", tag: "V01", text: "First sketch: drums + 808", who: "Nova" },
  { when: "Sat 17 Oct, 23:15", lane: "main", tag: "V02", text: "Chord progression on pads", who: "Nova" },
  { when: "Sun 18 Oct, 16:20", lane: "main", tag: "V03", text: "Added lead vocal take 3", who: "Nova" },
  { when: "Sun 18 Oct, 23:50", lane: "vocal-ideas", tag: "V01", text: "Alt hook: pitched vocal chops", who: "Nova" },
  { when: "Mon 19 Oct, 14:10", lane: "vocal-ideas", tag: "V02", text: "Double-time verse flow", who: "Nova" },
  { when: "Mon 19 Oct, 22:05", lane: "main", tag: "V04", text: "Mix pass: sidechain + EQ on 808", who: "Nova" },
  { when: "Tue 20 Oct, 19:30", lane: "main", tag: "V05", text: "Arrangement: bridge and outro", who: "Nova" },
  { when: "Wed 21 Oct, 07:42", lane: "main", tag: "V06", text: "Master v1 for the label", who: "Nova" },
  {
    when: "Wed 21 Oct, 08:00",
    lane: "vocal-ideas",
    tag: "Restore",
    text: "Double-time verse flow, restored for the label",
    who: "Nova",
    restore: true,
  },
  { when: "Wed 21 Oct, 08:19", lane: "kai-topline", tag: "V01", text: "Topline: chorus melody + harmonies", who: "Kai" },
];

// Rows between a lane's first and last event, where its line is drawn. main is the
// trunk, so it runs to the bottom of the graph.
const spans = Object.fromEntries(
  LANES.map(({ name }) => {
    const rows = events.flatMap((event, i) => (event.lane === name ? [i] : []));
    return [name, { first: rows[0], last: name === "main" ? events.length - 1 : rows[rows.length - 1] }];
  })
) as Record<LaneName, { first: number; last: number }>;

const LANE_WIDTH = 28;

function Lanes({ row }: { row: number }) {
  const event = events[row];
  const laneIndex = LANES.findIndex((lane) => lane.name === event.lane);
  const branchesOff = laneIndex > 0 && spans[event.lane].first === row;

  return (
    <div aria-hidden="true" className="relative shrink-0" style={{ width: LANES.length * LANE_WIDTH }}>
      {LANES.map((lane, i) => {
        const { first, last } = spans[lane.name];
        if (row < first || row > last) return null;
        return (
          <span
            key={lane.name}
            className={`absolute w-0.5 ${lane.line}`}
            style={{
              left: i * LANE_WIDTH + LANE_WIDTH / 2 - 1,
              top: row === first ? "50%" : 0,
              bottom: row === last ? "50%" : 0,
            }}
          />
        );
      })}
      {branchesOff && (
        <span
          className={`absolute top-1/2 h-0.5 ${LANES[laneIndex].line}`}
          style={{ left: LANE_WIDTH / 2, width: laneIndex * LANE_WIDTH }}
        />
      )}
      <span
        className={`absolute top-1/2 h-3 w-3 -translate-x-1/2 -translate-y-1/2 ${
          event.restore ? "border-2 border-paper bg-ink" : LANES[laneIndex].dot
        }`}
        style={{ left: laneIndex * LANE_WIDTH + LANE_WIDTH / 2 }}
      />
    </div>
  );
}

export default function SessionTimeline() {
  return (
    <motion.div
      className="flex flex-col gap-8 bg-ink p-6 text-paper sm:p-10"
      initial={{ opacity: 0, y: 40 }}
      whileInView={{ opacity: 1, y: 0 }}
      viewport={{ once: true, margin: "-10% 0px" }}
      transition={{ duration: 0.7, ease: EASE_OUT }}
    >
      <div className="flex flex-wrap items-end justify-between gap-6">
        <div className="flex max-w-[560px] flex-col gap-3">
          <span className={`${LABEL} text-lime`}>The full session</span>
          <h3 className="font-display text-[clamp(1.75rem,3.2vw,2.75rem)] leading-[0.95] font-extrabold tracking-[-0.03em] uppercase">
            One week of Midnight Drive.
          </h3>
          <p className="leading-relaxed text-paper-soft">
            Every save, branch and restore from the walkthrough, in order. Nothing was overwritten, and the take the
            label asked for was one click away.
          </p>
        </div>
        <ul className={`flex flex-wrap gap-x-5 gap-y-2 ${LABEL}`} aria-label="Branches">
          {LANES.map((lane) => (
            <li key={lane.name} className="flex items-center gap-2">
              <span aria-hidden="true" className={`h-2.5 w-2.5 ${lane.dot}`} />
              {lane.name}
            </li>
          ))}
        </ul>
      </div>

      <ol className="flex flex-col border-t border-paper-line">
        {events.map((event, i) => (
          <li key={`${event.lane}-${event.tag}-${event.when}`} className="flex gap-4 border-b border-paper-line sm:gap-6">
            <Lanes row={i} />
            <div className="flex min-w-0 flex-1 flex-wrap items-baseline gap-x-4 gap-y-1 py-3.5">
              <span className={`${LABEL} w-36 shrink-0 text-paper-soft`}>{event.when}</span>
              <span className={`${LABEL} shrink-0 sm:w-44 ${event.restore ? "text-paper" : "text-lime"}`}>
                {event.lane} · {event.tag}
              </span>
              <span className="min-w-0 font-semibold">{event.text}</span>
              <span className="text-sm text-paper-soft">{event.who}</span>
            </div>
          </li>
        ))}
      </ol>
    </motion.div>
  );
}
