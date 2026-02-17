# 📋 Product Backlog

This document lists the features identified for the StemHub project. They are prioritized using the MoSCOW method to clearly define the scope of the MVP (Minimum Viable Product).

---

## 🔴 M - MUST HAVE (The MVP)
*Non-negotiable features. If one is missing, the product is not viable[cite: 62].*

| ID | Title | User Story (As a... I want... So that...) | Status |
|:---|:---|:---|:---|
| **US-01** | **Secure Authentication** | As a **Producer**, I want **to log in securely (MFA/Auth0)** so that **I protect my unreleased tracks from leaks**. | `[ ] Todo` |
| **US-02** | **Project Upload** | As a **Musician**, I want **to backup my DAW project to the cloud (AWS S3)** so that **I never lose my source files**. | `[ ] In Progress` |
| **US-03** | **Version Restoration** | As a **Sound Engineer**, I want **to download a specific previous version** so that **I can revert changes after a destructive error**. | `[ ] Todo` |
| **US-04** | **Audio Format Support** | As a **User**, I want **the system to support .wav and .mp3 files** so that **I can manage my audio exports effectively**. | `[ ] Done` |

---

## 🟡 S - SHOULD HAVE
*Important features but not vital for launch. The product works without them, but they add significant value[cite: 64].*

- [ ] **US-05 : DAW Plugin Integration**
  > As a **Beatmaker**, I want **access to version control directly inside FL Studio/Ableton** so that **I don't break my creative flow by switching windows**.

- [ ] **US-06 : Visual History**
  > As a **User**, I want **to see a visual timeline of my modifications** so that **I can understand the evolution of my project at a glance**.

---

## 🟢 C - COULD HAVE
*Nice-to-have features. Included only if there is extra time and budget[cite: 66].*

- [ ] **US-07 : Audio Comparison (Diff)**
  > As a **Producer**, I want **to listen to two versions simultaneously** so that **I can compare mixing differences**.

- [ ] **US-08 : Dark Mode**
  > As a **Night Musician**, I want **a dark interface** so that **I don't strain my eyes in the studio**.

---

## ⚪ W - WON'T HAVE (Out of Scope)
[cite_start]*Features explicitly excluded from the current scope/MVP[cite: 67].*

* **US-99 : Real-time Collaboration** (Postponed to V2 - Too complex for MVP).
* **US-100 : AI Music Generation** (Excluded for legal/copyright reasons).