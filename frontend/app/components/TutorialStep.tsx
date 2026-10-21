"use client";

import { useRef, useState } from "react";
import Image from "next/image";
import { motion, useReducedMotion } from "framer-motion";
import { EASE_OUT, LABEL } from "./landing";
import type { Step } from "./tutorial";

// One step of the walkthrough: numbered instructions on the left, the plugin as it
// looks for the selected instruction on the right. All of a step's screenshots are
// stacked in the frame so switching between them never waits on the network.
export default function TutorialStep({ step }: { step: Step }) {
  const [active, setActive] = useState(0);
  const frameRef = useRef<HTMLDivElement>(null);
  const reduceMotion = useReducedMotion();
  const figureId = `how-${step.number}-screen`;
  const current = step.substeps[active];

  // On narrow screens the frame sits above the list and can be scrolled away:
  // bring it back so the new screenshot is actually seen. On wide screens it is
  // sticky and always visible, so this never scrolls.
  function select(index: number) {
    setActive(index);
    const frame = frameRef.current;
    if (!frame) return;
    const { top, bottom } = frame.getBoundingClientRect();
    if (bottom < 120 || top > window.innerHeight) {
      frame.scrollIntoView({ block: "center", behavior: reduceMotion ? "auto" : "smooth" });
    }
  }

  return (
    <motion.article
      id={`how-${step.number}`}
      aria-labelledby={`how-${step.number}-title`}
      className="grid scroll-mt-24 grid-cols-1 gap-x-14 gap-y-8 border-t-2 border-ink pt-10 lg:grid-cols-[minmax(0,5fr)_minmax(0,7fr)]"
      initial={{ opacity: 0, y: 40 }}
      whileInView={{ opacity: 1, y: 0 }}
      viewport={{ once: true, margin: "-10% 0px" }}
      transition={{ duration: 0.7, ease: EASE_OUT }}
    >
      <header className="flex flex-col gap-4">
        <div className="flex items-baseline gap-4">
          <span className="font-display text-6xl leading-none font-extrabold tracking-[-0.04em]">{step.number}</span>
          <span className={`${LABEL} text-ink-soft`}>Step {step.number} / 04</span>
        </div>
        <h3
          id={`how-${step.number}-title`}
          className="font-display text-[clamp(1.75rem,3.2vw,2.75rem)] leading-[0.95] font-extrabold tracking-[-0.03em] uppercase"
        >
          {step.title}
        </h3>
        <p className="max-w-[460px] text-lg leading-relaxed text-ink-soft">{step.intro}</p>
      </header>

      <figure className="self-start lg:sticky lg:top-24 lg:col-start-2 lg:row-span-2 lg:row-start-1">
        <div id={figureId} ref={frameRef} className="border-2 border-ink bg-ink">
          <div className={`flex items-center justify-between gap-4 px-4 py-3 text-paper ${LABEL}`}>
            <span className="flex items-center gap-2.5">
              <span aria-hidden="true" className="h-2.5 w-2.5 bg-lime" />
              Stemhub Session · FL Studio
            </span>
            <span className="text-paper-soft" aria-hidden="true">
              {String(active + 1).padStart(2, "0")} / {String(step.substeps.length).padStart(2, "0")}
            </span>
          </div>
          <div className="relative aspect-[9/7] overflow-hidden">
            {step.substeps.map((substep, i) => (
              <Image
                key={substep.shot.src}
                src={substep.shot.src}
                alt={i === active ? substep.shot.alt : ""}
                aria-hidden={i === active ? undefined : true}
                fill
                sizes="(min-width: 1320px) 720px, (min-width: 1024px) 56vw, 100vw"
                className={`object-cover transition-opacity duration-300 ease-out motion-reduce:transition-none ${
                  i === active ? "opacity-100" : "opacity-0"
                }`}
              />
            ))}
          </div>
        </div>
        <figcaption className="mt-3 flex items-start justify-between gap-4 text-sm text-ink-soft">
          <span className="flex items-start gap-2.5" aria-live="polite">
            <span aria-hidden="true" className="mt-1.5 h-2 w-2 shrink-0 bg-ink" />
            {current.shot.caption}
          </span>
          <a
            href={current.shot.src}
            target="_blank"
            rel="noopener"
            className="shrink-0 font-bold text-ink underline-offset-4 hover:underline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ink"
          >
            Full size ↗
          </a>
        </figcaption>
      </figure>

      <div className="flex flex-col gap-8">
        <ol className="flex flex-col gap-1.5">
          {step.substeps.map((substep, i) => {
            const selected = i === active;
            return (
              <li key={substep.title}>
                <button
                  type="button"
                  onClick={() => select(i)}
                  aria-pressed={selected}
                  aria-controls={figureId}
                  className={`flex w-full gap-4 border-2 p-4 text-left transition-colors duration-300 ease-snap focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ink motion-reduce:transition-none ${
                    selected ? "border-ink bg-ink text-paper" : "border-transparent hover:border-ink"
                  }`}
                >
                  <span
                    aria-hidden="true"
                    className={`flex h-7 w-7 shrink-0 items-center justify-center text-xs font-bold ${
                      selected ? "bg-lime text-ink" : "border-2 border-ink"
                    }`}
                  >
                    {i + 1}
                  </span>
                  <span className="flex flex-col gap-1">
                    <span className="font-bold">{substep.title}</span>
                    <span className={`leading-relaxed ${selected ? "text-paper-soft" : "text-ink-soft"}`}>
                      {substep.body}
                    </span>
                  </span>
                </button>
              </li>
            );
          })}
        </ol>

        <aside className="flex flex-col gap-3 border-l-4 border-lime bg-ink px-5 py-4 text-paper">
          <span className="flex flex-col gap-1">
            <span className={`${LABEL} text-lime`}>From the session</span>
            <span className="text-sm font-bold">
              {step.story.who} · {step.story.when}
            </span>
          </span>
          <p className="leading-relaxed text-paper-soft">{step.story.text}</p>
        </aside>
      </div>
    </motion.article>
  );
}
