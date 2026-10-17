// The StemHub figure "catching the signal", on the same 100×100 grid as the
// plugin's paintLogoMark (plugin/stemhub/Source/src/ui/PluginTheme.cpp).
// The identity rules forbid redrawing it: change the geometry in both places.

export const FIGURE_HEAD = { cx: 43.7, cy: 22, r: 8.2 };
export const FIGURE_TORSO = { x: 35.5, y: 32.9, width: 17.8, height: 27, rx: 1.2 };

export const FIGURE_LIMBS = [
  { x1: 39, y1: 36.5, x2: 20.6, y2: 46.6, width: 6.6 }, // lower arm
  { x1: 40.6, y1: 57, x2: 25.8, y2: 84, width: 8.4 }, // back leg
  { x1: 48.9, y1: 57, x2: 48.9, y2: 87.4, width: 8.6 }, // standing leg
  { x1: 49, y1: 37.5, x2: 65.8, y2: 21.8, width: 7 }, // arm reaching for the signal
] as const;

export const FIGURE_BARS = [
  { x: 68, y: 10.1, width: 4.4, height: 7.4 },
  { x: 74.4, y: 5.5, width: 4.5, height: 16.5 },
  { x: 81.1, y: 8.6, width: 4.4, height: 10.5 },
] as const;

// Tight box around the figure, as figureBounds in the plugin.
export const FIGURE_VIEWBOX = "16 5 70 88";

function FigureParts() {
  return (
    <>
      <circle {...FIGURE_HEAD} />
      <rect {...FIGURE_TORSO} />
      {FIGURE_LIMBS.map(({ width, ...line }) => (
        <line key={`${line.x1}-${line.y1}`} {...line} stroke="currentColor" strokeWidth={width} strokeLinecap="round" />
      ))}
      {FIGURE_BARS.map((bar) => (
        <rect key={bar.x} {...bar} rx={bar.width / 2} />
      ))}
    </>
  );
}

export function BrandFigure({ className, title }: { className?: string; title?: string }) {
  return (
    <svg
      viewBox={FIGURE_VIEWBOX}
      fill="currentColor"
      className={className}
      role={title ? "img" : undefined}
      aria-label={title}
      aria-hidden={title ? undefined : true}
    >
      <FigureParts />
    </svg>
  );
}

// App-icon tile: a rounded square with the figure on the full 100×100 grid.
export function BrandTile({ className }: { className?: string }) {
  return (
    <svg viewBox="0 0 100 100" className={className} aria-hidden="true">
      <rect width="100" height="100" rx="24" fill="var(--color-ink)" />
      <g fill="var(--color-paper)" color="var(--color-paper)">
        <FigureParts />
      </g>
    </svg>
  );
}

export function Wordmark({ className = "" }: { className?: string }) {
  return (
    <span className={`font-display font-extrabold uppercase tracking-[-0.02em] ${className}`}>
      StemHub<sup className="ml-0.5 text-[0.4em]">®</sup>
    </span>
  );
}
