from pydantic import BaseModel, EmailStr
from uuid import UUID
from datetime import datetime
from typing import Optional

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
    created_at: datetime
    is_active: bool

    class Config:
        from_attributes = True

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
    storage_path: Optional[str] = None
    parent_version_id: Optional[UUID] = None

class VersionCreate(VersionBase):
    pass

class VersionUpdate(BaseModel):
    commit_message: Optional[str] = None
    storage_path: Optional[str] = None

class VersionResponse(VersionBase):
    id: UUID
    branch_id: UUID
    created_at: datetime
    is_deleted: bool
    deleted_at: Optional[datetime] = None

    class Config:
        from_attributes = True


# ── Auth Schemas ──

class LoginRequest(BaseModel):
    email: EmailStr
    password: str
