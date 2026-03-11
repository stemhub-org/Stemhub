"use client";

import { useEffect, useState } from "react";
import { motion } from "framer-motion";
import { Menu, Sun } from "lucide-react";
import Link from "next/link";
import { useTheme } from "next-themes";

const navLinks = [
  { label: "Product", href: "#product" },
  { label: "Features", href: "#features" },
  { label: "Pricing", href: "#pricing" },
];

export default function Navbar() {
  const [mobileOpen, setMobileOpen] = useState(false);
  const [scrolled, setScrolled] = useState(false);
  const { resolvedTheme, setTheme } = useTheme();
  const isDark = resolvedTheme === "dark";

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
          ? "border-b border-accent/10 bg-background/80 backdrop-blur-xl dark:bg-background/90"
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
              className="text-sm font-light tracking-wide text-foreground transition-colors duration-300 hover:opacity-80"
              style={{ fontFamily: "var(--font-jakarta)" }}
            >
              {link.label}
            </a>
          ))}
        </div>

        <div className="hidden items-center gap-4 md:flex">
          <Link
            href="/login"
            className="text-sm font-light tracking-wide text-foreground transition-colors duration-300 hover:opacity-80"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Log in
          </Link>
          <Link
            href="/register"
            className="rounded-full border border-foreground/20 px-6 py-2.5 text-sm font-light tracking-wide text-foreground transition-all duration-300 hover:bg-foreground/5 hover:border-foreground/30"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Get started
          </Link>
          <button
            type="button"
            onClick={() => setTheme(isDark ? "light" : "dark")}
            className="p-2 rounded-full hover:bg-foreground/5 text-foreground transition-colors"
            aria-label="Toggle theme"
            title={isDark ? "Light mode" : "Dark mode"}
          >
            <Sun size={18} />
          </button>
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
          className="border-b border-accent/10 bg-background/90 dark:bg-background/95 px-6 pb-6 backdrop-blur-xl md:hidden"
        >
          {navLinks.map((link) => (
            <a
              key={link.label}
              href={link.href}
              className="block py-3 text-sm font-light text-foreground"
              onClick={() => setMobileOpen(false)}
            >
              {link.label}
            </a>
          ))}
          <div className="mt-3 flex flex-col gap-3">
            <Link
              href="/login"
              className="text-sm font-light text-foreground"
              onClick={() => setMobileOpen(false)}
            >
              Log in
            </Link>
            <Link
              href="/register"
              className="inline-block rounded-full border border-foreground/20 text-foreground px-6 py-2.5 text-center text-sm font-light"
              onClick={() => setMobileOpen(false)}
            >
              Get started
            </Link>
            <button
              type="button"
              onClick={() => { setTheme(isDark ? "light" : "dark"); setMobileOpen(false); }}
              className="flex items-center gap-2 text-sm font-light text-foreground"
              aria-label="Toggle theme"
            >
              <Sun size={18} /> {isDark ? "Light mode" : "Dark mode"}
            </button>
          </div>
        </motion.div>
      )}
    </motion.header>
  );
}
