# Project model

The **project model** is the DAW-neutral JSON of a project's musical content: tempo, instruments, patterns and notes, the arrangement with its tracks and clips, the mixer and automation. Terms follow the glossary in [SPECIFICATION.md §19](./SPECIFICATION.md#19--glossary).

This page describes **schema 0.1.0** (#170). Today the backend validates documents (`POST /api/project-models/validate`); reading them from FL Studio project files and writing them back come later (#153).

- Models (the source of truth): `backend/src/stemhub/project_model/schema.py`
- JSON Schema, generated from the models and committed: `backend/src/stemhub/project_model/schema/project-model-0.1.0.schema.json`
- Example documents: `backend/tests/fixtures/project_model/examples/` (`minimal.json`, `basic-beat.json`, `fl2025-no-tempo.json`)

## Shape

The model follows FL Studio's general shape: a pattern holds notes for several instruments, a track holds any kind of clip, and the mixer is separate from the tracks. Units borrow from DAWproject, so a later export can flatten the model to other DAWs.

```json
{
  "schema": "stemhub.project-model", "schema_version": "0.1.0",
  "generator": {"name": "stemhub-fl", "version": "0.1.0", "pyflp": "54d2b96"},
  "source": {"daw": "fl_studio", "daw_version": "24.1.2.4430", "ppq": 96},
  "project": {"title": "Basic beat", "artists": null, "genre": "Hip hop"},
  "tempo_bpm": 90.0, "time_signature": {"numerator": 4, "denominator": 4},
  "instruments": [{"id": "instrument:0", "kind": "sampler", "name": "Kick", "color": "#5C656A", "enabled": true,
      "volume": {"fader": 0.78125, "db": null}, "pan": 0.0, "insert": {"status": "known", "insert_id": "insert:1"},
      "sample": {"location": "project", "asset_path": "Samples/Drums/Kick 01.wav"}, "plugin": null,
      "extensions": {"fl_studio": {"channel_iid": 0}}}],
  "audio_sources": [],
  "patterns": [{"id": "pattern:1", "name": "Drums", "length_ticks": 384,
      "notes": [{"instrument_id": "instrument:0", "pitch": 60, "start_ticks": 0, "length_ticks": 48,
                 "velocity": 0.78125, "pan": 0.0, "fine_pitch_cents": 0, "extensions": {"fl_studio": {"ordinal": 0}}}]}],
  "arrangements": [{"id": "arrangement:0", "name": "Arrangement", "markers": [],
      "tracks": [{"id": "track:1", "number": 1, "name": "Drums", "enabled": true,
          "clips": [{"kind": "pattern", "source_id": "pattern:1", "start_ticks": 0, "length_ticks": 384,
                     "offset_start_ticks": null, "offset_end_ticks": null, "muted": false,
                     "extensions": {"fl_studio": {"ordinal": 0}}}]}]}],
  "current_arrangement_id": "arrangement:0",
  "mixer": {"inserts": [
      {"id": "insert:0", "number": 0, "name": "Master", "enabled": true, "volume": {"fader": 1.0, "db": null},
       "pan": 0.0, "stereo_separation": 0.0, "sends": [], "effect_slots": []},
      {"id": "insert:1", "number": 1, "name": "Drums", "enabled": true, "volume": {"fader": 0.9, "db": null},
       "pan": 0.0, "stereo_separation": 0.0, "sends": [{"to_insert_id": "insert:0", "level": 1.0}], "effect_slots": []}]},
  "automation": [],
  "unsupported": [], "warnings": [],
  "extensions": {}
}
```

Every field is present in every document: an absent value is an explicit `null`, never a missing key. Only `extensions` may be left out (it defaults to `{}`).

| Field | Holds |
|---|---|
| `schema`, `schema_version` | Always `"stemhub.project-model"` and `"0.1.0"`. |
| `generator` | What wrote the document: `name`, `version`, and the PyFLP commit (`pyflp`, or `null`). |
| `source` | The project file it was read from: `daw` (`"fl_studio"`), `daw_version` (e.g. `25.2.4.4960`, or `null`) and `ppq`, the ticks per quarter note of every `*_ticks` value. `ppq` is never assumed: FL Studio files use 96, and one was seen with 72. |
| `project` | `title`, `artists`, `genre` (each a name or `null`). |
| `tempo_bpm` | The tempo, or `null` only when the project file stores none (with an `unsupported[]` entry). |
| `time_signature` | `numerator` 1…99, `denominator` 1, 2, 4, 8, 16 or 32. |
| `instruments[]` | `kind` `sampler` (plays `sample`; `plugin` is `null`) or `plugin` (played by `plugin`; `sample` is `null`), plus `name`, `color` (`#RRGGBB` or `null`), `enabled`, `volume`, `pan` and `insert`, the insert it's routed to. |
| `audio_sources[]` | Audio that audio clips play (FL Studio: an audio clip channel): `name`, `sample`, `insert`. |
| `patterns[]` | `name`, `length_ticks` and `notes[]`: `instrument_id`, `pitch`, `start_ticks`, `length_ticks` (0 for step-sequencer notes), `velocity`, `pan`, `fine_pitch_cents`. |
| `arrangements[]` | At least one. `name`, `markers[]` (`position_ticks`, `name`) and `tracks[]`: `number`, `name`, `enabled`, `clips[]`. A clip's `kind` is `pattern`, `audio` or `automation` and its `source_id` must name that kind of source; it also has `start_ticks`, `length_ticks`, `offset_start_ticks` and `offset_end_ticks` (`null` when not trimmed) and `muted`. |
| `current_arrangement_id` | The arrangement FL Studio shows. |
| `mixer.inserts[]` | `number` (Master insert = 0), `name`, `enabled`, `volume`, `pan`, `stereo_separation`, `sends[]` (`to_insert_id`, `level`) and `effect_slots[]` (`index`, `enabled`, `dry_wet`, `plugin`). |
| `automation[]` | A parameter's change over time: `name`, `target` and `points[]` (`position_ticks` from the clip's start, `value` 0…1 normalized, `tension` −1…1). `target` is `null` (linked to nothing), `{"kind": "tempo"}`, an instrument's `volume`/`pan`, an insert's `volume`/`pan`/`stereo_separation`, an effect slot's `enabled`/`dry_wet`, or unknown. |
| `unsupported[]` | What the project file holds that the model doesn't: `path` (a JSON pointer, covering that value and everything below it) and `reason`. |
| `warnings[]` | What was read with a loss, e.g. a name with invalid characters replaced: `path` and `message`. |
| `extensions` | On the document and most objects: free-form JSON per DAW namespace (`fl_studio`) for what the core fields don't model. |

A **plugin** (instrument or effect) is `format` (`fl_native`, `vst2` or `vst3`), `name`, `vendor`, `plugin_id` (e.g. a VST3 class ID, or `null`) and `state`: the SHA-256 and size of its saved state, whose bytes stay in the project file.

### Unknown values

A value the reader can't read is never `null` where `null` means something else. An insert routing or automation target it can't decode is `{"status": "unknown"}`; a tempo the file doesn't store is `null`. Either way the document has an `unsupported[]` entry at that path or above it (`/instruments/0/insert`, `/instruments/0` or `/instruments`). This keeps an FL Studio 2025 version, whose routing isn't decoded yet, from looking "unrouted" next to an FL Studio 24.1 one.

### What isn't listed

An insert or track that isn't listed has default values: it is not "removed". This is how FL Studio 2025's sparse mixer and FL Studio 20's 127 inserts and 500 tracks compare cleanly. An insert that something refers to (an instrument or audio source routed to it, a send, an automation target) is listed, even with default values.

## IDs

IDs are strings built from FL Studio's own numbering, never random, so the same file always gives the same IDs:

| ID | From FL Studio |
|---|---|
| `instrument:<iid>` | The channel's index in the Channel Rack (sampler and generator channels). |
| `audio:<iid>` | An audio clip channel's index. |
| `automation:<iid>` | An automation clip channel's index. |
| `pattern:<iid>` | The pattern's number. |
| `arrangement:<iid>` | The arrangement's index. |
| `insert:<n>` | The insert's number, 0 (Master) to 126. It equals the insert's `number`. |
| `track:<n>` | The playlist track's number, 1 to 500. It equals the track's `number`. |

Numbers have no leading zeros. Notes and clips have no ID: they carry their place in the project file as `extensions.fl_studio.ordinal`, so a writer can keep each item's original bytes. Ordinals are non-negative integers, unique per pattern for notes and per arrangement for clips (FL Studio keeps one list of clips per arrangement). Diffs between versions match notes and clips by content, not by ordinal.

## Units

Every FL Studio value converts exactly both ways: `to_raw(from_raw(raw)) == raw` for every raw value FL Studio can store, which the tests check value by value. So the model never needs to keep raw values. The conversions live in `backend/src/stemhub/project_model/units.py`.

| Field | Model unit | FL Studio raw value | Conversion |
|---|---|---|---|
| `instruments[].volume.fader` | 0…1 (1.0 = FL's 100%, the channel fader's top) | Channel volume 0…12800 (default 10000, shown as 78%) | raw / 12800 |
| `mixer.inserts[].volume.fader` | 0…1.25 (1.25 = FL's 125%, the insert fader's top) | Insert volume 0…16000 (12800 = 100%, 0 dB) | raw / 12800 |
| `volume.db` | Always `null` in 0.1.0 | — | None yet: the fader-to-dB curve is calibrated in M2, and a later schema version gives `db` a value |
| `instruments[].pan` | −1 (left)…+1 (right) | Channel pan 0…12800, 6400 centred | (raw − 6400) / 6400 |
| `mixer.inserts[].pan` | −1…+1 | Insert pan −6400…6400 | raw / 6400 |
| `stereo_separation` | −1 (merged, mono)…+1 (separated) | −64 (separated)…64 (merged) | −raw / 64 |
| `notes[].velocity` | 0…1 | 0…128 (default 100) | raw / 128 |
| `notes[].pan` | −1…+1 | 0…128, 64 centred | (raw − 64) / 64 |
| `notes[].pitch` | Integer 0…131, MIDI numbering (60 = middle C, FL's "C5") | Key 0…131 | Unchanged. Named `pitch` because §19 keeps "key" for the musical key. |
| `notes[].fine_pitch_cents` | −1200…1200 cents, in steps of 10 | 0…240, 120 = no detune | (raw − 120) × 10 |
| `effect_slots[].dry_wet` | 0…1 | Slot mix 0…12800 (default 12800) | raw / 12800 |
| `sends[].level` | 0…1 | Route level 0…12800 (to confirm on FL Studio 2025 files) | raw / 12800 |
| `tempo_bpm` | BPM | BPM × 1000, 10 000…522 000 | raw / 1000 |
| `*_ticks` | Integer ≥ 0, at `source.ppq` | Ticks | Unchanged (at most 2³¹ − 1) |

Effect slot `index` is 0-based; FL Studio shows it as index + 1 (10 slots per insert).

## Privacy

Project files are personal data (spec §11). The model has no field for, so never holds: the licensee, the data folder path, the project comments, URL, creation date and time spent, or any absolute path.

A `sample` is one of:

- `{"location": "project", "asset_path": …}`: a file inside the project folder, by its path as in the version's manifest (relative, `/`-separated, at most 255 characters, no `..`, drive letter, backslash, control character or reserved Windows name: the StemHub plugin's manifest path rules);
- `{"location": "factory", "path": "%FLStudioFactoryData%/…"}`: a file shipped with FL Studio, under its `%FL…%` folder variable;
- `{"location": "external", "file_name": …}`: anything else, reduced to its file name. The name follows the rules of one segment of an asset path (not `.` or `..`, no colon, not ending with a dot or a space, no reserved Windows name) and holds no bidirectional control character (U+202A to U+202E, U+2066 to U+2069), which could make it display as another name.

Validation errors never quote the submitted values (names, paths, IDs) nor the keys the submitter chose: only where the problem is, a code and a message. In an error's `path`, an unknown field shows as `<unknown field>` and an `extensions` namespace as `<namespace>` (`/project/<unknown field>`, `/extensions/<namespace>`).

## Validation

`validate_json` / `validate_document` (`stemhub.project_model`) run two passes and report the problems as `{path, code, message}`, `path` being an RFC 6901 JSON pointer (`""` is the whole document) in which an unknown field's name and an `extensions` namespace are replaced by `<unknown field>` and `<namespace>` (see [Privacy](#privacy)).

Reports are bounded by the format's shape, not by the body's size, so a large body can't make validation build millions of errors: in the first pass, each list reports its first invalid item only, each object its first 8 unknown fields only, and an `extensions` object with more than 16 namespaces is one `too_long` error. Fix the reported problems and validate again to see the next ones. The second pass reports every problem it finds.

**Parsing.** UTF-8 JSON only; an object repeating a key is refused (`json_invalid`).

**First pass: each object on its own** (the Pydantic models). Types and ranges above; unknown fields refused; integers strict (`1.0` and `true` are not integers, so `4.0` is not a time signature `denominator`), numbers accept integers; no NaN or infinity; ID shapes; a clip's `source_id` matching its `kind`; `id` matching `number` for inserts and tracks; names at most 255 characters without NUL. `extensions` hold any JSON, at most 8 levels deep (the `extensions` object is level 1) and 64 KiB as compact UTF-8 JSON each, under at most 16 namespaces named in snake case.

| Cap | Limit |
|---|---|
| Notes, in the whole model | 100 000 |
| Clips, in the whole model | 20 000 |
| Patterns | 999 |
| Inserts | 127 |
| Tracks per arrangement | 500 |
| Effect slots per insert | 10 |
| Instruments, audio sources, automation, arrangements, markers per arrangement | 999 each |
| Points per automation | 10 000 |
| `unsupported[]`, `warnings[]` | 1000 each |

**Second pass: references** (`references.check_references`), on a model the first pass accepted:

- no duplicate IDs, track numbers (per arrangement), insert numbers, effect slot indexes (per insert) or sends to one destination; no insert sending to itself;
- every reference resolves: a note's instrument, a clip's source, `current_arrangement_id`, an instrument's or audio source's insert, a send's destination, an automation target;
- notes sorted by (`start_ticks`, ordinal), clips of a track likewise, automation points by `position_ticks`; ordinals valid and unique;
- a `null` tempo or an `{"status": "unknown"}` value has an `unsupported[]` entry at its path or above it.

Codes include `missing`, `extra_forbidden`, `int_type`, `string_pattern_mismatch`, `reserved_file_name`, `invalid_kind` (and `invalid_status`, `invalid_location`), `too_long`, `too_many_notes`, `too_many_clips`, `extensions_too_deep`, `extensions_too_large`, `duplicate_id`, `duplicate_number`, `dangling_reference`, `not_sorted`, `duplicate_ordinal`, `invalid_ordinal`, `send_to_self` and `missing_unsupported_entry`.

The committed JSON Schema expresses the first pass except strict integers (JSON Schema counts `1.0` as an integer, so it accepts a `denominator` of `4.0`: only the backend refuses it), reserved Windows names, `id` matching `number`, the `extensions` depth and size caps and the whole-model caps. The second pass is backend-only.

### Endpoint

`POST /api/project-models/validate`, for signed-in users. The body is the document (`application/json`), at most 10 MiB: a larger `Content-Length`, or more bytes actually received, gives 413. Nothing is stored.

- `200 {"valid": true, "schema_version": "0.1.0"}`
- `422 {"valid": false, "errors": [{"path", "code", "message"}], "truncated": false}`: at most 100 errors; `truncated` says more were found. A body that isn't JSON is one `json_invalid` error at `""`.
- `429 {"detail": "Too many validations in progress, retry shortly"}` with `Retry-After: 1`: validating a body at the cap takes a few hundred MiB, so each backend process runs at most 2 validations at once. A request arriving while 2 run is answered right away, not queued.

## Versioning

`schema_version` names the format a document follows. Every change to the format gets a new version and its own schema file, `project-model-<version>.schema.json`, whose `$id` is `urn:stemhub:project-model:<version>` (a URN, so no web address is implied) and whose dialect is JSON Schema 2020-12. Versions 0.x may change incompatibly.

The JSON Schema is generated from the Pydantic models, never edited by hand. After changing a model, regenerate it from the repository root and commit it:

```bash
PYTHONPATH=backend/src python -m stemhub.project_model schema          # rewrite the committed file
PYTHONPATH=backend/src python -m stemhub.project_model schema --check  # exit 1 if it is out of date
PYTHONPATH=backend/src python -m stemhub.project_model validate model.json
```

A test fails when the committed file differs from the generated one, and every example document must validate against the committed file (with `jsonschema`) and the models. Pydantic is held to one minor version in `backend/pyproject.toml` because a new minor can change the generated schema.
