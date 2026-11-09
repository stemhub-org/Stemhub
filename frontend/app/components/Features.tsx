"use client";

import { motion } from "framer-motion";
import { CONTAINER, EASE_SNAP, RevealHeading, SECTION_TITLE } from "./landing";

type Tone = "ink" | "outline" | "lime";

const features: { title: string; description: string; tone: Tone }[] = [
  {
    title: "Branches",
    description: "Une autre idée, le même projet, l’original intact. Ramène-la avec une pull request.",
    tone: "ink",
  },
  {
    title: "Rien ne se perd",
    description: "Toutes tes versions. Restaure n’importe laquelle dans ton DAW, en copie séparée.",
    tone: "outline",
  },
  { title: "Diff du mixer", description: "Ce qui a changé dans le mixer de FL Studio, slot par slot.", tone: "lime" },
  { title: "Collabs", description: "Invite ton crew. C’est toi qui décides de ce qui entre dans main.", tone: "outline" },
  { title: "Chiffré", description: "Chiffrement de bout en bout par défaut. Tes stems restent à toi.", tone: "ink" },
  {
    title: "Dans ton DAW",
    description: "Enregistre, restaure et change de branche sans quitter ton projet.",
    tone: "outline",
  },
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
      <RevealHeading text="Taillé pour cogner." className={SECTION_TITLE} />
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
