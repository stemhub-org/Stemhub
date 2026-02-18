This document outlines the Technical, Legal, and Security audit for the StemHub project. It defines our data compliance framework, security benchmarks, and accessibility targets for the MVP.

---

## 1. Data & GDPR (Data Privacy)
*Personal data management policy and user privacy compliance.*

| Category | Description |
|:---|:---|
| **Data Collected** | Identity (Name, Email) and Intellectual Property (Audio files `.wav`, Project files `.als`). |
| **Storage & Rights** | Secure hosting in the EU (AWS/GCP). The user retains 100% ownership of their files. |
| **Right to Erasure** | Permanent and irreversible deletion of projects upon simple user request. |

---

## 2. French Legal Framework (LCEN & IPC)
*Compliance with French hosting laws and copyright regulations.*

* **Hosting Provider Status (LCEN Law 2004):** StemHub is not liable for uploaded content but commits to immediate removal upon report (*Notice & Takedown*).
* **Intellectual Property Code (IPC):** Strict compliance with French Moral Rights (non-transferable) and patrimonial rights.
* **AI Commitment:** Formal guarantee that music will not be used to train generative AI models without explicit consent.

---

## 3. Security & Technical Benchmark
*Strategy to protect intellectual property against cloud vulnerabilities.*

> **Context (Why?):** Recent leaks (GTA VI, artist demos) prove the critical vulnerability of traditional cloud storage.

| Component | Technical Choice & Justification |
|:---|:---|
| **Storage** | **AWS S3** is preferred over "Self-hosted" solutions for its resilience and ISO 27001 certification. |
| **Encryption** | **AES-256** (Banking Standard) to protect data at rest. |
| **Access Control** | **MFA** (Multi-Factor Authentication) is mandatory to prevent account hijacking. |

---

## 4. Accessibility Strategy (A11y)
*Ensuring an interface usable by everyone, including Persons with Disabilities (PWD).*

**Standard & Target Level**
* **Standard Adopted:** WCAG 2.1 (Web Content Accessibility Guidelines).
* **Target Level:** AA for the MVP.
* **Justification:** International audience of producers, requiring maximum compatibility with assistive technologies.

**Solutions by Disability Type:**

| Disability | Implemented Technical Solutions |
|:---|:---|
| **Visual** | Descriptive ARIA labels, 100% keyboard navigation (Tab, Enter, Arrows), minimum contrast ratio of 4.5:1 + high-visibility mode, text descriptions for audio visualizations. |
| **Motor** | Full control without a mouse (keyboard only), large clickable areas (min. 44x44px), support for customizable keyboard shortcuts. |
| **Auditory** | Visual notifications for all audio events, subtitles on video tutorials. |
| **Cognitive** | Clean interface with a linear flow, error messages in plain language, options to disable animations. |