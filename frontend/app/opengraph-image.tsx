import { ImageResponse } from "next/og";
import { FIGURE_BARS, FIGURE_HEAD, FIGURE_LIMBS, FIGURE_TORSO, FIGURE_VIEWBOX } from "./components/BrandMark";

// Identity system 2026: Ink ground, Paper type, Lime only as a fill, never as text on Paper.
// Set in ImageResponse's default font: satori can't read the site's woff2 files.
const INK = "#0A0A0A";
const PAPER = "#FFFFFF";
const PAPER_SOFT = "rgba(255, 255, 255, 0.64)";
const LIME = "#C8FF00";

export const alt = "StemHub, la gestion de versions pour producteurs\u00a0: la figure StemHub qui attrape un signal";
export const size = { width: 1200, height: 630 };
export const contentType = "image/png";

// The logo figure from BrandMark, with its three signal bars in Lime.
function Figure({ height }: { height: number }) {
  return (
    <svg viewBox={FIGURE_VIEWBOX} width={(height * 70) / 88} height={height}>
      <circle {...FIGURE_HEAD} fill={PAPER} />
      <rect {...FIGURE_TORSO} fill={PAPER} />
      {FIGURE_LIMBS.map(({ width, ...line }) => (
        <line key={`${line.x1}-${line.y1}`} {...line} stroke={PAPER} strokeWidth={width} strokeLinecap="round" />
      ))}
      {FIGURE_BARS.map((bar) => (
        <rect key={bar.x} {...bar} rx={bar.width / 2} fill={LIME} />
      ))}
    </svg>
  );
}

export default function OpengraphImage() {
  return new ImageResponse(
    (
      <div
        style={{
          width: "100%",
          height: "100%",
          display: "flex",
          alignItems: "center",
          justifyContent: "space-between",
          padding: "72px 88px",
          background: INK,
          color: PAPER,
        }}
      >
        <div style={{ display: "flex", flexDirection: "column", justifyContent: "space-between", height: "100%" }}>
          <div style={{ display: "flex", fontSize: 30, letterSpacing: 2 }}>STEMHUB®</div>
          <div style={{ display: "flex", flexDirection: "column", fontSize: 92, lineHeight: 0.95, letterSpacing: -3 }}>
            <span>TA BOUCLE</span>
            <span>VEUT DU MONDE.</span>
          </div>
          <div style={{ display: "flex", flexDirection: "column", alignItems: "flex-start", gap: 18 }}>
            <div style={{ display: "flex", background: LIME, color: INK, padding: "10px 16px", fontSize: 24 }}>
              OPEN STEMS / OPEN MINDS / NO GATEKEEPING
            </div>
            <div style={{ display: "flex", fontSize: 28, color: PAPER_SOFT }}>
              Gestion de versions pour FL Studio et Ableton Live
            </div>
          </div>
        </div>
        <Figure height={440} />
      </div>
    ),
    size,
  );
}
