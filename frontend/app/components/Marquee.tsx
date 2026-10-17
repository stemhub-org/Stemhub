"use client";

import { Fragment, useRef } from "react";
import {
  motion,
  useAnimationFrame,
  useMotionValue,
  useReducedMotion,
  useScroll,
  useSpring,
  useTransform,
  useVelocity,
} from "framer-motion";

const items = ["FL Studio", "Ableton Live", "Versions", "Branches", "Pull requests", "Mixer diff"];
const COPIES = 4;
const BASE_SPEED = 2.4; // % of the track per second

function wrap(min: number, max: number, value: number) {
  const range = max - min;
  return ((((value - min) % range) + range) % range) + min;
}

// A tape loop that drifts on its own and is pushed by the scroll: faster when you
// scroll fast, reversed when you scroll back up.
export default function Marquee() {
  const reduceMotion = useReducedMotion();
  const baseX = useMotionValue(0);
  const { scrollY } = useScroll();
  const scrollVelocity = useVelocity(scrollY);
  const smoothVelocity = useSpring(scrollVelocity, { damping: 50, stiffness: 400 });
  const velocityFactor = useTransform(smoothVelocity, [0, 1000], [0, 4], { clamp: false });
  const x = useTransform(baseX, (v) => `${wrap(-100 / COPIES, 0, v)}%`);
  const direction = useRef(-1);

  useAnimationFrame((_, delta) => {
    if (reduceMotion) return;
    const factor = velocityFactor.get();
    if (factor < 0) direction.current = 1;
    else if (factor > 0) direction.current = -1;
    const move = direction.current * BASE_SPEED * (delta / 1000);
    baseX.set(baseX.get() + move + move * Math.abs(factor));
  });

  return (
    <section aria-label="Works with FL Studio and Ableton Live" className="overflow-hidden bg-ink py-5 text-paper">
      <motion.div style={{ x }} className="flex w-max whitespace-nowrap will-change-transform">
        {Array.from({ length: COPIES }, (_, copy) => (
          <div
            key={copy}
            aria-hidden={copy > 0}
            className="font-display flex items-center gap-7 pr-7 text-[clamp(1.5rem,3vw,2.5rem)] font-extrabold tracking-[-0.03em] uppercase"
          >
            {items.map((item) => (
              <Fragment key={item}>
                <span>{item}</span>
                <span className="text-lime">/</span>
              </Fragment>
            ))}
          </div>
        ))}
      </motion.div>
    </section>
  );
}
