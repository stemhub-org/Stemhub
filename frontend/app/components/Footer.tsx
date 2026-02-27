"use client";

import { useRef } from "react";
import { motion, useScroll, useTransform } from "framer-motion";

export default function Footer() {
  const sectionRef = useRef<HTMLDivElement>(null);

  const { scrollYProgress } = useScroll({
    target: sectionRef,
    offset: ["start end", "end end"],
  });

  const opacity = useTransform(scrollYProgress, [0, 0.5, 1], [0, 0.3, 0.08]);
  const y = useTransform(scrollYProgress, [0, 1], [100, 0]);

  return (
    <footer
      ref={sectionRef}
      className="relative overflow-hidden px-6 pt-32 pb-0"
    >
      {/* Top section with links */}
      <div className="mx-auto max-w-7xl">
        <motion.div
          className="mb-24 grid gap-12 md:grid-cols-3"
          initial={{ opacity: 0, y: 40 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.8, ease: [0.16, 1, 0.3, 1] }}
        >
          <div>
            <h3
              className="mb-4 text-sm font-light uppercase tracking-[0.2em] text-foreground/30"
              style={{ fontFamily: "var(--font-jakarta)" }}
            >
              Produit
            </h3>
            <ul className="space-y-3">
              {["Fonctionnalités", "Tarifs", "Changelog", "Roadmap"].map((item) => (
                <li key={item}>
                  <a
                    href="#"
                    className="text-sm font-light text-foreground/50 transition-colors duration-300 hover:text-foreground"
                    style={{ fontFamily: "var(--font-jakarta)" }}
                  >
                    {item}
                  </a>
                </li>
              ))}
            </ul>
          </div>

          <div>
            <h3
              className="mb-4 text-sm font-light uppercase tracking-[0.2em] text-foreground/30"
              style={{ fontFamily: "var(--font-jakarta)" }}
            >
              Ressources
            </h3>
            <ul className="space-y-3">
              {["Documentation", "API", "Guides", "Communauté"].map((item) => (
                <li key={item}>
                  <a
                    href="#"
                    className="text-sm font-light text-foreground/50 transition-colors duration-300 hover:text-foreground"
                    style={{ fontFamily: "var(--font-jakarta)" }}
                  >
                    {item}
                  </a>
                </li>
              ))}
            </ul>
          </div>

          <div>
            <h3
              className="mb-4 text-sm font-light uppercase tracking-[0.2em] text-foreground/30"
              style={{ fontFamily: "var(--font-jakarta)" }}
            >
              Entreprise
            </h3>
            <ul className="space-y-3">
              {["À propos", "Blog", "Carrières", "Contact"].map((item) => (
                <li key={item}>
                  <a
                    href="#"
                    className="text-sm font-light text-foreground/50 transition-colors duration-300 hover:text-foreground"
                    style={{ fontFamily: "var(--font-jakarta)" }}
                  >
                    {item}
                  </a>
                </li>
              ))}
            </ul>
          </div>
        </motion.div>

        {/* Divider */}
        <div className="h-[1px] w-full bg-foreground/[0.06]" />

        {/* Bottom bar */}
        <div className="flex items-center justify-between py-8">
          <p
            className="text-xs font-light text-foreground/30"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            &copy; 2025 StemHub. Tous droits réservés.
          </p>
          <div className="flex gap-6">
            {["Confidentialité", "CGU"].map((item) => (
              <a
                key={item}
                href="#"
                className="text-xs font-light text-foreground/30 transition-colors duration-300 hover:text-foreground/60"
                style={{ fontFamily: "var(--font-jakarta)" }}
              >
                {item}
              </a>
            ))}
          </div>
        </div>
      </div>

      {/* Giant STEMHUB text */}
      <motion.div
        className="pointer-events-none relative -mb-[0.2em] select-none overflow-hidden"
        style={{ opacity, y }}
      >
        <h2
          className="whitespace-nowrap text-center font-extralight leading-[0.85] tracking-tighter text-foreground"
          style={{
            fontFamily: "var(--font-syne)",
            fontSize: "clamp(6rem, 20vw, 22rem)",
          }}
        >
          STEMHUB
        </h2>
      </motion.div>
    </footer>
  );
}
