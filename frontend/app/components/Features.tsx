"use client";

import { motion } from "framer-motion";
import { CONTAINER, EASE_SNAP, RevealHeading, SECTION_TITLE } from "./landing";

type Tone = "ink" | "outline" | "lime";

const features: { title: string; description: string; tone: Tone }[] = [
  {
    title: "Branches",
    description: "Another idea, same project, original untouched. Bring it back with a pull request.",
    tone: "ink",
  },
  { title: "Full history", description: "Every version. Restore any of them into your DAW as a separate copy.", tone: "outline" },
  { title: "Mixer diff", description: "What changed in the FL Studio mixer, slot by slot.", tone: "lime" },
  { title: "Collabs", description: "Invite your crew. You decide what goes into main.", tone: "outline" },
  { title: "Encrypted", description: "End-to-end encryption by default. Your stems stay yours.", tone: "ink" },
  { title: "In your DAW", description: "Save, restore and switch branches without leaving the session.", tone: "outline" },
];

// Resting colours, and the colours each block flips to on hover.
const TONES: Record<Tone, { block: string; text: string }> = {
  ink: { block: "bg-ink text-paper hover:bg-lime hover:text-ink", text: "text-paper-soft group-hover:text-ink" },
  outline: {
    block: "border-2 border-ink hover:bg-ink hover:text-paper",
    text: "text-ink-soft group-hover:text-paper-soft",
  },
  lime: { block: "bg-lime hover:bg-ink hover:text-paper", text: "group-hover:text-paper-soft" },
};

export default function Features() {
  return (
    <section id="features" className={`${CONTAINER} flex scroll-mt-24 flex-col gap-12 py-24 md:py-32`}>
      <RevealHeading text="Built to hit." className={SECTION_TITLE} />
      <motion.div
        className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-3"
        initial="hidden"
        whileInView="visible"
        viewport={{ once: true, margin: "-10% 0px" }}
        transition={{ staggerChildren: 0.07 }}
      >
        {features.map((feature) => {
          const tone = TONES[feature.tone];
          return (
            <motion.article
              key={feature.title}
              variants={{ hidden: { opacity: 0, y: 48, scale: 0.96 }, visible: { opacity: 1, y: 0, scale: 1 } }}
              transition={{ duration: 0.55, ease: EASE_SNAP }}
              className={`group flex min-h-[210px] flex-col justify-between gap-4 p-7 transition-colors duration-300 ease-snap ${tone.block}`}
            >
              <h3 className="font-display text-[28px] font-extrabold tracking-[-0.03em] uppercase">{feature.title}</h3>
              <p className={`leading-relaxed transition-colors duration-300 ${tone.text}`}>{feature.description}</p>
            </motion.article>
          );
        })}
      </motion.div>
    </section>
  );
}
