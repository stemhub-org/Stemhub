"use client";

import { motion } from "framer-motion";

const logos = [
  "FL Studio",
  "Ableton",
  "Logic Pro",
  "Pro Tools",
  "Cubase",
  "Bitwig",
];

export default function Logos() {
  return (
    <section className="relative overflow-hidden border-y border-foreground/[0.04] px-6 py-16 md:px-12">
      <div className="mx-auto max-w-7xl">
        <motion.p
          className="mb-10 text-center text-xs font-light uppercase tracking-[0.25em] text-foreground/30"
          style={{ fontFamily: "var(--font-jakarta)" }}
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          transition={{ duration: 0.8 }}
        >
          Compatible avec vos outils
        </motion.p>

        <motion.div
          className="flex flex-wrap items-center justify-center gap-x-16 gap-y-6"
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          transition={{ duration: 0.8, delay: 0.2 }}
        >
          {logos.map((name, i) => (
            <motion.span
              key={name}
              className="text-lg font-extralight tracking-wide text-foreground/20 transition-colors duration-500 hover:text-foreground/50"
              style={{ fontFamily: "var(--font-syne)" }}
              initial={{ opacity: 0, y: 10 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true }}
              transition={{
                duration: 0.5,
                delay: 0.1 + i * 0.06,
                ease: [0.16, 1, 0.3, 1],
              }}
            >
              {name}
            </motion.span>
          ))}
        </motion.div>
      </div>
    </section>
  );
}
