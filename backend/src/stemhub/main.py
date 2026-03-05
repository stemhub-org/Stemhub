from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
import os
from contextlib import asynccontextmanager
from .auth import router as auth_router
from .routers.files import router as files_router
from .routers.projects import router as projects_router
from .routers.branches import router as branches_router
from .routers.versions import router as versions_router
<<<<<<< HEAD
=======
from .routers.tracks import router as tracks_router
from .routers.collaborators import router as collaborators_router
>>>>>>> origin/dev
from .database import engine, Base

@asynccontextmanager
async def lifespan(app: FastAPI):
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
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
<<<<<<< HEAD
=======
app.include_router(tracks_router)
app.include_router(collaborators_router)
>>>>>>> origin/dev

@app.get("/")
async def root():
    return {"message": "Welcome to the StemHub API"}
