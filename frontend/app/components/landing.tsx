"use client";

import Link from "next/link";
import type { ReactNode } from "react";
import { motion } from "framer-motion";

// Motion vocabulary of the landing page: "out" settles content in, "snap" is the
// hard hit used for stamps and blocks (identity system: "built to hit").
export const EASE_OUT = [0.16, 1, 0.3, 1] as const;
export const EASE_SNAP = [0.7, 0, 0.2, 1] as const;

export const CONTAINER = "mx-auto w-full max-w-[1320px] px-4 sm:px-8 lg:px-12";
const LABEL_STYLE = "font-bold uppercase tracking-[0.04em]";
export const LABEL = `text-xs ${LABEL_STYLE}`;
export const SECTION_TITLE =
  "font-display text-[clamp(2.25rem,5.6vw,4.75rem)] font-extrabold uppercase leading-[0.9] tracking-[-0.04em]";

// Section title whose words rise out of a mask once, when it scrolls into view.
export function RevealHeading({
  text,
  as: Tag = "h2",
  className = SECTION_TITLE,
}: {
  text: string;
  as?: "h1" | "h2";
  className?: string;
}) {
  return (
    <Tag className={className}>
      <span className="sr-only">{text}</span>
      <motion.span
        aria-hidden="true"
        initial="hidden"
        whileInView="visible"
        viewport={{ once: true, margin: "-12% 0px" }}
        transition={{ staggerChildren: 0.06 }}
      >
        {text.split(" ").map((word, i) => (
          <span key={`${word}-${i}`} className="inline-block overflow-hidden pr-[0.24em] pb-[0.06em] align-top">
            <motion.span
              className="inline-block"
              variants={{ hidden: { y: "105%" }, visible: { y: "0%" } }}
              transition={{ duration: 0.8, ease: EASE_OUT }}
            >
              {word}
            </motion.span>
          </span>
        ))}
      </motion.span>
    </Tag>
  );
}

type Variant = "ink" | "lime" | "outline" | "outline-paper";

// Each variant wipes a second colour up from the bottom on hover.
const VARIANTS: Record<Variant, { base: string; wipe: string; hover: string }> = {
  ink: { base: "bg-ink text-paper", wipe: "bg-lime", hover: "group-hover:text-ink" },
  lime: { base: "bg-lime text-ink", wipe: "bg-ink", hover: "group-hover:text-paper" },
  outline: { base: "border-2 border-ink text-ink", wipe: "bg-ink", hover: "group-hover:text-paper" },
  "outline-paper": { base: "border-2 border-paper text-paper", wipe: "bg-paper", hover: "group-hover:text-ink" },
};

const SIZES = {
  sm: "h-11 px-4 text-xs",
  md: "h-[54px] px-6 text-[13px]",
  lg: "h-[60px] px-7 text-[13px]",
};

export function WipeLink({
  href,
  variant,
  size = "md",
  children,
  className = "",
}: {
  href: string;
  variant: Variant;
  size?: keyof typeof SIZES;
  children: ReactNode;
  className?: string;
}) {
  const v = VARIANTS[variant];
  return (
    <Link
      href={href}
      className={`group relative inline-flex items-center justify-center overflow-hidden ${SIZES[size]} ${LABEL_STYLE} focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-current ${v.base} ${className}`}
    >
      <span
        aria-hidden="true"
        className={`absolute inset-0 translate-y-full transition-transform duration-500 ease-snap group-hover:translate-y-0 motion-reduce:transition-none ${v.wipe}`}
      />
      <span className={`relative transition-colors duration-500 ease-snap motion-reduce:transition-none ${v.hover}`}>
        {children}
      </span>
    </Link>
  );
}
