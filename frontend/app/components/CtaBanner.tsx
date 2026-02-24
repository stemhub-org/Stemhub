"use client";

import { motion } from "framer-motion";
import { ArrowRight } from "lucide-react";
import Link from "next/link";

export default function CtaBanner() {
  return (
    <section className="relative overflow-hidden px-6 py-32 md:px-12">
      {/* Background glow */}
      <div className="pointer-events-none absolute inset-0 flex items-center justify-center">
        <div
          className="h-[500px] w-[700px] rounded-full opacity-25 blur-[140px]"
          style={{
            background:
              "radial-gradient(circle, rgba(156,87,223,0.35) 0%, transparent 70%)",
          }}
        />
      </div>

      <div className="relative z-10 mx-auto max-w-3xl text-center">
        <motion.h2
          className="mb-6 text-[clamp(2rem,5vw,4rem)] font-extralight leading-[1.1] tracking-tight"
          style={{ fontFamily: "var(--font-syne)" }}
          initial={{ opacity: 0, y: 40 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.8, ease: [0.16, 1, 0.3, 1] }}
        >
          Prêt à ne plus jamais
          <br />
          <span className="text-accent">perdre une idée ?</span>
        </motion.h2>

        <motion.p
          className="mx-auto mb-10 max-w-md text-base font-light leading-relaxed text-foreground/45"
          style={{ fontFamily: "var(--font-jakarta)" }}
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.8, delay: 0.15, ease: [0.16, 1, 0.3, 1] }}
        >
          Rejoignez les producteurs qui ne laissent plus le hasard décider
          de leurs projets.
        </motion.p>

        <motion.div
          className="flex flex-col items-center justify-center gap-4 sm:flex-row"
          initial={{ opacity: 0, y: 20 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.8, delay: 0.3, ease: [0.16, 1, 0.3, 1] }}
        >
          <Link
            href="/register"
            className="flex items-center gap-2 rounded-full bg-foreground px-8 py-4 text-sm font-light tracking-wide text-background transition-all duration-300 hover:bg-accent"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            Créer un compte
            <ArrowRight size={16} strokeWidth={1.5} />
          </Link>

          <a
            href="#produit"
            className="rounded-full border border-foreground/10 px-8 py-4 text-sm font-light tracking-wide text-foreground/60 transition-all duration-300 hover:border-accent/30 hover:text-accent"
            style={{ fontFamily: "var(--font-jakarta)" }}
          >
            En savoir plus
          </a>
        </motion.div>
      </div>
    </section>
  );
}
