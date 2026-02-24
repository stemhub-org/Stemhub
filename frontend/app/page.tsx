"use client";

import SmoothScroll from "./components/SmoothScroll";
import Navbar from "./components/Navbar";
import Hero from "./components/Hero";
import Logos from "./components/Logos";
import DiffSection from "./components/DiffSection";
import HowItWorks from "./components/HowItWorks";
import Features from "./components/Features";
import Pricing from "./components/Pricing";
import CtaBanner from "./components/CtaBanner";
import Footer from "./components/Footer";

export default function Home() {
  return (
    <SmoothScroll>
      <Navbar />
      <main>
        <Hero />
        <Logos />
        <DiffSection />
        <HowItWorks />
        <Features />
        <Pricing />
        <CtaBanner />
      </main>
      <Footer />
    </SmoothScroll>
  );
}
