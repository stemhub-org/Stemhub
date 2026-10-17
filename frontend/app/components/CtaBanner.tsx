"use client";

import { useRef } from "react";
import { motion, useReducedMotion, useScroll, useTransform } from "framer-motion";
import { CONTAINER, WipeLink } from "./landing";

export default function CtaBanner() {
  const ref = useRef<HTMLElement>(null);
  const reduceMotion = useReducedMotion();
  const { scrollYProgress } = useScroll({ target: ref, offset: ["start end", "start 25%"] });
  // The lime panel opens from an inset card to full bleed as it reaches the top.
  const clipPath = useTransform(
    scrollYProgress,
    [0, 1],
    ["inset(14% 7% 14% 7% round 28px)", "inset(0% 0% 0% 0% round 0px)"]
  );
  const scale = useTransform(scrollYProgress, [0, 1], [0.86, 1]);

  return (
    <motion.section ref={ref} style={reduceMotion ? undefined : { clipPath }} className="bg-lime text-ink">
      <div className={`${CONTAINER} flex flex-wrap items-end justify-between gap-8 py-24 md:py-32`}>
        <motion.h2
          style={reduceMotion ? undefined : { scale }}
          className="font-display origin-bottom-left text-[clamp(2.5rem,8vw,7.25rem)] leading-[0.88] font-extrabold tracking-[-0.04em] uppercase"
        >
          Drop the perfect.
          <br />
          Share the spark.
        </motion.h2>
        <WipeLink href="/register" variant="ink" size="lg">
          Drop a stem +
        </WipeLink>
      </div>
    </motion.section>
  );
}
