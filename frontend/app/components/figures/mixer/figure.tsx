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
        title="Le même diff, sur la console"
        hint="Défile · clique pour avant / après"
        readout={readoutOf(CHANGES, WET_AFTER)}
        keys={[
          { mark: "lit", label: "Le dernier changement affiché" },
          { mark: "raised", label: "Les modules restés en place" },
          { mark: "dotted", label: "L’emplacement d’un module retiré" },
        ]}
        caption="Cinq inserts, deux slots chacun. Une reverb arrive dans le slot 2 de Lead, le delay quitte le slot 1 de Hats, et le compresseur du Drum Bus passe de 100&nbsp;% à 50&nbsp;% de wet&nbsp;: 12800 → 6400."
      >
        <MixerArt />
      </Plate>
    </div>
  );
}
