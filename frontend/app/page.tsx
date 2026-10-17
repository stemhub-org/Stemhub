"use client";

import { MotionConfig } from "framer-motion";
import Navbar from "./components/Navbar";
import Hero from "./components/Hero";
import Marquee from "./components/Marquee";
import HowItWorks from "./components/HowItWorks";
import DiffSection from "./components/DiffSection";
import Features from "./components/Features";
import Pricing from "./components/Pricing";
import CtaBanner from "./components/CtaBanner";
import Footer from "./components/Footer";

export default function Home() {
  return (
    // The landing page is always on Paper, whatever the app theme.
    <MotionConfig reducedMotion="user">
      <div className="bg-paper text-ink selection:bg-lime selection:text-ink">
        <Navbar />
        <main>
          <Hero />
          <Marquee />
          <HowItWorks />
          <DiffSection />
          <Features />
          <Pricing />
          <CtaBanner />
        </main>
        <Footer />
      </div>
    </MotionConfig>
  );
}
