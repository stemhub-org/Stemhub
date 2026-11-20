# Parser Fixture Corpus

This corpus defines the FL Studio project files StemHub uses to validate parser-facing work.

## Goals

- keep parser regressions tied to named fixtures instead of one-off local exports
- let roadmap work add assertions against known projects incrementally
- preserve both success and failure fixtures so error handling stays covered

## Current seed corpus

Raw FLP fixtures live under `backend/tests/fixtures/parser_corpus/assets/fl_studio`.
They were seeded from the vendored `PyFLP_v2` corpus so StemHub starts from
known-good parser samples while owning its own test inputs.

The corpus holds no FL Studio 2024 or 2025 project: those would carry personal
data or Image-Line content. What the pinned parser must read in such files
(event 172, the stored insert count, mixer parameters keyed from 448) is pinned
on synthetic files in `backend/tests/test_parser_regressions.py`, next to the
effect slots of the FL 20.8.4 reference project.

## Adding a fixture

1. Add the `.flp` project file under `assets/fl_studio/`.
2. Create a new entry in `corpus.json`.
3. Record only stable expectations:
   - parse success or error
   - project metadata that should not drift accidentally
   - mixer facts needed by current roadmap work
4. Prefer message substrings over full parser exception messages when validating failures.

## Schema notes

- `id`: stable fixture identifier used by test output
- `path`: repository-relative path to the binary project fixture
- `kind`: currently only `fl_studio_project`
- `expectations.parse`: `success` or `error`
- `expectations.project`: optional project-level assertions for successful parses
- `expectations.mixer`: optional assertions on the mixer StemHub reads from the project file (`fl_mixer.parse_fl_mixer`): `mixer_supported`, `insert_count`, `effect_slot_count` (the loaded effect slots of all inserts), `named_inserts_prefix`, `flp_sha256`, `flp_size_bytes`
- `expectations.error`: expected exception type and message substring for invalid fixtures
