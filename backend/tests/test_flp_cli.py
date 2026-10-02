"""The flp command line: python -m stemhub.project_model flp dump|diff|scrub."""
from __future__ import annotations

import builtins
import errno
import os
from pathlib import Path

import pytest

import stemhub.project_model.__main__ as cli_module
from flp_helpers import (
    COMMENTS,
    FAKE_USER,
    FL_VERSION,
    GOOD_BODY,
    NEW_IN_25_2_3,
    NOTES,
    PLUGIN_DATA,
    TEMPO,
    TITLE,
    ascii_text,
    fixed_event,
    fl2025_file,
    flp_data,
    licensee_file,
    private_file,
    utf16_text,
    var_event,
)
from stemhub.project_model.__main__ import main as cli_main
from stemhub.project_model.flp_events import NOTE_NAME_NOT_SEARCHED, parse, scrub, serialize


def _write(tmp_path: Path, name: str, data: bytes) -> Path:
    path = tmp_path / name
    path.write_bytes(data)
    return path


def test_cli_dump_lists_every_event(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    path = _write(tmp_path, "song.flp", fl2025_file(120000))

    assert cli_main(["flp", "dump", str(path)]) == 0

    out = capsys.readouterr().out
    assert "FL 25.2.4.4960" in out and "4 events" in out
    assert "id 156" in out and "120000" in out
    assert "'FL Studio 25.2.4.4960.4960'" in out


def test_cli_dump_hides_private_text_unless_asked(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    path = _write(tmp_path, "private.flp", private_file())

    assert cli_main(["flp", "dump", str(path)]) == 0
    hidden = capsys.readouterr().out
    assert cli_main(["flp", "dump", "--show-private", str(path)]) == 0
    shown = capsys.readouterr().out

    assert FAKE_USER not in hidden and FAKE_USER.encode().hex() not in hidden.replace(" ", "")
    assert "Music/Projects" not in hidden and "private" in hidden
    assert f"/Users/{FAKE_USER}/Music/Projects/" in shown


def test_cli_dump_and_diff_warn_about_a_misread_stream(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    odd = flp_data(
        var_event(FL_VERSION, ascii_text("25.2.4.4960"))
        + fixed_event(NEW_IN_25_2_3, 0x000101, 3)
        + fixed_event(TEMPO, 1, 4)
    )
    path = _write(tmp_path, "odd.flp", odd)

    assert cli_main(["flp", "dump", str(path)]) == 0
    dumped = capsys.readouterr()
    assert cli_main(["flp", "diff", str(path), str(path)]) == 0
    diffed = capsys.readouterr()

    assert "warning" in dumped.err and "event 172" in dumped.err and "warning" not in dumped.out
    assert diffed.err.count("warning") == 2


def test_cli_diff_reports_changes_with_its_exit_code(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    a = _write(tmp_path, "a.flp", fl2025_file(140000))
    b = _write(tmp_path, "b.flp", fl2025_file(128000))

    assert cli_main(["flp", "diff", str(a), str(a)]) == 0
    assert "No differences" in capsys.readouterr().out
    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    out = capsys.readouterr().out
    assert "140000" in out and "128000" in out and "1 change" in out
    assert cli_main(["flp", "diff", str(a), str(b), "--ignore", "156,237"]) == 0


def test_cli_diff_reports_a_header_change(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    a = _write(tmp_path, "a.flp", flp_data(GOOD_BODY, ppq=96))
    b = _write(tmp_path, "b.flp", flp_data(GOOD_BODY, ppq=192))

    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    assert "ppq 96 -> 192" in capsys.readouterr().out


def test_cli_diff_hides_private_text(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    a = _write(tmp_path, "a.flp", private_file())
    b = _write(tmp_path, "b.flp", private_file().replace(b"M\x00u\x00s\x00i\x00c\x00", b"D\x00r\x00u\x00m\x00s\x00"))

    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    out = capsys.readouterr().out
    assert "id 202" in out and "Drums" not in out and FAKE_USER not in out
    assert "differs in private data only" in out


def test_cli_diff_shows_private_data_when_asked(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    a = _write(tmp_path, "a.flp", private_file())
    b = _write(tmp_path, "b.flp", private_file().replace(FAKE_USER.encode("utf-16-le"), "JohnRoe".encode("utf-16-le")))

    assert cli_main(["flp", "diff", "--show-private", str(a), str(b)]) == 1
    out = capsys.readouterr().out
    assert "JohnRoe" in out and FAKE_USER in out and "private data only" not in out


def test_cli_diff_says_when_only_the_licensee_differs(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    a = _write(tmp_path, "a.flp", licensee_file())
    b = _write(tmp_path, "b.flp", licensee_file(licensee="JohnRoe20240917"))

    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    out = capsys.readouterr().out
    assert "id 200" in out and "differs in private data only" in out
    assert FAKE_USER not in out and "JohnRoe" not in out


def _plugin_state(marker: int, *, user: str = FAKE_USER) -> bytes:
    """100 bytes of plugin data: a user-folder path, then filler with ``marker`` at byte 80."""
    head = f"/Users/{user}/x.fxp".encode() + b"\x00"
    body = bytearray(range(len(head), 100))
    body[80 - len(head)] = marker
    return head + bytes(body)


def test_cli_diff_shows_where_a_long_event_differs(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    a = _write(tmp_path, "a.flp", flp_data(GOOD_BODY + var_event(PLUGIN_DATA, _plugin_state(80))))
    b = _write(tmp_path, "b.flp", flp_data(GOOD_BODY + var_event(PLUGIN_DATA, _plugin_state(0xFF))))

    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    out = capsys.readouterr().out
    assert "first difference at byte 80" in out
    assert "4f 50 51" in out and "4f ff 51" in out  # bytes 79 to 81 of each
    assert "private data only" not in out  # the previews match, the events don't
    assert FAKE_USER not in out


def test_cli_diff_shows_where_a_long_text_differs(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    text = "a" * 130 + "{}" + "c" * 10
    a = _write(tmp_path, "a.flp", flp_data(GOOD_BODY + var_event(COMMENTS, utf16_text(text.format("B")))))
    b = _write(tmp_path, "b.flp", flp_data(GOOD_BODY + var_event(COMMENTS, utf16_text(text.format("D")))))

    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    out = capsys.readouterr().out
    assert "first difference at byte 260" in out and "42 00" in out and "44 00" in out
    assert "private data only" not in out


def test_cli_diff_short_events_need_no_window(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    a = _write(tmp_path, "a.flp", flp_data(GOOD_BODY + var_event(COMMENTS, utf16_text("take 1"))))
    b = _write(tmp_path, "b.flp", flp_data(GOOD_BODY + var_event(COMMENTS, utf16_text("take 2"))))

    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    out = capsys.readouterr().out
    assert "'take 1'" in out and "'take 2'" in out and "first difference" not in out


def test_cli_diff_says_when_a_long_event_differs_in_a_user_name_only(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    a = _write(tmp_path, "a.flp", flp_data(GOOD_BODY + var_event(PLUGIN_DATA, _plugin_state(80))))
    b = _write(tmp_path, "b.flp", flp_data(GOOD_BODY + var_event(PLUGIN_DATA, _plugin_state(80, user="JohnRoe"))))

    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    hidden = capsys.readouterr().out
    assert cli_main(["flp", "diff", "--show-private", str(a), str(b)]) == 1
    shown = capsys.readouterr().out

    assert "differs in private data only" in hidden and "first difference" not in hidden
    assert "first difference at byte 8" in shown and "private data only" not in shown


def test_cli_diff_shows_a_longer_length_prefix(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    a = _write(tmp_path, "a.flp", flp_data(GOOD_BODY + bytes([COMMENTS, 0x84, 0x00]) + b"a\x00b\x00"))
    b = _write(tmp_path, "b.flp", flp_data(GOOD_BODY + bytes([COMMENTS, 0x04]) + b"a\x00b\x00"))

    assert cli_main(["flp", "diff", str(a), str(b)]) == 1
    out = capsys.readouterr().out
    assert "length prefix 2 bytes" in out and "private data only" not in out


def test_cli_scrub_writes_a_scrubbed_copy(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    source = _write(tmp_path, "private.flp", private_file())
    output = tmp_path / "public.flp"

    assert cli_main(["flp", "scrub", str(source), "-o", str(output)]) == 0

    assert "Wrote" in capsys.readouterr().out
    assert source.read_bytes() == private_file()
    assert output.read_bytes() == serialize(scrub(parse(private_file())))


def test_cli_scrub_reports_an_output_it_cannot_write(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    source = _write(tmp_path, "private.flp", private_file())

    assert cli_main(["flp", "scrub", str(source), "-o", str(tmp_path / "missing" / "public.flp")]) == 2
    assert "cannot write" in capsys.readouterr().err


def test_cli_scrub_lists_what_it_changed_and_what_to_check(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    body = licensee_file(var_event(TITLE, utf16_text("Night drive")), var_event(COMMENTS, utf16_text(FAKE_USER)))
    source = _write(tmp_path, "private.flp", body)

    assert cli_main(["flp", "scrub", str(source), "-o", str(tmp_path / "public.flp")]) == 0

    out = capsys.readouterr().out
    assert "2 event(s) scrubbed" in out
    assert "#1 id 200: licensee blanked" in out and "#3 id 195: text holding the licensee's name blanked" in out
    assert "#2 id 194 (title)" in out and "Night drive" not in out and FAKE_USER not in out
    assert "Note:" not in out


def _notes_spelling_the_name() -> bytes:
    """A FL 25 file whose notes event (#2) holds bytes that spell the licensee's name."""
    return licensee_file(var_event(NOTES, (b"\x00" * 4 + FAKE_USER.encode()).ljust(24, b"\x00")))


def test_cli_scrub_lists_structured_data_it_kept(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    source = _write(tmp_path, "private.flp", _notes_spelling_the_name())
    output = tmp_path / "public.flp"

    assert cli_main(["flp", "scrub", str(source), "-o", str(output)]) == 0

    out = capsys.readouterr().out
    assert "1 event(s) scrubbed" in out
    assert "#2 id 224 (licensee's name matched in structured data, not replaced)" in out
    assert FAKE_USER not in out and FAKE_USER.encode().hex(" ") not in out
    assert parse(output.read_bytes()).events[2] == parse(source.read_bytes()).events[2]


def test_cli_scrub_says_when_the_name_was_too_short_to_search(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    body = licensee_file(var_event(TITLE, utf16_text("Lee demo")), licensee="Lee1234567")
    source = _write(tmp_path, "private.flp", body)

    assert cli_main(["flp", "scrub", str(source), "-o", str(tmp_path / "public.flp")]) == 0

    out = capsys.readouterr().out
    assert f"Note: {NOTE_NAME_NOT_SEARCHED}" in out
    assert "Lee" not in out


def test_cli_dump_and_diff_hide_structured_data_holding_the_name(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    path = _write(tmp_path, "notes.flp", _notes_spelling_the_name())
    other = _write(tmp_path, "other.flp", licensee_file(var_event(NOTES, bytes(24))))
    spelled = FAKE_USER.encode().hex(" ")

    assert cli_main(["flp", "dump", str(path)]) == 0
    hidden = capsys.readouterr().out
    assert cli_main(["flp", "diff", str(other), str(path)]) == 1
    diffed = capsys.readouterr().out
    assert cli_main(["flp", "dump", "--show-private", str(path)]) == 0
    shown = capsys.readouterr().out

    assert spelled not in hidden and "id 224 len 24  <private: --show-private shows it>" in hidden
    assert spelled not in diffed and "first difference" not in diffed
    assert spelled in shown


def _refused(source: Path, output: Path, capsys: pytest.CaptureFixture[str]) -> None:
    for force in ([], ["--force"]):
        assert cli_main(["flp", "scrub", str(source), "-o", str(output), *force]) == 2
        assert "input" in capsys.readouterr().err
    assert source.read_bytes() == private_file()


def test_cli_scrub_never_overwrites_its_input(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    source = _write(tmp_path, "private.flp", private_file())

    _refused(source, source, capsys)
    _refused(source, tmp_path / "." / "private.flp", capsys)


def test_cli_scrub_never_overwrites_its_input_through_a_link(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    source = _write(tmp_path, "private.flp", private_file())
    hard_link, symbolic_link = tmp_path / "hard.flp", tmp_path / "symbolic.flp"
    try:
        os.link(source, hard_link)
        os.symlink(source, symbolic_link)
    except (OSError, NotImplementedError) as error:
        pytest.skip(f"this file system has no links: {error}")

    _refused(source, hard_link, capsys)
    _refused(source, symbolic_link, capsys)


def test_cli_scrub_never_overwrites_its_input_under_another_case(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    source = _write(tmp_path, "private.flp", private_file())
    other_case = tmp_path / "PRIVATE.flp"
    if not other_case.exists():
        pytest.skip("this file system tells names apart by case")

    _refused(source, other_case, capsys)


def test_cli_scrub_replaces_an_existing_file_only_with_force(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    source = _write(tmp_path, "private.flp", private_file())
    output = _write(tmp_path, "public.flp", b"an older copy")
    os.chmod(output, 0o640)

    assert cli_main(["flp", "scrub", str(source), "-o", str(output)]) == 2
    assert "--force" in capsys.readouterr().err
    assert output.read_bytes() == b"an older copy"

    assert cli_main(["flp", "scrub", str(source), "-o", str(output), "--force"]) == 0
    assert output.read_bytes() == serialize(scrub(parse(private_file())))
    assert os.stat(output).st_mode & 0o777 == 0o640
    assert sorted(path.name for path in tmp_path.iterdir()) == ["private.flp", "public.flp"]


class _FullDisk:
    """A file opened for writing on a full disk: every write fails."""

    def __init__(self, stream) -> None:
        self._stream = stream

    def __enter__(self) -> "_FullDisk":
        return self

    def __exit__(self, *exc_info: object) -> None:
        self._stream.close()

    def write(self, data: bytes) -> int:
        raise OSError(errno.ENOSPC, "No space left on device")


def test_cli_scrub_removes_a_new_output_it_could_not_finish(
    tmp_path: Path, capsys: pytest.CaptureFixture[str], monkeypatch: pytest.MonkeyPatch
) -> None:
    source = _write(tmp_path, "private.flp", private_file())
    monkeypatch.setattr(cli_module, "open", lambda path, mode: _FullDisk(builtins.open(path, mode)), raising=False)

    assert cli_main(["flp", "scrub", str(source), "-o", str(tmp_path / "public.flp")]) == 2
    assert "No space left" in capsys.readouterr().err
    assert sorted(path.name for path in tmp_path.iterdir()) == ["private.flp"]


def test_cli_scrub_with_force_leaves_no_temporary_file_when_it_fails(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    source = _write(tmp_path, "private.flp", private_file())
    (tmp_path / "folder.flp").mkdir()

    assert cli_main(["flp", "scrub", str(source), "-o", str(tmp_path / "folder.flp"), "--force"]) == 2
    assert "cannot write" in capsys.readouterr().err
    assert sorted(path.name for path in tmp_path.iterdir()) == ["folder.flp", "private.flp"]


@pytest.mark.parametrize(
    "command",
    [
        ["flp", "dump", "{bad}"],
        ["flp", "dump", "{missing}"],
        ["flp", "diff", "{bad}", "{bad}"],
        ["flp", "scrub", "{missing}", "-o", "{out}"],
    ],
)
def test_cli_refuses_files_it_cannot_read(
    tmp_path: Path, capsys: pytest.CaptureFixture[str], command: list[str]
) -> None:
    bad = _write(tmp_path, "bad.flp", b"RIFF not an FL Studio project")
    paths = {"bad": str(bad), "missing": str(tmp_path / "missing.flp"), "out": str(tmp_path / "out.flp")}

    assert cli_main([part.format(**paths) for part in command]) == 2
    assert "RIFF" not in capsys.readouterr().err


def test_cli_ignore_takes_event_ids_only() -> None:
    with pytest.raises(SystemExit):
        cli_main(["flp", "diff", "a.flp", "b.flp", "--ignore", "tempo"])
    with pytest.raises(SystemExit):
        cli_main(["flp", "diff", "a.flp", "b.flp", "--ignore", "300"])
