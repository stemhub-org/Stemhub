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
        title="StemHub, dans ton DAW"
        hint="Édite le beat · Enregistre · Branches · Restaure"
        readout={readoutOf(initialStudio())}
        keys={[
          { mark: "lit", label: "La version sélectionnée dans le plugin" },
          { mark: "raised", label: "Un disque par version enregistrée, un axe par branche" },
        ]}
        caption="Tout ce qui est à l’écran fonctionne. Clique sur les pas pour changer le beat, enregistre une version avec son message, passe sur la branche late-night-idea ou restaure n’importe quelle version&nbsp;: elle s’ouvre dans ton DAW comme une copie à part, ton fichier de projet reste tel quel."
      >
        <StudioArt />
      </Plate>
    </div>
  );
}
