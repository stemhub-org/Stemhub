from fastapi import APIRouter, Depends, HTTPException, status, UploadFile, File
from fastapi.responses import FileResponse
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from typing import List
from uuid import UUID
import os
import shutil

from stemhub.database import get_db
from stemhub.models import Track, Version, Branch, Project, User
from stemhub.schemas import TrackCreate, TrackResponse
from stemhub.auth import get_current_user
from stemhub.storage import get_storage_service, StorageNotFoundError

router = APIRouter(tags=["tracks"])

UPLOAD_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "..", "uploads", "tracks")
os.makedirs(UPLOAD_DIR, exist_ok=True)



async def _get_owned_version(
    *,
    version_id: UUID,
    current_user: User,
    db: AsyncSession,
) -> Version:
    result = await db.execute(
        select(Version).join(Branch).join(Project).where(
            Version.id == version_id,
            Project.owner_id == current_user.id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    version = result.scalars().first()
    if not version:
        raise HTTPException(status_code=404, detail="Version not found")
    return version


@router.post("/versions/{version_id}/tracks/", response_model=TrackResponse, status_code=status.HTTP_201_CREATED)
async def create_track(
    version_id: UUID,
    track_in: TrackCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Create a new track for a specific version.
    """
    await _get_owned_version(version_id=version_id, current_user=current_user, db=db)

    track_data = track_in.model_dump(exclude_none=True)
    db_track = Track(**track_data, version_id=version_id)
    db.add(db_track)
    await db.commit()
    await db.refresh(db_track)
    return db_track
@router.post("/versions/{version_id}/tracks/upload", response_model=TrackResponse, status_code=status.HTTP_201_CREATED)
async def upload_track(
    version_id: UUID,
    file: UploadFile = File(...),
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Upload an audio file to create a new track for a specific version.
    """
    # Fetch all relevant Project constraints to verify access and generate the storage path 
    result = await db.execute(
        select(Version, Branch, Project)
        .join(Branch, Version.branch_id == Branch.id)
        .join(Project, Branch.project_id == Project.id)
        .where(
            Version.id == version_id,
            Project.owner_id == current_user.id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    row = result.first()
    if not row:
        raise HTTPException(status_code=404, detail="Version not found")
    version, branch, project = row

    # Secure file name and validate type
    file_ext = os.path.splitext(file.filename)[1].lower() if file.filename else ".wav"
    allowed_extensions = {".wav", ".mp3", ".ogg", ".flac"}
    if file_ext not in allowed_extensions:
        raise HTTPException(status_code=400, detail="Unsupported audio format")

    # Validate size without reading entire file into memory (Max 50MB) 
    MAX_FILE_SIZE = 50 * 1024 * 1024
    file.file.seek(0, os.SEEK_END)
    if file.file.tell() > MAX_FILE_SIZE:
        raise HTTPException(status_code=413, detail="File too large")
    file.file.seek(0)

    db_track = Track(
        version_id=version_id,
        name=file.filename or "Untitled Track",
        file_type=file_ext
    )
    db.add(db_track)
    await db.commit()
    await db.refresh(db_track)

    try:
        # Stream file directly to GCS, avoiding any local disk writes
        storage_service = get_storage_service()
        stored_artifact = storage_service.store_version_artifact(
            project_id=project.id,
            branch_id=branch.id,
            version_id=version.id,
            filename=f"{db_track.id}{file_ext}",
            source=file.file,
        )
        db_track.storage_path = stored_artifact.path
    except Exception as e:
        # Rollback DB if file write fails or upload fails
        await db.delete(db_track)
        await db.commit()
        raise HTTPException(status_code=500, detail="Could not save file to storage")

    await db.commit()
    await db.refresh(db_track)
    return db_track

@router.get("/versions/{version_id}/tracks/", response_model=List[TrackResponse])
async def list_tracks(
    version_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    List all tracks for a specific version.
    """
    await _get_owned_version(version_id=version_id, current_user=current_user, db=db)

    result = await db.execute(select(Track).where(Track.version_id == version_id))
    return result.scalars().all()


@router.get("/tracks/{track_id}", response_model=TrackResponse)
async def get_track(
    track_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Get a specific track by ID.
    """
    result = await db.execute(
        select(Track).join(Version).join(Branch).join(Project).where(
            Track.id == track_id,
            Project.owner_id == current_user.id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    track = result.scalars().first()
    if not track:
        raise HTTPException(status_code=404, detail="Track not found")
    return track

@router.get("/tracks/{track_id}/audio")
async def get_track_audio(
    track_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Stream the actual audio file for a given track.
    """
    result = await db.execute(
        select(Track, Project)
        .join(Version, Track.version_id == Version.id)
        .join(Branch, Version.branch_id == Branch.id)
        .join(Project, Branch.project_id == Project.id)
        .where(
            Track.id == track_id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    row = result.first()
    if not row:
        raise HTTPException(status_code=404, detail="Track not found")
        
    track, project = row
    
    # Access check: owner or collaborator
    owner_id_str = str(project.owner_id).lower().strip()
    user_id_str = str(current_user.id).lower().strip()
    is_owner = owner_id_str == user_id_str
    
    if not is_owner:
        from stemhub.models import Collaborator
        collab_result = await db.execute(
            select(Collaborator).where(
                Collaborator.project_id == project.id,
                Collaborator.user_id == current_user.id,
            )
        )
        if not collab_result.scalars().first():
            detail = f"Access denied for user {user_id_str} on project {project.id}"
            raise HTTPException(status_code=403, detail=detail)

    if not track.storage_path:
        raise HTTPException(status_code=404, detail="Audio file empty storage")
        
    serve_path = track.storage_path
    
    if serve_path.startswith("/") and os.path.exists(serve_path):
        pass # Local file exists
    else:
        try:
            storage_service = get_storage_service()
            serve_path = storage_service.resolve_artifact_path(serve_path)
        except StorageNotFoundError:
            raise HTTPException(status_code=404, detail="Audio file not found in storage")

    # Serve the file (supports 206 Partial Content automatically for range requests if appropriate)
    return FileResponse(
        path=serve_path, 
        media_type="audio/wav" if track.file_type == ".wav" else "audio/mpeg",
        filename=track.name
    )


@router.delete("/tracks/{track_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_track(
    track_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """
    Delete a track.
    """
    result = await db.execute(
        select(Track).join(Version).join(Branch).join(Project).where(
            Track.id == track_id,
            Project.owner_id == current_user.id,
            Version.is_deleted == False,
            Branch.is_deleted == False,
            Project.is_deleted == False,
        )
    )
    track = result.scalars().first()
    if not track:
        raise HTTPException(status_code=404, detail="Track not found")

    await db.delete(track)
    await db.commit()
