import type { Metadata } from "next";
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

const title = "StemHub — Gestion de versions pour producteurs";
const description =
  "Enregistre chaque version de tes projets FL Studio et Ableton Live, teste tes idées sur des branches et collabore sans jamais écraser l’original.";

export const metadata: Metadata = {
  title,
  description,
  alternates: { canonical: "/" },
  openGraph: { type: "website", locale: "fr_FR", siteName: "StemHub", url: "/", title, description },
  twitter: { card: "summary_large_image" },
};

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
