"use client";

import * as React from "react";
import { BRANCHES, CHANNELS, PROJECT, STEPS, branchVersions, changeCount, label, messageOf, type BranchName, type Studio } from "./model";
import { SCREEN_H, SCREEN_W } from "./view";

// The monitor's picture, in flat screen units (u right, v down). The DAW on the left, the
// StemHub plugin docked on the right, drawn after the plugin's own layout and wording (in French here).
// The DAW's own labels stay as FL Studio shows them.

export const RACK = { u: 3, v: 12, w: 127, h: 64 };
export const PLAYLIST = { u: 3, v: 79, w: 127, h: 56 };
export const PLUGIN = { u: 133, v: 12, w: 110, h: 123 };
const STEP_U0 = RACK.u + 27;
const STEP_W = 6.1;
const ROW_V0 = RACK.v + 9;
const ROW_H = 8.7;
export const LANE_U0 = PLAYLIST.u + 24;
export const LANE_U1 = PLAYLIST.u + PLAYLIST.w - 2;
const LANE_V0 = PLAYLIST.v + 10;
const LANE_H = 14.6;
const BAR_W = (LANE_U1 - LANE_U0) / 4;
const HISTORY_V0 = PLUGIN.v + 72;
const HISTORY_ROW = 8.4;
const HISTORY_ROWS = 4;

export const stepU = (step: number) => STEP_U0 + step * STEP_W;
export const STEP_TOP = ROW_V0 - 1;
export const STEP_BOTTOM = ROW_V0 + CHANNELS.length * ROW_H - 1.6;
export const LANES_TOP = LANE_V0 - 1;
export const LANES_BOTTOM = LANE_V0 + LANE_H * 3;

interface Handlers {
  toggle: (channel: number, step: number) => void;
  save: () => void;
  branch: (branch: BranchName) => void;
  select: (id: string) => void;
  restore: () => void;
  play: () => void;
}

// A clickable region with keyboard support, for the controls drawn on the screen.
function Hit({ onPress, label: name, children, className, disabled = false, focusable = true }: { onPress: () => void; label: string; children: React.ReactNode; className?: string; disabled?: boolean; focusable?: boolean }) {
  return (
    <g
      role="button"
      tabIndex={disabled || !focusable ? -1 : 0}
      aria-label={name}
      aria-disabled={disabled || undefined}
      className={`st-hit ${className ?? ""}`}
      onClick={(event) => {
        event.stopPropagation();
        if (!disabled) onPress();
      }}
      onKeyDown={(event) => {
        if (event.key !== "Enter" && event.key !== " ") return;
        event.preventDefault();
        event.stopPropagation();
        if (!disabled) onPress();
      }}
    >
      {children}
    </g>
  );
}

// Each clip in the playlist shows a miniature of the pattern it plays.
function clipPreview(studio: Studio, u0: number, v0: number) {
  const cell = (BAR_W - 3) / STEPS;
  const rowH = (LANE_H - 5) / CHANNELS.length;
  let d = "";
  studio.pattern.forEach((line, channel) =>
    line.forEach((on, step) => {
      if (on) d += `M${(u0 + 1.5 + step * cell).toFixed(2)} ${(v0 + 2.5 + channel * rowH).toFixed(2)}h${(cell * 0.8).toFixed(2)}v${(rowH * 0.7).toFixed(2)}h-${(cell * 0.8).toFixed(2)}Z`;
    }),
  );
  return d;
}

const WAVE = Array.from({ length: 48 }, (_, index) => {
  const amp = 1.2 + Math.abs(Math.sin(index * 0.9) * 3.2) * (index % 7 === 0 ? 0.5 : 1);
  return Math.round(amp * 10) / 10;
});

function waveform(u0: number, v0: number) {
  const mid = v0 + LANE_H / 2 - 1;
  const step = (BAR_W * 2 - 3) / WAVE.length;
  return WAVE.map((amp, index) => `M${(u0 + 1.5 + index * step).toFixed(2)} ${(mid - amp).toFixed(2)}v${(amp * 2).toFixed(2)}`).join("");
}

export function Screen({ studio, playing, handlers, playhead, column, progress }: { studio: Studio; playing: boolean; handlers: Handlers; playhead: React.Ref<SVGGElement>; column: React.Ref<SVGRectElement>; progress: React.Ref<SVGRectElement> }) {
  const count = changeCount(studio);
  const busy = studio.activity.kind !== "idle";
  const message = messageOf(studio);
  const history = [...branchVersions(studio)].reverse().slice(0, HISTORY_ROWS);
  const selected = studio.versions.find((version) => version.id === studio.selected);

  return (
    <g className="st-screen">
      <rect className="st-bg" x={0} y={0} width={SCREEN_W} height={SCREEN_H} />

      {/* Title bar: menus, the open file, transport. */}
      <rect className="st-bar" x={0} y={0} width={SCREEN_W} height={9} />
      <text className="st-t st-dim" x={3} y={6.1} fontSize={3.1}>
        FILE  EDIT  ADD  PATTERNS  VIEW  TOOLS
      </text>
      <text className="st-t" x={78} y={6.2} fontSize={3.5}>
        {studio.file}
      </text>
      <Hit onPress={handlers.play} label={playing ? "Arrêter la lecture" : "Lecture"} className="st-play">
        <rect className={playing ? "st-key st-key-on" : "st-key"} x={176} y={1.6} width={9} height={5.8} rx={0.8} />
        {playing ? <rect className="st-glyph" x={178.9} y={3.1} width={3.2} height={2.8} /> : <path className="st-glyph" d="M179 3l3.6 1.5L179 6z" />}
      </Hit>
      <rect className="st-field" x={188} y={1.6} width={20} height={5.8} rx={0.6} />
      <text className="st-t st-mono" x={198} y={5.6} fontSize={3.2} textAnchor="middle">
        140.000
      </text>
      <rect className="st-field" x={210} y={1.6} width={33} height={5.8} rx={0.6} />
      <text className="st-t st-mono st-dim" x={226.5} y={5.6} fontSize={3.2} textAnchor="middle">
        {PROJECT.toUpperCase()}
      </text>

      {/* Channel rack: click a step to change the beat. */}
      <rect className="st-panel" x={RACK.u} y={RACK.v} width={RACK.w} height={RACK.h} rx={1} />
      <text className="st-t st-dim" x={RACK.u + 3} y={RACK.v + 5.5} fontSize={3.1}>
        Channel rack
      </text>
      <rect ref={column} className="st-column" x={stepU(0) - 0.5} y={STEP_TOP} width={STEP_W - 0.2} height={STEP_BOTTOM - STEP_TOP + 1} rx={0.8} style={{ opacity: playing ? 1 : 0 }} />
      {CHANNELS.map((name, channel) => (
        <g key={name}>
          <rect className="st-name" x={RACK.u + 3} y={ROW_V0 + channel * ROW_H} width={21} height={6.6} rx={0.8} />
          <text className="st-t" x={RACK.u + 5} y={ROW_V0 + channel * ROW_H + 4.6} fontSize={3.2}>
            {name}
          </text>
          {Array.from({ length: STEPS }, (_, step) => {
            const on = studio.pattern[channel]![step]!;
            return (
              <Hit key={step} onPress={() => handlers.toggle(channel, step)} label={`${name}, pas ${step + 1}, ${on ? "activé" : "désactivé"}`} disabled={busy} focusable={false}>
                <rect
                  className={on ? "st-step st-step-on" : Math.floor(step / 4) % 2 ? "st-step st-step-alt" : "st-step"}
                  x={stepU(step)}
                  y={ROW_V0 + channel * ROW_H}
                  width={STEP_W - 0.9}
                  height={6.6}
                  rx={0.7}
                />
              </Hit>
            );
          })}
        </g>
      ))}

      {/* Playlist: the pattern's clips, a vocal and an automation lane. */}
      <rect className="st-panel" x={PLAYLIST.u} y={PLAYLIST.v} width={PLAYLIST.w} height={PLAYLIST.h} rx={1} />
      <text className="st-t st-dim" x={PLAYLIST.u + 3} y={PLAYLIST.v + 5.5} fontSize={3.1}>
        Playlist
      </text>
      {[0, 1, 2, 3].map((bar) => (
        <text key={bar} className="st-t st-dim st-mono" x={LANE_U0 + bar * BAR_W + 1} y={PLAYLIST.v + 6} fontSize={2.8}>
          {bar + 1}
        </text>
      ))}
      {["Beat", "Vocal chop", "Filtre"].map((name, lane) => (
        <g key={name}>
          <rect className="st-name" x={PLAYLIST.u + 3} y={LANE_V0 + lane * LANE_H} width={19} height={LANE_H - 2} rx={0.8} />
          <text className="st-t" x={PLAYLIST.u + 4.6} y={LANE_V0 + lane * LANE_H + 5} fontSize={2.9}>
            {name}
          </text>
        </g>
      ))}
      {[0, 1, 2, 3].map((bar) => (
        <g key={bar}>
          <rect className="st-clip" x={LANE_U0 + bar * BAR_W + 0.4} y={LANE_V0} width={BAR_W - 0.8} height={LANE_H - 2} rx={0.8} />
          <path className="st-clip-dots" d={clipPreview(studio, LANE_U0 + bar * BAR_W, LANE_V0)} />
        </g>
      ))}
      <rect className="st-clip st-clip-audio" x={LANE_U0 + BAR_W * 2 + 0.4} y={LANE_V0 + LANE_H} width={BAR_W * 2 - 0.8} height={LANE_H - 2} rx={0.8} />
      <path className="st-wave" d={waveform(LANE_U0 + BAR_W * 2, LANE_V0 + LANE_H)} />
      <path className="st-auto" d={`M${LANE_U0} ${LANE_V0 + LANE_H * 2 + 10}C${LANE_U0 + 40} ${LANE_V0 + LANE_H * 2 + 10} ${LANE_U0 + 60} ${LANE_V0 + LANE_H * 2 + 2} ${LANE_U1} ${LANE_V0 + LANE_H * 2 + 3}`} />
      <g ref={playhead} className="st-playhead" style={{ opacity: playing ? 1 : 0.55 }}>
        <path d={`M${LANE_U0} ${LANES_TOP}V${LANES_BOTTOM}`} />
        <path className="st-playhead-cap" d={`M${LANE_U0 - 1.4} ${LANES_TOP - 1.6}h2.8l-1.4 1.6z`} />
      </g>

      {/* The StemHub plugin. */}
      <rect className="st-plugin" x={PLUGIN.u} y={PLUGIN.v} width={PLUGIN.w} height={PLUGIN.h} rx={1.2} />
      <rect className="st-plugin-bar" x={PLUGIN.u} y={PLUGIN.v} width={PLUGIN.w} height={7} rx={1.2} />
      <text className="st-t st-lime st-bold" x={PLUGIN.u + 3} y={PLUGIN.v + 5} fontSize={3.2}>
        STEMHUB
      </text>
      <text className="st-t st-dim" x={PLUGIN.u + PLUGIN.w - 3} y={PLUGIN.v + 5} fontSize={2.8} textAnchor="end">
        stemhub.io/late-night
      </text>
      <text className="st-t st-bold" x={PLUGIN.u + 3} y={PLUGIN.v + 14} fontSize={4.6}>
        {PROJECT.toUpperCase()}
      </text>

      <text className="st-t st-dim" x={PLUGIN.u + 3} y={PLUGIN.v + 20} fontSize={2.8}>
        Branche
      </text>
      {BRANCHES.map((name, index) => {
        const u = PLUGIN.u + 3 + (index ? 26 : 0);
        const w = index ? 52 : 23;
        const on = studio.branch === name;
        return (
          <Hit key={name} onPress={() => handlers.branch(name)} label={`Passer sur la branche ${name}`} disabled={busy}>
            <rect className={on ? "st-chip st-chip-on" : "st-chip"} x={u} y={PLUGIN.v + 22} width={w} height={6.6} rx={3.3} />
            <text className={on ? "st-t st-ink" : "st-t"} x={u + w / 2} y={PLUGIN.v + 26.4} fontSize={3.1} textAnchor="middle">
              {name}
            </text>
          </Hit>
        );
      })}

      <rect className="st-input" x={PLUGIN.u + 3} y={PLUGIN.v + 32} width={PLUGIN.w - 6} height={7.4} rx={0.8} />
      <text className={message ? "st-t" : "st-t st-faint"} x={PLUGIN.u + 5} y={PLUGIN.v + 36.9} fontSize={3.1}>
        {message || "Qu’est-ce qui a changé\u00a0? (facultatif)"}
      </text>

      <Hit onPress={handlers.save} label="Enregistrer la version" disabled={busy || count === 0}>
        <rect className={count && !busy ? "st-save" : studio.activity.kind === "saving" ? "st-save st-save-busy" : "st-save st-save-off"} x={PLUGIN.u + 3} y={PLUGIN.v + 42} width={PLUGIN.w - 6} height={8.6} rx={1} />
        <text className={count && !busy ? "st-t st-ink st-bold" : "st-t st-dim st-bold"} x={PLUGIN.u + PLUGIN.w / 2} y={PLUGIN.v + 47.4} fontSize={3.4} textAnchor="middle">
          {studio.activity.kind === "saving" ? "Sauvegarde…" : "Enregistrer la version"}
        </text>
      </Hit>
      <rect className="st-track" x={PLUGIN.u + 3} y={PLUGIN.v + 52} width={PLUGIN.w - 6} height={1} />
      <rect ref={progress} className="st-progress" x={PLUGIN.u + 3} y={PLUGIN.v + 52} width={0} height={1} />
      <text className={studio.status.endsWith(" enregistrée.") || studio.status.startsWith("Ouverte") ? "st-t st-lime" : "st-t st-dim"} x={PLUGIN.u + 3} y={PLUGIN.v + 58.6} fontSize={3}>
        {studio.status}
      </text>

      <text className="st-t st-dim" x={PLUGIN.u + 3} y={HISTORY_V0 - 3} fontSize={2.8}>
        Historique · {studio.branch}
      </text>
      {history.map((version, index) => {
        const v = HISTORY_V0 + index * HISTORY_ROW;
        const on = version.id === studio.selected;
        return (
          <Hit key={version.id} onPress={() => handlers.select(version.id)} label={`${label(version)}, ${version.message}`} disabled={busy}>
            <rect className={on ? "st-row st-row-on" : "st-row"} x={PLUGIN.u + 3} y={v} width={PLUGIN.w - 6} height={HISTORY_ROW - 0.8} rx={0.6} />
            {on ? <rect className="st-row-bar" x={PLUGIN.u + 3} y={v} width={1} height={HISTORY_ROW - 0.8} /> : null}
            <text className="st-t st-mono st-dim" x={PLUGIN.u + 5.6} y={v + 3.4} fontSize={2.7}>
              {label(version)}
            </text>
            <text className="st-t" x={PLUGIN.u + 16} y={v + 3.4} fontSize={3}>
              {version.message.length > 24 ? `${version.message.slice(0, 23)}…` : version.message}
            </text>
            <text className="st-t st-faint" x={PLUGIN.u + 16} y={v + 6.6} fontSize={2.4}>
              {version.when}
              {version.id === studio.base && studio.file.includes("(") ? " · dans ton DAW" : ""}
            </text>
          </Hit>
        );
      })}

      <Hit onPress={handlers.restore} label={selected ? `Restaurer ${label(selected)}` : "Restaurer cette version"} disabled={busy || !selected}>
        <rect className={studio.activity.kind === "restoring" ? "st-restore st-restore-busy" : "st-restore"} x={PLUGIN.u + 3} y={PLUGIN.v + PLUGIN.h - 12.4} width={PLUGIN.w - 6} height={8.4} rx={1} />
        <text className="st-t st-bold" x={PLUGIN.u + PLUGIN.w / 2} y={PLUGIN.v + PLUGIN.h - 7.1} fontSize={3.3} textAnchor="middle">
          {studio.activity.kind === "restoring" ? "Restauration…" : `Restaurer ${selected ? label(selected) : "cette version"}`}
        </text>
      </Hit>
    </g>
  );
}
