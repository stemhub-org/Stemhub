"use client";

import { useRef, useState } from "react";
import {
  motion,
  useMotionValueEvent,
  useReducedMotion,
  useScroll,
  useTransform,
  type MotionValue,
} from "framer-motion";
import { CONTAINER, EASE_OUT, LABEL, RevealHeading, SECTION_TITLE } from "./landing";

// The diff of a save in the StemHub plugin, worded as the backend reports it
// (dry/wet values are FL Studio's raw values).
const changes = [
  { kind: "added", marker: "+", target: "Insert 3 “Lead” · slot 2", detail: "Reverb added" },
  { kind: "removed", marker: "−", target: "Insert 5 “Hats” · slot 1", detail: "Delay removed" },
  { kind: "changed", marker: "~", target: "Insert 2 “Drum Bus” · slot 1", detail: "Dry/wet 12800 → 6400" },
] as const;

const MARKER_STYLES = {
  added: "bg-lime text-ink",
  removed: "border-2 border-paper",
  changed: "bg-paper text-ink",
};

// Each row owns a slice of the scroll range; it slides in as the playhead crosses it.
function threshold(index: number) {
  return 0.15 + index * 0.25;
}

function DiffRow({
  change,
  index,
  progress,
  reduceMotion,
}: {
  change: (typeof changes)[number];
  index: number;
  progress: MotionValue<number>;
  reduceMotion: boolean;
}) {
  const start = threshold(index);
  const opacity = useTransform(progress, [start, start + 0.12], [0, 1]);
  const x = useTransform(progress, [start, start + 0.15], [-32, 0]);
  const removed = change.kind === "removed";

  return (
    <motion.div
      style={reduceMotion ? undefined : { opacity, x }}
      className="flex items-center gap-4 border-b border-paper-line px-5 py-4 last:border-b-0"
    >
      <span
        aria-hidden="true"
        className={`flex h-7 w-7 shrink-0 items-center justify-center font-bold ${MARKER_STYLES[change.kind]}`}
      >
        {change.marker}
      </span>
      <span className={`min-w-0 flex-1 ${removed ? "text-paper-soft" : ""}`}>{change.target}</span>
      <span className={`text-sm font-semibold ${removed ? "text-paper-soft line-through" : ""}`}>{change.detail}</span>
    </motion.div>
  );
}

export default function DiffSection() {
  const cardRef = useRef<HTMLDivElement>(null);
  const reduceMotion = useReducedMotion() ?? false;
  const { scrollYProgress } = useScroll({ target: cardRef, offset: ["start 85%", "end 45%"] });
  const [revealed, setRevealed] = useState(0);

  useMotionValueEvent(scrollYProgress, "change", (value) => {
    setRevealed(changes.filter((_, i) => value >= threshold(i) + 0.06).length);
  });

  const count = reduceMotion ? changes.length : revealed;

  return (
    <section id="product" className="scroll-mt-16 bg-ink text-paper">
      <div className={`${CONTAINER} flex flex-wrap items-center gap-14 py-24 md:py-32`}>
        <div className="flex flex-[1_1_380px] flex-col gap-5">
          <span className={`${LABEL} text-lime`}>The diff</span>
          <RevealHeading text="See what moved." className={SECTION_TITLE} />
          <motion.p
            className="max-w-[420px] text-[17px] leading-relaxed text-paper-soft"
            initial={{ opacity: 0, y: 16 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true }}
            transition={{ duration: 0.8, ease: EASE_OUT }}
          >
            Each version lists what changed in the FL Studio mixer since the one before: insert by insert, effect
            slot by effect slot.
          </motion.p>
        </div>

        <div ref={cardRef} className="min-w-0 flex-[1_1_520px] border-2 border-paper">
          <div className={`flex justify-between gap-4 border-b-2 border-paper px-5 py-3.5 ${LABEL}`}>
            <span>Late Night / “Late-night idea”</span>
            <span className="text-lime" aria-live="polite">
              {count} {count === 1 ? "change" : "changes"}
            </span>
          </div>
          <div className="h-0.5 bg-paper-line">
            <motion.div
              className="h-full origin-left bg-lime"
              style={{ scaleX: reduceMotion ? 1 : scrollYProgress }}
            />
          </div>
          {changes.map((change, i) => (
            <DiffRow key={change.kind} change={change} index={i} progress={scrollYProgress} reduceMotion={reduceMotion} />
          ))}
        </div>
      </div>
    </section>
  );
}
