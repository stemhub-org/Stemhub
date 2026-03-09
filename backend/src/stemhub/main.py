from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
import os
from contextlib import asynccontextmanager
from .auth import router as auth_router
from .routers.files import router as files_router
from .routers.projects import router as projects_router
from .routers.branches import router as branches_router
from .routers.versions import router as versions_router
from .routers.collaborators import router as collaborators_router
from .routers.stats import router as stats_router
from .database import engine
from .migrations import check_migrations_async

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Check that database schema is up to date before starting the application
    # This prevents starting the app with pending migrations, avoiding schema drift
    try:
        await check_migrations_async()
    except Exception as e:
        import logging
        logging.getLogger(__name__).error(f"Startup failed due to pending migrations: {e}")
        raise RuntimeError("Pending migrations found. Please run 'alembic upgrade head' before starting the application.") from e
        
    yield

app = FastAPI(title="StemHub API", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=[os.getenv("FRONTEND_URL", "http://localhost:3000")],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth_router)
app.include_router(projects_router)
app.include_router(branches_router)
app.include_router(versions_router)
app.include_router(files_router)
app.include_router(collaborators_router)
app.include_router(stats_router)

@app.get("/")
async def root():
    return {"message": "Welcome to the StemHub API"}
