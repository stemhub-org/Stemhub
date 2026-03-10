# Cost & Revenue

This page outlines StemHub's infrastructure costs across three scenarios, considering that our team of 6 fullstack developers builds the platform internally, with access to student plans across multiple services.

---

## MVP Cost

**Target:** ~50 early adopters validating the core product.

| Category | Service | Cost |
|----------|---------|------|
| Compute | GCP Compute Engine (Free Tier / e2-micro) | $0 |
| Database | GCP Cloud SQL (PostgreSQL - Student Credits) | $0 |
| Frontend | Vercel (Hobby Plan) | $0 |
| Storage | Google Cloud Storage (GCS) — 30 GB | $0.70/month |
| Auth | Custom JWT + HttpOnly Cookies | $0 |
| CI/CD | GitHub Actions (Free) | $0 |
| Domain | stemhub.com | $12/year |

**Total: ~$20/year ($1.60/month)**

> Student plans and free tiers cover nearly all costs at this stage. Infrastructure becomes a real expense only after the MVP.

---

## 1,000 Users

**Assumptions:** 5 projects/user on average, ~250 MB per project, 30 commits/month.

| Category | Service | Monthly Cost |
|----------|---------|-------------|
| Compute | 2× GCP Compute Engine (e2-medium) | $50 |
| Database | GCP Cloud SQL (db-f1-micro) | $20 |
| Frontend | Vercel Pro | $20 |
| CDN | Google Cloud CDN | $15 |
| Storage | Google Cloud Storage (Standard) — 1.25 TB | $25 |
| Auth | Custom Implementation | $0 |
| Monitoring | Sentry + Google Cloud Monitoring | $30 |
| Email | SendGrid Starter | $10 |
| Backup | Automated snapshots | $5 |
| Misc | Bandwidth, domain | $15 |

**Total: ~$190/month — $2,280/year**
**Cost per user: $0.24/month**

---

## 10,000 Users

**Assumptions:** 8 projects/user on average, ~300 MB per project (with FLAC compression applied), 40 commits/month.

| Category | Service | Monthly Cost |
|----------|---------|-------------|
| Compute | 4× GCP Compute Engine (n2-standard-4) | $480 |
| Database | GCP Cloud SQL (8 vCPU, 30 GB RAM) | $260 |
| Cache | GCP Memorystore (Redis) | $45 |
| Load Balancer | Google Cloud Load Balancer | $20 |
| Frontend | Vercel Pro | $20 |
| CDN | Google Cloud CDN | $150 |
| Storage | Google Cloud Storage (Nearline) — ~9 TB | $180 |
| Auth | Custom Implementation (Scaled) | $0 |
| Monitoring | Sentry + Google Cloud Suite | $220 |
| Email | SendGrid Pro | $90 |
| Security | Cloud Armor + audits + backups | $180 |
| Misc | Bandwidth (Egress), status page | $250 |

**Total: ~$1,895/month — $22,740/year**
**Cost per user: $0.23/month**

> **Key optimization:** FLAC compression + file deduplication + lifecycle policies reduce storage from 24 TB to ~9 TB, saving ~$140/month.

> **Key infrastructure choice:** Using **Google Cloud Storage (GCS)** lifecycle policies and **Nearline storage** for older versions ensures cost-efficiency at scale.

---

## Summary

| Scenario | Annual Cost | Cost/User/Month |
|----------|-------------|-----------------|
| MVP | $18 | $0.03 |
| 1,000 users | $2,892 | $0.24 |
| 10,000 users | $28,008 | $0.23 |

Costs per user remain stable between 1K and 10K users, demonstrating that the architecture scales efficiently.

---

## Revenue Model

StemHub targets a freemium model inspired by GitHub and Figma.

| Plan | Price | Storage | Key Features |
|------|-------|---------|-------------|
| Free | $0/month | 2 GB | 3 projects, basic version control |
| Creator | $9/month | 50 GB | Unlimited projects, collaboration, multi-DAW export |
| Pro | $19/month | 200 GB | Open-source projects, pull requests, analytics |
| Studio | $49/month | 1 TB | Teams, advanced permissions, priority support |

**Projected at 10,000 users (10% paid conversion):**

| Segment | Users | Annual Revenue |
|---------|-------|---------------|
| Creator (10%) | 1,000 | $108,000 |
| Pro (3%) | 300 | $68,400 |
| Studio (1%) | 100 | $58,800 |
| **Total** | | **$235,200/year** |

With $28,008 in infrastructure costs, this yields an **88% gross margin** — comparable to best-in-class SaaS benchmarks.

Break-even is achievable at roughly **300–500 paying users**.
