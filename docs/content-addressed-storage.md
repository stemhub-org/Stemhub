# Content-Addressed Storage — Design

## Problem

The current storage model treats every version as a single opaque bundle: each commit re-uploads and re-stores the entire DAW project (FLP + all audio stems). For a typical mixing workflow — where a session might contain 200–500 MB of unchanged audio and 1–2 MB of edited FLP — this wastes:

- **Storage:** N copies of the same 500 MB stem set for a project with N versions.
- **Bandwidth:** The plugin re-uploads the same stems on every commit; slow, and painful on residential upload speeds.
- **Time:** Push/pull latency scales with total session size, not with what actually changed.

## Goal

Store each unique audio file **once per project** and represent each version as a lightweight JSON manifest that references those shared blobs by SHA-256. This is exactly how Git's object model works (blobs + tree + commit).

## Model

### `blob` table

Content-addressed rows, one per unique file per project (see "Scope" below).

| column          | type          | notes                                          |
|-----------------|---------------|------------------------------------------------|
| `sha256`        | `CHAR(64)` PK | Hex-encoded SHA-256 of the raw bytes.          |
| `project_id`    | `UUID` FK     | Blobs are project-scoped (privacy — see below).|
| `size_bytes`    | `BIGINT`      | Uncompressed size.                             |
| `mime_type`     | `VARCHAR(80)` | Best-effort; not authoritative.                |
| `storage_uri`   | `TEXT`        | Backend-specific location (localfs or GCS).    |
| `created_at`    | `TIMESTAMPTZ` | First time this blob was uploaded.             |
| `ref_count`     | `INT`         | Number of live manifests referencing it. GC uses this. |

**Composite PK:** `(project_id, sha256)`. A file with the same bytes uploaded to two different projects is stored twice — see "Scope: project-scoped vs global."

### `version` table (changes)

Existing columns stay for backward compatibility during rollout. Add:

| column           | type   | notes                                       |
|------------------|--------|---------------------------------------------|
| `manifest_json`  | `JSONB`| The full manifest (see below).              |
| `manifest_version` | `INT` | Schema version of `manifest_json`. Start at 1. |

Deprecate (but don't drop yet): `artifact_path`, `artifact_size_bytes`, `artifact_checksum`. These are replaced by references inside `manifest_json`.

### Manifest schema (v1)

```json
{
  "manifest_version": 1,
  "source_daw": "FL Studio",
  "source_project_filename": "MyTrack.flp",
  "project_file": {
    "sha256": "abc...",
    "size_bytes": 1843200,
    "filename": "MyTrack.flp"
  },
  "tracks": [
    {
      "name": "Kick",
      "sha256": "def...",
      "size_bytes": 4823040,
      "filename": "kick.wav",
      "bpm": 128,
      "key": "F#m",
      "duration_seconds": 12
    }
  ],
  "mixer_state": { ... existing snapshot_manifest ... }
}
```

Rules:
- `manifest_version` is required from day one. Bump when schema changes.
- All referenced blobs must exist in the `blob` table for this project before the manifest can be saved.
- Filenames inside the manifest are display-only. Actual retrieval is by SHA-256.

## Scope: project-scoped vs global blobs

**Decision: project-scoped.**

Global dedupe would maximize storage savings (samples reused across projects would coalesce), but it opens a **privacy oracle**: a user could upload a suspected file and check whether the server "already has it" — leaking the fact that another user has that content. This is a known attack against consumer deduplicated cloud storage (Dropbox had CVE-style disclosures around this).

Project-scoped dedupe still solves 95% of the problem (the wasteful case is *the same project* being pushed repeatedly), with no privacy leak.

## Upload flow (plugin ↔ backend)

Client-side hashing is mandatory — the whole point is that the client can skip uploading blobs the server already has.

```
1. Plugin snapshots the DAW session → list of files (FLP + stems).
2. For each file, plugin computes SHA-256 locally.
3. Plugin POSTs POST /projects/{pid}/blobs/check
     body: {"sha256s": ["abc...", "def...", ...]}
   → returns {"missing": ["def..."]}   (only blobs the server needs)
4. For each missing sha256:
     Server returns a presigned PUT URL (GCS) or a token-scoped upload endpoint (localfs).
     Plugin uploads bytes directly to that URL.
     Server verifies SHA-256 after upload (integrity guarantee). On mismatch → delete blob, 400.
5. Plugin POSTs POST /projects/{pid}/branches/{bid}/versions
     body: { "commit_message": "...", "manifest": { ...manifest v1... } }
     Server validates:
       - manifest_version supported
       - every referenced sha256 exists in blob table for this project
       - increments ref_count for each referenced blob
     Server writes the Version row atomically.
```

**Atomicity:** Uploads are idempotent by SHA-256 (uploading twice produces the same row). Version creation is a single DB transaction. If step 5 fails, the uploaded blobs stay — they'll be reclaimed by GC because `ref_count == 0`.

## Download flow (pull)

```
1. Plugin GETs GET /versions/{vid}     → returns manifest_json.
2. Plugin diffs local filesystem against manifest → list of missing sha256s.
3. For each missing sha256, plugin GETs GET /projects/{pid}/blobs/{sha256}
   → server returns a presigned GET URL (GCS) or 302 to a signed local endpoint.
4. Plugin downloads, verifies SHA-256 locally before writing to disk.
```

## Garbage collection

Two options; recommend **eager ref_count with periodic sweep as safety net**:

1. **Eager ref_count:** Increment on version create, decrement on soft-delete. Cheap, correct if all writes go through the version-create endpoint.
2. **Periodic sweep:** A daily job (start weekly) scans blobs where `ref_count == 0 AND created_at < now() - 24h` and deletes them. The 24h grace window prevents races where a client uploaded blobs but hasn't POSTed the version yet.

## Presigned URLs

**GCS:** use `blob.generate_signed_url(expiration=15min, method="PUT"/"GET")`. Cheap, offloads all bytes from the API server, works with the existing GCS service-account credentials.

**LocalFS (dev only):** implement a token-scoped `/internal/blob-upload/{token}` endpoint. Token is a signed JWT containing `{sha256, project_id, exp}`. Same API shape as GCS presigned URLs, so client code is uniform.

## Migration path (from current schema)

The user is at < 100 users. Simplest path:

1. Ship the new schema behind an unused endpoint set (`/v2/...` or feature-flagged).
2. Backfill: for each existing Version, treat the whole artifact bundle as a single blob (SHA-256 the archive, insert one `blob` row, build a minimal manifest with `manifest_version: 0` meaning "legacy bundle"). This is a one-shot script.
3. Switch the plugin to the new endpoints. Old versions remain readable via the legacy code path.
4. After 30 days with no legacy pulls, delete the legacy fields.

If wiping alpha data is acceptable, skip steps 2–4 and cut over directly.

## Non-goals for v1

- Cross-project dedupe (privacy).
- Delta compression within a blob (git's pack files). Blob-level dedupe already solves the common case.
- Client-side diff of FLP internals (would require a DAW-format-aware differ; huge scope).

## Open questions

- **Blob size ceiling.** Multi-gigabyte stems will need multipart/resumable upload (GCS supports it). Defer until we see it in practice.
- **Encryption at rest.** GCS default is fine for alpha; if we later need customer-managed keys, the blob abstraction contains the change.
- **Public projects & blob visibility.** When a project is `is_public=True`, blobs referenced by public versions must be readable by anonymous clients. Solution: presigned GET URLs from an authenticated endpoint that checks the project's visibility.
