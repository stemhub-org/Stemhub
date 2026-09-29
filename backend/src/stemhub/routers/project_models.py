"""Project model endpoints (docs/project-model.md).

POST /api/project-models/validate checks a project model document against
schema 0.1.0: the models first, then the references. It is the backend's input
boundary for project models (#170); nothing is stored.

The body is read here rather than by FastAPI so its size is capped before it is
parsed: by Content-Length and by the bytes actually received. Validating a body
at the cap takes a few hundred MiB, so at most MAX_CONCURRENT_VALIDATIONS run at
once per backend process; a request arriving while they all run gets 429 at
once rather than waiting. Errors carry a JSON pointer, a code and a message,
never the submitted values or keys.
"""
import asyncio
import logging
from typing import Literal

from fastapi import APIRouter, Depends, HTTPException, Request
from fastapi.concurrency import run_in_threadpool
from fastapi.responses import JSONResponse
from pydantic import BaseModel

from stemhub.auth import get_current_user
from stemhub.models import User
from stemhub.project_model import SCHEMA_ID, SCHEMA_VERSION, validate_json

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/api/project-models", tags=["project-models"])

MAX_PROJECT_MODEL_BYTES = 10 * 1024 * 1024
MAX_REPORTED_ERRORS = 100
MAX_CONCURRENT_VALIDATIONS = 2
TOO_MANY_VALIDATIONS = "Too many validations in progress, retry shortly"
RETRY_AFTER_SECONDS = 1

# Never waited on (a busy endpoint answers 429), so it is not bound to an event loop.
_validation_slots = asyncio.Semaphore(MAX_CONCURRENT_VALIDATIONS)


class ProjectModelError(BaseModel):
    path: str  # RFC 6901 JSON pointer; "" is the whole document
    code: str
    message: str


class ProjectModelValid(BaseModel):
    valid: Literal[True]
    schema_version: str


class ProjectModelInvalid(BaseModel):
    valid: Literal[False]
    errors: list[ProjectModelError]
    truncated: bool  # more errors were found than the MAX_REPORTED_ERRORS listed


async def read_capped_body(request: Request, *, limit: int) -> bytes:
    """The request body, or 413 as soon as it is known to exceed ``limit`` bytes."""
    declared = request.headers.get("content-length")
    if declared is not None:
        if not (declared.isascii() and declared.isdigit()):
            raise HTTPException(status_code=400, detail="Invalid Content-Length.")
        if int(declared) > limit:
            raise _too_large(limit)

    chunks: list[bytes] = []
    received = 0
    async for chunk in request.stream():
        received += len(chunk)
        if received > limit:
            raise _too_large(limit)
        chunks.append(chunk)
    return b"".join(chunks)


def _too_large(limit: int) -> HTTPException:
    return HTTPException(
        status_code=413,
        detail=f"The body is larger than the {limit}-byte limit.",
    )


@router.post(
    "/validate",
    response_model=ProjectModelValid,
    summary="Validate a project model document",
    responses={
        413: {"description": "The body is larger than 10 MiB."},
        422: {
            "model": ProjectModelInvalid,
            "description": "The body is not a valid project model (or not JSON).",
        },
        429: {"description": f"{MAX_CONCURRENT_VALIDATIONS} validations are already running: retry shortly."},
    },
    openapi_extra={
        "requestBody": {
            "required": True,
            "content": {
                "application/json": {
                    "schema": {"type": "object", "description": f"A project model document ({SCHEMA_ID})."}
                }
            },
        }
    },
)
async def validate_project_model(
    request: Request,
    current_user: User = Depends(get_current_user),
):
    del current_user  # being signed in is the only requirement
    body = await read_capped_body(request, limit=MAX_PROJECT_MODEL_BYTES)
    # No await between the check and the acquire: acquiring never waits.
    if _validation_slots.locked():
        logger.warning("Project model validation refused: %d already running", MAX_CONCURRENT_VALIDATIONS)
        raise HTTPException(
            status_code=429,
            detail=TOO_MANY_VALIDATIONS,
            headers={"Retry-After": str(RETRY_AFTER_SECONDS)},
        )
    async with _validation_slots:
        result = await run_in_threadpool(validate_json, body, max_issues=MAX_REPORTED_ERRORS)
    logger.info(
        "Project model validated: valid=%s errors=%d truncated=%s size_bytes=%d",
        result.valid,
        len(result.issues),
        result.truncated,
        len(body),
    )
    if result.valid:
        return ProjectModelValid(valid=True, schema_version=SCHEMA_VERSION)
    invalid = ProjectModelInvalid(
        valid=False,
        errors=[ProjectModelError(**issue.as_dict()) for issue in result.issues],
        truncated=result.truncated,
    )
    return JSONResponse(status_code=422, content=invalid.model_dump())
