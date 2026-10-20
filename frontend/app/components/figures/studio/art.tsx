import * as React from "react";
import { Dots, FaceText, Line, Solid } from "../../iso/react/draw";
import { DESK_ART, KEYBOARD_ART, MONITOR_ART, SPINDLE_ART } from "./geometry";
import { StudioLive } from "./live";

const DESK = (
  <g key="desk">
    <path className="iso-halo" d={DESK_ART.halo} />
    {DESK_ART.feet.map((parts, index) => (
      <g key={index}>
        {parts.map((paths, part) => (
          <Solid key={part} paths={paths} tone={part ? "mid" : "lo"} />
        ))}
      </g>
    ))}
    <Solid paths={DESK_ART.slab} tone="mid" />
    <Line d={DESK_ART.groove} tone="lo" />
    <Line d={DESK_ART.screwRings} tone="lo" />
    <Dots points={DESK_ART.screws} size={0.5} tone="mid" />
    <Solid paths={DESK_ART.strip} tone="lo" crease="none" />
    <Line d={DESK_ART.ticks.minor} tone="lo" />
    <Line d={DESK_ART.ticks.major} tone="mid" />
  </g>
);

const MONITOR = (
  <g key="monitor">
    <path className="iso-halo" d={MONITOR_ART.halo} />
    <Solid paths={MONITOR_ART.foot} tone="mid" />
    <Line d={MONITOR_ART.footLine} tone="faint" />
    <Solid paths={MONITOR_ART.neck} tone="mid" inner={<Line d={MONITOR_ART.neckSeam} tone="faint" />} />
    <Solid paths={MONITOR_ART.body} tone="mid" inner={<Line d={MONITOR_ART.bodySeam} tone="faint" />} />
    <Dots points={MONITOR_ART.buttons} size={0.55} tone="mid" />
  </g>
);

const SCREEN_RIM = (
  <g key="rim">
    <Line d={MONITOR_ART.screenRim} tone="hi" />
    <circle className="st-power" cx={MONITOR_ART.power[0]} cy={MONITOR_ART.power[1]} r={0.8} />
  </g>
);

const KEYBOARD = (
  <g key="keyboard">
    <path className="iso-halo" d={KEYBOARD_ART.halo} />
    <Solid paths={KEYBOARD_ART.base} tone="mid" inner={<Line d={KEYBOARD_ART.seam} tone="faint" />} />
    <Line d={KEYBOARD_ART.bed} tone="lo" />
    {KEYBOARD_ART.knobs.map((knob, index) => (
      <g key={`k${index}`}>
        <Solid paths={knob.body} tone="mid" />
        <Line d={knob.pointer} tone="hi" free="end" />
      </g>
    ))}
    {KEYBOARD_ART.pads.map((paths, index) => (
      <Solid key={`p${index}`} paths={paths} tone="mid" />
    ))}
    <path className="st-oled" d={KEYBOARD_ART.displayFill} />
    <Line d={KEYBOARD_ART.display} tone="lo" />
    <FaceText transform={KEYBOARD_ART.displayText} size={3.4} tone="hi" anchor="middle">
      140 BPM
    </FaceText>
    {KEYBOARD_ART.whites.map((paths, index) => (
      <Solid key={`w${index}`} paths={paths} tone="mid" crease="faint" />
    ))}
    {KEYBOARD_ART.blacks.map((paths, index) => (
      <Solid key={`b${index}`} paths={paths} tone="hi" className="st-black" />
    ))}
  </g>
);

const SPINDLE_BASES = SPINDLE_ART.map((spindle) => (
  <g key={spindle.branch}>
    <path className="iso-halo" d={spindle.halo} />
    <Solid paths={spindle.plate} tone="lo" crease="none" />
    <Line d={spindle.plateLine} tone="faint" />
    <FaceText transform={spindle.plateText} size={3.2} tone="mid" anchor="middle">
      {spindle.branch}
    </FaceText>
    <Solid paths={spindle.base} tone="mid" />
    <Dots points={spindle.screws} size={0.5} tone="mid" />
    <Solid paths={spindle.post} tone="mid" crease="none" />
  </g>
));

const LABEL =
  "A producer's desk. A monitor runs the DAW with the StemHub plugin docked beside the channel rack and playlist; in front sits a MIDI keyboard. Beside the monitor, a version tower holds one spindle per branch, main and late-night-idea, with one disc per saved version. Change the beat on screen, save a version and a disc drops onto the branch's spindle; switch branches or restore any version and the session on screen follows.";

export function StudioArt() {
  return <StudioLive label={LABEL} desk={DESK} monitor={MONITOR} rim={SCREEN_RIM} keyboard={KEYBOARD} spindles={SPINDLE_BASES} />;
}
