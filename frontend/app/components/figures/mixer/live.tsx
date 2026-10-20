"use client";

import * as React from "react";
import { useMotionValueEvent, useScroll } from "framer-motion";
import { translateAlong } from "../../iso/iso-kit";
import { IsoFigure } from "../../iso/react/draw";
import { asksForStillness, follow, useReadout } from "../../iso/react/live";
import { wetPointer } from "./frame";
import { CHANGES, WET_AFTER, WET_BEFORE, WET_STRIP, readoutOf, spokenOf, wetAngle } from "./model";
import { HEIGHT, LIFT, P, WIDTH } from "./view";

const MOST_DT = 1 / 30;
const SEAT = 0.26;
const TURN = 0.4;
// Where along the figure's pass through the viewport each change happens.
const THRESHOLDS = [0.18, 0.42, 0.66];

interface ModulePart {
  key: string;
  role: "kept" | "added" | "removed";
  clip: string;
  box: React.ReactNode;
  plate: React.ReactNode;
}

interface Props {
  label: string;
  defs: React.ReactNode;
  base: React.ReactNode;
  desk: React.ReactNode;
  slots: React.ReactNode;
  ghost: React.ReactNode;
  modules: ModulePart[];
  deskTop: React.ReactNode;
}

interface Model {
  target: number;
  added: number;
  removed: number;
  wet: number;
}

const WET_KEY = `module-${WET_STRIP}-0`;
const countAt = (progress: number) => THRESHOLDS.filter((threshold) => progress >= threshold).length;
const clamp01 = (value: number) => Math.min(1, Math.max(0, value));
const fine = (value: number) => `${Math.round(value * 1000) / 1000}`;

export function MixerLive(props: Props) {
  const { anchor: root, say } = useReadout<HTMLDivElement>();
  const added = React.useRef<SVGGElement[]>([]);
  const removed = React.useRef<SVGGElement[]>([]);
  const ghost = React.useRef<SVGGElement | null>(null);
  const pointer = React.useRef<SVGPathElement | null>(null);
  const model = React.useRef<Model>({ target: CHANGES, added: 1, removed: 1, wet: WET_AFTER });
  const manual = React.useRef(false);
  const fromScroll = React.useRef(CHANGES);
  const frame = React.useRef(0);
  const last = React.useRef(0);
  const visible = React.useRef(false);
  const { scrollYProgress } = useScroll({ target: root, offset: ["start 85%", "end 50%"] });

  const draw = React.useCallback(() => {
    const m = model.current;
    const addLift = (1 - m.added) * LIFT;
    const addShow = clamp01(m.added * 2.4);
    added.current.forEach((node) => {
      node.setAttribute("transform", addLift > 0.01 ? translateAlong(P, [0, 0, addLift]) : "");
      node.style.opacity = addShow < 1 ? fine(addShow) : "";
    });
    const outLift = m.removed * LIFT;
    const outShow = 1 - clamp01((m.removed - 0.45) / 0.55);
    removed.current.forEach((node) => {
      node.setAttribute("transform", outLift > 0.01 ? translateAlong(P, [0, 0, outLift]) : "");
      node.style.opacity = outShow < 1 ? fine(outShow) : "";
    });
    if (ghost.current) ghost.current.style.opacity = fine(clamp01(m.removed));
    pointer.current?.setAttribute("d", wetPointer(wetAngle(m.wet)));
    // The lit part is the change shown last.
    const lit = m.target - 1;
    root.current?.querySelectorAll<SVGGElement>(".mx-added, .mx-removed, .mx-wet").forEach((node) => {
      const role = node.classList.contains("mx-added") ? 0 : node.classList.contains("mx-removed") ? 1 : 2;
      if (role === lit) node.setAttribute("data-lit", "");
      else node.removeAttribute("data-lit");
    });
    const applied = (m.added > 0.5 ? 1 : 0) + (m.removed > 0.5 ? 1 : 0) + (Math.abs(m.wet - WET_AFTER) < (WET_BEFORE - WET_AFTER) / 2 ? 1 : 0);
    say(readoutOf(applied, m.wet));
  }, [root, say]);

  const run = React.useCallback(() => {
    if (frame.current || !visible.current) return;
    last.current = 0;
    const tick = (now: number) => {
      const dt = last.current ? Math.min((now - last.current) / 1000, MOST_DT) : 1 / 60;
      last.current = now;
      const calm = asksForStillness();
      const m = model.current;
      const goal = { added: m.target >= 1 ? 1 : 0, removed: m.target >= 2 ? 1 : 0, wet: m.target >= 3 ? WET_AFTER : WET_BEFORE };
      m.added = follow(m.added, goal.added, dt, SEAT, calm);
      m.removed = follow(m.removed, goal.removed, dt, SEAT, calm);
      m.wet = follow(m.wet, goal.wet, dt, TURN, calm);
      if (Math.abs(m.wet - goal.wet) < 2) m.wet = goal.wet;
      draw();
      const settled = m.added === goal.added && m.removed === goal.removed && m.wet === goal.wet;
      frame.current = settled || !visible.current ? 0 : requestAnimationFrame(tick);
    };
    frame.current = requestAnimationFrame(tick);
  }, [draw]);

  const steer = React.useCallback(
    (target: number) => {
      const next = Math.max(0, Math.min(CHANGES, target));
      const m = model.current;
      if (next === m.target) return;
      m.target = next;
      root.current?.setAttribute("aria-valuenow", `${next}`);
      root.current?.setAttribute("aria-valuetext", spokenOf(next));
      run();
    },
    [root, run],
  );

  // Before the first frame, put the desk where the scroll position says it should be,
  // without animating: it is usually still below the fold.
  React.useEffect(() => {
    if (asksForStillness()) {
      draw();
      return;
    }
    const count = countAt(scrollYProgress.get());
    fromScroll.current = count;
    const m = model.current;
    m.target = count;
    m.added = count >= 1 ? 1 : 0;
    m.removed = count >= 2 ? 1 : 0;
    m.wet = count >= 3 ? WET_AFTER : WET_BEFORE;
    draw();
  }, [draw, scrollYProgress]);

  useMotionValueEvent(scrollYProgress, "change", (progress) => {
    if (asksForStillness()) return;
    const count = countAt(progress);
    if (count === fromScroll.current) return;
    fromScroll.current = count;
    manual.current = false;
    steer(count);
  });

  React.useEffect(() => {
    const node = root.current;
    if (!node) return;
    const observer = new IntersectionObserver(
      (entries) => {
        visible.current = Boolean(entries.at(-1)?.isIntersecting);
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

  const toggle = () => {
    manual.current = true;
    steer(model.current.target === CHANGES ? 0 : CHANGES);
  };

  const bindAdded = (index: number) => (node: SVGGElement | null) => {
    if (node) added.current[index] = node;
  };
  const bindRemoved = (index: number) => (node: SVGGElement | null) => {
    if (node) removed.current[index] = node;
  };

  return (
    <div
      ref={root}
      className="mx"
      role="slider"
      tabIndex={0}
      aria-label="Mixing desk replaying a save: Space or a click switches between before and after the save, arrow keys step through the three changes"
      aria-valuemin={0}
      aria-valuemax={CHANGES}
      aria-valuenow={CHANGES}
      aria-valuetext={spokenOf(CHANGES)}
      onClick={toggle}
      onKeyDown={(event) => {
        let next: number | null = null;
        if (event.key === " " || event.key === "Enter") {
          event.preventDefault();
          if (!event.repeat) toggle();
          return;
        }
        if (event.key === "ArrowLeft" || event.key === "ArrowDown") next = model.current.target - 1;
        else if (event.key === "ArrowRight" || event.key === "ArrowUp") next = model.current.target + 1;
        else if (event.key === "Home") next = 0;
        else if (event.key === "End") next = CHANGES;
        if (next === null) return;
        event.preventDefault();
        manual.current = true;
        steer(next);
      }}
    >
      <IsoFigure width={WIDTH} height={HEIGHT} label={props.label}>
        {props.defs}
        {props.base}
        {props.desk}
        {props.slots}
        <g ref={ghost} style={{ opacity: 1 }}>
          {props.ghost}
        </g>
        {props.modules.map((part) => {
          const ref = part.role === "added" ? bindAdded(0) : part.role === "removed" ? bindRemoved(0) : undefined;
          const plateRef = part.role === "added" ? bindAdded(1) : part.role === "removed" ? bindRemoved(1) : undefined;
          const hidden = part.role === "removed" ? { opacity: 0 } : undefined;
          return (
            <React.Fragment key={part.key}>
              <g clipPath={`url(#${part.clip})`}>
                <g ref={ref} style={hidden} transform={part.role === "removed" ? translateAlong(P, [0, 0, LIFT]) : undefined}>
                  {part.box}
                </g>
              </g>
              <g ref={plateRef} style={hidden} transform={part.role === "removed" ? translateAlong(P, [0, 0, LIFT]) : undefined}>
                {part.plate}
                {part.key === WET_KEY ? (
                  <path ref={pointer} className="iso-line" data-tone="lit" data-free="end" d={wetPointer(wetAngle(WET_AFTER))} />
                ) : null}
              </g>
            </React.Fragment>
          );
        })}
        {props.deskTop}
      </IsoFigure>
    </div>
  );
}
