"use client";

import { useState } from "react";
import Link from "next/link";
import { AnimatePresence, motion, useMotionValueEvent, useScroll } from "framer-motion";
import { Menu, X } from "lucide-react";
import { BrandTile, Wordmark } from "./BrandMark";
import { CONTAINER, EASE_OUT, EASE_SNAP, LABEL, WipeLink } from "./landing";

const navLinks = [
  { label: "How it works", href: "#how" },
  { label: "The diff", href: "#product" },
  { label: "Features", href: "#features" },
  { label: "Pricing", href: "#pricing" },
];

export default function Navbar() {
  const [mobileOpen, setMobileOpen] = useState(false);
  const [hidden, setHidden] = useState(false);
  const { scrollY } = useScroll();

  // Gets out of the way while reading down, comes back as soon as you scroll up.
  useMotionValueEvent(scrollY, "change", (latest) => {
    const previous = scrollY.getPrevious() ?? 0;
    setHidden(latest > previous && latest > 160 && !mobileOpen);
  });

  return (
    <motion.header
      initial={{ y: "-100%" }}
      animate={{ y: hidden ? "-100%" : "0%" }}
      transition={{ duration: 0.5, ease: hidden ? EASE_SNAP : EASE_OUT }}
      className="fixed inset-x-0 top-0 z-50 bg-paper/95 text-ink backdrop-blur-sm"
    >
      <nav className={`${CONTAINER} flex items-center justify-between gap-6 border-b-2 border-ink py-4`}>
        <Link href="/" className="flex items-center gap-2.5" aria-label="StemHub home">
          <BrandTile className="h-9 w-9" />
          <Wordmark className="text-xl" />
        </Link>

        <div className={`hidden items-center gap-7 md:flex ${LABEL}`}>
          {navLinks.map((link) => (
            <a key={link.href} href={link.href} className="group relative py-2">
              {link.label}
              <span className="absolute inset-x-0 bottom-1 h-0.5 origin-left scale-x-0 bg-ink transition-transform duration-300 ease-snap group-hover:scale-x-100" />
            </a>
          ))}
        </div>

        <div className="hidden items-center gap-2 md:flex">
          <Link href="/login" className="px-3 py-3 text-sm font-medium hover:underline">
            Sign in
          </Link>
          <WipeLink href="/register" variant="lime" size="sm">
            Drop a stem +
          </WipeLink>
        </div>

        <button
          type="button"
          className="flex h-11 w-11 items-center justify-center md:hidden"
          onClick={() => setMobileOpen((open) => !open)}
          aria-label={mobileOpen ? "Close menu" : "Open menu"}
          aria-expanded={mobileOpen}
        >
          {mobileOpen ? <X size={22} /> : <Menu size={22} />}
        </button>
      </nav>

      <AnimatePresence>
        {mobileOpen && (
          <motion.div
            initial={{ height: 0 }}
            animate={{ height: "auto" }}
            exit={{ height: 0 }}
            transition={{ duration: 0.4, ease: EASE_SNAP }}
            className="overflow-hidden border-b-2 border-ink bg-paper md:hidden"
          >
            <div className={`${CONTAINER} flex flex-col gap-1 py-4`}>
              {navLinks.map((link) => (
                <a
                  key={link.href}
                  href={link.href}
                  onClick={() => setMobileOpen(false)}
                  className="font-display py-2 text-3xl font-extrabold uppercase tracking-[-0.03em]"
                >
                  {link.label}
                </a>
              ))}
              <div className="mt-4 flex gap-2">
                <WipeLink href="/login" variant="outline" className="flex-1">
                  Sign in
                </WipeLink>
                <WipeLink href="/register" variant="ink" className="flex-1">
                  Drop a stem +
                </WipeLink>
              </div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </motion.header>
  );
}
