"""The problems validation reports: where (a JSON pointer), what (a code) and why (a message).

Messages never quote the submitted values: a project model holds names and
sample paths, and errors end up in API responses and logs.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Union

PathPart = Union[str, int]


@dataclass(frozen=True)
class Issue:
    """One problem in a project model.

    ``path`` is an RFC 6901 JSON pointer to the value ("" is the whole
    document), ``code`` a stable machine-readable name, ``message`` a sentence
    for people.
    """

    path: str
    code: str
    message: str

    def as_dict(self) -> dict[str, str]:
        return {"path": self.path, "code": self.code, "message": self.message}


def pointer(*parts: PathPart) -> str:
    """The JSON pointer of a value, from its keys and list indexes."""
    return "".join("/" + _escape(str(part)) for part in parts)


def _escape(part: str) -> str:
    return part.replace("~", "~0").replace("/", "~1")
