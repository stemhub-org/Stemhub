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

The interface is built around three core colors:

<img width="500" alt="Color Palette" src="https://github.com/user-attachments/assets/03586183-4884-4438-8890-6bb26dd6b587" />

| Name | Hex | Usage |
|------|-----|-------|
| **Purple** | `#9C57DF` | Primary actions, buttons, highlights, active states |
| **Dark** | `#1E1E1E` | App background, dark surfaces |
| **Light** | `#F1F1F1` | Text on dark backgrounds, secondary surfaces |

The purple accent is intentionally distinct from Git/GitHub's green to establish StemHub's own identity in the music production space.

---

## UI Mockups

The following screens illustrate the core user flows of the web platform.

### Screen 1 — User Dashboard

The main landing page after login. Shows recent repositories, activity feed, and quick access to key sections (Pull Requests, Issues).

<img width="900" alt="User Dashboard" src="https://github.com/user-attachments/assets/52f3fff9-4d0b-4c5a-ac7b-e93184673332" />

---

### Screen 2 — Repository View

The project page for a music repository. Displays the file tree organized by stem categories (Vocals, Drums, Synths), with the last commit message per file and multi-DAW format support (`.als`, `.rpp`, `.flp`).

<img width="900" alt="Repository View" src="https://github.com/user-attachments/assets/ac4a042f-6a2a-4347-90bf-3d628b3aec0a" />

---

### Screen 3 — Audio Preview & File Tree

Continuation of the repository view showing the integrated audio player. The waveform player renders directly in the browser (client-side via Wavesurfer.js), allowing producers to preview the latest bounce without leaving the platform.

<img width="900" alt="Audio Preview" src="https://github.com/user-attachments/assets/a033e0f3-b074-45ed-b27f-a6056ce6f499" />

---

### Screen 4 — Visual Diff

A key differentiator from standard Git tools. When comparing two versions of an audio file, StemHub surfaces audio-specific metadata changes: EQ adjustments, compression settings, peak levels, RMS averages. Producers can accept or reject commits with full context.

<img width="900" alt="Visual Diff" src="https://github.com/user-attachments/assets/f77af900-0d57-406e-b7e5-b7bfcfcb2c8f" />

---

### Screen 5 — Pull Request

The musical equivalent of a GitHub Pull Request. Collaborators can leave timestamped comments with embedded audio snippets, reviewers are assigned by role (Vocal Producer, Mixing Engineer, Mastering Engineer), and the merge is blocked until approval thresholds are met.

<img width="900" alt="Pull Request" src="https://github.com/user-attachments/assets/745ac26d-d220-4eaa-8ce9-1654dec3e5ba" />

---

## Design Principles

The interface deliberately adopts patterns familiar to developers (repository structure, branches, commits, pull requests) while replacing code-specific terminology and visuals with music production equivalents. The goal is to make the learning curve minimal for producers who already collaborate via Discord or WeTransfer, without requiring any knowledge of Git.
