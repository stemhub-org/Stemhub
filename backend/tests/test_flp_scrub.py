"""Scrubbing personal data from FLP files (flp_events.scrub_report, scrub, licensee_names).

The synthetic files hold a made-up licensee and user name (flp_helpers.FAKE_USER).
STEMHUB_EXTENDED_CORPUS adds local files, which are never committed (see
test_flp_events.py).
"""
from __future__ import annotations

import dataclasses
from pathlib import Path
from typing import Optional

import pytest

from flp_helpers import (
    ARTISTS,
    CHANNEL_NAME,
    COMMENTS,
    DATA_PATH,
    EXTENDED_CORPUS,
    FAKE_LICENSEE,
    FAKE_USER,
    FL_VERSION,
    INSERT_NAME,
    LICENSEE,
    MIXER_PARAMS,
    NOTES,
    PLAYLIST,
    PLUGIN_DATA,
    SAMPLE_PATH,
    TITLE,
    TRACK_DATA,
    TRACK_NAME,
    VALID_FIXTURES,
    ascii_text,
    assert_names_are_gone,
    assert_same_structure,
    fl2025_file,
    flp_data,
    licensee_file,
    private_file,
    scrambled,
    utf16_text,
    var_event,
)
from stemhub.project_model.flp_events import (
    NOTE_NAME_NOT_SEARCHED,
    PLACEHOLDER_CHAR,
    REASON_LICENSEE,
    REASON_NAME_IN_DATA,
    REASON_NAME_IN_STRUCTURED,
    REASON_NAME_IN_TEXT,
    REASON_USER_FOLDER,
    REASON_USER_FOLDER_IN_STRUCTURED,
    STRUCTURED_DATA_EVENTS,
    TEXT_EVENT_IDS,
    FlpFile,
    ScrubChange,
    ScrubReport,
    ScrubReview,
    decode_text,
    licensee_names,
    parse,
    payload_size,
    scrub,
    scrub_report,
    serialize,
    user_names,
)

SHORT_LICENSEE = "Lee12345678"  # a made-up short handle: the name alone, "Lee", is too short to search


def _assert_scrubbed(file: FlpFile, report: ScrubReport) -> None:
    """What scrub_report promises for any file: the same structure, structured data untouched,
    no user name or licensee name left outside the structured events it lists."""
    scrubbed = report.file
    assert_same_structure(file, scrubbed)
    assert parse(serialize(scrubbed)) == scrubbed
    assert all(set(name) == {PLACEHOLDER_CHAR} for name in user_names(scrubbed))
    assert licensee_names(scrubbed) == frozenset()
    structured = [index for index, event in enumerate(file.events) if event.id in STRUCTURED_DATA_EVENTS]
    assert all(scrubbed.events[index] is file.events[index] for index in structured)
    listed = {entry.index for entry in report.to_review if entry.event_id in STRUCTURED_DATA_EVENTS}
    rest = tuple(event for index, event in enumerate(scrubbed.events) if index not in listed)
    assert_names_are_gone(licensee_names(file), serialize(dataclasses.replace(scrubbed, events=rest)))


# --- the local corpora (never committed) ---------------------------------------------------


@pytest.mark.parametrize("path", EXTENDED_CORPUS)
def test_scrubbing_a_local_project_file_keeps_its_structure(path: Optional[Path]) -> None:
    if path is None:
        pytest.fail("STEMHUB_EXTENDED_CORPUS is set but holds no .flp file")
    file = parse(path.read_bytes())

    assert licensee_names(file)
    _assert_scrubbed(file, scrub_report(file))


# --- scrubbing -----------------------------------------------------------------------------


def test_scrub_blanks_the_licensee_and_the_user_names() -> None:
    file = parse(private_file())
    scrubbed = scrub(file)
    data = serialize(scrubbed)

    assert_same_structure(file, scrubbed)
    assert len(data) == len(private_file())
    assert FAKE_USER.encode("ascii") not in data
    assert FAKE_USER.encode("utf-16-le") not in data
    licensee = next(event for event in scrubbed.events if event.id == LICENSEE)
    assert licensee.payload == bytes(len(licensee.payload))
    assert decode_text(scrubbed.events[3]) == "/Users/xxxxxxx/Music/Projects/"
    assert decode_text(scrubbed.events[4]) == "C:\\Users\\xxxxxxx\\Samples\\kick.wav"


def test_scrub_keeps_shared_folders_and_everything_else() -> None:
    file = parse(private_file())
    scrubbed = scrub(file)

    assert decode_text(scrubbed.events[5]) == "/Users/Shared/StemHub/snare.wav"
    assert b"/Users/Shared/Loops\x00" in scrubbed.events[6].payload
    assert b"\\Desktop\\kit\x00" in scrubbed.events[6].payload
    changed_ids = (LICENSEE, DATA_PATH, SAMPLE_PATH, PLUGIN_DATA)
    unchanged = [index for index, event in enumerate(file.events) if event.id not in changed_ids]
    assert all(scrubbed.events[index] is file.events[index] for index in unchanged)


def test_scrub_finds_every_user_name_first() -> None:
    assert user_names(parse(private_file())) == frozenset({FAKE_USER})
    assert user_names(scrub(parse(private_file()))) == frozenset({PLACEHOLDER_CHAR * len(FAKE_USER)})


def test_a_scrubbed_file_parses_back_and_scrubbing_twice_changes_nothing() -> None:
    original = private_file()
    file = parse(original)
    scrubbed = scrub(file)

    assert parse(serialize(scrubbed)) == scrubbed
    assert scrub(scrubbed) == scrubbed
    assert serialize(file) == original


@pytest.mark.parametrize("path", VALID_FIXTURES)
def test_scrubbing_a_fixture_keeps_its_structure(path: Path) -> None:
    file = parse(path.read_bytes())

    assert licensee_names(file)
    _assert_scrubbed(file, scrub_report(file))


def test_a_user_name_with_characters_outside_the_bmp_keeps_its_length() -> None:
    data_path = var_event(DATA_PATH, utf16_text("/Users/Zoë\U0001d11e/Music/"))
    file = parse(flp_data(var_event(FL_VERSION, ascii_text("25.2.4.4960")) + data_path))
    scrubbed = scrub(file)

    assert decode_text(scrubbed.events[1]) == "/Users/xxxxx/Music/"
    assert_same_structure(file, scrubbed)


def test_licensee_names_unscrambles_the_licensee() -> None:
    # Every licensee seen is a name then 6 to 8 digits; the name alone is searched too.
    assert licensee_names(parse(licensee_file())) == frozenset({FAKE_LICENSEE, FAKE_USER})


@pytest.mark.parametrize(
    ("licensee", "names"),
    [
        pytest.param("JaneDoe", {"JaneDoe"}, id="no digits"),
        pytest.param("Janet20240917", {"Janet20240917", "Janet"}, id="name of 5 characters"),
        pytest.param("Anna20240917", {"Anna20240917"}, id="name of 4 characters"),
        pytest.param(SHORT_LICENSEE, {SHORT_LICENSEE}, id="name of 3 characters"),
        pytest.param("Al20240917", {"Al20240917"}, id="name too short alone"),
        pytest.param("20240917", {"20240917"}, id="digits only"),
        pytest.param("Anna", set(), id="no digits, 4 characters"),
        pytest.param("Al", set(), id="too short"),
        pytest.param("", set(), id="empty"),
    ],
)
def test_licensee_names_leaves_out_names_too_short_to_search(licensee: str, names: set) -> None:
    assert licensee_names(parse(licensee_file(licensee=licensee))) == frozenset(names)


def test_a_file_without_a_licensee_has_no_licensee_name() -> None:
    assert licensee_names(parse(fl2025_file())) == frozenset()
    assert licensee_names(parse(flp_data(var_event(LICENSEE, bytes(12))))) == frozenset()  # a trial version's


def test_scrub_blanks_text_holding_the_licensees_name_and_replaces_it_in_data() -> None:
    file = parse(
        licensee_file(
            var_event(TITLE, utf16_text(f"Beat for {FAKE_USER.lower()}")),
            var_event(CHANNEL_NAME, utf16_text("Kick")),
            var_event(
                PLUGIN_DATA, b"author=" + FAKE_USER.upper().encode() + b"\x00by " + FAKE_USER.encode("utf-16-le")
            ),
            var_event(DATA_PATH, utf16_text(f"/Users/{FAKE_USER}/Projects/")),
            var_event(SAMPLE_PATH, utf16_text(f"D:\\{FAKE_LICENSEE} kit\\kick.wav")),
        )
    )
    report = scrub_report(file)
    scrubbed = report.file

    assert_same_structure(file, scrubbed)
    assert_names_are_gone(frozenset({FAKE_USER}), serialize(scrubbed))
    assert scrubbed.events[2].payload == bytes(len(file.events[2].payload))
    assert scrubbed.events[3] is file.events[3]
    assert scrubbed.events[4].payload == b"author=xxxxxxx\x00by " + "xxxxxxx".encode("utf-16-le")
    assert decode_text(scrubbed.events[5]) == "/Users/xxxxxxx/Projects/"  # a user folder keeps its path
    assert scrubbed.events[6].payload == bytes(len(file.events[6].payload))
    assert report.changes == (
        ScrubChange(1, LICENSEE, (REASON_LICENSEE,)),
        ScrubChange(2, TITLE, (REASON_NAME_IN_TEXT,)),
        ScrubChange(4, PLUGIN_DATA, (REASON_NAME_IN_DATA,)),
        ScrubChange(5, DATA_PATH, (REASON_USER_FOLDER,)),
        ScrubChange(6, SAMPLE_PATH, (REASON_NAME_IN_TEXT,)),
    )


def test_scrub_reports_every_reason_an_event_changed() -> None:
    plugin_state = b"/Users/" + FAKE_USER.encode() + b"/x.fxp\x00owner=" + FAKE_USER.encode() + b"\x00"
    report = scrub_report(parse(licensee_file(var_event(PLUGIN_DATA, plugin_state))))

    assert report.changes[-1] == ScrubChange(2, PLUGIN_DATA, (REASON_USER_FOLDER, REASON_NAME_IN_DATA))
    assert report.file.events[2].payload == b"/Users/xxxxxxx/x.fxp\x00owner=xxxxxxx\x00"


def test_scrub_lists_the_projects_own_text_for_a_check_by_hand() -> None:
    file = parse(
        licensee_file(
            var_event(TITLE, utf16_text("Night drive")),
            var_event(ARTISTS, utf16_text("")),
            var_event(COMMENTS, utf16_text("Mixed at home")),
            var_event(CHANNEL_NAME, utf16_text("Kick")),
        )
    )
    report = scrub_report(file)

    # The title and comments are kept as they are; empty ones aren't listed.
    assert report.to_review == (ScrubReview(2, TITLE, ("title",)), ScrubReview(4, COMMENTS, ("comments",)))
    assert report.file.events[2:] == file.events[2:]


def test_a_licensee_before_fl_11_5_is_single_byte_text() -> None:
    body = (
        var_event(FL_VERSION, ascii_text("11.0.3"))
        + var_event(LICENSEE, (scrambled(FAKE_LICENSEE) + "\0").encode("cp1252"))
        + var_event(TITLE, f"{FAKE_USER} demo\0".encode("cp1252"))
    )
    file = parse(flp_data(body))

    assert licensee_names(file) == frozenset({FAKE_LICENSEE, FAKE_USER})
    assert scrub(file).events[2].payload == bytes(len(file.events[2].payload))


def test_scrubbing_twice_reports_nothing_the_second_time() -> None:
    scrubbed = scrub(parse(licensee_file(var_event(TITLE, utf16_text(f"Beat for {FAKE_USER}")))))

    assert scrub_report(scrubbed).changes == ()
    assert scrub(scrubbed) == scrubbed


# --- names too short to search -------------------------------------------------------------


def test_a_name_under_5_characters_is_not_searched_without_its_digits() -> None:
    # Searched alone, "Lee" would blank the track and insert names and change the playlist's bytes.
    playlist = (b"\x00\x01Lee\x00" + b"\x7f" * 8 + b"LEE").ljust(80, b"\x00")
    file = parse(
        licensee_file(
            var_event(TRACK_NAME, utf16_text("Lee bass")),
            var_event(INSERT_NAME, utf16_text("Sleeper")),
            var_event(PLAYLIST, playlist),
            var_event(PLUGIN_DATA, b"sleep=1\x00owner=" + SHORT_LICENSEE.encode() + b"\x00"),
            var_event(SAMPLE_PATH, utf16_text(f"D:\\{SHORT_LICENSEE.upper()}\\kick.wav")),
            licensee=SHORT_LICENSEE,
        )
    )
    report = scrub_report(file)

    assert licensee_names(file) == frozenset({SHORT_LICENSEE})
    assert report.file.events[2:5] == file.events[2:5]
    assert report.file.events[5].payload == b"sleep=1\x00owner=" + b"x" * len(SHORT_LICENSEE) + b"\x00"
    assert report.file.events[6].payload == bytes(len(file.events[6].payload))
    assert [change.index for change in report.changes] == [1, 5, 6]
    assert report.to_review == ()
    assert report.notes == (NOTE_NAME_NOT_SEARCHED,)


@pytest.mark.parametrize(
    ("licensee", "noted"),
    [
        pytest.param(FAKE_LICENSEE, False, id="long name"),
        pytest.param("Janet20240917", False, id="name of 5 characters"),
        pytest.param("20240917", False, id="digits only"),
        pytest.param("", False, id="empty"),
        pytest.param(SHORT_LICENSEE, True, id="name of 3 characters"),
        pytest.param("Anna20240917", True, id="name of 4 characters"),
        pytest.param("Lee", True, id="no digits, 3 characters"),
    ],
)
def test_the_report_notes_a_name_too_short_to_search(licensee: str, noted: bool) -> None:
    report = scrub_report(parse(licensee_file(var_event(TITLE, utf16_text("Lee and Anna")), licensee=licensee)))

    assert report.notes == ((NOTE_NAME_NOT_SEARCHED,) if noted else ())


def test_a_file_without_a_licensee_gets_no_note() -> None:
    assert scrub_report(parse(fl2025_file())).notes == ()


def test_the_note_never_holds_the_name() -> None:
    assert "Lee" not in NOTE_NAME_NOT_SEARCHED and "{" not in NOTE_NAME_NOT_SEARCHED
    assert "5 characters" in NOTE_NAME_NOT_SEARCHED


# --- structured data -----------------------------------------------------------------------


def test_structured_data_events_are_data_events_without_text() -> None:
    assert {NOTES, MIXER_PARAMS, PLAYLIST, TRACK_DATA} <= set(STRUCTURED_DATA_EVENTS)
    assert all(payload_size(event_id) is None for event_id in STRUCTURED_DATA_EVENTS)
    assert not set(STRUCTURED_DATA_EVENTS) & (TEXT_EVENT_IDS | {LICENSEE, PLUGIN_DATA})


def _spelling(name_bytes: bytes, size: int = 80) -> bytes:
    """Numeric data whose bytes happen to spell ``name_bytes``, ``size`` bytes long."""
    return (b"\x00\x01\x02\x03" + name_bytes).ljust(size, b"\x7f")


@pytest.mark.parametrize("event_id", [NOTES, MIXER_PARAMS, PLAYLIST, TRACK_DATA])
@pytest.mark.parametrize(
    "name_bytes",
    [
        pytest.param(FAKE_USER.upper().encode("latin-1"), id="single-byte"),
        pytest.param(FAKE_USER.encode("utf-16-le"), id="UTF-16LE"),
        pytest.param(FAKE_LICENSEE.lower().encode("latin-1"), id="with its digits"),
    ],
)
def test_scrub_never_changes_structured_data_and_lists_a_match(event_id: int, name_bytes: bytes) -> None:
    owner = b"owner=" + FAKE_USER.encode() + b"\x00"
    file = parse(licensee_file(var_event(event_id, _spelling(name_bytes)), var_event(PLUGIN_DATA, owner)))
    report = scrub_report(file)

    assert report.file.events[2] is file.events[2]
    assert report.to_review == (ScrubReview(2, event_id, (REASON_NAME_IN_STRUCTURED,)),)
    assert [change.index for change in report.changes] == [1, 3]  # the licensee and the plugin data only
    _assert_scrubbed(file, report)


def test_scrub_lists_a_user_folder_path_in_structured_data_without_changing_it() -> None:
    notes = _spelling(f"/Users/{FAKE_USER}/Music".encode(), 48)
    file = parse(licensee_file(var_event(NOTES, notes), var_event(TRACK_DATA, _spelling(b"C:\\Users\\Bob\\"))))
    report = scrub_report(file)

    assert report.file.events[2:] == file.events[2:]
    assert report.to_review == (
        ScrubReview(2, NOTES, (REASON_USER_FOLDER_IN_STRUCTURED, REASON_NAME_IN_STRUCTURED)),
        ScrubReview(3, TRACK_DATA, (REASON_USER_FOLDER_IN_STRUCTURED,)),
    )
    assert user_names(file) == frozenset()  # structured data holds numbers, not paths


def test_structured_data_without_a_match_is_not_listed() -> None:
    file = parse(
        licensee_file(var_event(NOTES, _spelling(b"Jane Doe")), var_event(PLAYLIST, _spelling(b"/Users/Shared/")))
    )

    assert scrub_report(file).to_review == ()
