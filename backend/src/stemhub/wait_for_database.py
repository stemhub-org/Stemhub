"""Wait for the database's TCP listener before applying startup migrations."""
from __future__ import annotations

import os
import socket
import time
from urllib.parse import urlsplit


def database_endpoint(url: str | None) -> tuple[str, int]:
    if not url:
        return "db", 5432
    try:
        parsed = urlsplit(url)
        if parsed.scheme not in {"postgres", "postgresql", "postgresql+asyncpg"} or not parsed.hostname:
            raise ValueError
        port = parsed.port if parsed.port is not None else 5432
        if port == 0:
            raise ValueError
        return parsed.hostname, port
    except ValueError:
        # Never include a connection URL (which contains credentials) in logs.
        raise ValueError("DATABASE_URL must contain a valid PostgreSQL hostname and port") from None


def wait_for_database(url: str | None, *, timeout_seconds: float = 60) -> None:
    host, port = database_endpoint(url)
    deadline = time.monotonic() + timeout_seconds
    print(f"Connecting to database at {host}:{port}...", flush=True)
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"Database at {host}:{port} did not become reachable within {timeout_seconds}s")
        try:
            with socket.create_connection((host, port), timeout=min(3, remaining)):
                return
        except OSError:
            time.sleep(min(0.5, max(0, deadline - time.monotonic())))


if __name__ == "__main__":
    wait_for_database(os.getenv("DATABASE_URL"))
