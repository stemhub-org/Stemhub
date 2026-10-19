"""The project model: the DAW-neutral JSON of a project's musical content (docs/project-model.md).

- ``schema``: the Pydantic models of schema 0.1.0 (first validation pass) and
  the generator of the committed JSON Schema;
- ``references``: the second validation pass, on how objects refer to one another;
- ``validation``: both passes behind ``validate_json`` / ``validate_document``;
- ``units``: FL Studio raw values <-> model units, exactly invertible.

Regenerate the JSON Schema with ``python -m stemhub.project_model schema``.
"""
from .issues import Issue
from .references import check_references
from .schema import (
    JSON_SCHEMA_PATH,
    SCHEMA_ID,
    SCHEMA_NAME,
    SCHEMA_VERSION,
    ProjectModel,
    generate_json_schema,
    render_json_schema,
)
from .validation import ValidationResult, validate_document, validate_json

__all__ = [
    "JSON_SCHEMA_PATH",
    "SCHEMA_ID",
    "SCHEMA_NAME",
    "SCHEMA_VERSION",
    "Issue",
    "ProjectModel",
    "ValidationResult",
    "check_references",
    "generate_json_schema",
    "render_json_schema",
    "validate_document",
    "validate_json",
]
