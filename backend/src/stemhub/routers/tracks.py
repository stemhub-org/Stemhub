from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.future import select
from typing import List
from uuid import UUID

from stemhub.database import get_db
from stemhub.models import Track, Version, Branch, Project, User
from stemhub.schemas import TrackCreate, TrackResponse
from stemhub.auth import get_current_user

router = APIRouter(tags=["tracks"])


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

    db_track = Track(**track_in.model_dump(), version_id=version_id)
    db.add(db_track)
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
