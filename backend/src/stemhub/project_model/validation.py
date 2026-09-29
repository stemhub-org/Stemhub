"""Validate a project model document: parse, first pass (the models), second pass (references).

Both entry points return the problems found (the first pass reports a list's
first invalid item only, see ``schema``) as ``Issue``s with a JSON pointer, a
code and a message that never quote the submitted values: Pydantic's
``input`` and ``ctx`` are dropped, and the union tags Pydantic adds to error
locations (``"sampler"`` in ``instruments/0/sampler/name``) are removed so the
pointer names the value in the document. Nor do they quote the keys the
submitter chose: in the pointer, an unknown field is UNKNOWN_FIELD and an
extensions namespace is NAMESPACE.
"""
from __future__ import annotations

import json
import typing
from dataclasses import dataclass
from typing import Annotated, Any, Optional, Union

from pydantic import BaseModel, Discriminator, ValidationError

from .issues import Issue, PathPart, pointer
from .references import check_references
from .schema import ProjectModel

# The keys whose value is a union tag (see schema._tagged and schema._target_tag).
_TAG_KEYS = ("kind", "location", "status")

# What an issue's pointer shows in place of a key the submitter chose.
UNKNOWN_FIELD = "<unknown field>"
NAMESPACE = "<namespace>"
_EXTENSIONS_FIELD = "extensions"

# Pydantic messages whose template can quote part of the input: replaced, just in case.
_SAFE_MESSAGES = {
    "union_tag_invalid": "The object's type is not one of the expected ones.",
    "union_tag_not_found": "The object's type is missing.",
    "value_error": "The value is not valid.",
    "assertion_error": "The value is not valid.",
    "json_invalid": "The value is not valid JSON.",
}


@dataclass(frozen=True)
class ValidationResult:
    """The outcome of validating one document.

    ``model`` is set when the first pass accepted the document, even if the
    reference pass then found problems. ``truncated`` is true when more issues
    were found than ``max_issues`` allowed to keep.
    """

    model: Optional[ProjectModel]
    issues: tuple[Issue, ...]
    truncated: bool = False

    @property
    def valid(self) -> bool:
        return not self.issues


def validate_json(body: Union[bytes, str], *, max_issues: Optional[int] = None) -> ValidationResult:
    """Parse a JSON document (UTF-8) and validate it."""
    try:
        document = parse_json(body)
    except ValueError:
        return ValidationResult(model=None, issues=(_invalid_json(),))
    return validate_document(document, max_issues=max_issues)


def validate_document(document: Any, *, max_issues: Optional[int] = None) -> ValidationResult:
    """Validate an already parsed document: the models first, then the references."""
    try:
        model = ProjectModel.model_validate(document)
    except ValidationError as error:
        issues, truncated = _issues_from_validation_error(error, document, max_issues)
        return ValidationResult(model=None, issues=issues, truncated=truncated)
    reference_issues = check_references(model)
    kept = reference_issues if max_issues is None else reference_issues[:max_issues]
    return ValidationResult(model=model, issues=tuple(kept), truncated=len(kept) < len(reference_issues))


def parse_json(body: Union[bytes, str]) -> Any:
    """Parse strict-ish JSON: UTF-8 only and no key repeated in an object.

    Raises ValueError when the body is not such a document. NaN and Infinity
    parse (Python's json module accepts them) and are then refused by the
    models, which reports where they are.
    """
    text = body.decode("utf-8") if isinstance(body, bytes) else body
    try:
        return json.loads(text, object_pairs_hook=_object_without_repeated_keys)
    except RecursionError:
        raise ValueError("The document nests too deeply.") from None


def _object_without_repeated_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    # Readers disagree on which of two equal keys wins, so a document holding both is refused.
    obj = dict(pairs)
    if len(obj) != len(pairs):
        raise ValueError("An object repeats a key.")
    return obj


def _invalid_json() -> Issue:
    return Issue(path="", code="json_invalid", message="The body is not a JSON document in UTF-8 without repeated keys.")


def _issues_from_validation_error(
    error: ValidationError, document: Any, max_issues: Optional[int]
) -> tuple[tuple[Issue, ...], bool]:
    details = error.errors(include_url=False, include_context=False, include_input=False)
    kept = details if max_issues is None else details[:max_issues]
    issues = tuple(
        Issue(
            path=json_pointer(detail["loc"], document, unknown_field=detail["type"] == "extra_forbidden"),
            code=detail["type"],
            message=_SAFE_MESSAGES.get(detail["type"], detail["msg"]),
        )
        for detail in kept
    )
    return issues, len(kept) < len(details)


def json_pointer(loc: tuple[PathPart, ...], document: Any, *, unknown_field: bool = False) -> str:
    """The JSON pointer of a Pydantic error location, walking the submitted document.

    Pydantic inserts the chosen member's tag after the location of a tagged
    union (``instruments/0/sampler/name``); the walk drops it, once, where the
    document holds a union (an item of a list of unions, or the value of a
    union field) and the object there carries that tag. "[key]" (an invalid
    extensions namespace) is dropped too: the pointer then ends at the namespace.

    Keys the submitter chose are never quoted: an extensions namespace is
    NAMESPACE and, with ``unknown_field`` (the location of an unknown field),
    the last key is UNKNOWN_FIELD.
    """
    parts: list[PathPart] = []
    node: Any = document
    at_union = False
    for part in loc:
        if part == "[key]":
            continue
        if at_union and isinstance(node, dict) and _is_tag_of(node, part):
            at_union = False
            continue
        at_union = _enters_union(part, parts[-1] if parts else None)
        parts.append(part)
        node = _child(node, part)
    return pointer(*_without_chosen_keys(parts, unknown_field=unknown_field))


def _without_chosen_keys(parts: list[PathPart], *, unknown_field: bool) -> list[PathPart]:
    # Extensions are the only dicts of the models: the key after "extensions" is always a namespace.
    shown = [
        NAMESPACE if index > 0 and parts[index - 1] == _EXTENSIONS_FIELD and isinstance(part, str) else part
        for index, part in enumerate(parts)
    ]
    if unknown_field and shown:
        shown[-1] = UNKNOWN_FIELD
    return shown


def _enters_union(part: PathPart, previous: Optional[PathPart]) -> bool:
    # A union field ("insert") or an item of a list of unions ("instruments", 0).
    if isinstance(part, int):
        return previous in _UNION_FIELDS
    return part in _UNION_FIELDS


def _is_tag_of(node: dict[str, Any], part: PathPart) -> bool:
    return isinstance(part, str) and any(node.get(key) == part for key in _TAG_KEYS)


def _child(node: Any, part: PathPart) -> Any:
    if isinstance(node, dict) and isinstance(part, str):
        return node.get(part)
    if isinstance(node, list) and isinstance(part, int) and 0 <= part < len(node):
        return node[part]
    return None


def _union_field_names(root: type[BaseModel]) -> frozenset[str]:
    """The fields of the models, from ``root`` down, that hold a tagged union or a list of them."""
    names: set[str] = set()
    seen: set[type[BaseModel]] = set()
    pending = [root]
    while pending:
        model = pending.pop()
        if model in seen:
            continue
        seen.add(model)
        for name, field in model.model_fields.items():
            # A union annotated at the top of a field lands in field.metadata; nested, it stays in the annotation.
            if any(isinstance(meta, Discriminator) for meta in field.metadata) or _has_discriminator(field.annotation):
                names.add(name)
            pending.extend(_models_in(field.annotation))
    return frozenset(names)


def _has_discriminator(annotation: Any) -> bool:
    if typing.get_origin(annotation) is Annotated and any(
        isinstance(meta, Discriminator) for meta in annotation.__metadata__
    ):
        return True
    return any(_has_discriminator(arg) for arg in typing.get_args(annotation))


def _models_in(annotation: Any) -> list[type[BaseModel]]:
    if isinstance(annotation, type) and issubclass(annotation, BaseModel):
        return [annotation]
    return [model for arg in typing.get_args(annotation) for model in _models_in(arg)]


_UNION_FIELDS = _union_field_names(ProjectModel)
