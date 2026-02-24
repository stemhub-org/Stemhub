"use client";

import { useEffect, useRef, useState } from "react";
import {
  motion,
  useScroll,
  useTransform,
  type MotionValue,
} from "framer-motion";
import { GitBranch, GitCommit, GitMerge } from "lucide-react";

const terminalLines = [
  { prefix: "$", text: "stemhub status", color: "text-foreground/80" },
  { prefix: ">", text: 'Piste "Kick" modifiée...', color: "text-accent" },
  { prefix: ">", text: '"Synth Lead" ajouté en V2', color: "text-accent" },
  { prefix: ">", text: "Mix Bus compressé — gain +2dB", color: "text-foreground/50" },
  { prefix: "$", text: "stemhub commit -m 'Session nocturne'", color: "text-foreground/80" },
  { prefix: ">", text: "Commit #a7f3d2 enregistré.", color: "text-green-500" },
  { prefix: "$", text: "stemhub diff v1..v2", color: "text-foreground/80" },
  { prefix: "+", text: "Synth Lead — 808 pattern alterné", color: "text-green-500" },
  { prefix: "-", text: "Hi-hat roll supprimé", color: "text-red-400" },
  { prefix: ">", text: "3 fichiers modifiés, 1 ajouté", color: "text-foreground/50" },
];

function useMotionValueState(value: MotionValue<number>): number {
  const [current, setCurrent] = useState(0);

  useEffect(() => {
    const unsubscribe = value.on("change", (v) => {
      setCurrent(v);
    });
    return unsubscribe;
  }, [value]);

  return current;
}

function TerminalLine({
  line,
  index,
  progress,
}: {
  line: (typeof terminalLines)[number];
  index: number;
  progress: number;
}) {
  const lineStart = index / terminalLines.length;
  const lineEnd = (index + 0.8) / terminalLines.length;
  const lineProgress = Math.max(0, Math.min(1, (progress - lineStart) / (lineEnd - lineStart)));

  const visibleChars = Math.floor(lineProgress * line.text.length);
  const displayText = line.text.substring(0, visibleChars);
  const showCursor = lineProgress > 0 && lineProgress < 1;

  if (lineProgress <= 0) return null;

  return (
    <div className="flex gap-2 font-mono text-sm leading-relaxed md:text-base">
      <span
        className={
          line.prefix === "+"
            ? "text-green-500"
            : line.prefix === "-"
              ? "text-red-400"
              : "text-foreground/30"
        }
      >
        {line.prefix}
      </span>
      <span className={line.color}>
        {displayText}
        {showCursor && (
          <span className="ml-0.5 inline-block h-4 w-[2px] animate-pulse bg-accent align-middle" />
        )}
      </span>
    </div>
  );
}

function TerminalContent({ progress }: { progress: MotionValue<number> }) {
  const currentProgress = useMotionValueState(progress);

  return (
    <>
      {terminalLines.map((line, i) => (
        <TerminalLine key={i} line={line} index={i} progress={currentProgress} />
      ))}
    </>
  );
}

export default function DiffSection() {
  const sectionRef = useRef<HTMLDivElement>(null);

  const { scrollYProgress } = useScroll({
    target: sectionRef,
    offset: ["start end", "end start"],
  });

  const terminalProgress = useTransform(scrollYProgress, [0.15, 0.75], [0, 1]);
  const floatY1 = useTransform(scrollYProgress, [0, 1], [60, -60]);
  const floatY2 = useTransform(scrollYProgress, [0, 1], [100, -100]);
  const floatY3 = useTransform(scrollYProgress, [0, 1], [40, -80]);

  return (
    <section
      ref={sectionRef}
      id="produit"
      className="relative min-h-[120vh] overflow-hidden px-6 py-32 md:px-12"
    >
      <div className="mx-auto max-w-7xl">
        {/* Section header */}
        <motion.div
          className="mb-20"
          initial={{ opacity: 0, y: 40 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.8, ease: [0.16, 1, 0.3, 1] }}
        >
          <p
            className="mb-4 text-sm font-light uppercase tracking-[0.2em] text-accent"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Le Diff
          </p>
          <h2
            className="max-w-2xl text-[clamp(2rem,5vw,4.5rem)] font-extralight leading-[1.1] tracking-tight"
            style={{ fontFamily: "var(--font-syne)" }}
          >
            Chaque changement,
            <br />
            <span className="gradient-text">capturé.</span>
          </h2>
        </motion.div>

        {/* Asymmetric layout */}
        <div className="grid gap-12 lg:grid-cols-12">
          {/* Terminal */}
          <motion.div
            className="lg:col-span-7"
            initial={{ opacity: 0, x: -40 }}
            whileInView={{ opacity: 1, x: 0 }}
            viewport={{ once: true, margin: "-100px" }}
            transition={{ duration: 0.8, ease: [0.16, 1, 0.3, 1] }}
          >
            <div className="overflow-hidden rounded-2xl border border-foreground/[0.06] bg-background-secondary/80 backdrop-blur-sm">
              {/* Terminal header */}
              <div className="flex items-center gap-2 border-b border-foreground/[0.06] px-5 py-3.5">
                <div className="h-2.5 w-2.5 rounded-full bg-foreground/10" />
                <div className="h-2.5 w-2.5 rounded-full bg-foreground/10" />
                <div className="h-2.5 w-2.5 rounded-full bg-foreground/10" />
                <span className="ml-3 font-mono text-xs text-foreground/30">
                  stemhub — terminal
                </span>
              </div>

              {/* Terminal content */}
              <div className="min-h-[320px] space-y-1.5 p-6">
                <TerminalContent progress={terminalProgress} />
              </div>
            </div>
          </motion.div>

          {/* Floating elements with parallax */}
          <div className="relative hidden lg:col-span-5 lg:block">
            <motion.div
              className="absolute top-0 right-0 rounded-2xl border border-foreground/[0.06] bg-background-secondary/60 p-6 backdrop-blur-sm"
              style={{ y: floatY1 }}
            >
              <GitBranch size={20} strokeWidth={1.5} className="mb-3 text-accent" />
              <p className="text-sm font-light text-foreground/60" style={{ fontFamily: "var(--font-jakarta)" }}>
                Branches illimitées
              </p>
              <p className="mt-1 text-xs text-foreground/30" style={{ fontFamily: "var(--font-jakarta)" }}>
                Explorez sans risque
              </p>
            </motion.div>

            <motion.div
              className="absolute top-40 left-8 rounded-2xl border border-foreground/[0.06] bg-background-secondary/60 p-6 backdrop-blur-sm"
              style={{ y: floatY2 }}
            >
              <GitCommit size={20} strokeWidth={1.5} className="mb-3 text-accent" />
              <p className="text-sm font-light text-foreground/60" style={{ fontFamily: "var(--font-jakarta)" }}>
                Commits granulaires
              </p>
              <p className="mt-1 text-xs text-foreground/30" style={{ fontFamily: "var(--font-jakarta)" }}>
                Piste par piste
              </p>
            </motion.div>

            <motion.div
              className="absolute top-80 right-12 rounded-2xl border border-foreground/[0.06] bg-background-secondary/60 p-6 backdrop-blur-sm"
              style={{ y: floatY3 }}
            >
              <GitMerge size={20} strokeWidth={1.5} className="mb-3 text-accent" />
              <p className="text-sm font-light text-foreground/60" style={{ fontFamily: "var(--font-jakarta)" }}>
                Merge intelligent
              </p>
              <p className="mt-1 text-xs text-foreground/30" style={{ fontFamily: "var(--font-jakarta)" }}>
                Fusionnez vos sessions
              </p>
            </motion.div>
          </div>
        </div>
      </div>
    </section>
  );
}
