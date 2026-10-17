"use client";

import { motion } from "framer-motion";
import { CONTAINER, EASE_OUT, LABEL, RevealHeading, SECTION_TITLE, WipeLink } from "./landing";

const plans = [
  {
    name: "Solo",
    price: "0",
    features: ["3 projects", "500 MB storage", "30-day history", "FL Studio & Ableton Live plugin"],
    cta: "Start for free",
    href: "/register",
    highlighted: false,
  },
  {
    name: "Pro",
    price: "12",
    features: [
      "Unlimited projects",
      "50 GB storage",
      "Unlimited history",
      "Branches & pull requests",
      "Up to 5 collaborators",
      "Priority support",
    ],
    cta: "14-day free trial",
    href: "/register",
    highlighted: true,
  },
  {
    name: "Studio",
    price: "39",
    features: [
      "Everything in Pro",
      "Unlimited storage & members",
      "SSO & permissions",
      "API & webhooks",
      "Dedicated account manager",
    ],
    cta: "Talk to us",
    href: "/register",
    highlighted: false,
  },
];

export default function Pricing() {
  return (
    <section id="pricing" className="scroll-mt-16 border-t-2 border-ink">
      <div className={`${CONTAINER} flex flex-col gap-12 py-24 md:py-32`}>
        <RevealHeading text="Pick your room." className={SECTION_TITLE} />
        <div className="grid grid-cols-1 gap-3 lg:grid-cols-3">
          {plans.map((plan, i) => (
            <motion.div
              key={plan.name}
              initial={{ opacity: 0, y: 40 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-10% 0px" }}
              transition={{ delay: i * 0.1, duration: 0.7, ease: EASE_OUT }}
              className={`flex flex-col gap-5 p-8 ${plan.highlighted ? "bg-ink text-paper" : "border-2 border-ink"}`}
            >
              <div className="flex items-center justify-between">
                <span className={LABEL}>{plan.name}</span>
                {plan.highlighted && (
                  <motion.span
                    className={`bg-lime px-2 py-1 text-ink ${LABEL}`}
                    initial={{ scale: 1.4, rotate: 10, opacity: 0 }}
                    whileInView={{ scale: 1, rotate: -3, opacity: 1 }}
                    viewport={{ once: true }}
                    transition={{ delay: 0.5, type: "spring", stiffness: 420, damping: 15 }}
                  >
                    Most picked
                  </motion.span>
                )}
              </div>
              <p className="font-display text-[64px] leading-none font-extrabold tracking-[-0.04em]">
                {plan.price} €<span className="ml-1 [font-family:var(--font-jakarta)] text-[15px] font-medium tracking-normal">/ month</span>
              </p>
              <ul
                className={`flex flex-1 flex-col gap-2.5 border-t pt-5 text-[15px] ${plan.highlighted ? "border-paper-line" : "border-ink-line"}`}
              >
                {plan.features.map((feature) => (
                  <li key={feature}>{feature}</li>
                ))}
              </ul>
              <WipeLink href={plan.href} variant={plan.highlighted ? "lime" : "outline"} className="w-full">
                {plan.cta}
              </WipeLink>
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}
