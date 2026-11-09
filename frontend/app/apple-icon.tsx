import { ImageResponse } from "next/og";
import { FIGURE_BARS, FIGURE_HEAD, FIGURE_LIMBS, FIGURE_TORSO } from "./components/BrandMark";

// Same mark as icon.svg, on a full square: iOS rounds the corners itself.
export const size = { width: 180, height: 180 };
export const contentType = "image/png";

export default function AppleIcon() {
  return new ImageResponse(
    (
      <div style={{ width: "100%", height: "100%", display: "flex", background: "#0A0A0A" }}>
        <svg viewBox="0 0 100 100" width={180} height={180}>
          <circle {...FIGURE_HEAD} fill="#FFFFFF" />
          <rect {...FIGURE_TORSO} fill="#FFFFFF" />
          {FIGURE_LIMBS.map(({ width, ...line }) => (
            <line key={`${line.x1}-${line.y1}`} {...line} stroke="#FFFFFF" strokeWidth={width} strokeLinecap="round" />
          ))}
          {FIGURE_BARS.map((bar) => (
            <rect key={bar.x} {...bar} rx={bar.width / 2} fill="#C8FF00" />
          ))}
        </svg>
      </div>
    ),
    size,
  );
}
