"use client";

import { motion } from "framer-motion";
import { Download, GitCommit, GitBranch, Share2 } from "lucide-react";

const steps = [
  {
    number: "01",
    icon: Download,
    title: "Installez le plugin",
    description:
      "Ajoutez StemHub à FL Studio ou Ableton en un clic. Aucune configuration nécessaire.",
  },
  {
    number: "02",
    icon: GitCommit,
    title: "Commitez vos sessions",
    description:
      "Chaque modification est capturée automatiquement. Nommez vos versions, annotez vos choix.",
  },
  {
    number: "03",
    icon: GitBranch,
    title: "Créez des branches",
    description:
      "Testez un nouveau mix sans toucher à l'original. Comparez, revenez en arrière, fusionnez.",
  },
  {
    number: "04",
    icon: Share2,
    title: "Collaborez",
    description:
      "Partagez une branche avec votre équipe ou vos clients. Chacun travaille sur sa version.",
  },
];

export default function HowItWorks() {
  return (
    <section className="relative px-6 py-32 md:px-12">
      <div className="mx-auto max-w-7xl">
        {/* Header */}
        <motion.div
          className="mb-20 text-center"
          initial={{ opacity: 0, y: 40 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.8, ease: [0.16, 1, 0.3, 1] }}
        >
          <p
            className="mb-4 text-sm font-light uppercase tracking-[0.2em] text-accent"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Comment ça marche
          </p>
          <h2
            className="mx-auto max-w-xl text-[clamp(2rem,5vw,4.5rem)] font-extralight leading-[1.1] tracking-tight"
            style={{ fontFamily: "var(--font-syne)" }}
          >
            Quatre étapes.
            <br />
            <span className="text-foreground/30">Zéro friction.</span>
          </h2>
        </motion.div>

        {/* Steps */}
        <div className="relative grid gap-8 md:grid-cols-2 lg:grid-cols-4">
          {/* Connecting line (desktop) */}
          <div className="pointer-events-none absolute top-16 right-0 left-0 hidden h-[1px] bg-gradient-to-r from-transparent via-foreground/[0.08] to-transparent lg:block" />

          {steps.map((step, i) => {
            const Icon = step.icon;
            return (
              <motion.div
                key={step.number}
                className="group relative"
                initial={{ opacity: 0, y: 40 }}
                whileInView={{ opacity: 1, y: 0 }}
                viewport={{ once: true, margin: "-80px" }}
                transition={{
                  duration: 0.6,
                  delay: i * 0.12,
                  ease: [0.16, 1, 0.3, 1],
                }}
              >
                {/* Step number + icon */}
                <div className="relative mb-8 flex h-16 w-16 items-center justify-center rounded-2xl border border-foreground/[0.06] bg-background-secondary/50 transition-all duration-500 group-hover:border-accent/20 group-hover:bg-accent/5">
                  <Icon
                    size={22}
                    strokeWidth={1.5}
                    className="text-foreground/40 transition-colors duration-500 group-hover:text-accent"
                  />
                  <span
                    className="absolute -top-2.5 -right-2.5 flex h-6 w-6 items-center justify-center rounded-full bg-background text-[10px] font-light text-foreground/30 ring-1 ring-foreground/[0.06]"
                    style={{ fontFamily: "var(--font-jakarta)" }}
                  >
                    {step.number}
                  </span>
                </div>

                {/* Content */}
                <h3
                  className="mb-3 text-lg font-light tracking-tight"
                  style={{ fontFamily: "var(--font-syne)" }}
                >
                  {step.title}
                </h3>
                <p
                  className="text-sm font-light leading-relaxed text-foreground/45"
                  style={{ fontFamily: "var(--font-jakarta)" }}
                >
                  {step.description}
                </p>
              </motion.div>
            );
          })}
        </div>
      </div>
    </section>
  );
}
