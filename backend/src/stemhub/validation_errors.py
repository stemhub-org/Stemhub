"""422 responses that never echo what the client sent.

FastAPI's default 422 body repeats each invalid value under "input" (and
sometimes "ctx"), so a refused password came back in the response, where
proxies, browser tools and error trackers can record it. Only the location,
type and message of each error are kept.
"""
from typing import Any

from fastapi import FastAPI, Request
from fastapi.encoders import jsonable_encoder
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

# Pydantic prefixes the messages of our own validators (raised as ValueError).
VALUE_ERROR_PREFIX = "Value error, "


def _safe_error(error: dict[str, Any]) -> dict[str, Any]:
    message = error.get("msg", "")
    if message.startswith(VALUE_ERROR_PREFIX):
        message = message[len(VALUE_ERROR_PREFIX):]
    return {"type": error.get("type"), "loc": error.get("loc"), "msg": message}


async def validation_error_handler(request: Request, exc: RequestValidationError) -> JSONResponse:
    return JSONResponse(
        status_code=422,
        content={"detail": jsonable_encoder([_safe_error(error) for error in exc.errors()])},
    )


def install_validation_error_handler(app: FastAPI) -> None:
    app.add_exception_handler(RequestValidationError, validation_error_handler)
