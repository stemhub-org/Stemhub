# FL Studio project file (.flp): what changes between versions

Reference for StemHub's FL reader and writer ([project-model.md](./project-model.md), #269–#272). It records what the raw events look like in each FL Studio version, with how sure we are of each point:

- **confirmed**: checked on every local file of the versions concerned, and consistent;
- **likely**: holds on the files seen, but not proven by a one-change test project;
- **hypothesis**: a guess to confirm with a test project.

Evidence came first from 28 local files, FL 12.9 to 25.2.4: the two repo fixtures (`backend/tests/fixtures/parser_corpus`), FL Studio's bundled demo projects (used locally only: Image-Line content, never committed) and the team's own FL 25.2.4 test projects. The [extended ids](#extended-ids-0xac-then-a-u32-confirmed-in-fls-code) and [zipped projects](#zipped-projects-zipped-loop-package) sections add 286 local files, FL 10.9 to 26.1.7 (FL Studio 2026's demos and templates, the team's FL 26.1.7 saves, and six zipped projects saved by FL 20.6 and 20.7), and FL Studio 2026's own code. FL versions are compared with the **version string** (event 199, e.g. `25.2.4.4960`), never the build number (event 159): build numbers don't increase with versions (25.2.3 is build 5164, 25.2.4 is build 4960).

The version boundary written "FL ≥ 24.2" below means "after 24.1.0.4212 and no later than 24.2.99.4720": no 24.2.0–24.2.2 file has been seen.

## File layout

`FLhd` + u32 6 + (i16 format, u16 channel count, u16 ppq), then `FLdt` + u32 data length + events. An event is an id and a payload whose size follows from the id:

| Id range | Payload |
|---|---|
| 0–63 | 1 byte |
| 64–127 | 2 bytes |
| 128–191 | 4 bytes |
| 192–255 | LEB128 varint length, then that many bytes |

### Extended ids: 0xAC then a u32 (confirmed in FL's code)

**Byte 0xAC (172) is not an event: it escapes an id above 255**, written right after it as a little-endian u32. The id's top 4 bits then give the payload size, as the ranges above do for one-byte ids:

| Extended id | Payload |
|---|---|
| `0x0…` (from 0x100) | 1 byte |
| `0x4…` | 2 bytes |
| `0x8…` | 4 bytes |
| `0xC…` | LEB128 varint length, then that many bytes |
| any other top 4 bits | none (no file seen holds one) |

This comes from FL Studio 2026's own code (26.1.7.5421, `Contents/Libs/FLEngine_x64.dylib`, arm64 slice, offsets in that slice): the event writer at `0x4e7b20` writes an id of 0–255 as one byte and any other as `0xAC` then 4 bytes; the reader at `0x4e6dd0` reads one byte, reads a u32 id after a `0xAC`, then sizes the payload by id range, or for an extended id by `id & 0xF0000000` (0, 0x4…, 0x8…, 0xC… → handlers reading 1, 2 and 4 bytes, and a LEB128 length). Every local file (286, FL 10.9 to 26.1.7) reads to its exact end with this rule. So FL can never write event 172 itself.

Two extended events exist so far (confirmed on every local file):

| Id | Payload | Versions | Meaning |
|---|---|---|---|
| `0xC0000101` | text (UTF-16LE) | ≥ 25.2.3 | `FL Studio <version>.<build>`, always the second event, right after 199 |
| `0x00000100` | 1 byte, 0 | ≥ 26.1.0 | unknown; written after the project comments (195), before the timestamps (237) |

Bytes on disk: `ac 01 01 00 c0 36` + 54 bytes of text is extended id `0xC0000101`, length 54. Until this was read in FL's code, StemHub and the `stemhub-org/PyFLP_v2` fork (#269) read it as a 3-byte event 0xAC (`01 01 00`) followed by text event 192, and FL 26's `ac 00 01 00 00` + `00` as a 3-byte 0xAC then an event 0. That lands on the same byte boundaries for every extended id whose top 4 bits are 0, 4, 8 or C: the id's top byte, last of the u32, then reads as a one-byte id of the same size (the format seems built that way). It goes out of step on the other top bits, which hold no data. Both now read extended ids as FL does, and `backend/tests/test_parser_regressions.py` fails if a fork that doesn't is pinned. Upstream PyFLP and FLPEdit read 0xAC as a plain 4-byte event, which swallows the next event's id and loses the tempo of every FL ≥ 25.2.3 file.

StemHub's tokenizer (`backend/src/stemhub/project_model/flp_events.py`) reads extended ids as FL does: an `Event` id is 0–255 or an extended id up to `0xFFFFFFFF`, and the tools print extended ids as 8-digit hex (`0xc0000101`). `check_structure` warns when an extended id has none of the four size classes (a stray 0xAC, a sign of a stream read out of step) or when the `FL Studio <version>` text doesn't name the version of event 199. [project-model.md](./project-model.md#scrubbing) says what scrubbing removes before a file is shared.

**Obfuscated payloads (observed in the writer, not in a file).** The same writer can XOR each payload with a key starting at `0xFF − (offset & 0xFF) + 0x7A` and growing by 3 per byte (`0x4e7c50`), and every reader handler checks the same flag before using a payload. FL 26's song writer writes event 28 as 1, or as 0 and then turns the flag on. No local file has 28 = 0 (199 have 1, and 87 older ones 3). PyFLP names event 28 `ProjectID.Licensed`, so these are likely the saves of an unregistered (trial) FL, which can't reopen its own projects (likely, not checked on a trial save). A reader meeting 28 = 0 should expect unreadable payloads after it.

Text is UTF-16LE with a trailing NUL from FL 11.5 on, except event 199 (ASCII).

## Project

| Event | Meaning | Versions | Confidence |
|---|---|---|---|
| 156 | Tempo, BPM × 1000 (u32). Present in every file, even at the default 140. | all | confirmed |
| 199 | FL version string (ASCII). Always the first event. | all | confirmed |
| 169 | 4 bytes, always 7. | ≥ 24.2 | confirmed (meaning unknown) |
| 200 | Licensee: the user's registered name, scrambled. Personal data: scrub before sharing. | all | confirmed |
| 202 | Data path (may hold `/Users/<name>/…`). | all | confirmed |
| 237 | Timestamps (creation and time spent). Changes on every save. | all | confirmed |
| 167 | Flips from 0 to 1 on the first "Save new version". | all | likely |
| 103 | Number of mixer insert blocks (see Mixer). | ≥ 24.2 | confirmed |

## Instruments (Channel Rack channels)

| Event | Meaning | Versions | Confidence |
|---|---|---|---|
| 0 | Enabled (1 byte: 1 on, 0 off). | all | confirmed |
| 22 | Mixer insert the channel is routed to (1 byte, FL numbering, 0 = Master). | ≤ 24.1 | confirmed |
| 104 | Replaces 22 at the same position: routed insert (2 bytes, 0 = Master). | ≥ 24.2 | confirmed on statistics; a one-change test project will prove it |
| 50 | 1 byte after 104, usually 1. | ≥ 24.2 | confirmed (meaning unknown) |
| 51, 170 | Close every channel: 51 = 0; 170 = 0 (≤ 25.1) or `0xFFFFFFFF` (≥ 25.2.0). | ≥ 24.2.99.48xx | confirmed (meaning unknown) |
| 196 | Sample path of a sampler. | all | confirmed |
| 251 | 62 bytes on an empty sampler; disappears when a sample is loaded. | 25.2.4 seen | likely |

## Patterns

Notes are event 224 (24-byte items: position, length, key, velocity, pan, fine pitch, instrument...) as in earlier versions (confirmed on FL 20.8.4). From FL 25.2.0 each pattern also carries events 52 (1 byte, 0), 171 (4 bytes, 0), 252 (2 bytes, 0) and 253 (UTF-16 `0,0,0,…`) (confirmed, meaning unknown).

## Arrangement: playlist items (event 233)

No prefix; the payload is a list of fixed-size items sorted by position. **The item size depends only on the FL version** (confirmed), and the payload length can't decide it (55440 and 119520 divide by both 60 and 80):

| FL version | Item size |
|---|---|
| ≤ 20.8.4 | 32 bytes |
| 20.99 – 24.1 | 60 bytes |
| ≥ 24.2 | 80 bytes |

The 80-byte item is the 60-byte item plus 20 bytes (f32 0.0 at +60, f64 1.0 at +64, 8 zero bytes). Fields (little-endian, likely unless noted):

| Offset | Field |
|---|---|
| +0 u32 | Position, in ticks (confirmed) |
| +4 u16 | Pattern base, always 20480 (confirmed) |
| +6 u16 | Source: ≥ 20480 is a pattern clip of pattern `value − 20480`; smaller is a channel clip (audio or automation) of that channel (confirmed) |
| +8 u32 | Length, in ticks (confirmed) |
| +12 u16 | `499 − track index` (0-based; 500 tracks per arrangement) (confirmed) |
| +14 u16 | Group (0 = none) |
| +18 u16 | Item flags, `0x0040` by default; `0x2000` = muted is a **hypothesis** |
| +24, +28 | Start and end offsets: int32 ticks for pattern clips (−1 = full length); float32 for channel clips (−1.0 = unset; unit probably milliseconds) |
| +32 u32 | Unique item id, increasing (60- and 80-byte items) |

Upstream PyFLP guesses the size from `len % 60`, which misreads every FL ≥ 24.2 playlist; the pinned fork takes the size from the FL version (#269).

Track data (event 238, one per track, 500 per arrangement) is 61 bytes in FL 12.9, 66 bytes in FL 20.8.4–24.1 and 70 bytes from FL 24.2 (confirmed); the 4 extra bytes are zero in every file. Fields (likely): +0 track number, +4 color, +8 icon, +12 enabled, +13 height (f32), +46 grouped, +47 locked.

Time markers: position = `raw & 0xFFFFFF`, action = `raw >> 24` (0x08 = time signature) (confirmed on FL 20.8.4).

## Mixer

**Inserts are a contiguous list of blocks** (confirmed): Master first, then inserts 1…N−2, then the "current" insert last. An insert's number is its block position; no event stores it. FL 12.9 to 24.1 always stores 127 blocks (older versions store fewer); FL ≥ 24.2 stores N blocks, with N in event 103 (18 in the empty template).

Each block: `[149 color] 42 [95 icon] [204 name] 236 flags`, then 10 effect slots, each as its plugin events **followed by** event 98 (the slot index ends the slot), then `[235 routing] 165 166 49 154 147`. 42 = 1 when the block has a color; 49 is always 0; 165 and 166 are usually 3 and 1 (meanings unknown). Older files lack some of these: 165 and 166 exist from FL 20.99, 42 from FL 21.0, and only 49 is new in FL ≥ 24.2 (with 103 and 169 of the project and 104, 50, 51 and 170 of the channels). An FL 20.8.4 block is `[149] [95] [204] 236`, its slots, then `[235] 154 147`.

Insert flags (event 236, bytes 4–7): 0x04 effects enabled, 0x08 enabled, 0x40 docked middle (0x4c is the default); Master and current use 0x0c (confirmed).

Routing (event 235): one bool per destination insert. FL ≤ 24.1 stores 127 entries; FL ≥ 24.2 cuts the list after its last true entry (confirmed).

**Mixer parameters (event 225)**: 12-byte items `u32 0, u8 parameter, u8 group, u16 channel data, i32 value`. The slot is `channel data & 0x3F`; with `key = channel data >> 6` (confirmed):

| FL version | Insert | Current insert |
|---|---|---|
| ≤ 24.1 | `key − 128` | keyed by its position like the others: `128 + position`, so 254 only with 127 blocks |
| ≥ 24.2 | `key − 448` | key 949 |

The ≥ 24.2 layout applies when event 103 is stored, which is how the pinned PyFLP chooses it.

One global item has key 256 in both (meaning unknown). Per insert (group 31): parameter 0 = slot enabled, 1 = slot dry/wet (0–12800), 192 = volume (12800 = 100%), 193 = pan, 194 = stereo separation, 208–226 = EQ. Send levels: FL ≤ 24.1 uses group 31 and parameter `64 + destination`; FL ≥ 24.2 uses group 32 and parameter = destination insert (confirmed). Send items seem to exist only for non-default levels (likely).

Upstream PyFLP reads the insert as `key & 0x7F` (wrong for FL ≥ 24.2), groups an effect slot with the next slot's plugin, and gives no slot enabled or dry/wet. These were fixed in the `stemhub-org/PyFLP_v2` fork (#269), which StemHub pins (`backend/vendor/PyFLP_v2`, `PYFLP_COMMIT`): each effect slot holds its own plugin, slot enabled and dry/wet are read, and insert volume and pan are read in FL ≥ 24.2 files. The fork also reads an insert's sends from the routing flags and their levels, and a channel's insert from event 104. `backend/tests/test_parser_regressions.py` fails if a parser without these fixes is pinned.

## Zipped projects ("Zipped loop package")

FL's *File > Export > Zipped loop package* writes a ZIP that `backend/src/stemhub/project_model/flp_bundle.py` reads (`read_bundle`). Confirmed on six bundles saved by FL 20.6 and 20.7, 139 sample files:

- **Layout.** One `.flp` and one file per distinct sample, **all at the root**, each sample under its own file name, factory samples (`%FLStudioFactoryData%`) included. No folders, no manifest, no extra field; deflated (a few entries stored), with DOS (FAT) attributes.
- **Paths are not rewritten.** Each sample path in the `.flp` (event 196) stays as it was when saved: an absolute path on the saving machine (`D:\Musique\…\kick.wav`), a folder variable (`%FLStudioUserData%\Audio\Sliced audio\…`), or, for a project opened from a bundle and saved again, the temporary folder FL extracted it to (`C:\Users\<name>\AppData\Local\Temp\Image-Line\{GUID}\Zip\…`). These paths are personal data: scrub before sharing.
- **Resolution is by file name.** Every sample path of the six bundles matches exactly one root entry by its last segment, ignoring case, and no entry is left over. Several channels using one file share one entry.
- FLEngine's messages for a bad bundle: "Could not find any FLP project file in this zipped archive." and "An error occured while opening the zipped project." (strings in `FLEngine_x64.dylib`).

`read_bundle` works in memory and never extracts. It refuses an archive with no `.flp` at the root or several, two names differing only in case (one would overwrite the other on Windows and macOS), an encrypted entry, or a name that isn't a project-model asset path (absolute, `..`, drive, backslash, control character, reserved Windows name), so a resolved sample's entry name can be a model `sample.asset_path` as is. It inflates only the `.flp`, up to `BundleLimits.max_project_bytes`, and caps the entry count and the total declared size. Errors give entry indexes, never names. Samples named inside a plugin's state (Slicex, DirectWave, FPC…) are not resolved; none of the six bundles has any left over.

## Still to confirm with one-change test projects

Instrument volume and pan events in FL 2025; routing through event 104 (direct proof); the mute flag bit; clip offset units for audio clips; send items at the default level; insert names and effect slots written into a given block. The ordered list of test projects is kept with #269.
