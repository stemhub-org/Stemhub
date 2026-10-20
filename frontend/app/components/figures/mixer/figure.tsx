import { Plate } from "../../iso/react/draw";
import { MixerArt } from "./art";
import { CHANGES, WET_AFTER, readoutOf } from "./model";
import "../../iso/iso.css";
import "../brand.css";
import "./mixer.css";

export function MixerFigure() {
  return (
    <div className="iso lp-figure">
      <Plate
        fig="Fig. 02"
        title="The same save, on the desk"
        hint="Scroll · click for before / after"
        readout={readoutOf(CHANGES, WET_AFTER)}
        keys={[
          { mark: "lit", label: "The change shown last" },
          { mark: "raised", label: "Modules that stayed" },
          { mark: "dotted", label: "Where a removed module sat" },
        ]}
        caption="Five inserts, two slots each. A reverb drops into Lead's slot 2, the delay leaves Hats' slot 1, and the Drum Bus compressor goes from fully wet to half: 12800 → 6400."
      >
        <MixerArt />
      </Plate>
    </div>
  );
}
