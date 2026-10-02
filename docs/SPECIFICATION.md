# StemHub — Project Specification

> **Status:** Living document — canonical reference for the whole EIP year (2026 → 2027).
> **Owner:** Erwan (Project Leader).
> **Last updated:** 2026-09-29.
> **Scope:** This document consolidates the vision, scope, requirements, success criteria, and ways of working. It links to the detailed technical docs rather than duplicating them.

---

## How to use this document

- Any team member should be able to open this file and understand **what** we're building, **why**, **for whom**, and **how we know we're done**.
- Section headings are stable. When something changes, edit in place and add a line to the **Change Log** at the bottom.
- Deep-dive topics (data model, CAS design, StemHub plugin data flow, roadmap, costs) live in dedicated docs — this file links to them but does not duplicate their content.
- Items marked **TBD** are decisions we haven't taken yet. They must be resolved by the milestone noted next to them.
- Words for StemHub's concepts (project file, asset, version, branch, save, restore, insert…) follow the **Glossary** in [§19](#19--glossary). Code, UI and docs use the same terms.

---

## Table of contents

1. [Vision & positioning](#1--vision--positioning)
2. [Scope & MVP](#2--scope--mvp)
3. [Feature catalogue (MoSCoW)](#3--feature-catalogue-moscow)
4. [Personas & use cases](#4--personas--use-cases)
5. [Success criteria & KPIs](#5--success-criteria--kpis)
6. [Architecture & tech stack](#6--architecture--tech-stack)
7. [Data model, API, storage](#7--data-model-api-storage)
8. [Open technical decisions](#8--open-technical-decisions)
9. [Testing & quality](#9--testing--quality)
10. [Deployment & environments](#10--deployment--environments)
11. [Security, privacy, compliance](#11--security-privacy-compliance)
12. [Team, roles & ways of working](#12--team-roles--ways-of-working)
13. [Beta program](#13--beta-program)
14. [Design & UX](#14--design--ux)
15. [Community & communication](#15--community--communication)
16. [Business model](#16--business-model)
17. [Roadmap & milestones](#17--roadmap--milestones)
18. [Risks](#18--risks)
19. [Glossary](#19--glossary)
20. [Change log](#20--change-log)

---

## 1 — Vision & positioning

### Pitch (one line)
Version history and teamwork for music, so no one ever loses a good idea again.

### Mission
Creativity shouldn't be held back by lost files, messy exports, or distance. StemHub exists to give music producers the same safety net and collaboration tools that transformed software and design, so they can experiment freely, never lose an idea, and create together from anywhere.

### What StemHub is NOT
- **Not a DAW.** StemHub does not make, edit, or play music. It lives inside FL Studio, Ableton, and others as the StemHub plugin and stays out of the creative process.
- **Not a streaming or distribution platform.** Portfolios and playback exist so producers can show and review work, not to publish releases to listeners or compete with Spotify, SoundCloud, or DistroKid.
- **Not an AI music generator.** Explicitly excluded (backlog US-100) for legal and copyright reasons. This is a trust signal for producers worried about their music being used for training.
- **Not a real-time co-editing tool (for now).** Collaboration works through saved versions, restores and reviews (Git-style), not Google-Docs-style simultaneous editing. Deferred to v2 (backlog US-99).
- **Not a generic cloud drive or file transfer service.** It versions music projects specifically. It is not a Dropbox or WeTransfer replacement for arbitrary files.
- **Not a rights or royalty management tool.** StemHub does not handle contracts, splits, or copyright registration, even though it records who contributed what.
- **Not a mixing or mastering service.** No audio processing, no effects, no automated mastering.

### Top 3 differentiators
1. **Runs inside the DAW producers already use.** BandLab's versioning only covers projects made in BandLab. Drive, Dropbox, and WeTransfer don't understand music projects at all. The StemHub plugin runs inside FL Studio and Ableton, so professionals keep their tools and get versioning where they work (US-05). This is the strongest edge against BandLab.
2. **Understands music projects, not just files.** Drive and Dropbox see an `.flp` as an opaque blob: they can restore the whole thing but can't tell you what changed. StemHub parses project files (via `PyFLP_v2`), so it can show track-level changes, restore a single bassline (Mabé's use case), and send only new audio (Laura's use case). A 2 GB WeTransfer upload becomes a light save.
3. **A true review workflow for collaboration.** WeTransfer is one-way. Drive and Dropbox cause overwrite conflicts. StemHub brings branches, "musical pull requests," timestamped comments (v1.1), and track locking, so collaborators propose changes and the owner decides what goes in. BandLab lets people fork and edit but has no structured propose-review-accept step — which matters for professional teams and labels (the Studio plan).

Full feature-by-feature comparison against Splice, Git/GitHub, Dropbox/Drive, and Sessionwire: [COMPARATIVE_BENCHMARK.md](./COMPARATIVE_BENCHMARK.md).

---

## 2 — Scope & MVP

### MVP definition (what must ship for the July 2027 Greenlight jury)

The MVP is the smallest system that lets a producer keep their music safe, collaborate with someone else, and see what changed — end-to-end, on the production environment.

1. **Account and access.** Sign up and log in on the web, sign in with the same account in the StemHub plugin. JWT / HttpOnly cookie + Google OAuth on a deployed server (not localhost).
2. **Create a project from the DAW.** In FL Studio, open the StemHub plugin, select the `.flp`, create the project.
3. **Save a version.** Message → Save → files uploaded, encrypted on GCS, appears in history.
4. **See history in both places.** Same version list in the StemHub plugin and on the web dashboard, with author, date, message, and an audio preview on the web (hear a version without opening the DAW).
5. **Restore a previous version into the DAW.** Not just download a file — actually reopen the chosen version in FL Studio. This is Mabé's use case and blocks the demo if missing.
6. **Branches.** Create a branch, save to it, switch back to main.
7. **Two-person collaboration.** Invite a second user; they restore the latest version, add something, save; the owner sees the new version attributed to the collaborator and can comment on it.
8. **A basic "what changed" summary.** Using `PyFLP_v2`, show tracks and channels added, removed, or renamed between two versions. Simple, but it proves we understand music projects rather than storing files.

### DAW support
- **MVP:** FL Studio only, VST3 build.
- **Ableton:** stretch goal — added only if FL Studio work finishes with real slack, otherwise deferred.
- **Format at MVP:** the project file plus all its audio & MIDI assets, stored content-addressed.

### Platform / plugin format
- **VST3** only for MVP — supported by all target hosts on both macOS and Windows.
- macOS build is signed and notarized (Apple developer account is a hard MVP requirement — see [§10](#10--deployment--environments)).
- Windows build is unsigned for the beta (shows warnings, does not block install).

### Post-MVP (deferred to v1.1 or later)
- Full automatic merge engine (arrangement, automation, mixer). Close to a research problem — deferred until real user demand.
- More DAWs (Pro Tools, Logic, Reaper, Bitwig). Each requires its own parser.
- Public projects and forking. Requires licensing metadata and moderation.
- Cross-DAW export (limited to audio stems and basic structure).
- Public API and webhooks (Studio plan feature).
- Distribution-service integrations (linking out, never becoming one).

---

## 3 — Feature catalogue (MoSCoW)

Priorities for the MVP shipping to the Greenlight jury. Reviewed at every mentorat session.

| Feature | Priority | Rationale |
|---|---|---|
| Save a version | **Must** | Core of the product; already implemented in the StemHub plugin. |
| Restore into DAW | **Must** | Must actually reopen the version, not just refresh the history list. |
| Branches (create, switch, save to) | **Must** | Proves safe experimentation. Includes "accept PR" as branch promotion. |
| StemHub plugin UI in the DAW | **Must** | Main differentiator vs BandLab, Drive, Dropbox. |
| Web history browser | **Must** | Lets the jury and collaborators see versions without opening a DAW. |
| Collaborator invites | **Must** | Without a second user, StemHub looks like a backup tool. |
| Comments on a version | **Must** | Simple, per-version. Timestamped comments deferred to v1.1. |
| Diff view (text summary) | **Must** | Track adds/removes/renames via `PyFLP_v2`. Visual/audio diff is a Could. |
| Web waveform preview | **Must** | Hearing a version without the DAW is part of the demo. Cheap with Wavesurfer.js. |
| Pull requests | **Should** | In scope for M2 (before the jury). "Accept PR" = branch promotion. Automatic merge stays Won't. |
| MFA (TOTP) | **Should** | First security feature after the core loop; ship before the beta if time allows. |
| Notifications (in-app) | **Should** | Email & push notifications are Could. |
| Offline mode | **Could** | MVP shows a "connection lost" state. Queued saves come later. |
| Explore / discover page | **Could** | Endpoints exist, but the page uses mock data. Hide until content is real. |
| Public projects | **Could** | Requires licensing metadata and privacy controls first. |
| Licensing metadata | **Could** | Prerequisite for public projects and forks. |
| Automatic merge engine | **Won't (school year)** | Research-grade problem; branch promotion + PRs cover the workflow. |
| Forks | **Won't (school year)** | Depends on public projects, licensing, moderation. |
| Mobile app | **Won't (school year)** | Production happens on desktop; responsive web covers mobile review. |

---

## 4 — Personas & use cases

Four detailed personas live in [PERSONAS_USE_CASES.md](./PERSONAS_USE_CASES.md).

| Persona | Role | Primary pain point | Key use case for MVP |
|---|---|---|---|
| Tom Beats | Young producer, FL Studio & Ableton | "Save As" version chaos | Branch-based experimentation |
| Frank Bass | Session bassist | Lost takes in `.zip` archives | Restore a specific old track |
| Mabé | Bedroom beatmaker | Overwrites & disk anxiety | Restore a deleted bassline |
| Laura | Sound designer | Heavy exports for remote collab | Upload-only-what-changed collaboration |

**Primary MVP persona:** **Tom Beats** — chosen because his workflow exercises every core MVP feature (project creation, branches, save, restore, collaboration).

---

## 5 — Success criteria & KPIs

### 5.1 Product KPIs (measured across the beta)

Success at the jury means real producers use the core loop and come back — not vanity numbers.

| KPI | Target |
|---|---|
| Beta testers recruited | 20 |
| Beta testers activated (≥1 saved version) | 15 |
| Weekly active testers (≥1 saved version/week for 4 consecutive weeks) | ≥10 |
| Onboarding time (signup → first saved version) | <10 minutes |
| Total versions saved | ≥200 |
| Restores performed by testers | ≥20 (validates "never lose an idea") |
| Projects with ≥2 contributors | ≥10 (validates collaboration, not just backup) |
| Versions lost or corrupted | **0 (hard requirement)** |
| System Usability Scale (SUS) | ≥70 |
| Testers signalling willingness to pay for Pro | ≥3 (waitlist, pre-order, explicit ask) |

All product analytics must be **EU-hosted with explicit user consent** to match our GDPR promise (see [§11](#11--security-privacy-compliance)).

### 5.2 Technical KPIs

Reference project sizes for perf measurement:

| Size | Description |
|---|---|
| Small | Project file only, <5 MB |
| Medium | ~200 MB with its samples and recordings |
| Large | ~1 GB |

Measured on a fixed **20 Mbps upload** connection.

**Latency:**
- Small project: save and restore complete in <5 s at p95.
- Medium & large: StemHub adds <5 s beyond raw transfer time; uploads are resumable.
- History refresh in the StemHub plugin: <1 s.
- Non-upload backend endpoints: <300 ms p95.

**StemHub plugin (real-time safety):**
- No work on the audio thread (no allocations, no locks, no network calls).
- <100 MB memory at idle.
- Streaming upload (memory does not grow with project size).
- Passes `pluginval --strictness-level 5 --rtcheck`.
- Never causes audio dropouts or DAW freezes.

**Service:**
- 99% monthly uptime during beta, 99.9% during demo week.
- ≥98% save success rate; every failure surfaced to the user.
- RTO 2 h; RPO 24 h (no loss of stored files once a version is saved).
- Login, save, history, restore covered by automated tests in CI.

**Delivery-blocking engineering task:** move uploads from "through the Python server" to **signed URLs directly to storage** (GCS presigned PUT). Required to hit the large-project latency budget.

### 5.3 Beta outcome interpretation

**Success** (all of):
- ≥10 testers still saving versions weekly at end without being chased.
- 0 testers lost a version or had data leaked.
- ≥5 real two-person collaborations happened.
- ≥50% of testers who restored a version say it saved them real work.
- The 5-minute demo runs on the production environment.

**Partial / fixable:**
- Testers activate but don't return → onboarding or StemHub plugin friction.
- Saving too slow on large projects → known engineering path (signed URLs, resumable uploads, compression).

**Failure signals** (require repositioning or major rework):
- Any tester loses work because of StemHub → **all new features paused until fixed.**
- <5 testers active after 4 weeks despite fixes.
- Testers only use it as backup and never collaborate → we're competing with Dropbox and should reposition.
- Testers consistently prefer BandLab or WeTransfer.

A failed beta is still valuable for the jury if we can show what we measured, what we learned, and what we changed.

---

## 6 — Architecture & tech stack

Full detail lives in [TECHNICAL_CONTEXT.md](./TECHNICAL_CONTEXT.md) and [plugin-data-flow.md](./plugin-data-flow.md). Summary:

### Components
- **Backend** — FastAPI + Python 3.10+, async SQLAlchemy 2.0, Alembic migrations, JWT + Google OAuth2, pluggable storage layer.
- **Frontend** — Next.js 16 App Router (React 19), TypeScript, Tailwind CSS, shadcn/ui, `next-themes`, Wavesurfer.js.
- **StemHub plugin** — JUCE C++17 VST3, state-machine driven UI, background job coordinator, dependency-injected API client, saved sign-in (credential store).
- **Database** — PostgreSQL (relational; Git-like history requires strict FK integrity).
- **Object storage** — Google Cloud Storage (production), local filesystem (dev). AES-256 at rest.
- **Auth** — JWT with HttpOnly cookies, Google OAuth2, optional TOTP (post-MVP).

### Cross-cutting patterns (from CLAUDE.md)
- **Backend:** async-first, dependency injection, soft deletes (`is_deleted` + `deleted_at`), Pydantic validation at boundaries.
- **Frontend:** centralized `authFetch` for all API calls (token + error handling), App Router file-based routing, theme provider.
- **StemHub plugin:** one `StemhubSession` owning the state (`UIState`, `OperationState`), background jobs tagged with a request epoch and ended by one rule, `ChangeBroadcaster` for editor updates, `IProjectApi` for testability.

### Standards
- **Accessibility:** WCAG 2.1 AA for the web app.
- **API contract:** OpenAPI at `/docs`.
- **Code style / testing:** governed by the project's global rules and `~/.claude/rules/`.

---

## 7 — Data model, API, storage

Full detail:
- **Data model & API:** [DATA_API_MODELING.md](./DATA_API_MODELING.md).
- **Content-addressed storage design:** [content-addressed-storage.md](./content-addressed-storage.md).
- **StemHub plugin request lifecycle:** [plugin-data-flow.md](./plugin-data-flow.md).

### Core entities

```
User → Project → Branch → Version (parent_version_id → Version)
Project → Collaborator → User
Project → Blob (sha256, ref_count) — content-addressed, project-scoped
Version.manifest_json → Blob (project file + assets, by sha256)
```

Key design decisions:
- **Version chain** via self-referential `parent_version_id` (Git-like).
- **Content-addressed storage** at blob level, **project-scoped** (not global) — global dedupe was rejected because of the privacy oracle attack.
- **Reference counting** on blobs, with eager decrement and a periodic GC sweep as safety net.
- **Manifests** (JSONB) list a version's project file and assets and reference their blobs by SHA-256; the paths inside a manifest only say where each file goes in the project folder, never where its bytes are stored.
- **Uploads:** MVP goes through FastAPI; **before the jury**, move to signed direct-to-GCS URLs to hit the latency budget.

---

## 8 — Open technical decisions

Resolutions to open questions raised during spec review:

| Question | Decision |
|---|---|
| Which file formats at MVP? | The project file plus all its audio & MIDI assets, stored content-addressed. |
| Max project size at MVP? | **2 GB per save** (new data per save). Applies across Free and Studio tiers; pricing page updated accordingly. |
| Pull requests? | Promoted from Could to **Should** — in scope for M2 (before the jury). "Promote branch to main" is renamed **"accept PR."** Automatic merge remains **Won't** for this school year. |
| Concurrent saves to the same branch head? | **Optimistic concurrency.** When two users save onto the same branch head at the same time, exactly one succeeds. The other receives a conflict, saves to a new branch, and both versions exist with correct content. |
| Merge strategy for the school year | No 3-way merge. Branch promotion + PR review only. |

---

## 9 — Testing & quality

### 9.1 Coverage targets

| Component | Target | Notes |
|---|---|---|
| Backend | 80% | pytest + coverage; enforced in CI. |
| Frontend | 80% | Component + integration + Playwright. |
| StemHub plugin (non-UI: `StemhubSession` state, saved sign-in, API client, version upload & restore) | 60% | Lower than global rule; DSP/UI is harder to test. |
| StemHub plugin (UI) | No coverage target | Validated by `pluginval --strictness 5 --rtcheck` + manual QA checklist. |

### 9.2 Testing strategy

Hybrid approach: **automate the core loop at every layer possible; keep manual QA only for running the StemHub plugin inside a real DAW.**

**Backbone — API-level E2E in pytest** against the real Docker Compose stack (PostgreSQL + localfs). Walks the full demo flow: signup → create project → branch → save a real `.flp` → fetch history → compare versions → restore with checksum verification → invite second user → comment. Checksum verification on every download backs the "zero lost versions" KPI.

**Web — Playwright** (chosen over Cypress because it handles multiple users in one test — required for collaboration flows). Limited to 4–5 key specs with data seeded through the API.

**StemHub plugin — headless C++ test harness** that drives the processor directly against the backend (sign in, save, refresh, restore). Plus unit tests with a fake `IProjectApi` for error cases. `pluginval --strictness-level 5` runs in CI to catch crashes and audio-thread issues.

**Cross-surface** — one test saves a version from the StemHub plugin harness and confirms it appears on the web.

**Manual QA checklist** (FL Studio can't be automated) — run on Windows and Mac before each beta release and before the jury: load, sign in, save, restore, branch switch, no audio dropouts. Plus a rehearsal of the 5-minute demo.

**Priority order:** API E2E → pluginval + StemHub plugin harness → Playwright (once web screens stabilize).

### 9.3 Mandatory E2E scenarios

**Happy paths:**
1. **Signup → create project → save → restore.** New user signs up, creates a project from a real `.flp`, saves v1, saves v2, restores v1, gets byte-identical content (checksum match). Run at API level and through the StemHub plugin harness.
2. **Branch → save → switch back.** Create a branch, save to it, switch to main, confirm main's history and latest version are unchanged. Guards against versions leaking between branches.
3. **Collaboration round-trip.** User A invites User B; B accepts, restores A's latest, saves a change; A sees B's version attributed to B and can comment. Run at API level and in Playwright with two browser contexts.
4. **StemHub plugin save → web shows it.** Cross-surface: a version saved from the StemHub plugin harness appears on the web history page with correct message, author, audio preview.
5. **Compare two versions.** Save two fixture `.flp` files with a known difference (a track added); the change summary reports exactly that.

**Failure and safety flows:**

6. **Access control.** A non-collaborator gets 404 when trying to read, download, save to, or comment on someone else's project — including by guessing IDs. Given the pitch about leaked demos, this matters as much as the happy path.
7. **Interrupted save.** A save uploads the files the server doesn't have yet, then creates the version from its manifest. Simulate the upload failing partway; check that no broken version is left in history (or that it's clearly marked failed and not restorable). Otherwise a network drop creates exactly the "lost version" our beta criteria call a failure.
8. **Expired or invalid sign-in session.** An expired token during a save produces a clear "sign in again" state in the StemHub plugin; nothing gets half-saved.
9. **Removed collaborator.** After A removes B, B can no longer restore, save to, or see the project.
10. **Project deletion.** Deleting a project removes it from history, blocks downloads, deletes its stored files (blobs). Backs the GDPR "right to erasure" commitment.

### 9.4 Definition of Done

A feature is **Done** when:
- Code is merged on `dev`.
- Tests exist and pass in CI at the coverage targets above.
- Documentation is updated where the change affects public behavior.

---

## 10 — Deployment & environments

### 10.1 Environments
- **dev** — local Docker Compose stack (Postgres + localfs).
- **staging** — Cloud Run in EU region, auto-deploy on merge to `dev`.
- **production** — Cloud Run in EU region, deploy from `main` after **manual approval**.

### 10.2 Hosting (MVP suggested setup)

| Layer | Choice | Notes |
|---|---|---|
| Backend | **Cloud Run** (EU: `europe-west9` Paris or `europe-west1`) | Serverless HTTP; scales to zero for staging. |
| Frontend | Cloud Run (same region) | Vercel is a reasonable alternative for Next.js, but one provider is simpler to explain to the jury. |
| Database | **Cloud SQL for PostgreSQL** (smallest tier) | Automated daily backups + PITR **on**. |
| Object storage | **GCS** (existing bucket) | Object versioning **on**. |
| Secrets | **Secret Manager** | No secrets in the repo, no plaintext env vars in Cloud Run. |
| Migrations | **Cloud Run Job**: `alembic upgrade head` runs before each deploy | App refuses to start with pending migrations. |
| CI/CD | GitHub Actions | Build → test → deploy staging → manual promotion to prod. |

### 10.3 Rollout strategy
- Deploy `main` → **staging** → (manual approval) → **prod**.
- **Instant rollback** available.
- A few **lightweight feature flags** for demo-day safety.
- **No canary releases** at MVP — not worth the complexity yet.

### 10.4 Plugin distribution to beta testers
- **macOS:** the StemHub plugin is signed & notarized (Apple Developer account is a hard requirement — unsigned plugins block real users on macOS).
- **Windows:** unsigned for the beta — SmartScreen warnings are annoying but not blocking. Windows signing certificate is post-beta.

---

## 11 — Security, privacy, compliance

### 11.1 Data residency & GDPR promise
All user data — project files, database, backups, logs — is stored and processed in a **single EU Google Cloud region**. Not strictly required by GDPR, but a core trust promise to producers.

Project files and their assets are treated as **personal data**: they can contain usernames, sample paths, and voice recordings.

### 11.2 Privacy policy commitments (kept separate from ToS)
- EU storage with a published list of sub-processors.
- Users keep full ownership of their music.
- **No AI training on user content without explicit consent.**
- No selling of data. No ads.
- One-click export of all projects and versions.
- Deletion from the service immediately; purged from backups within 35 days.
- Breach notification to the CNIL within 72 hours.
- Minimum age: 15.
- Named privacy contact.
- **Backup retention aligned to 35 days** to match the deletion promise (overrides longer retention in current resilience doc).
- Final wording reviewed by a legal advisor before the beta.

### 11.3 Retention & deletion
> "Permanently deleted after **30 days in the trash**, or **immediately on account deletion or erasure request**, and **purged from backups within 7 days after that**."

Free vs paid tiers use the same policy at MVP.

### 11.4 Copyright & content policy
- **No automated copyright scanning at MVP.** Projects are private by default. Producers routinely store licensed samples; StemHub does not inspect private content and acts only on reports.
- **Notice & Takedown** flow: documented at MVP, implemented reactively.
- **Trigger for stronger obligations:** when public projects, forks, or explore launch, StemHub becomes a content-sharing platform (EU copyright directive art. 17 territory). A **content policy review is a hard prerequisite** for any of those features. Final wording reviewed by the legal advisor before the beta.

### 11.5 MFA
- **Optional TOTP (authenticator app)** is the **first security feature added after the core loop works**, ideally before the beta if time allows.
- Full MFA (WebAuthn, backup codes) is post-MVP.

### 11.6 Security baseline
- AES-256 at rest (GCS default).
- TLS everywhere.
- JWT in HttpOnly cookies (no localStorage for tokens on the web where possible).
- `SECRET_KEY` startup validation rejects weak/default keys.
- OWASP Top 10 review before public launch.
- pluginval `--rtcheck` in CI for audio-thread safety.

Further detail: [DEPLOYMENT_RESILIENCE.md](./DEPLOYMENT_RESILIENCE.md), [RISK_MANAGEMENT.md](./RISK_MANAGEMENT.md).

---

## 12 — Team, roles & ways of working

### 12.1 Team & ownership

| Member | Role | Primary area |
|---|---|---|
| **Erwan** | Project Leader | Owns spec, understanding of every part, architectural validation |
| **Raphaël** | Engineer | Backend |
| **JB** | Engineer + Community co-lead | Frontend, community |
| **Dryss** | Engineer | Frontend |
| **Hubert** | Engineer | Backend + Frontend |
| **Gabin** | Engineer | Frontend + StemHub plugin |

### 12.2 Decision-making
- **Architectural decisions are made by Erwan.** The team can and should discuss and propose alternatives; Erwan validates.
- Non-architectural implementation choices are made by the area owner.

### 12.3 Cadence
- **Sprints:** 2 weeks.
- **Sprint review & planning:** Monday.
- **Daily async updates:** Discord.
- **Written culture:** documentation and reports on Notion.
- **Team workspace:** [StemHub Notion](https://www.notion.so/tude-compl-te-Ressources-Dev-2f517c19845381d09717cb09c9bd698f?source=copy_link).

### 12.4 Definition of Done
See [§9.4](#94-definition-of-done): merged on `dev` with tests.

### 12.5 Code review
Every PR requires review from at least one other team member before merge. Merges to `main` require Erwan's approval.

---

## 13 — Beta program

### 13.1 Who
- **Target: 20 beta testers** (Epitech deliverable, end of March 2027).
- Sourcing: producers we know personally, friends who produce music, music schools, Discord communities. Anyone who would use the service is welcome.

### 13.2 Incentive
- **Max-tier subscription (Studio), free for life.**

### 13.3 Feedback channels
- **Web form** (structured feedback, SUS survey, bug reports).
- **Dedicated Discord channel** (async conversations, quick reports).

### 13.4 Cadence
- Beta launch: **March 2027** (per roadmap).
- Weekly check-in during the beta.
- Mid-beta review at the end of April.
- Final results & personas documented by **end of May 2027**.

---

## 14 — Design & UX

### 14.1 Visual identity
- **TBD** — logo, color system, typography are still being decided. **Must be locked by end of November 2026** to match the Figma v1 deliverable (December 2026).
- **Working board:** [StemHub Figma](https://www.figma.com/board/fq9wtwZCWY9xfX0eHkmMd8/stemhub?node-id=0-1&t=iJ0XTCjBf4emb178-1) — visual design system, UI mockups, comparative study, audits, and user flow diagrams.

### 14.2 Reference products
- **Primary:** GitHub — but adapted to music (more visual, more entertaining).
- Secondary: Splice (music-industry familiarity), Figma (collaboration mental model).

### 14.3 Surface priority
- **Web app and StemHub plugin are almost equal in importance.**
- **StemHub plugin:** producing (create, save, restore, branch).
- **Web app:** collaboration, project overview, history browsing, comments, PRs.

### 14.4 Accessibility
WCAG 2.1 AA target. Full breakdown in [TECHNICAL_CONTEXT.md § Accessibility](./TECHNICAL_CONTEXT.md).

### 14.5 Design system
Frontend tokens and primitives live in [DESIGN_SYSTEM.md](./DESIGN_SYSTEM.md).

---

## 15 — Community & communication

### 15.1 Channels (Epitech deliverable: ≥2 public channels)
- **Confirmed intention:** Discord (community hub).
- **Under consideration:** Instagram, TikTok, X.
- **Decision required by:** end of October 2026 (Epitech "Canaux & Identité" milestone).

### 15.2 Editorial line
- Tutorials.
- Content that generates engagement (workflow tips, artist stories, dev-log posts).
- Cadence: **≥2 posts per month per channel** (Epitech requirement — no empty months, screenshots archived).

### 15.3 Community lead
- **JB** owns community. A co-lead may be added.

---

## 16 — Business model

Full detail: [COST_AND_REVENUE.md](./COST_AND_REVENUE.md).

### 16.1 Pricing (freemium)

| Plan | Price | Storage | Key features |
|---|---|---|---|
| Free | $0/mo | 2 GB | 3 projects, basic version control |
| Creator | $9/mo | 50 GB | Unlimited projects, collaboration, multi-DAW export |
| Pro | $19/mo | 200 GB | Open-source projects, pull requests, analytics |
| Studio | $49/mo | 1 TB | Teams, advanced permissions, priority support |

> Pricing page must reflect the **2 GB per-save cap** at all tiers (see [§8](#8--open-technical-decisions)).

### 16.2 Unit economics
- **Cost per user at scale:** ~$0.23/mo (stable from 1K → 10K users).
- **Break-even:** ~300–500 paying users.
- **Projected margin at 10K users (10% paid):** 88% gross margin.

---

## 17 — Roadmap & milestones

Full Gantt + Epitech deliverables table: [ROADMAP.md](./ROADMAP.md).

### 17.1 Tech milestones (from ROADMAP.md)
| Milestone | Window | Deliverable |
|---|---|---|
| **M1** | Sep–Oct 2026 | Canonical JSON schema + PR base |
| **M2** | Nov–Dec 2026 | Merge engine + Web PR diff |
| **M3** | Jan–Feb 2027 | Cloudflare R2 (or GCS) + StemHub plugin packaging |
| **M4** | Mar–May 2027 | Beta support + hotfixes |
| **M5** | May–Jun 2027 | Consolidation + optimizations |

### 17.2 Hard external deadlines
- **19 Oct 2026** — Mentorat S3: lock the 2 complementary objectives (no changes after).
- **Dec 2026** — Figma v1 interactive prototype.
- **End Mar 2027** — 20 beta testers onboarded.
- **End Apr 2027** — Figma v3 final.
- **End May 2027** — Beta results, restitution, UX bilan.
- **July 2027** — Greenlight jury (final).

---

## 18 — Risks

Full register: [RISK_MANAGEMENT.md](./RISK_MANAGEMENT.md).

Top risks specific to this specification:

| Risk | Impact | Mitigation |
|---|---|---|
| A tester loses a version because of StemHub | **Catastrophic** for trust and jury narrative | Zero-loss is a **hard product KPI**; all new work pauses if it happens. Checksum verification on every download. Interrupted-save safety test. |
| StemHub plugin causes audio dropouts in FL Studio | Trust-breaking for producers | Real-time-safety rules enforced (`pluginval --rtcheck` in CI). No allocations / locks / I/O on audio thread. |
| Signed URLs migration not done in time for large-project targets | Save latency KPI misses at demo | Prioritize the migration by end of M3 (Feb 2027). Fall back on smaller demo fixtures if late. |
| Ableton MVP support attempted but not finished | Scope creep threatens FL Studio quality | Ableton is a **stretch goal only**. FL Studio quality never traded for Ableton coverage. |
| Visual identity locked late | Blocks Figma v1 (Dec 2026 deliverable) | **Hard deadline: end of November 2026** for identity decisions. |
| Legal review of privacy policy / content policy is late | Blocks beta launch (March 2027) | Legal review must complete **before beta**, target end of February 2027. |
| Only 1 backend engineer (Raphaël) | Bus factor | Hubert covers backend as secondary owner. Erwan reviews all backend architecture. |

---

## 19 — Glossary

These are the words for StemHub's concepts, everywhere: code identifiers, UI copy, API fields, comments, tests and docs. UI labels that differ from the term are given in parentheses.

The **Replaces** column lists the words not to use for that concept. It doesn't ban a word in its own meaning: a recording *take*, the Git *merge* of our own code, a *session* musician, a musical *note*. Code that mirrors FL Studio or PyFLP (the FL mixer reader, for example) may keep FL's names, and code that reads the legacy manifest v1 keeps its field names (`tracks`, `filename`).

### 19.1 Project and files

| Term | Meaning | In FL Studio / Ableton Live | Replaces |
|---|---|---|---|
| **Project** | StemHub's container for one piece of music: its branches, versions and collaborators. | — | repository, repo, song |
| **Project file** | The DAW document itself. | FL: the `.flp`. Ableton: the Live Set (`.als`). | DAW file, project binary, `flp` (in DAW-neutral code), session |
| **Project folder** | The folder holding the project file. A save collects the assets in it and its subfolders. | FL: the folder of the `.flp`. Ableton: the Project folder that "Collect All and Save" fills. | session bundle, bundle |
| **Asset** (UI: "Audio & MIDI files") | Any audio or MIDI file of the project besides the project file: samples, recordings, MIDI files. | FL: the samples and recordings its channels and audio clips play. Ableton: the files under the Project's `Samples/` folder. | track, stem, "the other files", sample (meaning all files) |
| **Stem** | A rendered audio file of one part of the mix, usually a group of related tracks (drums, bass, vocals), with its effects printed, exported full length from the song's start so that all stems played together rebuild the mix. In StemHub terms it is the rendered output of one insert or group track. StemHub doesn't produce stems yet (a future "stem export"); a stem a producer exported into the project folder is just an asset to StemHub. | FL: Export with "Split mixer tracks" (one file per insert). Ableton: Export Audio/Video with Rendered Track "All Individual Tracks" (one file per track, group tracks included). | "stems" for raw project files: samples, recordings and MIDI are not stems |
| **Mixdown** | A rendered audio file of the full mix: the whole song in one stereo file. | FL: Export of the song through the Master. Ableton: Export Audio/Video of the Main track (Master before Live 12). | stems (for the full mix), master (for the file) |
| **Manifest** (UI: file list) | The JSON list of a version's project file and assets, each by SHA-256, size and path in the project folder. Format in [content-addressed-storage.md](./content-addressed-storage.md). | — | snapshot_manifest, bundle, artifact |
| **Blob** | Stored bytes, addressed by their SHA-256: one per unique file per project (project-scoped). Internal; users never see the word. | — | artifact |
| **CAS** | Content-Addressed Storage: each unique file stored once per project as a blob, and versions refer to blobs through their manifest. See [content-addressed-storage.md](./content-addressed-storage.md). | — | — |
| **Project model** | The DAW-neutral JSON of a project's musical content (#170), schema 0.1.0. Format: [project-model.md](./project-model.md). | — | mixer_state, mixer snapshot |

### 19.2 Versions and collaboration

| Term | Meaning | In FL Studio / Ableton Live | Replaces |
|---|---|---|---|
| **Version** | An immutable saved state of a project on a branch: its manifest, message, author, date and parent version. | — (FL's "Save new version" writes another file instead) | commit, snapshot, change, take, "Saved" |
| **Branch** | A named line of versions (e.g. `main`, `feature-fast-tempo`). | — | workspace, lane, take, timeline |
| **History** | A branch's versions, newest first. | — | changes (meaning versions), timeline |
| **Head** | A branch's newest version. | — | — |
| **Save** (a version) | The StemHub plugin action that creates a version on the open branch from the working copy. | Not the DAW's own save (Ctrl/Cmd+S), which only writes the project file. | commit, push (in UI), sync, "save snapshot" |
| **Message** | The text a version is saved with. | — | commit message (in UI), save note, note |
| **Restore** | Download a version and open it in the DAW as a separate copy, in a new folder. The working copy is never overwritten. | — | pull, checkout, roll back, sync |
| **Refresh** | Reload a branch's history. Moves no files. | — | sync, pull |
| **Working copy** | The local project file the StemHub plugin saves from. | — | local file, local copy, working file (in UI) |
| **Diff** / **Change** | A comparison of two versions / one typed difference in it (e.g. an insert's volume changed). | — | "changes" meaning versions |
| **Pull request (PR)** | A proposal to bring one branch into another, subject to owner review. Its status is `OPEN`, the only non-terminal state, until it is accepted (`MERGED`) or closed without being accepted (`CLOSED`). `MERGED` and `CLOSED` are terminal: a closed PR is never reopened, users open a new one. | — | — |
| **Accept PR** | The MVP way to take a pull request: branch promotion (renamed from "promote branch to main"). No content merge and no automatic 3-way merge (see [§8](#8--open-technical-decisions)). | — | merge, smart merge |
| **Preview** | A mixdown uploaded to StemHub so people can listen on the web without the DAW (one per project today). | — | stems, export |
| **Download** | Fetch stored files from StemHub, unchanged. | — | export, quick export, pull |
| **Export** / **Render** (bounce) | Produce new files from the project, as every DAW uses the word: render audio (a mixdown or stems) or, post-MVP, a project for another DAW ("cross-DAW export", [§2](#2--scope--mvp)). Always makes something new, never a plain download. | FL: Export. Ableton: Export Audio/Video. | export (meaning a download) |

### 19.3 Music terms (inside the project file)

DAW-neutral names for what a project file contains. The FL mixer diff uses them today and the project model (#170) will use them throughout; #170 also normalizes the units.

| Term | Meaning | In FL Studio / Ableton Live | Replaces |
|---|---|---|---|
| **Instrument** | A sound source that plays notes: a synth or a sampler. | FL: a channel of the Channel Rack. Ableton: an instrument device on a MIDI track. | channel (in DAW-neutral code) |
| **Pattern** | A block of notes that can be placed in the arrangement. | FL: a pattern. Ableton: the nearest equivalent is a MIDI clip. | — |
| **Note** | One musical note: pitch, start, length, velocity. | FL: a Piano roll note. Ableton: a MIDI note. | — (a version's text is its *message*, never a note) |
| **Arrangement** | The song laid out over time. | FL: the Playlist. Ableton: the Arrangement View. | playlist, timeline (in DAW-neutral code) |
| **Track** | One lane of the arrangement. Never an audio file (asset), a mixer insert or a whole song. | FL: a Playlist track. Ableton: a track (audio, MIDI, group or return). | mixer track (that is an insert) |
| **Clip** | A region placed on a track: audio, notes or automation. | FL: a pattern, audio or automation clip in the Playlist. Ableton: a clip. | — |
| **Mixer** | Where the signals are balanced, processed and routed. | FL: the Mixer. Ableton: the Mixer section. | — |
| **Insert** | One mixer channel, with its volume, pan, effect slots and sends. Numbered as in FL Studio. | FL: a mixer track (Insert 1, 2, …). Ableton: a track's mixer strip. | mixer track, channel strip |
| **Master insert** | Insert 0, where every insert is summed. | FL: the Master mixer track. Ableton: the Main track (Master before Live 12). | — |
| **Effect slot** | One place for an effect on an insert. Shown numbered from 1, as FL Studio shows it (index + 1). | FL: a mixer track's effect slot (10 per insert). Ableton: a device in a track's device chain. | slot (bare, in UI) |
| **Send** | Part of an insert's signal sent to another insert. | FL: a route with a send level. Ableton: a Send to a return track. | route |
| **Dry/wet** | How much of an effect slot's processed signal is heard. | FL: an effect slot's mix knob. Ableton: a device's Dry/Wet. | mix (for this value) |
| **Tempo** | Speed in beats per minute (API field `tempo_bpm`). | Both: tempo. | bpm (as a project field name) |
| **Key** | The musical key of a project or asset. | — | "key" for anything else (an effect's identifier is its `plugin_name`) |
| **Time signature** | Beats per bar and beat unit (e.g. 4/4). | Both: time signature. | — |
| **Volume** | An insert's or instrument's level. The project model stores the fader position (`fader`, where FL's 100% is 1.0) and leaves `db` empty until the dB curve is calibrated (M2). | — | — |
| **Pan** | Stereo position, from −1 (left) to +1 (right). | — | — |
| **Automation** | A parameter's change over time. | FL: an automation clip. Ableton: an automation envelope. | — |

### 19.4 Product and operations

| Term | Meaning | Replaces |
|---|---|---|
| **StemHub plugin** | Our VST3, running inside the DAW. A bare **plugin** is a native or third-party instrument or effect inside the project. Hosts list the StemHub plugin under its product name "Stemhub Session", kept so hosts don't rescan. | bare "plugin" (for ours), "Stemhub Session" |
| **Sign-in session** | How long a sign-in stays valid. | bare "session", "restore session" |
| **RTO / RPO** | Recovery Time Objective / Recovery Point Objective. | — |
| **SUS** | System Usability Scale — standardized 10-question usability survey. | — |
| **pluginval** | Reference validator for VST3 plugins; strictness 5 + `--rtcheck` is mandatory. | — |
| **APVTS** | JUCE `AudioProcessorValueTreeState` — thread-safe parameter management. | — |

---

## 20 — Change log

| Date | Author | Change |
|---|---|---|
| 2026-09-16 | Erwan (via Claude) | Initial consolidated specification. |
| 2026-09-29 | Erwan | Vocabulary cleanup: §19 rewritten as the project-wide glossary (project file, asset, stem, mixdown, version, branch, save, restore, refresh, insert… with FL Studio / Ableton equivalents and the words each replaces); its pull request row also writes down the status rule the backend already enforces (`OPEN` until `MERGED` or `CLOSED`, both terminal). Wording elsewhere aligned to it (push/commit → save, pull → restore, session bundles → project file plus audio & MIDI assets stored content-addressed, Track entity removed from §7). Milestone deliverables (§17.1) are unchanged; only M3's "plugin" now reads "StemHub plugin". No decision, priority, KPI or commitment changed. |
| 2026-09-29 | Erwan | Project model schema 0.1.0 (#170): the §19 Project model row links its format ([project-model.md](./project-model.md)); Volume is stored as a fader position with dB left empty until calibrated (M2), Pan as −1…+1. No decision, priority, KPI or commitment changed. |
