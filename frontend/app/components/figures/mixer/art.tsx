import * as React from "react";
import { Dots, Line, Solid } from "../../iso/react/draw";
import { ADDED_ART, BASE_ART, DESK_ART, DESK_ITEMS, MODULES, REMOVED_ART, SLOTS, type DeskItem, type Knob } from "./geometry";
import { MixerLive } from "./live";

type ModuleArt = (typeof MODULES)[number];

function KnobArt({ knob, className }: { knob: Knob; className?: string }) {
  return (
    <>
      {knob.dial ? <Solid paths={knob.dial} tone="lo" crease="none" /> : null}
      {knob.minor ? <Line d={knob.minor} tone="lo" /> : null}
      {knob.major ? <Line d={knob.major} tone="mid" /> : null}
      <Solid paths={knob.body} tone="mid" className={className} />
      <Line d={knob.knurl} tone="faint" />
    </>
  );
}

// A module in two parts: its box, seen only through the slot, and its faceplate on top.
function moduleBox(module: ModuleArt) {
  return (
    <g key={`${module.key}-box`}>
      <Solid paths={module.box} tone="mid" inner={<Line d={module.boxSeam} tone="faint" />} />
    </g>
  );
}

function modulePlate(module: ModuleArt, role: "kept" | "added" | "removed", wet = false) {
  return (
    <g key={`${module.key}-plate`}>
      <Solid paths={module.plate} tone="hi" className={role === "kept" ? undefined : `mx-${role}`} />
      <Line d={module.plateLine} tone="faint" />
      <Dots points={module.screws} size={0.4} tone="mid" />
      <Line d={module.rule} tone="lo" free />
      <Dots points={[module.led]} size={0.6} tone="hi" />
      {module.button ? <Solid paths={module.button} tone="mid" /> : null}
      {module.knobs.map((knob, index) => (
        <React.Fragment key={index}>
          <KnobArt knob={knob} className={wet && index === 0 ? "mx-wet" : undefined} />
          {wet && index === 0 ? null : <Line d={knob.pointer} tone="hi" free="end" />}
        </React.Fragment>
      ))}
    </g>
  );
}

const BASE = (
  <g key="base">
    <path className="iso-halo" d={BASE_ART.halo} />
    {BASE_ART.feet.map((parts, index) => (
      <g key={index}>
        {parts.map((paths, part) => (
          <Solid key={part} paths={paths} tone={part ? "mid" : "lo"} />
        ))}
      </g>
    ))}
    <Solid paths={BASE_ART.slab} tone="mid" />
    <Line d={BASE_ART.groove} tone="lo" />
    <Line d={BASE_ART.screwRings} tone="lo" />
    <Dots points={BASE_ART.screws} size={0.5} tone="mid" />
  </g>
);

const DESK = (
  <g key="desk">
    <path className="iso-halo" d={DESK_ART.halo} />
    {DESK_ART.feet.map((paths, index) => (
      <Solid key={index} paths={paths} tone="lo" />
    ))}
    <Solid paths={DESK_ART.body} tone="mid" inner={<Line d={DESK_ART.seam} tone="faint" />} />
    <Line d={DESK_ART.jack} tone="lo" />
    <Dots points={DESK_ART.frontScrews} size={0.45} tone="mid" />
    <Line d={DESK_ART.separators} tone="faint" />
    <Solid paths={DESK_ART.bridge} tone="mid" inner={<Line d={DESK_ART.bridgeSeam} tone="faint" />} />
    <Line d={DESK_ART.scribbles} tone="lo" />
    <Line d={DESK_ART.scribbleRules} tone="mid" free />
    {DESK_ART.signal.map((paths, index) => (
      <Solid key={index} paths={paths} tone="lo" crease="none" />
    ))}
    <Solid paths={DESK_ART.masterPlate} tone="lo" crease="none" />
    <Line d={DESK_ART.masterPlateLine} tone="faint" />
    <Line d={DESK_ART.masterPlateRules} tone="lo" free />
    <Dots points={DESK_ART.masterScrews} size={0.45} tone="mid" />
  </g>
);

const SLOT_ART = (
  <g key="slots">
    {SLOTS.map((slot) => (
      <g key={slot.key}>
        <path className="mx-hole" d={slot.hole} />
        <path className="iso-shade" data-shade="1" d={slot.back} />
        <path className="iso-shade" data-shade="0" d={slot.left} />
        <Line d={slot.back + slot.left} tone="faint" />
        <Line d={slot.guides} tone="faint" />
        <Line d={slot.rim} tone="lo" />
      </g>
    ))}
  </g>
);

const DEFS = (
  <defs key="defs">
    {[...MODULES, ADDED_ART, REMOVED_ART].map((module) => {
      const slot = SLOTS.find((entry) => entry.key === `slot-${module.strip}-${module.slot}`)!;
      return (
        <clipPath key={module.clip} id={module.clip}>
          <path d={slot.clip} />
        </clipPath>
      );
    })}
  </defs>
);

const GHOST = <Line key="ghost" d={REMOVED_ART.ghost} tone="mid" dotted />;

function deskItem(item: DeskItem) {
  if (item.kind === "knob" && item.knob) {
    return (
      <g key={item.key}>
        <KnobArt knob={item.knob} />
        <Line d={item.knob.pointer} tone="hi" free="end" />
      </g>
    );
  }
  if (item.kind === "track" && item.track) {
    return (
      <g key={item.key}>
        <path className="mx-hole" d={item.track.slot} />
        <Line d={item.track.rim} tone="lo" />
        <Solid paths={item.track.ruler} tone="lo" crease="none" />
        <Line d={item.track.minor} tone="lo" />
        <Line d={item.track.major} tone="mid" />
      </g>
    );
  }
  return (
    <g key={item.key}>
      <Solid paths={item.paths!} tone={item.kind === "fader" ? "hi" : "mid"} />
      <Line d={item.line!} tone={item.kind === "fader" ? "mid" : "lo"} />
    </g>
  );
}

const DESK_TOP = <g key="desk-top">{DESK_ITEMS.map(deskItem)}</g>;

// Modules in depth order, each tagged with its role so the live code can move it.
const ALL = [
  ...MODULES.map((module) => ({ module, role: "kept" as const })),
  { module: ADDED_ART, role: "added" as const },
  { module: REMOVED_ART, role: "removed" as const },
].sort((a, b) => a.module.depth - b.module.depth);

const MODULE_PARTS = ALL.map(({ module, role }) => ({
  key: module.key,
  role,
  clip: module.clip,
  box: moduleBox(module),
  plate: modulePlate(module, role, module.kind === "comp"),
}));

const LABEL =
  "A small mixing desk with five channel strips: Kick, Drum Bus, Lead, Pad and Hats. Each strip has two insert slots on the meter bridge, and a plugin is a module plugged into a slot. Scrolling plays the save: a reverb module drops into the Lead's second slot, the delay module lifts out of the Hats' first slot, and the dry/wet knob on the Drum Bus compressor turns from 12800 to 6400.";

export function MixerArt() {
  return <MixerLive label={LABEL} defs={DEFS} base={BASE} desk={DESK} slots={SLOT_ART} ghost={GHOST} modules={MODULE_PARTS} deskTop={DESK_TOP} />;
}
