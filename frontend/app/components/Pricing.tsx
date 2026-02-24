"use client";

import { motion } from "framer-motion";
import { Check } from "lucide-react";
import Link from "next/link";

const plans = [
  {
    name: "Solo",
    price: "0",
    description: "Pour les producteurs indépendants qui débutent.",
    features: [
      "3 projets",
      "500 Mo de stockage",
      "Historique 30 jours",
      "Plugin FL Studio & Ableton",
    ],
    cta: "Commencer gratuitement",
    highlighted: false,
  },
  {
    name: "Pro",
    price: "12",
    description: "Pour les producteurs sérieux qui veulent tout garder.",
    features: [
      "Projets illimités",
      "50 Go de stockage",
      "Historique illimité",
      "Branches & merge",
      "Collaboration (5 membres)",
      "Support prioritaire",
    ],
    cta: "Essai gratuit 14 jours",
    highlighted: true,
  },
  {
    name: "Studio",
    price: "39",
    description: "Pour les labels et les équipes de production.",
    features: [
      "Tout de Pro",
      "Stockage illimité",
      "Membres illimités",
      "SSO & permissions",
      "API & webhooks",
      "Account manager dédié",
    ],
    cta: "Contacter l'équipe",
    highlighted: false,
  },
];

export default function Pricing() {
  return (
    <section id="tarifs" className="relative px-6 py-32 md:px-12">
      <div className="mx-auto max-w-7xl">
        {/* Header */}
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
            Tarifs
          </p>
          <h2
            className="mx-auto max-w-xl text-[clamp(2rem,5vw,4.5rem)] font-extralight leading-[1.1] tracking-tight"
            style={{ fontFamily: "var(--font-syne)" }}
          >
            Simple et transparent.
          </h2>
        </motion.div>

        {/* Cards */}
        <div className="grid gap-6 lg:grid-cols-3">
          {plans.map((plan, i) => (
            <motion.div
              key={plan.name}
              className={`relative overflow-hidden rounded-2xl border p-8 transition-all duration-500 ${
                plan.highlighted
                  ? "border-accent/20 bg-background-secondary/80"
                  : "border-foreground/[0.06] bg-background-secondary/40"
              }`}
              initial={{ opacity: 0, y: 40 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-80px" }}
              transition={{
                duration: 0.6,
                delay: i * 0.1,
                ease: [0.16, 1, 0.3, 1],
              }}
            >
              {plan.highlighted && (
                <div className="pointer-events-none absolute inset-0 bg-gradient-to-b from-accent/[0.04] to-transparent" />
              )}

              <div className="relative z-10">
                {/* Plan badge */}
                {plan.highlighted && (
                  <span
                    className="mb-6 inline-block rounded-full bg-accent/10 px-3 py-1 text-xs font-light text-accent"
                    style={{ fontFamily: "var(--font-jakarta)" }}
                  >
                    Populaire
                  </span>
                )}

                <h3
                  className="mb-2 text-xl font-light tracking-tight"
                  style={{ fontFamily: "var(--font-syne)" }}
                >
                  {plan.name}
                </h3>

                <p
                  className="mb-6 text-sm font-light text-foreground/40"
                  style={{ fontFamily: "var(--font-jakarta)" }}
                >
                  {plan.description}
                </p>

                {/* Price */}
                <div className="mb-8 flex items-baseline gap-1">
                  <span
                    className="text-4xl font-extralight tracking-tight"
                    style={{ fontFamily: "var(--font-syne)" }}
                  >
                    {plan.price}€
                  </span>
                  <span
                    className="text-sm font-light text-foreground/30"
                    style={{ fontFamily: "var(--font-jakarta)" }}
                  >
                    /mois
                  </span>
                </div>

                {/* Features */}
                <ul className="mb-8 space-y-3">
                  {plan.features.map((feature) => (
                    <li
                      key={feature}
                      className="flex items-center gap-3 text-sm font-light text-foreground/60"
                      style={{ fontFamily: "var(--font-jakarta)" }}
                    >
                      <Check
                        size={14}
                        strokeWidth={1.5}
                        className={plan.highlighted ? "text-accent" : "text-foreground/25"}
                      />
                      {feature}
                    </li>
                  ))}
                </ul>

                {/* CTA */}
                <Link
                  href="/register"
                  className={`block w-full rounded-xl py-3.5 text-center text-sm font-light tracking-wide transition-all duration-300 ${
                    plan.highlighted
                      ? "bg-foreground text-background hover:bg-accent"
                      : "border border-foreground/[0.08] text-foreground/60 hover:border-accent/30 hover:text-accent"
                  }`}
                  style={{ fontFamily: "var(--font-jakarta)" }}
                >
                  {plan.cta}
                </Link>
              </div>
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}
