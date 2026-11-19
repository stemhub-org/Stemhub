"""Authentication must not stop unrelated requests while passwords are checked."""
import asyncio
import threading
from types import SimpleNamespace

import httpx
from fastapi import FastAPI

from stemhub import auth
from stemhub.database import get_db
from stemhub.models import User


def test_health_request_can_run_during_password_verification(monkeypatch, no_login_throttle):
    entered = threading.Event()
    release = threading.Event()
    finished = threading.Event()

    def verify(*args):
        entered.set()
        release.wait(timeout=3)
        finished.set()
        return True

    monkeypatch.setattr(auth, "verify_password", verify)

    class Database:
        async def execute(self, _query):
            user = User(email="test@example.com", username="test", password_hash="stored",
                        is_active=True, is_deleted=False)
            return SimpleNamespace(scalars=lambda: SimpleNamespace(first=lambda: user))

        def add(self, _record):
            pass

    async def db():
        yield Database()

    async def start_session(*args, **kwargs):
        return "test-session"

    monkeypatch.setattr(auth, "_start_session", start_session)
    app = FastAPI()
    app.include_router(auth.router)
    app.dependency_overrides[get_db] = db

    @app.get("/health")
    async def health():
        pending = not finished.is_set()
        release.set()
        return {"password_work_pending": pending}

    async def scenario():
        async with httpx.AsyncClient(transport=httpx.ASGITransport(app=app), base_url="http://test") as client:
            login = asyncio.create_task(client.post("/auth/login", json={
                "email": "test@example.com", "password": "Valid!9-test-input",
            }))
            try:
                assert await asyncio.to_thread(entered.wait, 5)
                response = await client.get("/health")
                assert response.json()["password_work_pending"], "Password verification blocked an unrelated request"
            finally:
                release.set()
                result = await login
            assert result.status_code == 200, result.text

    asyncio.run(scenario())
