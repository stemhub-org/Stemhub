"use client";

import Link from "next/link";
import { motion } from "framer-motion";
import { CONTAINER, EASE_OUT, LABEL } from "./landing";

const columns = [
  {
    title: "Product",
    links: [
      { label: "How it works", href: "#how" },
      { label: "Features", href: "#features" },
      { label: "Pricing", href: "#pricing" },
    ],
  },
  {
    title: "Account",
    links: [
      { label: "Sign in", href: "/login" },
      { label: "Create an account", href: "/register" },
    ],
  },
  {
    title: "Legal",
    links: [
      { label: "Privacy", href: "#" },
      { label: "Terms", href: "#" },
    ],
  },
];

const WORDMARK = "STEMHUB";

export default function Footer() {
  return (
    <footer className="bg-ink text-paper">
      <div className={`${CONTAINER} @container flex flex-col gap-12 pt-14 pb-8`}>
        <div className="grid grid-cols-1 gap-8 text-[15px] sm:grid-cols-3">
          {columns.map((column) => (
            <div key={column.title} className="flex flex-col gap-2">
              <span className={`${LABEL} text-lime`}>{column.title}</span>
              {column.links.map((link) => (
                <Link key={link.label} href={link.href} className="self-start hover:underline">
                  {link.label}
                </Link>
              ))}
            </div>
          ))}
        </div>

        {/* Letters rise out of the baseline one after another, like a fader push. */}
        <motion.p
          aria-label="StemHub"
          className="font-display flex text-[12.5cqw] leading-[0.8] font-extrabold tracking-[-0.05em]"
          initial="hidden"
          whileInView="visible"
          viewport={{ once: true, margin: "-5% 0px" }}
          transition={{ staggerChildren: 0.05 }}
        >
          {WORDMARK.split("").map((letter, i) => (
            <span key={i} aria-hidden="true" className="inline-block overflow-hidden pb-[0.04em]">
              <motion.span
                className="inline-block"
                variants={{ hidden: { y: "100%" }, visible: { y: "0%" } }}
                transition={{ duration: 0.9, ease: EASE_OUT }}
              >
                {letter}
              </motion.span>
            </span>
          ))}
          <sup aria-hidden="true" className="mt-[0.1em] text-[0.15em]">
            ®
          </sup>
        </motion.p>

        <span className="text-xs text-paper-soft">© 2026 StemHub. Make it louder together.</span>
      </div>
    </footer>
  );
}
