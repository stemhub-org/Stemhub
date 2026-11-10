"""The raw FLP tokenizer and writer (stemhub.project_model.flp_events).

A no-op rebuild gives back the same bytes, and anything that isn't a
well-formed FLP raises FlpFormatError, never another exception. Scrubbing is
tested in test_flp_scrub.py, the command line in test_flp_cli.py; what they
share is in flp_helpers.py.

Image-Line's demo files and your own FL Studio 2025 files are never committed.
Point STEMHUB_EXTENDED_CORPUS at their folders (several folders separated by
the OS path separator) to run the same checks on every *.flp under them.
"""
from __future__ import annotations

import dataclasses
import random
import struct
from pathlib import Path
from typing import Optional

import pytest

from flp_helpers import (
    CORRUPTED_DIR,
    DATA_PATH,
    EXTENDED_CORPUS,
    FAKE_USER,
    FL_STUDIO_TEXT,
    FL_VERSION,
    GOOD_BODY,
    HEADER_SIZE,
    LICENSEE,
    NEW_IN_25_2_3,
    PLUGIN_DATA,
    REFERENCE,
    SAMPLE_PATH,
    TEMPO,
    TIMESTAMP,
    VALID_FIXTURES,
    ascii_text,
    fixed_event,
    fl2025_file,
    flp_data,
    private_file,
    utf16_text,
    var_event,
)
from stemhub.project_model.flp_events import (
    Event,
    FlpFile,
    FlpFormatError,
    FlpHeader,
    check_structure,
    decode_text,
    diff_events,
    event_offsets,
    fl_version,
    parse,
    payload_size,
    scrub,
    serialize,
)

# --- the local corpora (never committed) ---------------------------------------------------


@pytest.mark.parametrize("path", EXTENDED_CORPUS)
def test_every_local_project_file_rebuilds_byte_for_byte(path: Optional[Path]) -> None:
    if path is None:
        pytest.fail("STEMHUB_EXTENDED_CORPUS is set but holds no .flp file")
    data = path.read_bytes()
    file = parse(data)

    assert serialize(file) == data
    assert parse(serialize(file)) == file


@pytest.mark.parametrize("path", EXTENDED_CORPUS)
def test_every_local_project_file_has_its_tempo_and_a_sound_structure(path: Optional[Path]) -> None:
    if path is None:
        pytest.fail("STEMHUB_EXTENDED_CORPUS is set but holds no .flp file")
    file = parse(path.read_bytes())
    version = fl_version(file)

    assert TEMPO in [event.id for event in file.events]
    assert version is not None
    assert check_structure(file) == ()
    for event in file.events:
        if event.id == NEW_IN_25_2_3:
            assert version >= (25, 2, 3)  # 0xAC is read the same in every file; only 25.2.3+ has it


# --- the committed fixtures ----------------------------------------------------------------


@pytest.mark.parametrize("path", VALID_FIXTURES)
def test_a_no_op_rebuild_gives_the_same_bytes(path: Path) -> None:
    data = path.read_bytes()
    file = parse(data)

    assert serialize(file) == data
    assert parse(serialize(file)) == file


def test_the_reference_fixture_reads_as_fl_20_8_4() -> None:
    file = parse(REFERENCE.read_bytes())

    assert file.header == FlpHeader(format=0, channel_count=19, ppq=96)
    assert fl_version(file) == (20, 8, 4, 2576)
    assert file.events[0].id == FL_VERSION
    tempo = next(event for event in file.events if event.id == TEMPO)
    assert tempo.int_value == 69420


@pytest.mark.parametrize(
    ("name", "message"),
    [
        ("invalid-header-magic.flp", "FLhd"),
        ("invalid-data-magic.flp", "FLdt"),
        ("invalid-header-size.flp", "header chunk"),
        ("invalid-event-size.flp", "data chunk"),
    ],
)
def test_the_corrupted_fixtures_are_refused(name: str, message: str) -> None:
    with pytest.raises(FlpFormatError, match=message):
        parse((CORRUPTED_DIR / name).read_bytes())


@pytest.mark.parametrize("name", ["invalid-format.flp", "invalid-ppq.flp"])
def test_header_values_are_left_to_the_reader(name: str) -> None:
    # The raw layer judges the structure only: an odd format or PPQ still rebuilds.
    data = (CORRUPTED_DIR / name).read_bytes()

    assert serialize(parse(data)) == data


# --- event 0xAC (FL 25.2.3+) ---------------------------------------------------------------


def test_fl_25_2_3_event_0xac_has_three_bytes_and_the_tempo_follows() -> None:
    data = flp_data(
        var_event(FL_VERSION, ascii_text("25.2.4.4960"))
        + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
        + fixed_event(TEMPO, 120000, 4)
    )
    file = parse(data)

    assert [event.id for event in file.events] == [FL_VERSION, NEW_IN_25_2_3, TEMPO]
    assert file.events[1].payload == b"\x01\x01\x00"
    assert file.events[2].int_value == 120000
    assert serialize(file) == data


def test_the_fl_2025_text_event_follows_0xac() -> None:
    file = parse(fl2025_file(128000))

    assert [event.id for event in file.events] == [FL_VERSION, NEW_IN_25_2_3, FL_STUDIO_TEXT, TEMPO]
    assert decode_text(file.events[2], fl_version(file)) == "FL Studio 25.2.4.4960.4960"
    assert file.events[3].int_value == 128000


@pytest.mark.parametrize(
    "body",
    [
        pytest.param(fixed_event(NEW_IN_25_2_3, 0x000101, 3) + fixed_event(TEMPO, 120000, 4), id="no version event"),
        pytest.param(
            var_event(FL_VERSION, ascii_text("25.2.0.5125"))
            + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
            + fixed_event(TEMPO, 120000, 4),
            id="FL 25.2.0",
        ),
        pytest.param(
            var_event(FL_VERSION, ascii_text("12.9.3.321"))
            + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
            + fixed_event(TEMPO, 120000, 4),
            id="FL 12.9.3",
        ),
    ],
)
def test_event_0xac_has_three_bytes_whatever_the_version(body: bytes) -> None:
    # No file before 25.2.3 holds a 0xAC, and neither PyFLP nor FLPEdit names it: one rule for all.
    file = parse(flp_data(body))

    assert [(event.id, len(event.payload)) for event in file.events][-2:] == [(NEW_IN_25_2_3, 3), (TEMPO, 4)]
    assert file.events[-1].int_value == 120000


@pytest.mark.parametrize(
    ("event_id", "size"),
    [
        (0, 1),
        (63, 1),
        (64, 2),
        (127, 2),
        (128, 4),
        (TEMPO, 4),
        (NEW_IN_25_2_3 - 1, 4),
        (NEW_IN_25_2_3, 3),
        (NEW_IN_25_2_3 + 1, 4),
        (191, 4),
        (192, None),
        (255, None),
    ],
)
def test_payload_size_rules(event_id: int, size: Optional[int]) -> None:
    assert payload_size(event_id) == size


@pytest.mark.parametrize("event_id", [256, -1])
def test_payload_size_refuses_an_id_outside_a_byte(event_id: int) -> None:
    with pytest.raises(FlpFormatError):
        payload_size(event_id)


# --- checking the structure ----------------------------------------------------------------


def test_a_well_formed_fl_2025_file_has_no_structure_warning() -> None:
    assert check_structure(parse(fl2025_file())) == ()


@pytest.mark.parametrize("path", VALID_FIXTURES)
def test_the_committed_fixtures_have_no_structure_warning(path: Path) -> None:
    assert check_structure(parse(path.read_bytes())) == ()


@pytest.mark.parametrize(
    ("body", "message"),
    [
        pytest.param(
            var_event(FL_VERSION, ascii_text("25.2.4.4960"))
            + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
            + fixed_event(TEMPO, 1, 4),
            "is followed by event 156",
            id="0xAC then the tempo",
        ),
        pytest.param(
            var_event(FL_VERSION, ascii_text("25.2.4.4960")) + fixed_event(NEW_IN_25_2_3, 0x000101, 3),
            "is the last event",
            id="0xAC last",
        ),
        pytest.param(
            var_event(FL_VERSION, ascii_text("25.2.4.4960"))
            + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
            + var_event(FL_STUDIO_TEXT, utf16_text("FL Studio 25.2.3.5164.5164")),
            "does not name the FL version",
            id="another version",
        ),
        pytest.param(
            var_event(FL_VERSION, ascii_text("25.2.4.4960"))
            + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
            + var_event(FL_STUDIO_TEXT, utf16_text("FL Studio 25.2.4.49601")),
            "does not name the FL version",
            id="a longer build number",
        ),
        pytest.param(
            fixed_event(NEW_IN_25_2_3, 0x000101, 3) + var_event(FL_STUDIO_TEXT, utf16_text(f"Studio of {FAKE_USER}")),
            "does not name the FL version",
            id="not the FL Studio text",
        ),
        pytest.param(fixed_event(TEMPO, 1, 4), "no FL version event", id="no version event"),
    ],
)
def test_check_structure_warns_about_a_misread_stream(body: bytes, message: str) -> None:
    warnings = check_structure(parse(flp_data(body)))

    assert any(message in warning for warning in warnings)
    assert not any(FAKE_USER in warning or "Studio of" in warning for warning in warnings)


def test_without_a_version_event_the_fl_studio_text_still_counts() -> None:
    body = fixed_event(NEW_IN_25_2_3, 0x000101, 3) + var_event(FL_STUDIO_TEXT, utf16_text("FL Studio 25.2.4.4960.4960"))

    assert check_structure(parse(flp_data(body))) == ("no FL version event (199)",)


# --- length prefixes -----------------------------------------------------------------------


def test_a_longer_than_needed_length_prefix_is_kept() -> None:
    data = flp_data(bytes([LICENSEE, 0x84, 0x80, 0x00]) + b"abcd" + fixed_event(TEMPO, 1, 4))
    file = parse(data)

    assert file.events[0] == Event(LICENSEE, b"abcd", length_width=3)
    assert serialize(file) == data
    assert serialize(dataclasses.replace(file, events=(Event(LICENSEE, b"abcd"),) + file.events[1:])) != data


def test_long_payloads_take_a_multi_byte_length_prefix() -> None:
    payload = bytes(range(256)) * 3
    data = flp_data(bytes([PLUGIN_DATA]) + b"\x80\x06" + payload)
    file = parse(data)

    assert file.events == (Event(PLUGIN_DATA, payload),)
    assert serialize(file) == data
    assert event_offsets(file) == (HEADER_SIZE,)


def test_a_length_prefix_longer_than_five_bytes_is_refused() -> None:
    with pytest.raises(FlpFormatError, match="length prefix"):
        parse(flp_data(bytes([PLUGIN_DATA, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00])))


def test_a_length_width_is_a_minimum_and_the_shortest_one_is_not_recorded() -> None:
    assert Event(LICENSEE, b"ab", length_width=1).length_width is None
    assert Event(LICENSEE, b"ab", length_width=2).length_width == 2
    assert Event(LICENSEE, b"x" * 200, length_width=1).length_width is None
    assert Event(LICENSEE, b"x" * 200, length_width=3).length_width == 3


def test_a_payload_that_outgrows_a_long_length_prefix_widens_it() -> None:
    event = dataclasses.replace(Event(LICENSEE, b"ab", length_width=2), payload=b"x" * 20000)
    file = FlpFile(FlpHeader(0, 1, 96), (event,))

    assert event.length_width is None
    assert parse(serialize(file)) == file


@pytest.mark.parametrize(
    "build",
    [
        lambda: Event(256, b""),
        lambda: Event(-1, b""),
        lambda: Event(LICENSEE, b"", length_width=6),
        lambda: FlpHeader(format=0x8000, channel_count=1, ppq=96),
        lambda: FlpHeader(format=0, channel_count=-1, ppq=96),
        lambda: FlpHeader(format=0, channel_count=1, ppq=0x10000),
    ],
)
def test_values_that_cannot_be_written_are_refused(build) -> None:
    with pytest.raises(FlpFormatError):
        build()


@pytest.mark.parametrize(
    "build",
    [
        lambda: Event(True, b""),
        lambda: Event(TEMPO, "text"),
        lambda: FlpFile(FlpHeader(0, 1, 96), ("not an event",)),
        lambda: FlpFile("header", ()),
        lambda: FlpHeader(format="0", channel_count=1, ppq=96),
        lambda: Event(TEMPO, b"\x00" * 4, length_width="2"),
        lambda: parse("FLhd"),
    ],
)
def test_wrong_types_are_refused(build) -> None:
    with pytest.raises(TypeError):
        build()


def test_a_file_takes_any_sequence_of_events_and_keeps_a_tuple() -> None:
    file = FlpFile(FlpHeader(0, 1, 96), [Event(TEMPO, b"\x00\x00\x00\x00")])

    assert file.events == (Event(TEMPO, b"\x00\x00\x00\x00"),)


# --- writing -------------------------------------------------------------------------------


@pytest.mark.parametrize(
    ("events", "message"),
    [
        ((Event(TEMPO, b"\x01\x02\x03"),), "3-byte payload"),
        ((Event(FL_VERSION, ascii_text("25.2.4.4960")), Event(NEW_IN_25_2_3, b"\x01\x01\x00\x00")), "4-byte payload"),
        ((Event(FL_VERSION, ascii_text("25.2.0.5125")), Event(NEW_IN_25_2_3, b"\x01\x01\x00\x00")), "4-byte payload"),
        ((Event(TEMPO, b"\x01\x02\x03\x04", length_width=2),), "no length prefix"),
    ],
)
def test_serialize_refuses_what_fl_would_misread(events: tuple[Event, ...], message: str) -> None:
    with pytest.raises(FlpFormatError, match=message):
        serialize(FlpFile(FlpHeader(0, 1, 96), events))


def test_an_edited_event_is_written_in_place() -> None:
    file = parse(fl2025_file(120000))
    edited = dataclasses.replace(
        file,
        events=tuple(
            dataclasses.replace(event, payload=(95000).to_bytes(4, "little")) if event.id == TEMPO else event
            for event in file.events
        ),
    )

    assert serialize(edited) == fl2025_file(95000)


def test_event_offsets_point_at_each_event_id() -> None:
    data = fl2025_file()
    file = parse(data)
    offsets = event_offsets(file)

    assert offsets[0] == HEADER_SIZE
    assert [data[offset] for offset in offsets] == [event.id for event in file.events]


# --- refusing malformed files --------------------------------------------------------------





@pytest.mark.parametrize(
    ("data", "message"),
    [
        pytest.param(b"", "too short", id="empty"),
        pytest.param(b"FLhd\x06\x00", "too short", id="header cut"),
        pytest.param(b"RIFF" + flp_data(GOOD_BODY)[4:], "FLhd", id="header magic"),
        pytest.param(b"FLhd\x07" + flp_data(GOOD_BODY)[5:], "header chunk", id="header size"),
        pytest.param(flp_data(GOOD_BODY)[:14] + b"FLxx" + flp_data(GOOD_BODY)[18:], "FLdt", id="data magic"),
        pytest.param(flp_data(GOOD_BODY, data_len=len(GOOD_BODY) + 1), "data chunk", id="data size past the end"),
        pytest.param(flp_data(GOOD_BODY, data_len=len(GOOD_BODY) - 1), "data chunk", id="bytes after the data"),
        pytest.param(flp_data(GOOD_BODY, data_len=0xFFFFFFFF), "data chunk", id="huge data size"),
        pytest.param(flp_data(bytes([TEMPO, 1, 2])), "needs 4", id="fixed event cut"),
        pytest.param(flp_data(bytes([PLUGIN_DATA, 0x85])), "length prefix", id="length prefix cut"),
        pytest.param(flp_data(bytes([PLUGIN_DATA, 0x10, 1, 2])), "needs 16", id="payload cut"),
    ],
)
def test_malformed_files_raise_flp_format_error(data: bytes, message: str) -> None:
    with pytest.raises(FlpFormatError, match=message):
        parse(data)


def test_error_messages_never_quote_payload_text() -> None:
    secret = f"C:\\Users\\{FAKE_USER}\\secret.wav"
    body = var_event(FL_VERSION, ascii_text("25.2.4.4960")) + bytes([SAMPLE_PATH, 0x7F]) + utf16_text(secret)

    with pytest.raises(FlpFormatError) as raised:
        parse(flp_data(body))

    message = str(raised.value)
    assert FAKE_USER not in message and "secret" not in message
    assert "offset" in message and str(SAMPLE_PATH) in message


def test_parse_accepts_any_bytes_like_input() -> None:
    data = fl2025_file()

    assert parse(bytearray(data)) == parse(memoryview(data)) == parse(data)


def test_parse_can_be_held_to_an_event_budget() -> None:
    data = fl2025_file()  # 4 events

    assert len(parse(data, max_events=4).events) == 4
    assert parse(flp_data(b""), max_events=0).events == ()
    with pytest.raises(FlpFormatError, match="more than 3 events"):
        parse(data, max_events=3)
    with pytest.raises(FlpFormatError, match="more than 0 events"):
        parse(data, max_events=0)


@pytest.mark.parametrize(("budget", "error"), [(-1, ValueError), (True, TypeError), (2.0, TypeError), ("4", TypeError)])
def test_an_event_budget_must_be_a_count(budget: object, error: type) -> None:
    with pytest.raises(error):
        parse(fl2025_file(), max_events=budget)


# --- fuzzing -------------------------------------------------------------------------------


def _assert_parses_or_refuses(data: bytes) -> None:
    """Every input either raises FlpFormatError or rebuilds byte for byte; checking and scrubbing it never raise."""
    try:
        file = parse(data)
    except FlpFormatError:
        return
    assert serialize(file) == data
    assert isinstance(check_structure(file), tuple)
    assert len(serialize(scrub(file))) == len(data)


def _with_data_len(data: bytes) -> bytes:
    """The same bytes with a data size that matches them, so the events get read."""
    if len(data) < HEADER_SIZE:
        return data
    return data[:18] + struct.pack("<I", len(data) - HEADER_SIZE) + data[HEADER_SIZE:]


def _reference_slice(start: int, stop: int) -> bytes:
    """A few hundred real events, so random flips land on structure more often than on data."""
    file = parse(REFERENCE.read_bytes())
    return serialize(dataclasses.replace(file, events=file.events[start:stop]))


FUZZ_INPUTS = [
    pytest.param(_reference_slice(0, 300), id="FL 20.8.4 first events"),
    pytest.param(_reference_slice(-300, None), id="FL 20.8.4 last events"),
    pytest.param(fl2025_file(), id="FL 25.2.4 synthetic"),
    pytest.param(private_file(), id="private synthetic"),
]


@pytest.mark.parametrize("data", FUZZ_INPUTS)
def test_truncated_files_only_raise_flp_format_error(data: bytes) -> None:
    cuts = set(range(0, min(len(data), 96))) | set(range(0, len(data), max(1, len(data) // 400)))
    for cut in sorted(cuts):
        _assert_parses_or_refuses(data[:cut])
        _assert_parses_or_refuses(_with_data_len(data[:cut]))


@pytest.mark.parametrize("data", FUZZ_INPUTS)
def test_random_byte_flips_only_raise_flp_format_error(data: bytes) -> None:
    generator = random.Random(0x5EED)
    for _ in range(400):
        mutated = bytearray(data)
        for _ in range(generator.randint(1, 4)):
            mutated[generator.randrange(len(mutated))] = generator.randrange(256)
        _assert_parses_or_refuses(bytes(mutated))
        _assert_parses_or_refuses(_with_data_len(bytes(mutated)))


# --- versions and text ---------------------------------------------------------------------


@pytest.mark.parametrize(
    ("body", "version"),
    [
        (var_event(FL_VERSION, ascii_text("25.2.4.4960")), (25, 2, 4, 4960)),
        (var_event(FL_VERSION, b"12.9.3.321"), (12, 9, 3, 321)),
        (var_event(FL_VERSION, ascii_text("25.x.4")), None),
        (var_event(FL_VERSION, ascii_text("")), None),
        (var_event(FL_VERSION, ascii_text("1" * 40)), None),
        (var_event(FL_VERSION, "٢٥.1".encode("utf-8")), None),
        (fixed_event(TEMPO, 1, 4), None),
    ],
)
def test_fl_version_reads_the_first_version_event(body: bytes, version: Optional[tuple[int, ...]]) -> None:
    assert fl_version(parse(flp_data(body))) == version


def test_text_is_utf16_without_its_terminator() -> None:
    assert decode_text(Event(DATA_PATH, utf16_text("Mixdown été"))) == "Mixdown été"
    assert decode_text(Event(DATA_PATH, utf16_text("Mixdown")), (25, 2, 4)) == "Mixdown"


def test_the_version_text_is_ascii() -> None:
    assert decode_text(Event(FL_VERSION, ascii_text("25.2.4.4960")), (25, 2, 4)) == "25.2.4.4960"


def test_text_before_fl_11_5_is_single_byte() -> None:
    assert decode_text(Event(194, "Caf\xe9\0".encode("cp1252")), (11, 0, 3)) == "Café"


@pytest.mark.parametrize("payload", [b"\x41", b"\x00\xd8\x41\x00", b"\xff" * 7, b"\x81\x8d\x00"])
def test_decoding_text_never_raises(payload: bytes) -> None:
    for version in (None, (9, 0), (25, 2, 4)):
        assert isinstance(decode_text(Event(194, payload), version), str)
        assert isinstance(decode_text(Event(FL_VERSION, payload), version), str)



# --- diffing -------------------------------------------------------------------------------


def test_diff_finds_a_tempo_change() -> None:
    changes = diff_events(parse(fl2025_file(140000)), parse(fl2025_file(128000)))

    assert len(changes) == 1
    assert changes[0].kind == "replace"
    assert changes[0].a_indexes == changes[0].b_indexes == (3,)


def test_diff_leaves_out_ignored_events_and_reports_inserted_ones() -> None:
    a = parse(flp_data(fixed_event(TEMPO, 1, 4) + var_event(TIMESTAMP, b"\x01" * 16) + fixed_event(TEMPO, 2, 4)))
    b = parse(
        flp_data(
            fixed_event(TEMPO, 1, 4)
            + var_event(TIMESTAMP, b"\x02" * 16)
            + fixed_event(TEMPO, 2, 4)
            + fixed_event(TEMPO, 3, 4)
        )
    )

    changes = diff_events(a, b, ignore={TIMESTAMP})

    assert [(change.kind, change.a_indexes, change.b_indexes) for change in changes] == [("insert", (), (3,))]
    assert diff_events(a, a) == ()
