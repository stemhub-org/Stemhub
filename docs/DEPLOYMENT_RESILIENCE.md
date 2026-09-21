# Deployment & Resilience

> Deep-dive extending [SPECIFICATION.md §10](./SPECIFICATION.md) (environments, hosting, rollout) and [§11](./SPECIFICATION.md) (retention). This file only covers operational detail not already in the spec.

---

## Database migration strategy

**Handling schema changes** (Alembic on PostgreSQL):
- Migrations live in `backend/alembic/versions` and are version-controlled.
- **Startup guard:** the FastAPI lifespan checks for pending migrations and refuses to start if the schema is out of date.

**Expand-and-contract process** (used for any breaking column/table change, e.g. V1.0 → V1.2):
1. Deploy an additive migration (new column/table alongside the old).
2. Deploy application code that reads both old and new schema.
3. Backfill data into the new fields.
4. After a validation period, deploy a cleanup migration that drops the deprecated fields.
5. Deploy the final application version removing backward-compatibility code.

Example:
```sql
-- V1.0: projects.storage_path exists
-- V1.2: migrate to projects.storage_metadata (JSONB)
-- Step 1:
--   docker compose exec backend alembic revision --autogenerate -m "add storage_metadata"
```

---

## Single Points of Failure (SPOF)

| Component | Risk | Mitigation |
|---|---|---|
| PostgreSQL | High — single instance failure stops the system | Primary-replica replication with automated failover; read replicas for query load. |
| GCS | Medium — no file access | Multi-region redundancy (GCS default); local cache for hot files. |
| Backend API | High — no requests processed | Horizontal scaling behind Cloud Run; auto-scaling on CPU/memory. |
| Auth service | Critical — users locked out | Redundant instances; JWT validated via shared secret (stateless). |
| Plugin update server | Low — users keep working offline | CDN distribution; plugin functions without a connection. |

---

## Backup strategy

Backup retention is **capped at 35 days** to match the privacy promise in [SPECIFICATION.md §11.2](./SPECIFICATION.md) ("purged from backups within 35 days"). Longer archival is **not permitted** — it would break the GDPR erasure commitment.

**Database:**
- Automated daily backups, retention ≤ 35 days.
- Point-in-time recovery (PITR) for the last 7 days.
- No monthly/yearly cold archives.

**Object storage (GCS):**
- Object versioning on; deleted-file recovery within 30 days (trash window, per SPEC §11.3).
- No cross-provider replication of user files (would extend deletion latency beyond the 35-day cap).

**Configuration & code:**
- Infrastructure-as-Code in Git.
- Environment configuration encrypted in Secret Manager (see SPEC §10.2).
- Docker images tagged in the registry for rollback.

**Recovery testing:**
- Monthly restore drills against a scratch environment.
- **RTO 2 h / RPO 24 h** (targets from SPEC §5.2).

---

## Degraded mode

Graceful failure paths when a dependency is down:

1. **File storage unavailable** — plugin keeps working locally; web shows cached metadata; auto-sync on reconnect. UI banner: "Working offline — changes will sync when reconnected."
2. **Database in read-only** — reads continue; writes queue for retry; auth and payment writes blocked with a user-visible notice.
3. **Collaboration service down** — real-time features disabled; version control still functions; conflicts resolved on reconnect.
4. **DAW-format parser failure** — fallback to plain-file handling without diff/change-summary features. UI banner: "Advanced features temporarily unavailable."
