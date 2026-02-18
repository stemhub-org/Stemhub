# Cost & Revenue

This page outlines StemHub's infrastructure costs across three scenarios, considering that our team of 6 fullstack developers builds the platform internally, with access to student plans across multiple services.

---

## MVP Cost

**Target:** ~50 early adopters validating the core product.

| Category | Service | Cost |
|----------|---------|------|
| Compute | AWS EC2 t3.micro (Free Tier) | $0 |
| Database | AWS RDS PostgreSQL (Free Tier) | $0 |
| Frontend | Vercel (Hobby Plan) | $0 |
| Storage | Cloudflare R2 — 30 GB | $6/year |
| Auth | Auth0 (Free — up to 7,500 MAU) | $0 |
| CI/CD | GitHub (Student Plan) | $0 |
| Domain | stemhub.com | $12/year |

**Total: ~$18/year ($1.50/month)**

> Student plans and free tiers cover nearly all costs at this stage. Infrastructure becomes a real expense only after the MVP.

---

## 1,000 Users

**Assumptions:** 5 projects/user on average, ~250 MB per project, 30 commits/month.

| Category | Service | Monthly Cost |
|----------|---------|-------------|
| Compute | 2× AWS EC2 t3.medium | $60 |
| Database | AWS RDS db.t3.small | $30 |
| Frontend | Vercel Pro | $20 |
| CDN | Cloudflare Pro | $20 |
| Storage | Cloudflare R2 — 1.25 TB | $19 |
| Auth | Auth0 Essentials | $25 |
| Monitoring | Sentry + Datadog | $41 |
| Email | SendGrid Starter | $10 |
| Backup | Automated snapshots | $5 |
| Misc | Bandwidth, domain | $11 |

**Total: ~$241/month — $2,892/year**
**Cost per user: $0.24/month**

---

## 10,000 Users

**Assumptions:** 8 projects/user on average, ~300 MB per project (with FLAC compression applied), 40 commits/month.

| Category | Service | Monthly Cost |
|----------|---------|-------------|
| Compute | 4× AWS EC2 c5.xlarge (auto-scaling) | $550 |
| Database | RDS m5.large + Read Replica | $280 |
| Cache | Redis ElastiCache | $50 |
| Load Balancer | AWS ALB | $25 |
| Frontend | Vercel Pro | $20 |
| CDN | Cloudflare Business | $200 |
| Storage | Cloudflare R2 — ~9 TB (optimized) | $220 |
| Auth | Auth0 Professional | $240 |
| Monitoring | Sentry + Datadog + Logging | $260 |
| Email | SendGrid Pro | $90 |
| Security | WAF + audits + backups | $200 |
| Misc | Bandwidth, status page | $199 |

**Total: ~$2,334/month — $28,008/year**
**Cost per user: $0.23/month**

> **Key optimization:** FLAC compression + file deduplication + lifecycle policies reduce storage from 24 TB to ~9 TB, saving ~$140/month.

> **Key infrastructure choice:** Cloudflare R2 offers **free egress**, saving ~$1,800/month compared to AWS S3 at this scale.

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
