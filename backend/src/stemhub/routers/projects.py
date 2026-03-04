from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import func
from sqlalchemy.ext.asyncio import AsyncSession
from datetime import datetime, timezone
from sqlalchemy.future import select
from typing import List
from uuid import UUID

from stemhub.database import get_db
from stemhub.models import Branch, Project, User
from stemhub.schemas import ProjectCreate, ProjectUpdate, ProjectResponse
from stemhub.auth import get_current_user

router = APIRouter(prefix="/projects", tags=["projects"])


async def _project_name_exists(
    *,
    db: AsyncSession,
    owner_id: UUID,
    name: str,
    exclude_project_id: UUID | None = None,
) -> bool:
    query = select(Project.id).where(
        Project.owner_id == owner_id,
        func.lower(Project.name) == name.lower(),
        Project.is_deleted == False,
    )
    if exclude_project_id is not None:
        query = query.where(Project.id != exclude_project_id)

    result = await db.execute(query)
    return result.first() is not None


@router.post("/", response_model=ProjectResponse, status_code=status.HTTP_201_CREATED)
async def create_project(
    project_in: ProjectCreate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Create a new project.
    """
    project_name = project_in.name.strip()
    if project_name == "":
        raise HTTPException(status_code=400, detail="Project name cannot be empty")

    if await _project_name_exists(db=db, owner_id=current_user.id, name=project_name):
        raise HTTPException(status_code=409, detail="Project name already exists")

    payload = project_in.model_dump()
    payload["name"] = project_name
    db_project = Project(**payload, owner_id=current_user.id)
    db.add(db_project)
    await db.flush()

    db.add(Branch(project_id=db_project.id, name="main"))

    await db.commit()
    await db.refresh(db_project)
    return db_project

@router.get("/", response_model=List[ProjectResponse])
async def list_projects(
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    List all projects owned by the current user.
    """
    result = await db.execute(select(Project).where(Project.owner_id == current_user.id, Project.is_deleted == False))
    projects = result.scalars().all()
    return projects

@router.get("/{project_id}", response_model=ProjectResponse)
async def get_project(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific project by ID.
    """
    result = await db.execute(select(Project).where(Project.id == project_id, Project.owner_id == current_user.id, Project.is_deleted == False))
    project = result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")
    return project

@router.put("/{project_id}", response_model=ProjectResponse)
async def update_project(
    project_id: UUID,
    project_in: ProjectUpdate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Update a project by ID.
    """
    result = await db.execute(select(Project).where(Project.id == project_id, Project.owner_id == current_user.id, Project.is_deleted == False))
    project = result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")
    
    update_data = project_in.model_dump(exclude_unset=True)

    if "name" in update_data and update_data["name"] is not None:
        project_name = update_data["name"].strip()
        if project_name == "":
            raise HTTPException(status_code=400, detail="Project name cannot be empty")

        if await _project_name_exists(
            db=db,
            owner_id=current_user.id,
            name=project_name,
            exclude_project_id=project_id,
        ):
            raise HTTPException(status_code=409, detail="Project name already exists")

        update_data["name"] = project_name

    for field, value in update_data.items():
        setattr(project, field, value)
        
    await db.commit()
    await db.refresh(project)
    return project

@router.delete("/{project_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_project(
    project_id: UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db)
):
    """
    Delete a project by ID.
    """
    result = await db.execute(select(Project).where(Project.id == project_id, Project.owner_id == current_user.id, Project.is_deleted == False))
    project = result.scalars().first()
    if not project:
        raise HTTPException(status_code=404, detail="Project not found")
        
    project.is_deleted = True
    project.deleted_at = datetime.now(timezone.utc)
    await db.commit()
