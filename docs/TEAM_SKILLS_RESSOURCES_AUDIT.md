# Team Skills — Gap Analysis

> Team roster and role assignments live in [SPECIFICATION.md §12.1](./SPECIFICATION.md). This file only tracks the **skills we don't yet have** and how we plan to fill each gap.

| Gap | Criticality | Action |
|---|---|---|
| **Binary analysis / PyFLP mastery** — parsing proprietary DAW files is complex | Medium | **In progress.** `PyFLP_v2` integrated as a submodule (`backend/vendor/PyFLP_v2`) and extended in-house for the fields we need. |
| **Advanced cloud architecture** — secure, scalable GCS storage | Medium | **Implemented.** `StorageService` abstraction with `LocalStorageService` and `GcsStorageService`; pluggable via `STEMHUB_STORAGE_PROVIDER`. |
| **Audio copyright & IP law** — handling user stems, notice-and-takedown, French moral rights | Medium | **External review.** Privacy policy, ToS, and content policy reviewed by a legal advisor before the beta (SPEC §11.2, §11.4). |
| **Marketing & go-to-market** | Low (for MVP) | **Deferred.** Community-led acquisition through Discord + Instagram/TikTok/X (SPEC §15) covers the beta. Recruit a business student for post-beta launch. |
