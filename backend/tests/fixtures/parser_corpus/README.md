# Parser Fixture Corpus

This corpus defines the FL Studio project files StemHub uses to validate parser-facing work.

## Goals

- keep parser regressions tied to named fixtures instead of one-off local exports
- let roadmap work add assertions against known projects incrementally
- preserve both success and failure fixtures so error handling stays covered

## Current seed corpus

The initial entries are stored directly under:

- `backend/tests/fixtures/parser_corpus/assets/fl_studio` for raw FLP fixtures
- `backend/tests/fixtures/parser_corpus/assets/stemhub_snapshots` for StemHub-style snapshot archives

The FLP fixtures were seeded from the vendored `PyFLP_v2` corpus so StemHub starts from known-good parser samples while owning its own test inputs.

## Adding a fixture

1. Add the binary asset under `assets/`.
2. Create a new entry in `manifest.json`.
3. Record only stable expectations:
   - parse success or error
   - project metadata that should not drift accidentally
   - mixer snapshot facts needed by current roadmap work
   - for snapshot archives, the `snapshot_manifest` values needed by the loader
4. Prefer message substrings over full parser exception messages when validating failures.

## Schema notes

- `id`: stable fixture identifier used by test output
- `path`: repository-relative path to the binary project fixture
- `kind`: `fl_studio_project` or `stemhub_snapshot_archive`
- `expectations.parse`: `success` or `error`
- `expectations.project`: optional project-level assertions for successful parses
- `expectations.mixer_snapshot`: optional StemHub mixer snapshot assertions
- `expectations.error`: expected exception type and message substring for invalid fixtures
- `snapshot_manifest`: optional archive metadata used by `load_fl_studio_mixer_snapshot`
