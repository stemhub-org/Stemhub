"use client";

import { useRef } from "react";
import { motion, useReducedMotion, useScroll, useTransform } from "framer-motion";
import { CONTAINER, EASE_OUT, LABEL, RevealHeading, SECTION_TITLE } from "./landing";

const steps = [
  { number: "01", title: "Install the plugin", description: "Add StemHub to FL Studio or Ableton Live in one click." },
  { number: "02", title: "Save versions", description: "Every save gets a message so you find it again." },
  {
    number: "03",
    title: "Branch the idea",
    description: "Try something wild without touching the original. Compare, restore, keep the best.",
  },
  { number: "04", title: "Pass it on", description: "Invite collaborators. Everyone works on their own branch." },
];

// Step 03 is the one element of the composition allowed to collide: it swings in
// from an angle as you scroll and settles slightly off-axis.
function CollidingStep({ step }: { step: (typeof steps)[number] }) {
  const ref = useRef<HTMLDivElement>(null);
  const reduceMotion = useReducedMotion();
  const { scrollYProgress } = useScroll({ target: ref, offset: ["start end", "center 55%"] });
  const rotate = useTransform(scrollYProgress, [0, 1], [9, -1.5]);
  const y = useTransform(scrollYProgress, [0, 1], [90, 0]);

  return (
    <motion.div
      ref={ref}
      style={reduceMotion ? { rotate: -1.5 } : { rotate, y }}
      className="flex flex-col gap-3 bg-lime p-6"
    >
      <StepContent step={step} muted={false} />
    </motion.div>
  );
}

function StepContent({ step, muted }: { step: (typeof steps)[number]; muted: boolean }) {
  return (
    <>
      <span className="font-display text-6xl leading-none font-extrabold tracking-[-0.04em]">{step.number}</span>
      <h3 className="text-xl font-bold">{step.title}</h3>
      <p className={`leading-relaxed ${muted ? "text-ink-soft" : ""}`}>{step.description}</p>
    </>
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
            <motion.div
              key={step.number}
              className={`flex flex-col gap-3 py-6 pr-6 ${i === 3 ? "sm:pl-6" : ""}`}
              initial={{ opacity: 0, y: 32 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-10% 0px" }}
              transition={{ delay: i * 0.08, duration: 0.7, ease: EASE_OUT }}
            >
              <StepContent step={step} muted />
            </motion.div>
          )
        )}
      </div>
    </section>
  );
}
