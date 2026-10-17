"use client";

import { useRef } from "react";
import { motion, useScroll, useTransform } from "framer-motion";
import { FIGURE_BARS, FIGURE_HEAD, FIGURE_LIMBS, FIGURE_TORSO, FIGURE_VIEWBOX } from "./BrandMark";
import { CONTAINER, EASE_OUT, EASE_SNAP, LABEL, WipeLink } from "./landing";

const headline = ["Your", "loop", "wants", "company."];
const values = ["Reckless, not careless", "Raw, not retro", "Loud, not hostile", "Open, never elitist"];

// The logo figure, assembled in front of you: body first, limbs drawn out, and the
// reaching arm last, right before it catches the three signal bars.
function CatchFigure() {
  return (
    <svg viewBox={FIGURE_VIEWBOX} fill="currentColor" className="w-full max-w-[420px]" role="img" aria-label="StemHub figure catching a signal">
      <motion.circle
        {...FIGURE_HEAD}
        initial={{ scale: 0 }}
        animate={{ scale: 1 }}
        transition={{ delay: 0.9, duration: 0.5, ease: EASE_OUT }}
      />
      <motion.rect
        {...FIGURE_TORSO}
        initial={{ scaleY: 0 }}
        animate={{ scaleY: 1 }}
        style={{ originY: 0 }}
        transition={{ delay: 0.95, duration: 0.45, ease: EASE_SNAP }}
      />
      {FIGURE_LIMBS.map(({ width, ...line }, i) => (
        <motion.line
          key={`${line.x1}-${line.y1}`}
          {...line}
          stroke="currentColor"
          strokeWidth={width}
          strokeLinecap="round"
          initial={{ pathLength: 0, opacity: 0 }}
          animate={{ pathLength: 1, opacity: 1 }}
          transition={{
            delay: 1.15 + i * 0.09,
            pathLength: { delay: 1.15 + i * 0.09, duration: 0.45, ease: EASE_OUT },
            opacity: { delay: 1.15 + i * 0.09, duration: 0.01 },
          }}
        />
      ))}
      {FIGURE_BARS.map((bar, i) => (
        <motion.rect
          key={bar.x}
          {...bar}
          rx={bar.width / 2}
          initial={{ scaleY: 0 }}
          animate={{ scaleY: 1 }}
          style={{ originY: 1 }}
          transition={{ delay: 1.6 + i * 0.1, type: "spring", stiffness: 520, damping: 18 }}
        />
      ))}
    </svg>
  );
}

export default function Hero() {
  const ref = useRef<HTMLElement>(null);
  const { scrollYProgress } = useScroll({ target: ref, offset: ["start start", "end start"] });
  const figureY = useTransform(scrollYProgress, [0, 1], [0, -80]);
  const copyY = useTransform(scrollYProgress, [0, 1], [0, 60]);

  return (
    <section ref={ref} id="top" className={`${CONTAINER} flex flex-wrap items-stretch gap-10 pt-28 pb-16 md:pt-36 md:pb-24`}>
      <motion.div style={{ y: copyY }} className="@container flex min-w-0 flex-[3_1_560px] flex-col justify-between gap-9">
        <motion.span
          className={LABEL}
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ delay: 0.1, duration: 0.6 }}
        >
          StemHub® / Version control for producers
        </motion.span>

        {/* Sized on the column, not the window: Syne 800 runs ~1.1em per capital, so
            11cqw keeps "COMPANY." inside the column at every width. */}
        <h1 className="font-display text-[clamp(2.25rem,11cqw,8rem)] leading-[0.88] font-extrabold tracking-[-0.04em] uppercase">
          {headline.map((word, i) => (
            <span key={word} className="inline-block overflow-hidden pr-[0.22em] pb-[0.06em] align-top">
              <motion.span
                className="inline-block"
                initial={{ y: "105%" }}
                animate={{ y: "0%" }}
                transition={{ delay: 0.15 + i * 0.08, duration: 0.9, ease: EASE_OUT }}
              >
                {word}
              </motion.span>
            </span>
          ))}
        </h1>

        <motion.span
          className={`self-start bg-lime px-3 py-2 ${LABEL}`}
          initial={{ opacity: 0, scale: 1.25, rotate: 8 }}
          animate={{ opacity: 1, scale: 1, rotate: -2 }}
          transition={{ delay: 0.75, type: "spring", stiffness: 420, damping: 16 }}
        >
          Open stems / Open minds / No gatekeeping
        </motion.span>

        <div className="flex flex-wrap items-end justify-between gap-7">
          <motion.p
            className="max-w-[460px] text-lg leading-relaxed"
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.85, duration: 0.8, ease: EASE_OUT }}
          >
            StemHub is the shared workbench where producers catch an idea, pull it apart, and push it
            somewhere nobody could reach alone. Save every version straight from FL Studio or Ableton Live.
          </motion.p>
          <motion.div
            className="flex flex-wrap gap-2.5"
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.95, duration: 0.8, ease: EASE_OUT }}
          >
            <WipeLink href="/register" variant="ink">
              Start free
            </WipeLink>
            <WipeLink href="#how" variant="outline">
              How it works
            </WipeLink>
          </motion.div>
        </div>
      </motion.div>

      <motion.div
        className="relative flex min-h-[460px] flex-[2_1_340px] flex-col lg:max-w-[440px] justify-between overflow-hidden bg-ink p-7 text-paper md:min-h-[560px]"
        initial={{ clipPath: "inset(100% 0% 0% 0%)" }}
        animate={{ clipPath: "inset(0% 0% 0% 0%)" }}
        transition={{ delay: 0.3, duration: 1.1, ease: EASE_SNAP }}
      >
        <motion.div style={{ y: figureY }} className="mt-4 flex justify-center">
          <CatchFigure />
        </motion.div>
        <ul className={`${LABEL} leading-relaxed`}>
          {values.map((value, i) => (
            <li key={value} className="overflow-hidden">
              <motion.span
                className="inline-block"
                initial={{ y: "110%" }}
                animate={{ y: "0%" }}
                transition={{ delay: 1.7 + i * 0.07, duration: 0.6, ease: EASE_OUT }}
              >
                {value}
              </motion.span>
            </li>
          ))}
        </ul>
      </motion.div>
    </section>
  );
}
