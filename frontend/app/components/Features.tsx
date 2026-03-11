"use client";

import { useRef } from "react";
import { motion } from "framer-motion";
import {
  GitBranch,
  History,
  Shield,
  Layers,
  Zap,
  Users,
} from "lucide-react";
import type { LucideIcon } from "lucide-react";

interface Feature {
  icon: LucideIcon;
  title: string;
  description: string;
  size: "large" | "default";
}

const features: Feature[] = [
  {
    icon: GitBranch,
    title: "Audio versions",
    description:
      "Create alternate takes of your tracks without ever touching the original. Experiment freely, merge the best.",
    size: "large",
  },
  {
    icon: History,
    title: "Full timeline",
    description:
      "Navigate the history of every track. Restore any state in a single click.",
    size: "default",
  },
  {
    icon: Shield,
    title: "End‑to‑end encryption",
    description:
      "Your stems never leave your control. End‑to‑end encryption by default.",
    size: "default",
  },
  {
    icon: Layers,
    title: "Track‑by‑track diff",
    description:
      "Compare two versions and see exactly what changed — track by track, effect by effect.",
    size: "default",
  },
  {
    icon: Users,
    title: "Real‑time collaboration",
    description:
      "Share a version with your team. Everyone works on their take, then you merge the best.",
    size: "default",
  },
  {
    icon: Zap,
    title: "Plugin natif",
    description:
      "Intégré directement dans FL Studio et Ableton. Commitez sans quitter votre DAW. Zéro friction.",
    size: "large",
  },
];

function FeatureCard({
  feature,
  index,
}: {
  feature: Feature;
  index: number;
}) {
  const cardRef = useRef<HTMLDivElement>(null);

  const handleMouseMove = (e: React.MouseEvent) => {
    if (!cardRef.current) return;
    const rect = cardRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    cardRef.current.style.setProperty("--mouse-x", `${x}px`);
    cardRef.current.style.setProperty("--mouse-y", `${y}px`);
  };

  const Icon = feature.icon;
  const isLarge = feature.size === "large";

  return (
    <motion.div
      ref={cardRef}
      className={`group relative overflow-hidden rounded-2xl border border-accent/20 bg-accent/10 dark:bg-background-tertiary/50 transition-all duration-500 hover:scale-[1.02] hover:border-accent/40 ${
        isLarge ? "col-span-1 md:col-span-2 p-10" : "col-span-1 p-8"
      }`}
      onMouseMove={handleMouseMove}
      initial={{ opacity: 0, y: 40 }}
      whileInView={{ opacity: 1, y: 0 }}
      viewport={{ once: true, margin: "-80px" }}
      transition={{
        duration: 0.6,
        delay: index * 0.08,
        ease: [0.16, 1, 0.3, 1],
      }}
    >
      {/* Glow effect following mouse */}
      <div
        className="pointer-events-none absolute inset-0 opacity-0 transition-opacity duration-500 group-hover:opacity-100"
        style={{
          background:
            "radial-gradient(400px circle at var(--mouse-x) var(--mouse-y), rgba(156,87,223,0.08), transparent 40%)",
        }}
      />

      <div className={`relative z-10 ${isLarge ? "max-w-md" : ""}`}>
        <div className="mb-5 flex h-11 w-11 items-center justify-center rounded-xl border border-accent/10 bg-accent/[0.06]">
          <Icon size={20} strokeWidth={1.5} className="text-accent" />
        </div>

        <h3
          className={`mb-3 font-light tracking-tight ${isLarge ? "text-xl" : "text-lg"}`}
          style={{ fontFamily: "var(--font-syne)" }}
        >
          {feature.title}
        </h3>

        <p
          className="text-[0.85rem] font-light leading-[1.7] text-foreground"
          style={{ fontFamily: "var(--font-jakarta)" }}
        >
          {feature.description}
        </p>
      </div>
    </motion.div>
  );
}

export default function Features() {
  return (
    <section id="features" className="relative px-6 py-32 md:px-12">
      <div className="mx-auto max-w-7xl">
        {/* Section header */}
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
            Features
          </p>
          <h2
            className="mx-auto max-w-xl text-[clamp(2rem,5vw,4.5rem)] font-extralight leading-[1.1] tracking-tight"
            style={{ fontFamily: "var(--font-syne)" }}
          >
            Everything you need.
            <br />
            <span className="text-foreground">Nothing more.</span>
          </h2>
        </motion.div>

        {/* Bento Grid — row 1: large + small + small, row 2: small + small + large */}
        <div className="grid grid-cols-1 gap-4 md:grid-cols-3">
          {/* Row 1 */}
          <FeatureCard feature={features[0]} index={0} />
          <FeatureCard feature={features[1]} index={1} />
          <FeatureCard feature={features[2]} index={2} />

          {/* Row 2 */}
          <FeatureCard feature={features[3]} index={3} />
          <FeatureCard feature={features[4]} index={4} />
          <FeatureCard feature={features[5]} index={5} />
        </div>
      </div>
    </section>
  );
}
