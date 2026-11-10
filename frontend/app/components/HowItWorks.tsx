"use client";

import { useRef, type ReactNode } from "react";
import { motion, useReducedMotion, useScroll, useTransform } from "framer-motion";
import { CONTAINER, EASE_OUT, LABEL, RevealHeading, SECTION_TITLE } from "./landing";

const steps = [
  { number: "01", title: "Installe le plugin", description: "Ajoute StemHub à FL Studio ou Ableton Live en un clic." },
  { number: "02", title: "Enregistre tes versions", description: "Chaque version a son message\u00a0: tu la retrouves sans chercher." },
  {
    number: "03",
    title: "Ouvre une branche",
    description: "Tente un truc fou sans toucher à l’original. Compare, restaure, garde le meilleur.",
  },
  { number: "04", title: "Fais tourner", description: "Invite tes collaborateurs. Chacun bosse sur sa propre branche." },
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

export default function HowItWorks({ figure }: { figure?: ReactNode }) {
  return (
    <section id="how" className={`${CONTAINER} flex scroll-mt-24 flex-col gap-12 py-24 md:py-32`}>
      <div className="flex flex-wrap items-end justify-between gap-6">
        <RevealHeading text={"Attrape. Démonte. Va\u00a0plus loin."} className={`${SECTION_TITLE} max-w-[900px]`} />
        <span className={LABEL}>Comment ça marche / 01–04</span>
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

      {figure ? (
        <motion.div
          initial={{ opacity: 0, y: 40 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-10% 0px" }}
          transition={{ duration: 0.8, ease: EASE_OUT }}
        >
          {figure}
        </motion.div>
      ) : null}
    </section>
  );
}
