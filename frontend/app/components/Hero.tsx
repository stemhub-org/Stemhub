"use client";

import { useState } from "react";
import { motion } from "framer-motion";
import { ArrowRight } from "lucide-react";

const words: { text: string; accent?: boolean }[] = [
  { text: "La" },
  { text: "mémoire" },
  { text: "de" },
  { text: "votre" },
  { text: "musique.", accent: true },
];

export default function Hero() {
  const [isHovered, setIsHovered] = useState(false);

  return (
    <section className="relative flex min-h-screen flex-col items-center justify-center overflow-hidden px-6">
      {/* Subtle gradient orb */}
      <div className="pointer-events-none absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2">
        <div
          className="h-[600px] w-[600px] rounded-full opacity-30 blur-[120px]"
          style={{ background: "radial-gradient(circle, rgba(156,87,223,0.3) 0%, transparent 70%)" }}
        />
      </div>

      {/* Staggered H1 */}
      <h1 className="relative z-10 text-center">
        <span className="flex flex-wrap items-center justify-center gap-x-[0.3em]">
          {words.map((word, i) => (
            <span key={i} className="overflow-hidden inline-block py-1">
              <motion.span
                className={`inline-block text-[clamp(2.8rem,8vw,8rem)] font-extralight leading-[1.05] tracking-normal ${word.accent ? "text-accent" : ""}`}
                style={{ fontFamily: "var(--font-syne)" }}
                initial={{ y: "110%" }}
                animate={{ y: "0%" }}
                transition={{
                  duration: 1,
                  delay: 0.15 + i * 0.08,
                  ease: [0.16, 1, 0.3, 1],
                }}
              >
                {word.text}
              </motion.span>
            </span>
          ))}
        </span>
      </h1>

      {/* Subtitle */}
      <motion.p
        className="relative z-10 mt-8 max-w-lg text-center text-base font-light leading-relaxed text-foreground/50 md:text-lg"
        style={{ fontFamily: "var(--font-jakarta)" }}
        initial={{ opacity: 0, y: 20 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 1, delay: 0.8, ease: [0.16, 1, 0.3, 1] }}
      >
        Le versioning décentralisé pensé pour les producteurs.
        <br className="hidden sm:block" />
        Chaque session, chaque piste, chaque choix — préservé.
      </motion.p>

      {/* CTA Button */}
      <motion.div
        className="relative z-10 mt-12"
        initial={{ opacity: 0, y: 20 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 1, delay: 1, ease: [0.16, 1, 0.3, 1] }}
      >
        <button
          onMouseEnter={() => setIsHovered(true)}
          onMouseLeave={() => setIsHovered(false)}
          className="group relative flex items-center gap-3 overflow-hidden rounded-full bg-foreground px-8 py-4 text-sm font-light tracking-wide text-background transition-all duration-500 hover:bg-accent"
          style={{ fontFamily: "var(--font-jakarta)" }}
        >
          <span className="relative z-10">Commencer</span>
          <motion.span
            className="relative z-10"
            animate={{ x: isHovered ? 4 : 0 }}
            transition={{ duration: 0.3 }}
          >
            <ArrowRight size={16} strokeWidth={1.5} />
          </motion.span>
        </button>
      </motion.div>

    </section>
  );
}
