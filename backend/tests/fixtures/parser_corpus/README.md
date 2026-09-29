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
- `expectations.mixer`: optional assertions on the mixer StemHub reads from the project file (`fl_mixer.parse_fl_mixer`)
- `expectations.error`: expected exception type and message substring for invalid fixtures
