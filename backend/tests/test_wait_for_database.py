import socket

import pytest

from stemhub.wait_for_database import database_endpoint, wait_for_database


@pytest.mark.parametrize("url, expected", [
    (None, ("db", 5432)),
    ("", ("db", 5432)),
    ("postgresql://user:password@db/song", ("db", 5432)),
    ("postgres://user:password@db:5544/song", ("db", 5544)),
    ("postgresql+asyncpg://user:password@db:5432/song", ("db", 5432)),
    ("postgresql://user:p%40ss%27word@[::1]:5544/song", ("::1", 5544)),
    ("postgresql://user:p%40ss@db/song?sslmode=require", ("db", 5432)),
])
def test_provider_connection_url_variants(url, expected):
    assert database_endpoint(url) == expected


@pytest.mark.parametrize("url", [
    "postgresql://user:secret@db:not-a-port/song",
    "postgresql://user:secret@db:70000/song",
    "postgresql://user:secret@db:0/song",
    "postgresql:///song",
    "sqlite:///song",
])
def test_invalid_connection_configuration_fails_without_leaking_credentials(url):
    with pytest.raises(ValueError) as error:
        database_endpoint(url)
    assert "secret" not in str(error.value)
    assert url not in str(error.value)


def test_real_tcp_listener_is_reachable():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        host, port = listener.getsockname()
        wait_for_database(f"postgresql://{host}:{port}/song", timeout_seconds=1)


def test_unavailable_listener_has_a_bounded_wait():
    # Reserve a port without listening: no unrelated process can occupy it.
    with socket.socket() as unavailable:
        unavailable.bind(("127.0.0.1", 0))
        host, port = unavailable.getsockname()
        with pytest.raises(TimeoutError, match="did not become reachable"):
            wait_for_database(f"postgres://{host}:{port}/song", timeout_seconds=0.03)
