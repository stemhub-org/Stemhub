import type { Metadata } from "next";
import localFont from "next/font/local";
import "./globals.css";
import { ThemeProvider } from "@/components/theme-provider";

const syne = localFont({
  src: [
    { path: "../public/fonts/syne-latin.woff2", weight: "400 800", style: "normal" },
    { path: "../public/fonts/syne-latin-ext.woff2", weight: "400 800", style: "normal" },
  ],
  variable: "--font-syne",
  display: "swap",
});

const jakarta = localFont({
  src: [
    { path: "../public/fonts/jakarta-latin.woff2", weight: "200 600", style: "normal" },
    { path: "../public/fonts/jakarta-latin-ext.woff2", weight: "200 600", style: "normal" },
  ],
  variable: "--font-jakarta",
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
    <html lang="fr" suppressHydrationWarning>
      <body
        className={`${syne.variable} ${jakarta.variable} antialiased`}
        style={{ fontFamily: "var(--font-jakarta)" }}
      >
        <ThemeProvider
          attribute="class"
          defaultTheme="system"
          enableSystem
          disableTransitionOnChange
        >
          {children}
        </ThemeProvider>
      </body>
    </html>
  );
}
