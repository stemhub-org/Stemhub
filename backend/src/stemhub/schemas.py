from datetime import datetime
from typing import Any, Literal, Optional
from uuid import UUID

from pydantic import BaseModel, EmailStr, Field, field_validator

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
    is_admin: bool

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
    manifest_json: Optional[dict[str, Any]] = None
    manifest_version: Optional[int] = None

class VersionCreate(VersionBase):
    pass


class VersionResponse(VersionBase):
    id: UUID
    branch_id: UUID
    created_by: Optional[UUID] = None
    created_at: datetime
    is_deleted: bool
    deleted_at: Optional[datetime] = None

    class Config:
        from_attributes = True


# ── Content-Addressed Manifest (v1) ──
#
# See docs/content-addressed-storage.md. Every blob reference is a hex
# SHA-256 that must already exist in the project's blob table (uploaded
# via PUT /projects/{pid}/blobs/{sha256}) before a version can reference it.

_SHA256_HEX_RE = "^[0-9a-f]{64}$"


class ManifestBlobRef(BaseModel):
    sha256: str = Field(pattern=_SHA256_HEX_RE)
    size_bytes: int = Field(ge=0)
    filename: str = Field(min_length=1, max_length=255)


class ManifestTrack(ManifestBlobRef):
    name: str = Field(min_length=1, max_length=255)
    bpm: Optional[int] = Field(default=None, ge=1, le=1000)
    key: Optional[str] = Field(default=None, max_length=10)
    duration_seconds: Optional[int] = Field(default=None, ge=0)


class VersionManifestV1(BaseModel):
    manifest_version: Literal[1] = 1
    source_daw: Optional[str] = Field(default=None, max_length=50)
    source_project_filename: Optional[str] = Field(default=None, max_length=255)
    project_file: ManifestBlobRef
    tracks: list[ManifestTrack] = Field(default_factory=list, max_length=500)
    mixer_state: Optional[dict[str, Any]] = None

    def all_blob_shas(self) -> set[str]:
        shas = {self.project_file.sha256}
        shas.update(t.sha256 for t in self.tracks)
        return shas


class VersionFromManifestCreate(BaseModel):
    commit_message: Optional[str] = Field(default=None, max_length=500)
    parent_version_id: Optional[UUID] = None
    manifest: VersionManifestV1


# ── Collaborator Schemas ──

class CollaboratorCreate(BaseModel):
    username: str
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
    has_artifact: bool = False
    source_daw: Optional[str] = None
    source_project_filename: Optional[str] = None


class MixerDiffSummary(BaseModel):
    total_changes: int
    inserts_changed: int
    slots_changed: int
    parameter_changes: int


class MixerDiffChange(BaseModel):
    type: str
    insert_iid: int
    insert_name: Optional[str] = None
    slot_index: Optional[int] = None
    before: Any = None
    after: Any = None
    message: str


class MixerDiffResponse(BaseModel):
    summary: MixerDiffSummary
    changes: list[MixerDiffChange]


class VersionDiffHistoryEntry(BaseModel):
    version: VersionWithAuthor
    compared_to_version_id: UUID | None = None
    status: Literal["initial", "compared", "unsupported"]
    status_message: Optional[str] = None
    summary: Optional[MixerDiffSummary] = None
    changes: list[MixerDiffChange] = []


class TrackSummary(BaseModel):
    """A single stem/track surfaced for the repository overview UI.

    Sourced from whichever of the two track-data paths a version actually
    has: the content-addressed manifest (`Version.manifest_json["tracks"]`,
    populated by the from-manifest create flow) or the legacy `Track` table
    (populated by nothing today, kept for forward compatibility). A version
    with neither yields an empty list — callers should treat that as "no
    per-track data available for this version", not an error.
    """

    id: str
    name: str
    file_type: Optional[str] = None
    bpm: Optional[int] = None
    key: Optional[str] = None
    duration_seconds: Optional[int] = None
    size_bytes: Optional[int] = None
    source: Literal["manifest", "legacy"]


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
    latest_version_id: UUID | None = None
    has_preview: bool = False

# ── Auth Schemas ──

class LoginRequest(BaseModel):
    email: EmailStr
    password: str

class PasswordChangeRequest(BaseModel):
    current_password: str
    new_password: str

class EmailChangeRequest(BaseModel):
    new_email: EmailStr

# ── Explore & Community Schemas ──

class ExploreProjectResponse(BaseModel):
    id: UUID
    name: str
    description: Optional[str] = None
    category: str
    tags: Optional[list[str]] = None
    like_count: int = 0
    bpm: Optional[int] = None
    key: Optional[str] = None
    created_at: datetime
    owner: OwnerSummary

    class Config:
        from_attributes = True

class ExploreFeedResponse(BaseModel):
    id: UUID
    action_type: str
    project_id: UUID
    project_name: str
    producer: OwnerSummary
    created_at: datetime
    
class PlatformUpdateResponse(BaseModel):
    id: UUID
    version_string: str
    title: str
    description: str
    created_at: datetime

    class Config:
        from_attributes = True
    
class ProducerResponse(BaseModel):
    id: UUID
    username: str
    avatar_url: Optional[str] = None
    bio: Optional[str] = None
    follower_count: int = 0
    genres: Optional[list[str]] = None

    class Config:
        from_attributes = True

class ChallengeResponse(BaseModel):
    id: UUID
    title: str
    description: str
    level: str
    prize: str
    ends_at: datetime
    created_at: datetime

    class Config:
        from_attributes = True

class EventResponse(BaseModel):
    id: UUID
    type: str
    title: str
    host_name: str
    event_date: datetime
    created_at: datetime

    class Config:
        from_attributes = True

# ── Admin Schemas ──

class DailySignup(BaseModel):
    date: str
    count: int

class AdminStats(BaseModel):
    total_users: int
    total_projects: int
    active_users: int
    admin_users: int
    public_projects: int
    private_projects: int
    signups_last_30_days: list[DailySignup]

class UserWithProjects(UserResponse):
    project_count: int

class RecentUser(BaseModel):
    id: UUID
    username: str
    email: EmailStr
    avatar_url: Optional[str] = None
    created_at: datetime
    is_admin: bool

    class Config:
        from_attributes = True
