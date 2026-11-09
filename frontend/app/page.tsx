import Navbar from "./components/Navbar";
import Hero from "./components/Hero";
import Marquee from "./components/Marquee";
import HowItWorks from "./components/HowItWorks";
import DiffSection from "./components/DiffSection";
import Features from "./components/Features";
import Pricing from "./components/Pricing";
import CtaBanner from "./components/CtaBanner";
import Footer from "./components/Footer";
import LandingMotion from "./components/LandingMotion";
import { MixerFigure } from "./components/figures/mixer/figure";
import { StudioFigure } from "./components/figures/studio/figure";

export default function Home() {
  return (
    // The landing page is always on Paper, whatever the app theme, and in French while the
    // app stays in English (the root layout's lang).
    <LandingMotion>
      <div lang="fr" className="bg-paper text-ink selection:bg-lime selection:text-ink">
        <Navbar />
        <main>
          <Hero />
          <Marquee />
          <HowItWorks figure={<StudioFigure />} />
          <DiffSection figure={<MixerFigure />} />
          <Features />
          <Pricing />
          <CtaBanner />
        </main>
        <Footer />
      </div>
    </LandingMotion>
  );
}
