# Compliance, Legal & Security Audit

## 1. Data & GDPR (Data Privacy)
- **Collected Data**: Identity (Name, Email) and Intellectual Property (.wav Audio Files, .als Projects).
- **Storage & Rights**: Secure hosting in EU (AWS/GCP). The user retains 100% ownership of their files.
- **Right to be Forgotten**: Permanent and irreversible deletion of projects upon simple request.

## 2. French Legal Framework (LCEN & CPI)
- **Host Status (LCEN Law 2004)**: StemHub is not responsible for uploaded content but commits to immediate removal upon reporting (Notice & Takedown).
- **Intellectual Property Code (CPI)**: Strict respect for French Moral Rights (inalienable) and patrimonial rights.
- **AI Commitment**: Formal guarantee that music is not used to train generative AI without consent.

## 3. Security & Technical Benchmark
- **Context (Why?)**: Recent "leaks" (GTA VI, artist demos) prove the critical vulnerability of classic clouds.
- **Benchmark (Our Choices)**:
    - **Storage**: **Google Cloud Storage (GCS)** preferred over "Self-hosted" for its resilience and ISO 27001 certification. Supports local storage (`localfs`) for development.
    - **Encryption**: AES-256 (Banking Standard) for files at rest.
    - **Access Control**: HttpOnly Cookies + JWT for secure session management. MFA (Strong Authentication) planned to counter account theft.

---

# Skills Matrix

- **The Team**: Erwan, Raphaël, JB, Dryss, Hubert, Gabin
- **Technical Stack**:
    - Frontend: Next.js 16 (React 19) with TypeScript & Tailwind CSS
    - Backend: Python 3.10+ (FastAPI) with async SQLAlchemy 2.0
    - Database: PostgreSQL
    - Storage: Content-Addressed Storage over Google Cloud Storage (GCS) / Local filesystem
    - DAW Plugin: JUCE C++17 (VST3 & Standalone)

---

# Gap Analysis

- **Legal (Legal & IP)**: No internal expertise in Intellectual Property Law (Music Copyright).
- **Marketing**: No expertise in user acquisition strategy ("Go-to-market").
- **Advanced Audio DSP**: Need to deepen signal processing (C++) for complex functions.

---

# Action Plan & Solutions

- **Organization**: Defined roles (Product Owner / Rotating Scrum Master) and mandatory "Code Reviews" on GitHub.
- **Legal**: Use of validated open-source ToS templates and consultation with external mentors.
- **Skill Development**: Self-training (Peer-learning) on AWS optimization and audio algorithms.

---

# Architecture & Data

For a detailed visual representation and API contract, see the [Data & API Modeling](./DATA_API_MODELING.md).

## Database (Metadata): PostgreSQL
- **Why?** Need for strict relational integrity (Projects, Branches, Versions, Tracks, and Blobs).
- **Soft Deletion**: `User.is_deleted` and `Project.is_deleted` with indexed `deleted_at` timestamps prevent deleted entities from leaking into public feeds while maintaining historical integrity.

## File Storage: Content-Addressed Storage (CAS) over GCS / Local
- **Imperative**: Heavy audio stems and DAW snapshots must be deduplicated across versions to contain storage costs and speed up iterations.
- **Content-Addressed Architecture**: Project assets are stored in a project-scoped `Blob(project_id, sha256)` table with reference counting (`ref_count`). Uploads stream through a SHA-256 hasher.
- **Incremental Push/Pull**: Clients check missing blobs before uploading, only sending changed files. Versions store an explicit `manifest_json` asset map (`manifest_version`).
- **Garbage Collection**: Admin endpoint `/api/admin/blobs/gc` cleans up zero-ref blobs safely.

---

# Standard & Target Level (Accessibility)

- **Adopted Standard**: WCAG 2.1 (Web Content Accessibility Guidelines)
- **Target Level**: AA for the MVP
- **Justification**: International audience of producers, maximum compatibility with assistive technologies

## Solutions by Type of Disability

### Visual Impairment
- Descriptive ARIA labels on all interactive elements.
- 100% keyboard navigation (Tab, Enter, Arrows).
- Minimum contrast 4.5:1 + high-visibility mode.
- Text descriptions of audio visualizations.

### Motor Impairment
- Total control without mouse (keyboard only).
- Large clickable zones (min. 44x44px).
- Support for customizable keyboard shortcuts.

### Hearing Impairment
- Visual notifications for all audio events.
- Subtitles on video tutorials.

### Cognitive Impairment
- Clean interface with linear flow.
- Error messages in simple language.
- Disableable animations.

---

# StemHub Tech Stack Summary

## 1. Frontend (User Interface)
- **Technology**: Next.js 16 (React 19) App Router with TypeScript and Tailwind CSS.
- **Audio Visualization**: Wavesurfer.js client-side waveform rendering and smooth playback.
- **UX/UI**: Modern developer-grade design system adapted for music producers.

## 2. Backend (Logic & API)
- **Language**: Python 3.10+ (FastAPI) with async SQLAlchemy 2.0.
- **Database Migrations**: Alembic schema migrations with startup verification guards.
- **Observability & Hardening**: `/health` and `/ready` probes, stdlib JSON logging + `X-Request-ID` tracing middleware, and `SECRET_KEY` startup guard rejecting weak/default keys.
- **Key Libraries**: PyFLP (FL Studio project parsing) and Pydantic schema validation.

## 3. DAW Plugin (JUCE C++17)
- **Technology**: JUCE C++17 VST3 & Standalone plugin (`plugin/stemhub/`).
- **UI Design System**: Inter & JetBrains Mono typography, dark surface hierarchy (`#121214`, `#1A1A1E`, `#26262B`), Cyan Accent (`#00E5FF`), `LoginView` with offline mode, and `DashboardView` with searchable/filterable card grid (All/Local/Cloud).
- **CAS Integration**: Client-side SHA-256 hashing, incremental missing-blob upload (`checkMissingBlobs`), manifest-based version creation, and symmetric CAS restore.

## 4. Infrastructure, Storage & DevOps
- **Database**: PostgreSQL (Relational metadata + CAS `Blob` table).
- **Storage**: Content-Addressed Storage (GCS / Local filesystem).
- **Authentication**: Custom JWT-based auth (24h TTL) with **HttpOnly Cookies** and Google OAuth2 support.
- **CI/CD**: GitHub Actions automated pipeline with Docker and standalone dev loop script (`watch-plugin.sh`).

---

# Methodology & Organization

- **Adopted Method**: Agile Scrum.

## Scrum Rituals
- **Sprint Planning**: Defining objectives every 15 days.
- **Daily Stand-up**: Synchronization point several times a week.
- **Sprint Review**: Mandatory functional demo at the end of each sprint (e.g., "The plugin opens", "The push works").

## Adaptation to "Destinations"
- **Asynchronous Communication**: Use of Discord for daily updates to counter potential time zones.
- **Written Culture**: Systematic documentation and written reports on Notion.
- **Mandatory Code Review**: No merge without validation by a colleague.
