from datetime import datetime
from typing import Any, Optional
from uuid import UUID

from pydantic import BaseModel, EmailStr

# ── User Schemas ──

class UserCreate(BaseModel):
    email: EmailStr
    username: str
    password: str

class UserResponse(BaseModel):
    id: UUID
    email: EmailStr
    username: str
    avatar_url: Optional[str] = None
    bio: Optional[str] = None
    location: Optional[str] = None
    website: Optional[str] = None
    genres: Optional[list[str]] = None
    created_at: datetime
    is_active: bool

    class Config:
        from_attributes = True

class UserUpdate(BaseModel):
    username: Optional[str] = None
    avatar_url: Optional[str] = None
    bio: Optional[str] = None
    location: Optional[str] = None
    website: Optional[str] = None
    genres: Optional[list[str]] = None

class Token(BaseModel):
    access_token: str
    token_type: str

# ── Project Schemas ──

class ProjectBase(BaseModel):
    name: str
    description: Optional[str] = None
    category: Optional[str] = "General"
    is_public: Optional[bool] = False

class ProjectCreate(ProjectBase):
    pass

class ProjectUpdate(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None
    category: Optional[str] = None
    is_public: Optional[bool] = None

class ProjectResponse(ProjectBase):
    id: UUID
    owner_id: UUID
    created_at: datetime
    is_deleted: bool
    deleted_at: Optional[datetime] = None

    class Config:
        from_attributes = True

# ── Branch Schemas ──

class BranchBase(BaseModel):
    name: str

class BranchCreate(BranchBase):
    pass

class BranchUpdate(BaseModel):
    name: Optional[str] = None

class BranchResponse(BranchBase):
    id: UUID
    project_id: UUID
    created_at: datetime
    is_deleted: bool
    deleted_at: Optional[datetime] = None

    class Config:
        from_attributes = True

# ── Version Schemas ──

class VersionBase(BaseModel):
    commit_message: Optional[str] = None
    parent_version_id: Optional[UUID] = None
    artifact_path: Optional[str] = None
    artifact_size_bytes: Optional[int] = None
    artifact_checksum: Optional[str] = None
    source_daw: Optional[str] = None
    source_project_filename: Optional[str] = None
    snapshot_manifest: Optional[dict[str, Any]] = None

class VersionCreate(VersionBase):
    pass

class VersionUpdate(BaseModel):
    commit_message: Optional[str] = None

class VersionResponse(VersionBase):
    id: UUID
    branch_id: UUID
    created_by: Optional[UUID] = None
    created_at: datetime
    is_deleted: bool
    deleted_at: Optional[datetime] = None

    class Config:
        from_attributes = True

# ── Track Schemas ──

class TrackBase(BaseModel):
    name: str
    file_type: str = ".json"
    storage_path: Optional[str] = None

class TrackCreate(TrackBase):
    pass

class TrackResponse(TrackBase):
    id: UUID
    version_id: UUID

    class Config:
        from_attributes = True

# ── Collaborator Schemas ──

class CollaboratorCreate(BaseModel):
    user_id: UUID
    role: Optional[str] = "Viewer"

class CollaboratorResponse(BaseModel):
    project_id: UUID
    user_id: UUID
    role: str
    created_at: datetime
    user: Optional[UserResponse] = None


    class Config:
        from_attributes = True

# ── Stats Schemas ──

class DailyActivity(BaseModel):
    date: str
    count: int

class ActivityStatsResponse(BaseModel):
    daily_activity: list[DailyActivity]
    total_commits: int
    total_contributors: int

class ContributorStats(BaseModel):
    user_id: UUID
    username: str
    initials: str
    commits: int

class TopContributorsResponse(BaseModel):
    contributors: list[ContributorStats]

# ── Project Summary Schemas ──

class OwnerSummary(BaseModel):
    id: UUID
    username: str

    class Config:
        from_attributes = True

class VersionWithAuthor(BaseModel):
    id: UUID
    commit_message: Optional[str] = None
    created_at: datetime
    branch_name: str
    author: Optional[OwnerSummary] = None

class ProjectDetail(BaseModel):
    id: UUID
    name: str
    description: Optional[str] = None
    category: str
    is_public: bool
    created_at: datetime
    owner: OwnerSummary

    class Config:
        from_attributes = True

class ProjectSummaryResponse(BaseModel):
    project: ProjectDetail
    branches: list[BranchResponse]
    recent_versions: list[VersionWithAuthor]
    tracks: list[TrackResponse]

# ── Auth Schemas ──

class LoginRequest(BaseModel):
    email: EmailStr
    password: str

