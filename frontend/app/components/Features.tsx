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
    title: "Branches audio",
    description:
      "Créez des versions alternatives de vos morceaux sans jamais toucher à l'original. Expérimentez librement, fusionnez le meilleur.",
    size: "large",
  },
  {
    icon: History,
    title: "Timeline complète",
    description:
      "Naviguez dans l'historique de chaque piste. Restaurez n'importe quel état en un clic.",
    size: "default",
  },
  {
    icon: Shield,
    title: "Chiffrement E2E",
    description:
      "Vos stems ne quittent jamais votre contrôle. Chiffrement de bout en bout par défaut.",
    size: "default",
  },
  {
    icon: Layers,
    title: "Diff par piste",
    description:
      "Comparez deux versions et voyez exactement ce qui a changé — piste par piste, effet par effet.",
    size: "default",
  },
  {
    icon: Users,
    title: "Collaboration temps réel",
    description:
      "Partagez une branche avec votre équipe. Chacun travaille sur sa version, puis fusionnez le meilleur.",
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
      className={`group relative overflow-hidden rounded-2xl border border-foreground/[0.06] bg-background-secondary/50 transition-all duration-500 hover:scale-[1.02] hover:border-accent/20 ${
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
          className="text-[0.85rem] font-light leading-[1.7] text-foreground/45"
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
            Fonctionnalités
          </p>
          <h2
            className="mx-auto max-w-xl text-[clamp(2rem,5vw,4.5rem)] font-extralight leading-[1.1] tracking-tight"
            style={{ fontFamily: "var(--font-syne)" }}
          >
            Tout ce qu&apos;il vous faut.
            <br />
            <span className="text-foreground/30">Rien de plus.</span>
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
