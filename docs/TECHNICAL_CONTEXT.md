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
    - **Storage**: AWS S3 preferred over "Self-hosted" for its resilience and ISO 27001 certification.
    - **Encryption**: AES-256 (Banking Standard) for files at rest.
    - **Access Control**: MFA (Strong Authentication) mandatory to counter account theft.

---

# Skills Matrix

- **The Team**: Erwan, Raphaël, JB, Dryss, Hubert, Gabin
- **Technical Stack**:
    - Frontend: React.js
    - Backend: Python
    - Database: PostgreSQL
    - Storage: AWS S3 Bucket
    - DAW Plugin: C++

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
- **Why?** Need for strict relations (A Project has multiple Versions, a Version has multiple Tracks). NoSQL (Mongo) would be too messy to manage precise versioning history (Git-like).

## File Storage (Audio): AWS S3 (or equivalent like MinIO/Google Cloud Storage)
- **Imperative**: Never store audio files on the web server itself.
- **Generate "Signed URLs"** so the user's browser uploads directly to S3 (to avoid overloading your Python server).

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
- **Technology**: React.js (Single Page Application).
- **Audio Visualization**: Wavesurfer.js to display waveforms and manage smooth playback.
- **UX/UI**: Design inspired by standards (Splice/Drive) for rapid adoption by musicians.

## 2. Backend (Logic & API)
- **Language**: Python (FastAPI or Django Framework).
- **Key Library**: PyFLP (to parse FL Studio files) and struct libraries (for binary analysis).
- **Role**: Manages authentication, project metadata, and versioning logic.

## 3. Infrastructure & Storage (Cloud)
- **Database**: PostgreSQL (Relational) to store links between Artists, Projects, and Versions.
- **Heavy File Storage**: AWS S3 (Simple Storage Service).
- **Upload Architecture**: "Signed URLs" for direct upload from browser to S3 (bypassing the Python server for performance).

## 4. Security & DevOps
- **Authentication**: OAuth2 / Auth0 (MFA, Google/Apple Connection).
- **Encryption**: AES-256 for files at rest on S3.
- **CI/CD**: Automated deployment pipeline (GitHub Actions).

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
