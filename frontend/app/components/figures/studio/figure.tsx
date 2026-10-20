import { Plate } from "../../iso/react/draw";
import { StudioArt } from "./art";
import { initialStudio, readoutOf } from "./model";
import "../../iso/iso.css";
import "../brand.css";
import "./studio.css";

export function StudioFigure() {
  return (
    <div className="iso lp-figure" data-theme="light">
      <Plate
        fig="Fig. 01"
        title="StemHub, inside your DAW"
        hint="Edit the beat · Save · switch branch · Restore"
        readout={readoutOf(initialStudio())}
        keys={[
          { mark: "lit", label: "The version selected in the plugin" },
          { mark: "raised", label: "One disc per saved version, one spindle per branch" },
        ]}
        caption="Everything on the screen works. Click the steps to change the beat, save a version with its message, switch to the late-night-idea branch, or restore any version: it opens in your DAW as a separate copy, your project file stays as it is."
      >
        <StudioArt />
      </Plate>
    </div>
  );
}
