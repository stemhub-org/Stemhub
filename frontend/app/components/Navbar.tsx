"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { Menu } from "lucide-react";
import Link from "next/link";

const navLinks = [
  { label: "Produit", href: "#produit" },
  { label: "Fonctionnalités", href: "#features" },
  { label: "Tarifs", href: "#tarifs" },
];

export default function Navbar() {
  const [mobileOpen, setMobileOpen] = useState(false);
  const [scrolled, setScrolled] = useState(false);

  useEffect(() => {
    const handleScroll = () => {
      setScrolled(window.scrollY > 50);
    };
    window.addEventListener("scroll", handleScroll, { passive: true });
    return () => window.removeEventListener("scroll", handleScroll);
  }, []);

  return (
    <motion.header
      initial={{ y: -20, opacity: 0 }}
      animate={{ y: 0, opacity: 1 }}
      transition={{ duration: 0.8, ease: [0.16, 1, 0.3, 1] }}
      className={`fixed top-0 left-0 z-50 w-full transition-all duration-500 ${
        scrolled
          ? "border-b border-foreground/[0.04] bg-background/70 backdrop-blur-xl"
          : "bg-transparent"
      }`}
    >
      <nav className="mx-auto flex max-w-7xl items-center justify-between px-6 py-5 md:px-12">
        <Link href="/" className="text-xl font-medium tracking-tight" style={{ fontFamily: "var(--font-syne)" }}>
          stemhub<span className="text-accent">.</span>
        </Link>

        <div className="hidden items-center gap-10 md:flex">
          {navLinks.map((link) => (
            <a
              key={link.label}
              href={link.href}
              className="text-sm font-light tracking-wide text-foreground/60 transition-colors duration-300 hover:text-foreground"
              style={{ fontFamily: "var(--font-jakarta)" }}
            >
              {link.label}
            </a>
          ))}
        </div>

        <div className="hidden items-center gap-4 md:flex">
          <Link
            href="/login"
            className="text-sm font-light tracking-wide text-foreground/60 transition-colors duration-300 hover:text-foreground"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Connexion
          </Link>
          <Link
            href="/register"
            className="rounded-full border border-foreground/10 px-6 py-2.5 text-sm font-light tracking-wide transition-all duration-300 hover:border-accent hover:text-accent"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Commencer
          </Link>
        </div>

        <button
          className="md:hidden"
          onClick={() => setMobileOpen(!mobileOpen)}
          aria-label="Menu"
        >
          <Menu size={20} strokeWidth={1.5} />
        </button>
      </nav>

      {mobileOpen && (
        <motion.div
          initial={{ opacity: 0, y: -10 }}
          animate={{ opacity: 1, y: 0 }}
          className="border-b border-foreground/5 bg-background/90 px-6 pb-6 backdrop-blur-xl md:hidden"
        >
          {navLinks.map((link) => (
            <a
              key={link.label}
              href={link.href}
              className="block py-3 text-sm font-light text-foreground/60"
              onClick={() => setMobileOpen(false)}
            >
              {link.label}
            </a>
          ))}
          <div className="mt-3 flex flex-col gap-3">
            <Link
              href="/login"
              className="text-sm font-light text-foreground/60"
              onClick={() => setMobileOpen(false)}
            >
              Connexion
            </Link>
            <Link
              href="/register"
              className="inline-block rounded-full border border-foreground/10 px-6 py-2.5 text-center text-sm font-light"
              onClick={() => setMobileOpen(false)}
            >
              Commencer
            </Link>
          </div>
        </motion.div>
      )}
    </motion.header>
  );
}
