import type { Metadata } from "next";
import { Syne, Plus_Jakarta_Sans } from "next/font/google";
import "./globals.css";

const syne = Syne({
  subsets: ["latin"],
  variable: "--font-syne",
  weight: ["400", "500", "600", "700", "800"],
  display: "swap",
});

const jakarta = Plus_Jakarta_Sans({
  subsets: ["latin"],
  variable: "--font-jakarta",
  weight: ["200", "300", "400", "500", "600"],
  display: "swap",
});

export const metadata: Metadata = {
  title: "StemHub — La mémoire de votre musique",
  description:
    "Le versioning décentralisé pensé pour les producteurs de musique. Chaque session, chaque piste, chaque choix — préservé.",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="fr">
      <body
        className={`${syne.variable} ${jakarta.variable} antialiased`}
        style={{ fontFamily: "var(--font-jakarta)" }}
      >
        {children}
      </body>
    </html>
  );
}
