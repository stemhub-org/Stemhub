"use client";

import { useRef } from "react";
import { motion, useReducedMotion, useScroll, useTransform } from "framer-motion";
import { CONTAINER, EASE_OUT, LABEL, RevealHeading, SECTION_TITLE } from "./landing";
import SessionTimeline from "./SessionTimeline";
import TutorialStep from "./TutorialStep";
import { steps, type Step } from "./tutorial";

// The people and the file the walkthrough follows.
const cast = [
  { name: "Nova", role: "Producer, owns the project" },
  { name: "Kai", role: "Topliner, invited as an Editor" },
  { name: "Midnight Drive.flp", role: "FL Studio · 92 BPM · 7 files, 19.9 MB" },
];

// Each overview card jumps to its step of the walkthrough below.
const CARD_LINK =
  "focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-ink [&_h3]:underline-offset-4 hover:[&_h3]:underline";

// Step 03 is the one element of the composition allowed to collide: it swings in
// from an angle as you scroll and settles slightly off-axis.
function CollidingStep({ step }: { step: Step }) {
  const ref = useRef<HTMLAnchorElement>(null);
  const reduceMotion = useReducedMotion();
  const { scrollYProgress } = useScroll({ target: ref, offset: ["start end", "center 55%"] });
  const rotate = useTransform(scrollYProgress, [0, 1], [9, -1.5]);
  const y = useTransform(scrollYProgress, [0, 1], [90, 0]);

  return (
    <motion.a
      ref={ref}
      href={`#how-${step.number}`}
      style={reduceMotion ? { rotate: -1.5 } : { rotate, y }}
      className={`flex flex-col gap-3 bg-lime p-6 ${CARD_LINK}`}
    >
      <StepContent step={step} muted={false} />
    </motion.a>
  );
}

function StepContent({ step, muted }: { step: Step; muted: boolean }) {
  return (
    <>
      <span className="font-display text-6xl leading-none font-extrabold tracking-[-0.04em]">{step.number}</span>
      <h3 className="text-xl font-bold">{step.title}</h3>
      <p className={`leading-relaxed ${muted ? "text-ink-soft" : ""}`}>{step.summary}</p>
    </>
  );
}

function SessionIntro() {
  return (
    <motion.div
      className="grid grid-cols-1 gap-8 border-2 border-ink p-6 sm:p-10 lg:grid-cols-[minmax(0,6fr)_minmax(0,5fr)] lg:gap-14"
      initial={{ opacity: 0, y: 32 }}
      whileInView={{ opacity: 1, y: 0 }}
      viewport={{ once: true, margin: "-10% 0px" }}
      transition={{ duration: 0.7, ease: EASE_OUT }}
    >
      <div className="flex flex-col gap-4">
        <span className={LABEL}>A real session / Midnight Drive</span>
        <h3 className="font-display text-[clamp(1.75rem,3.2vw,2.75rem)] leading-[0.95] font-extrabold tracking-[-0.03em] uppercase">
          From first sketch to the label, in five days.
        </h3>
        <p className="max-w-[560px] text-lg leading-relaxed text-ink-soft">
          Nova produces in FL Studio. This week she&apos;s finishing Midnight Drive, a late-night 808 beat, and
          bringing Kai in for the topline. Every screen below is the StemHub plugin during that session, step by
          step.
        </p>
      </div>
      <dl className="flex flex-col self-end border-t-2 border-ink">
        {cast.map((member) => (
          <div key={member.name} className="flex flex-wrap items-baseline justify-between gap-x-6 gap-y-1 border-b border-ink-line py-3.5">
            <dt className="font-bold">{member.name}</dt>
            <dd className="text-ink-soft">{member.role}</dd>
          </div>
        ))}
      </dl>
    </motion.div>
  );
}

export default function HowItWorks() {
  return (
    <section id="how" className={`${CONTAINER} flex scroll-mt-24 flex-col gap-12 py-24 md:py-32`}>
      <div className="flex flex-wrap items-end justify-between gap-6">
        <RevealHeading text="Catch it. Pull it apart. Push it further." className={`${SECTION_TITLE} max-w-[900px]`} />
        <span className={LABEL}>How it works / 01–04</span>
      </div>

      <div className="grid grid-cols-1 border-t-2 border-ink sm:grid-cols-2 lg:grid-cols-4">
        {steps.map((step, i) =>
          step.number === "03" ? (
            <CollidingStep key={step.number} step={step} />
          ) : (
            <motion.a
              key={step.number}
              href={`#how-${step.number}`}
              className={`flex flex-col gap-3 py-6 pr-6 ${i === 3 ? "sm:pl-6" : ""} ${CARD_LINK}`}
              initial={{ opacity: 0, y: 32 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-10% 0px" }}
              transition={{ delay: i * 0.08, duration: 0.7, ease: EASE_OUT }}
            >
              <StepContent step={step} muted />
            </motion.a>
          )
        )}
      </div>

      <div className="mt-8 flex flex-col gap-20 md:gap-28">
        <SessionIntro />
        {steps.map((step) => (
          <TutorialStep key={step.number} step={step} />
        ))}
        <SessionTimeline />
      </div>
    </section>
  );
}
