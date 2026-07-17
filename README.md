# StemHub

> **Git meets music production** — Version control and collaboration platform for music producers.

StemHub brings Git/GitHub workflows to music production, enabling producers to version their projects, collaborate seamlessly across DAWs, and showcase their work through a unified platform.

*"Git revolutionized code. Figma transformed design. StemHub is here to revolutionize music production."*

---

## What is StemHub?

StemHub solves the version control problem that has plagued music production for decades. No more `project_final_v2_FINAL_real_THISONE.wav` — just clean commits, branches, and merge workflows that developers have enjoyed for 20 years.

**Dual-component system:**
- **StemHub Plugin (DAW)** — Commit, push, pull, and branch directly from your DAW
- **StemHub Platform (Web)** — Cloud storage, collaboration tools, portfolio showcase, and cross-DAW exports

---

## Key Features

- 🔄 **Git-like version control** — Commits, branches, merge, rollback
- ⚡ **Content-Addressed Storage** — SHA-256 deduplicated project storage with manifest-based versioning and incremental uploads
- 🎛️ **Modern DAW Plugin** — JUCE C++17 standalone & VST3 plugin with sleek developer-grade UI, searchable card grid, and offline support
- ☁️ **Cloud-first** — Zero local file management, stream playback
- 🔀 **Multi-DAW export** — Convert between Ableton, FL Studio, Reaper, Logic, Bitwig
- 🤝 **Real collaboration** — Musical pull requests, timestamped comments, track locking
- 🎨 **Portfolio & showcase** — Public profiles to display your work
- 🌍 **Open source projects** — Share, fork, and remix public musical projects

## Documentation

Comprehensive architecture, design system, and deployment documentation is available in [`docs/`](./docs/):
- [Technical Context](./docs/TECHNICAL_CONTEXT.md) — Full stack overview, architecture, security, and accessibility standards
- [Data & API Modeling](./docs/DATA_API_MODELING.md) — PostgreSQL relational schema, Content-Addressed Storage entity graph, and API contracts
- [Design System](./docs/DESIGN_SYSTEM.md) — Web & Plugin visual design system, tokens, and UI mockups
- [Plugin Full Data Flow](./docs/plugin-data-flow.md) — End-to-end JUCE plugin lifecycle, CAS push/pull, and background job architecture
- [Content-Addressed Storage](./docs/content-addressed-storage.md) — Project-scoped blob deduplication, reference counting, and garbage collection
- [Deployment & Resilience](./docs/DEPLOYMENT_RESILIENCE.md) — CI/CD pipelines, startup migration/auth guards, health probes, and backup strategy

Additional guides can also be found in our [Wiki](../../wiki).

---

## Team

**StemHub Team:**

| Member | GitHub |
|--------|--------|
| Erwan SEYTOR (project leader) | [@aernw](https://github.com/aernw) |
| Gabin RUDIGOZ | [@Metchee](https://github.com/Metchee) |
| Dryss MARGUERITTE | [@Dryss10](https://github.com/Dryss10) |
| Hubert TOURAINE | [@HubertTouraine](https://github.com/HubertTouraine) |
| Raphaël CHANLIONGCO | [@raprapchh](https://github.com/raprapchh) |
| Jean Baptiste BOSHRA | [@JeanBsh](https://github.com/JeanBsh) |

---

## License

**© 2026 StemHub Team. All Rights Reserved.**

This repository and all its contents are the intellectual property of the StemHub Team. Unauthorized reproduction, commercial use, or derivative works are prohibited without written permission.

---

<p align="center">
  <strong>StemHub</strong> • <em>Version control for music producers</em>
</p>
