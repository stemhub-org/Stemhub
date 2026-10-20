"use client";

import * as React from "react";
import { translateAlong } from "../../iso/iso-kit";
import { Line, Solid } from "../../iso/react/draw";
import { asksForStillness, useReadout } from "../../iso/react/live";
import { discAt, postAbove } from "./discs";
import { MAX_SAVES, RESTORE_SECONDS, SAVE_SECONDS, branchVersions, headOf, initialStudio, label, readoutOf, reduce, type Action, type Studio } from "./model";
import { LANE_U0, LANE_U1, Screen, stepU } from "./screen";
import { DROP, HEIGHT, P, SCREEN_H, SCREEN_MATRIX, SCREEN_W, SPINDLES, WIDTH } from "./view";
import { IsoFigure } from "../../iso/react/draw";

const MOST_DT = 1 / 30;
const STEP_SECONDS = 60 / 140 / 4;
const LOOP_SECONDS = STEP_SECONDS * 16 * 4;
const DROP_SECONDS = 0.7;
const TOUR_GAP = 1.5;
const TOUR_FIRST = 1500;
const TOUR_BACK = 6000;

interface Props {
  label: string;
  desk: React.ReactNode;
  monitor: React.ReactNode;
  rim: React.ReactNode;
  keyboard: React.ReactNode;
  spindles: React.ReactNode[];
}

type Step = (studio: Studio) => Action | "play" | null;

// The idle tour: change the beat, save, try the other branch, restore an older version.
const TOUR: Step[] = [
  () => "play",
  () => ({ type: "toggle", channel: 2, step: 0 }),
  () => ({ type: "toggle", channel: 2, step: 8 }),
  () => ({ type: "save", at: 0 }),
  () => ({ type: "branch", branch: "late-night-idea" }),
  () => ({ type: "select", id: "late-night-idea-1" }),
  () => ({ type: "restore", at: 0 }),
  () => ({ type: "branch", branch: "main" }),
  () => ({ type: "toggle", channel: 0, step: 14 }),
  () => ({ type: "save", at: 0 }),
  (studio) => ({ type: "select", id: branchVersions(studio, "main")[1]!.id }),
  () => ({ type: "restore", at: 0 }),
  (studio) => (studio.saves >= MAX_SAVES ? { type: "reset" } : { type: "select", id: headOf(studio, "main").id }),
];

// Ease in under gravity, then one small bounce.
function dropOffset(age: number) {
  const t = Math.min(1, age / DROP_SECONDS);
  if (t < 0.72) {
    const u = t / 0.72;
    return DROP * (1 - u * u);
  }
  const u = (t - 0.72) / 0.28;
  return 2.2 * Math.sin(u * Math.PI);
}

export function StudioLive(props: Props) {
  const { anchor: root, say } = useReadout<HTMLDivElement>();
  const [studio, dispatch] = React.useReducer(reduce, undefined, initialStudio);
  const [playing, setPlaying] = React.useState(false);
  const state = React.useRef(studio);
  const playingRef = React.useRef(false);
  const playhead = React.useRef<SVGGElement | null>(null);
  const column = React.useRef<SVGRectElement | null>(null);
  const progress = React.useRef<SVGRectElement | null>(null);
  const status = React.useRef<HTMLSpanElement | null>(null);
  const discs = React.useRef<Map<string, SVGGElement>>(new Map());
  const born = React.useRef<Map<string, number>>(new Map());
  const known = React.useRef<Set<string>>(new Set(initialStudio().versions.map((version) => version.id)));
  const clock = React.useRef(0);
  const playFrom = React.useRef(0);
  const frame = React.useRef(0);
  const last = React.useRef(0);
  const visible = React.useRef(false);
  const touring = React.useRef(false);
  const tourAt = React.useRef(0);
  const tourStep = React.useRef(0);
  const quietTimer = React.useRef(0);
  const [shown, setShown] = React.useState(false);

  React.useEffect(() => {
    state.current = studio;
    say(readoutOf(studio));
    if (status.current && status.current.textContent !== studio.status) status.current.textContent = studio.status;
    // A version nobody has seen yet drops onto its spindle.
    studio.versions.forEach((version) => {
      if (!known.current.has(version.id)) {
        known.current.add(version.id);
        born.current.set(version.id, clock.current);
      }
    });
    if (studio.saves === 0 && studio.versions.length === initialStudio().versions.length) {
      known.current = new Set(studio.versions.map((version) => version.id));
    }
  }, [studio, say]);

  const act = React.useCallback((action: Action) => {
    const stamped = action.type === "save" || action.type === "restore" ? { ...action, at: clock.current } : action;
    dispatch(stamped);
  }, []);

  const togglePlay = React.useCallback(() => {
    playingRef.current = !playingRef.current;
    playFrom.current = clock.current;
    setPlaying(playingRef.current);
  }, []);

  const run = React.useCallback(() => {
    if (frame.current || !visible.current) return;
    last.current = 0;
    const tick = (now: number) => {
      const dt = last.current ? Math.min((now - last.current) / 1000, MOST_DT) : 1 / 60;
      last.current = now;
      const calm = asksForStillness();
      clock.current += dt;
      const s = state.current;

      // Saving and restoring take a moment, shown by the progress bar.
      const activity = s.activity;
      const span = activity.kind === "saving" ? SAVE_SECONDS : RESTORE_SECONDS;
      const done = activity.kind === "idle" ? 0 : Math.min(1, (clock.current - activity.startedAt) / span);
      progress.current?.setAttribute("width", `${Math.round(done * 104 * 10) / 10}`);
      if (activity.kind === "saving" && (calm || done >= 1)) dispatch({ type: "saved" });
      if (activity.kind === "restoring" && (calm || done >= 1)) dispatch({ type: "restored" });

      // Transport.
      if (playingRef.current) {
        const t = (clock.current - playFrom.current) % LOOP_SECONDS;
        const along = t / LOOP_SECONDS;
        playhead.current?.setAttribute("transform", `translate(${Math.round(along * (LANE_U1 - LANE_U0) * 100) / 100} 0)`);
        const step = Math.floor(t / STEP_SECONDS) % 16;
        column.current?.setAttribute("x", `${stepU(step) - 0.5}`);
      }

      // Discs falling onto their spindle; the selected one lifts while it is restored.
      let falling = false;
      discs.current.forEach((node, id) => {
        const birth = born.current.get(id);
        let lift = 0;
        if (birth !== undefined) {
          const age = clock.current - birth;
          if (age < DROP_SECONDS && !calm) {
            lift = dropOffset(age);
            falling = true;
          } else born.current.delete(id);
        }
        if (activity.kind === "restoring" && s.selected === id && !calm) lift += 5 * Math.sin(Math.min(1, done) * Math.PI);
        node.setAttribute("transform", lift ? translateAlong(P, [0, 0, lift]) : "");
        node.style.opacity = birth !== undefined && lift > DROP * 0.6 ? `${Math.round((1 - (lift - DROP * 0.6) / (DROP * 0.4)) * 100) / 100}` : "";
      });

      // The idle tour.
      if (touring.current && !calm && s.activity.kind === "idle" && clock.current - tourAt.current >= TOUR_GAP) {
        tourAt.current = clock.current;
        const step = TOUR[tourStep.current % TOUR.length]!(s);
        tourStep.current += 1;
        if (step === "play") {
          if (!playingRef.current) togglePlay();
        } else if (step) act(step);
      }

      const more = touring.current || playingRef.current || falling || s.activity.kind !== "idle";
      frame.current = more && visible.current ? requestAnimationFrame(tick) : 0;
    };
    frame.current = requestAnimationFrame(tick);
  }, [act, togglePlay]);

  // Any state change may need frames (a save in progress, a disc to drop).
  React.useEffect(() => {
    run();
  }, [studio, playing, run]);

  React.useEffect(() => {
    const node = root.current;
    if (!node) return;
    const observer = new IntersectionObserver(
      (entries) => {
        visible.current = Boolean(entries.at(-1)?.isIntersecting);
        setShown(visible.current);
        if (visible.current) run();
      },
      { rootMargin: "120px 0px" },
    );
    observer.observe(node);
    return () => {
      observer.disconnect();
      cancelAnimationFrame(frame.current);
      frame.current = 0;
    };
  }, [root, run]);

  // The tour starts once the figure is on screen, and steps aside for a while whenever the
  // reader does something.
  const startTour = React.useCallback(() => {
    if (asksForStillness()) return;
    touring.current = true;
    tourAt.current = clock.current - TOUR_GAP + 0.6;
    run();
  }, [run]);

  React.useEffect(() => {
    if (!shown) {
      touring.current = false;
      return;
    }
    const timer = window.setTimeout(startTour, TOUR_FIRST);
    return () => {
      window.clearTimeout(timer);
      touring.current = false;
    };
  }, [shown, startTour]);

  const takeOver = () => {
    touring.current = false;
    window.clearTimeout(quietTimer.current);
    quietTimer.current = window.setTimeout(startTour, TOUR_BACK);
  };

  React.useEffect(() => () => window.clearTimeout(quietTimer.current), []);

  const handlers = {
    toggle: (channel: number, step: number) => {
      takeOver();
      act({ type: "toggle", channel, step });
    },
    save: () => {
      takeOver();
      act({ type: "save", at: 0 });
    },
    branch: (branch: Studio["branch"]) => {
      takeOver();
      act({ type: "branch", branch });
    },
    select: (id: string) => {
      takeOver();
      act({ type: "select", id });
    },
    restore: () => {
      takeOver();
      act({ type: "restore", at: 0 });
    },
    play: () => {
      takeOver();
      togglePlay();
    },
  };

  const bindDisc = (id: string) => (node: SVGGElement | null) => {
    if (node) discs.current.set(id, node);
    else discs.current.delete(id);
  };

  return (
    <div ref={root} className="st">
      <span ref={status} className="sr-only" aria-live="polite" />
      <IsoFigure width={WIDTH} height={HEIGHT} label={props.label}>
        <defs>
          <clipPath id="st-screen-clip">
            <rect x={0} y={0} width={SCREEN_W} height={SCREEN_H} />
          </clipPath>
        </defs>
        {props.desk}
        {props.monitor}
        <g transform={SCREEN_MATRIX}>
          <g clipPath="url(#st-screen-clip)">
            <Screen studio={studio} playing={playing} handlers={handlers} playhead={playhead} column={column} progress={progress} />
          </g>
        </g>
        {props.rim}
        {props.keyboard}
        {SPINDLES.map((spindle, index) => {
          const list = branchVersions(studio, spindle.branch);
          const above = postAbove(spindle.x, spindle.y, list.length);
          const parts = list.map((version, at) => {
            const disc = discAt(spindle.x, spindle.y, at);
            const lit = version.id === studio.selected;
            return (
              <g
                key={version.id}
                ref={bindDisc(version.id)}
                className="st-disc"
                role="button"
                tabIndex={-1}
                aria-label={`${spindle.branch} ${label(version)}, ${version.message}`}
                onClick={() => handlers.select(version.id)}
              >
                <Solid paths={disc.body} tone={lit ? "hi" : "mid"} lit={lit} />
                <Line d={disc.label} tone="faint" />
                <Line d={disc.hole} tone="lo" />
              </g>
            );
          });
          return (
            <g key={spindle.branch} className={studio.branch === spindle.branch ? "st-spindle st-spindle-on" : "st-spindle"}>
              {props.spindles[index]}
              {parts.slice(0, -1)}
              <Solid paths={above.post} tone="mid" crease="none" />
              <Solid paths={above.cap} tone="mid" />
              {parts.at(-1)}
            </g>
          );
        })}
      </IsoFigure>
    </div>
  );
}
