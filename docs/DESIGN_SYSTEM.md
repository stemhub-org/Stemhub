# Design System

This page documents StemHub's visual identity: logo, color palette, and UI mockups. The design follows a dark, developer-oriented aesthetic inspired by GitHub, adapted for music producers.

---

## Logo

StemHub's logo combines a waveform and a Git branch merge symbol, reflecting the core concept of version control applied to music.

Two variants are available depending on the background context:

**Dark variant** — for dark backgrounds and app icon usage

<img width="180" alt="StemHub Logo Dark" src="https://github.com/user-attachments/assets/6dd80b8f-e18c-480d-a37b-1d730ea5003f" />

**Light variant** — for light backgrounds and documentation

<img width="180" alt="StemHub Logo Light" src="https://github.com/user-attachments/assets/00337e57-1b7b-4ae1-bfd9-1f4e2fa808b2" />

---

## Color Palette

The 2026 identity system uses three colours:

| Name | Hex | Usage |
|------|-----|-------|
| **Ink** | `#0A0A0A` | Text on Paper, dark surfaces, the accent on Paper |
| **Paper** | `#FFFFFF` | Light surfaces, text on Ink |
| **Lime** | `#C8FF00` | The accent on Ink, and text on Ink fills |

**Lime is never text on Paper** (1.0:1 contrast). On Paper it is decorative only (a glow, a chart fill, a tint behind Ink text).

They are Tailwind colours (`ink`, `paper`, `lime`, plus `ink-soft`, `ink-line`, `paper-soft`, `paper-line`) defined in `frontend/app/globals.css`. The landing page always renders on Paper and uses them directly.

### App theme tokens

The web app follows the system light/dark theme, so its components use themed tokens instead of the raw colours:

| Token | Light (Paper) | Dark (Ink) | Use it for |
|-------|---------------|------------|------------|
| `background` | `#FFFFFF` | `#0A0A0A` | Page background |
| `background-secondary` / `-tertiary` | `#F4F4F5` / `#E4E4E7` | `#121212` / `#1A1A1A` | Cards, panels, inputs |
| `foreground` | `#0A0A0A` | `#FFFFFF` | Text |
| `foreground-muted` | `#71717A` | `#8A8A8A` | Secondary text |
| `accent` | Ink | Lime | Active states, highlights, primary fills |
| `accent-foreground` | Lime | Ink | Text and icons on an `accent` fill |

Rules:
- A `bg-accent` fill always takes `text-accent-foreground`, never `text-white`.
- Never hard-code a hex colour or `rgba()` glow in a component: use the tokens, or `--shadow-glow-accent` for glows.
- The admin panel is a fixed Paper surface: it uses `ink` for text and accents and `lime` only on Ink fills.

---

## Typography

| Role | Font | Notes |
|------|------|-------|
| Display (titles) | Syne 700–800 | Uppercase on the landing page, tracking −4 % to −2 % |
| Body and UI | Plus Jakarta Sans | |
| Labels | Plus Jakarta Sans 700 | Uppercase |

Both fonts are self-hosted in `frontend/public/fonts` and exposed as `--font-syne` and `--font-jakarta`.

---

## UI Mockups

The following screens illustrate the core user flows of the web platform.

### Screen 1 — User Dashboard

The main landing page after login. Shows recent projects, activity feed, and quick access to key sections (Pull Requests, Issues).

<img width="900" alt="User Dashboard" src="https://github.com/user-attachments/assets/52f3fff9-4d0b-4c5a-ac7b-e93184673332" />

---

### Screen 2 — Project View

The project page. Displays the project's audio & MIDI files organized by category (Vocals, Drums, Synths), with the message of the last version that changed each file and multi-DAW project file support (`.als`, `.rpp`, `.flp`).

<img width="900" alt="Project View" src="https://github.com/user-attachments/assets/ac4a042f-6a2a-4347-90bf-3d628b3aec0a" />

---

### Screen 3 — Audio Preview & File Tree

Continuation of the project view showing the integrated audio player. The waveform player renders directly in the browser (client-side via Wavesurfer.js), allowing producers to play the project's preview (its latest uploaded mixdown) without leaving the platform.

<img width="900" alt="Audio Preview" src="https://github.com/user-attachments/assets/a033e0f3-b074-45ed-b27f-a6056ce6f499" />

---

### Screen 4 — Visual Diff

A key differentiator from standard Git tools. When comparing two versions of an audio file, StemHub surfaces audio-specific metadata changes: EQ adjustments, compression settings, peak levels, RMS averages. Producers can accept or close a pull request with full context.

<img width="900" alt="Visual Diff" src="https://github.com/user-attachments/assets/f77af900-0d57-406e-b7e5-b7bfcfcb2c8f" />

---

### Screen 5 — Pull Request

The musical equivalent of a GitHub Pull Request. Collaborators can leave timestamped comments with embedded audio snippets, reviewers are assigned by role (Vocal Producer, Mixing Engineer, Mastering Engineer), and accepting it is blocked until approval thresholds are met.

<img width="900" alt="Pull Request" src="https://github.com/user-attachments/assets/745ac26d-d220-4eaa-8ce9-1654dec3e5ba" />

---

## Technical Implementation: SSR Hydration Resilience

Since StemHub uses Next.js, components that show browser-only values (the resolved theme, local storage, audio contexts) must render the same markup on the server and during hydration. Use the `useHydrated()` hook from `frontend/lib/useHydrated.ts`: it is `false` on the server and during hydration, `true` afterwards.

```tsx
const hydrated = useHydrated();
const isLight = hydrated && resolvedTheme === "light";
```

It is built on `useSyncExternalStore`, so it doesn't need a `setState` inside a mount effect (which the `react-hooks/set-state-in-effect` lint rule flags).

---

## Design Principles

The interface deliberately adopts patterns familiar to developers (a Git repository's branches, commits and pull requests) while replacing code-specific terminology and visuals with music production equivalents: project, version, save, restore (see the glossary in [SPECIFICATION.md §19](./SPECIFICATION.md#19--glossary)). The goal is to make the learning curve minimal for producers who already collaborate via Discord or WeTransfer, without requiring any knowledge of Git.
