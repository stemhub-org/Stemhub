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

    class Config:
        from_attributes = True
