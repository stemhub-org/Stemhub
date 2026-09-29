"""Command line: regenerate the committed JSON Schema, or validate a document.

    python -m stemhub.project_model schema [--output PATH] [--check]
    python -m stemhub.project_model validate FILE

``schema`` writes the JSON Schema generated from the models (by default to the
committed file); with ``--check`` it only says whether that file is up to date.
``validate`` runs both validation passes on a document. Exit codes: 0 fine,
1 out of date or invalid, 2 the file cannot be read.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Optional, Sequence

from .schema import JSON_SCHEMA_PATH, SCHEMA_VERSION, render_json_schema
from .validation import validate_json

REGENERATE_HINT = "Regenerate it with: python -m stemhub.project_model schema"


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = _parser().parse_args(argv)
    if args.command == "schema":
        return _schema(args.output, check=args.check)
    return _validate(args.file)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m stemhub.project_model", description="StemHub project model tools."
    )
    commands = parser.add_subparsers(dest="command", required=True)

    schema = commands.add_parser("schema", help="Write the JSON Schema generated from the models.")
    schema.add_argument("--output", type=Path, default=JSON_SCHEMA_PATH, help="Default: the committed file.")
    schema.add_argument("--check", action="store_true", help="Only check that the file is up to date.")

    validate = commands.add_parser("validate", help="Validate a project model document.")
    validate.add_argument("file", type=Path)
    return parser


def _schema(output: Path, *, check: bool) -> int:
    expected = render_json_schema()
    if not check:
        output.write_text(expected, encoding="utf-8")
        print(f"Wrote {output}")
        return 0
    try:
        current = output.read_text(encoding="utf-8")
    except OSError:
        current = None
    if current != expected:
        print(f"{output} is missing or out of date. {REGENERATE_HINT}", file=sys.stderr)
        return 1
    print(f"{output} is up to date.")
    return 0


def _validate(file: Path) -> int:
    try:
        body = file.read_bytes()
    except OSError as error:
        print(f"cannot read {file}: {error.strerror or error}", file=sys.stderr)
        return 2
    result = validate_json(body)
    if result.valid:
        print(f"{file}: valid (schema {SCHEMA_VERSION})")
        return 0
    print(f"{file}: {len(result.issues)} problem(s)")
    for issue in result.issues:
        print(f"  {issue.path or '(document)'}  {issue.code}  {issue.message}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
