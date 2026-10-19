# Content-Addressed Storage — Design

How StemHub stores a version's files and moves them between the StemHub plugin and the backend. Terms follow the glossary in [SPECIFICATION.md §19](./SPECIFICATION.md#19--glossary): a version has one **project file** (the `.flp` / `.als`) and its **assets** (the audio and MIDI files of the project folder); its **manifest** lists them; each file's bytes are stored once per project as a **blob**.

## Problem

The original storage model treated every version as one opaque upload: each save re-uploaded and re-stored the whole project (the project file plus all its audio). In a typical mixing workflow, a project folder holds 200–500 MB of audio that doesn't change and a 1–2 MB project file that does. That wastes:

- **Storage:** N copies of the same 500 MB of samples and recordings for a project with N versions.
- **Bandwidth:** The StemHub plugin re-uploads the same audio on every save; slow, and painful on residential upload speeds.
- **Time:** Save and restore latency scales with the project's total size, not with what actually changed.

## Goal

Store each unique file **once per project** and represent each version as a lightweight JSON manifest that references those shared blobs by SHA-256. This is how Git's object model works (blobs + tree + commit).

## Model

### `blob` table

Content-addressed rows, one per unique file per project (see "Scope" below).

| column          | type          | notes                                          |
|-----------------|---------------|------------------------------------------------|
| `sha256`        | `VARCHAR(64)` PK | Hex-encoded SHA-256 of the raw bytes.       |
| `project_id`    | `UUID` FK     | Blobs are project-scoped (privacy — see below).|
| `size_bytes`    | `BIGINT`      | Uncompressed size.                             |
| `mime_type`     | `VARCHAR(80)` | Best-effort; not authoritative.                |
| `storage_uri`   | `TEXT`        | Backend-specific location (localfs or GCS).    |
| `created_at`    | `TIMESTAMPTZ` | First time this blob was uploaded.             |
| `ref_count`     | `INT`         | Number of live manifests referencing it. GC uses this. |

**Composite PK:** `(project_id, sha256)`. A file with the same bytes uploaded to two different projects is stored twice — see "Scope: project-scoped vs global."

### `version` table

| column             | type    | notes                                       |
|--------------------|---------|---------------------------------------------|
| `manifest_json`    | `JSONB` | The version's manifest (see below). Never rewritten once stored. |
| `manifest_version` | `INT`   | Schema version of `manifest_json`: 2 for new versions, 1 for older ones. |

Every version is saved from a manifest. The columns of the earlier single-upload flow (`artifact_path`, `artifact_size_bytes`, `artifact_checksum`, `snapshot_manifest`) and the `track` table, which was never filled, were dropped in migration `1ddecc33a8b7`.

### Manifest v2 (current)

```json
{
  "manifest_version": 2,
  "source_daw": "FL Studio",
  "source_project_filename": "Song.flp",
  "project_file": {
    "sha256": "abc...",
    "size_bytes": 1843200,
    "path": "Song.flp"
  },
  "assets": [
    {
      "sha256": "def...",
      "size_bytes": 4823040,
      "path": "Samples/kick.wav"
    }
  ]
}
```

- `source_daw` is `"FL Studio"` or `"Ableton Live"` (the backend ignores case and extra spaces, and stores a blank as unknown).
- `project_file` is the DAW document; `assets` are the other audio and MIDI files, at most 500. The StemHub plugin refuses two different files at one path; the backend only enforces the 500-asset and 255-character limits.
- `path` is where the file goes in the project folder: relative to the project file's folder, `/`-separated, at most 255 characters.
- A file's display name (the file name without its extension) and type are derived from its `path` when needed (the web app's "Audio & MIDI files" list, `GET /versions/{id}/assets`); nothing else is stored per file. v1 manifests keep the `name` they were stored with.

The StemHub plugin writes and reads manifests in one place, `domain/Manifest.hpp` (`stemhub::manifest`). The backend validates incoming ones with `VersionManifestV2` (`schemas.py`) and reads stored ones only through `manifests.py` (`blob_refs`, `blob_shas`).

### Manifest v1 (legacy, still read)

Versions saved before v2 keep their v1 manifest, since stored rows are never rewritten, and the backend still accepts v1 from older StemHub plugins. v1 has the same content under other names:

| v1 | v2 |
|---|---|
| `tracks` (the asset list) | `assets` |
| `filename` (in `project_file` and in each asset) | `path` |
| `name` per asset, required | none: derived from `path` |
| `bpm`, `key`, `duration_seconds` per asset, optional and never filled | dropped |
| `mixer_state`, optional and never read | dropped (the project model, #170, is its successor) |

The backend reads a manifest without `manifest_version` as v1; stored manifests always carry it, and the StemHub plugin refuses one that doesn't. The earliest v1 manifests hold bare file names instead of paths; those files restore into one folder.

### Rules

- Every stored manifest has a `manifest_version`. Bump it when the schema changes, and keep reading every version still stored.
- All referenced blobs must exist in the `blob` table for this project before the version can be created.
- Blobs are retrieved by SHA-256, never by path. `path` is where the StemHub plugin writes the file on restore (`Samples/Imported/kick.wav`), so the folder layout the DAW expects comes back intact.
- The StemHub plugin treats `path` as untrusted: it only accepts relative paths made of plain segments (no `..`, absolute paths, drive letters, backslashes or reserved Windows names, at most 255 characters) and refuses a manifest that lists two different files at the same path (compared case-insensitively, as macOS and Windows file systems do).
- Limits (500 assets, 255-character paths) are checked by the StemHub plugin before it hashes anything, and again by the backend.

## Scope: project-scoped vs global blobs

**Decision: project-scoped.**

Global dedupe would maximize storage savings (samples reused across projects would coalesce), but it opens a **privacy oracle**: a user could upload a suspected file and check whether the server "already has it" — leaking the fact that another user has that content. This is a known attack against consumer deduplicated cloud storage (Dropbox had CVE-style disclosures around this).

Project-scoped dedupe still solves 95% of the problem (the wasteful case is *the same project* being saved repeatedly), with no privacy leak.

## Save flow (StemHub plugin → backend)

Client-side hashing is mandatory — the whole point is that the client can skip uploading blobs the server already has.

```
1. The StemHub plugin lists the files the save takes (VersionFiles): the project file, then the
   audio and MIDI files in its folder and subfolders, without hidden files, "Backup" folders and
   copies it restored there. It checks them against the limits (500 assets, 255-character paths)
   before anything is hashed.
2. For each file, it computes the SHA-256 locally.
3. It POSTs /projects/{pid}/blobs/check-missing
     body: {"sha256s": ["abc...", "def...", ...]}   (identical files are offered once)
   → returns {"missing": ["def..."]}   (only blobs the server needs)
4. For each missing sha256, it PUTs /projects/{pid}/blobs/{sha256} (multipart, field "file").
     The server hashes the bytes it received (integrity guarantee). On mismatch → delete, 400.
5. It POSTs /branches/{bid}/versions/from-manifest
     body: { "message": "...", "parent_version_id": "...", "manifest": { ...manifest v2... } }
     (message: optional, and "commit_message" is still accepted in its place;
      parent_version_id: the version the saved file was based on, when known)
     Server validates:
       - the manifest (v1 or v2), its limits and source_daw
       - parent_version_id is a live version of this branch (400 otherwise)
       - every referenced sha256 exists in the blob table for this project (409 missing_blobs otherwise)
     Server increments ref_count once for each distinct referenced blob and writes the Version
     row, in one transaction.
```

**Atomicity:** Uploads are idempotent by SHA-256 (uploading twice produces the same row). Version creation is a single DB transaction. If step 5 fails or never runs (a cancel, a network drop), no version exists: the uploaded blobs stay with `ref_count == 0`, and the next save reuses them or GC reclaims them.

## Restore flow

```
1. The StemHub plugin GETs /versions/{vid} → manifest_json, v1 or v2 (a version without one
   can't be restored).
2. It validates the manifest (hashes, sizes, safe paths, no two files at one path) and creates a
   hidden folder next to the destination (`.<name>.partial-<id>`).
3. For each file in the manifest, it GETs /projects/{pid}/blobs/{sha256}
   → the server returns the bytes, or a 307 to a signed GET URL (GCS). The StemHub plugin
     follows that redirect itself and does not send its bearer token to the storage host.
4. It writes each file at its path in the hidden folder and verifies its SHA-256 (a short file
   is reported as an interrupted download).
5. Once every file is there, it writes a `.stemhub-restored` marker into the hidden folder and
   renames it to the destination. A failure or a cancel deletes it, so a folder that looks
   restored is always complete.
```

Every restore goes to a new folder: the StemHub plugin never writes into an existing one. The
marker makes the copy a project of its own: saving a project whose folder contains it leaves it
out. The StemHub plugin also records which version the restored project file holds (see
[plugin-data-flow.md](./plugin-data-flow.md), section 12), so the copy's first save names that
version as its parent.

## Deleting a version and garbage collection

We use **eager ref_count with a periodic sweep as safety net**:

1. **Eager ref_count:** Incremented on version create, decremented on version (soft) delete. Both sides read the manifest through `manifests.blob_shas`, so a v1 and a v2 manifest count the same hashes going up and coming down. Correct as long as every version is created through `from-manifest`.
2. **Periodic sweep:** `blob_gc.sweep_orphan_blobs` deletes blobs where `ref_count == 0 AND created_at < now() - 24h`: the storage bytes first, then the row. The 24h grace window prevents races where a client uploaded blobs but hasn't created the version yet. It is a callable, not a scheduler: an external scheduler (or an admin) runs it through `POST /api/admin/blobs/gc`.

## Signed URLs

**Downloads (GCS):** done. `GET /projects/{pid}/blobs/{sha256}` answers a `307` to `blob.generate_signed_url(expiration=15 min, method="GET")`, so the bytes never pass through the API server. On localfs the API serves the bytes itself.

**Uploads:** still go through FastAPI. Moving them to signed PUT URLs direct to GCS is the delivery-blocking task of [SPECIFICATION.md §5.2](./SPECIFICATION.md#52-technical-kpis). A localfs equivalent for development (a token-scoped upload endpoint with the same shape as a GCS signed URL) is not built yet.

## Migration from the single-upload model (done)

The alpha had under 100 users, so we cut over directly instead of backfilling:

1. The blob table and `manifest_json` were added next to the old columns (`f7d2c1a48901`).
2. The StemHub plugin switched to the save and restore flows above.
3. The old upload columns and the `track` table were dropped (`1ddecc33a8b7`). Versions saved by the old flow have no manifest: they list no files and can't be restored.
4. Manifest v2 replaced v1 for new versions; v1 rows are kept as written and read by the same code.

## Non-goals for v1

- Cross-project dedupe (privacy).
- Delta compression within a blob (git's pack files). Blob-level dedupe already solves the common case.
- Client-side diff of project file internals (would require a DAW-format-aware differ; huge scope). The backend's FL Studio mixer diff is server-side and reads the stored project file.

## Open questions

- **Blob size ceiling.** Multi-gigabyte recordings will need multipart/resumable upload (GCS supports it). Defer until we see it in practice.
- **Encryption at rest.** GCS default is fine for alpha; if we later need customer-managed keys, the blob abstraction contains the change.
- **Public projects & blob visibility.** When a project is `is_public=True`, blobs referenced by public versions must be readable by anonymous clients. Solution: signed GET URLs from an authenticated endpoint that checks the project's visibility.
