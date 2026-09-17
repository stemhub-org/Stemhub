# Risk Management

> Deep-dive extending [SPECIFICATION.md §18](./SPECIFICATION.md). The spec lists risks tied to specific delivery decisions; this file is the broader operational register with Probability × Impact scoring.

**Scale:** Probability and Impact both 1 (low) → 5 (critical). Criticality = P × I.

## Risk register

| Risk | P | I | Criticality | Mitigation |
|---|:-:|:-:|:-:|---|
| **Technical** | | | | |
| Dependence on external services (GCS, Google OAuth, GitHub) — availability or pricing changes | 2 | 5 | **10** | Abstraction layer (`StorageService` already exists) to swap providers without touching business logic. |
| Critical bug in DAW-project parsing or diff logic corrupts a version | 3 | 5 | **15** | Checksum verification on every download (SPEC §5.2); unit + integration tests over `PyFLP_v2`; manual review of parsing changes. |
| Large-file uploads slow enough that testers churn on medium/large projects | 4 | 3 | **12** | Chunked / resumable uploads; move to signed direct-to-GCS URLs (SPEC §5.2 blocker); background compression. |
| Browser incompat with Web Audio API for the waveform preview | 3 | 4 | **12** | Cross-browser test matrix (Chrome, Firefox, Safari); graceful fallback when features unavailable. |
| **Operational** | | | | |
| User adoption barrier: Git jargon (commit/merge/PR) alienates musicians | 4 | 5 | **20** | Music-friendly wording ("save version", "propose changes"); "simple mode" default; interactive first-run tutorial; SUS ≥ 70 target. |
| Storage costs scale faster than revenue | 3 | 4 | **12** | Per-tier storage quotas; automated cleanup for soft-deleted projects after 30 days (SPEC §11.3); cost-monitoring alerts. |
| **Security** | | | | |
| User database leak (emails, hashes) | 1 | 5 | **5** | Password hashing (Argon2/bcrypt); AES-256 at rest (GCS default); scheduled security audits; MFA post-MVP (SPEC §11.5). |
| Leak of unreleased music (IP theft) | 2 | 5 | **10** | Signed URLs with short expiry; strict ACLs on GCS; audit logs for every artifact access; project-scoped blobs (no global dedupe oracle — see [content-addressed-storage.md](./content-addressed-storage.md)). |
| DDoS on API or storage | 2 | 4 | **8** | Cloud Armor + Cloudflare; per-endpoint rate limiting; Cloud Run auto-scaling. |

> Spec-tied risks (bus factor, scope creep on Ableton, visual identity slippage, legal review timing, signed-URL migration, plugin real-time safety) are tracked in [SPECIFICATION.md §18](./SPECIFICATION.md) — not duplicated here.

## Mitigation-strategy taxonomy

Every critical risk gets one of four responses:
- **Avoid** — change the plan so the risk cannot occur (e.g. delegate card handling to Stripe, never store PANs).
- **Reduce** — lower probability or impact (e.g. automated backups; checksum-on-download).
- **Transfer** — outsource to a managed service or insurer (e.g. GCS, Cloud SQL).
- **Accept** — acknowledge and monitor (e.g. minor UI polish gaps during beta).
