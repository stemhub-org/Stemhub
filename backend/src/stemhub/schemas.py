from datetime import datetime
from typing import Annotated, Any, Literal, Optional, Union
from uuid import UUID

from pydantic import AliasChoices, BaseModel, Discriminator, EmailStr, Field, Tag, field_validator, model_validator

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
    tags: Optional[list[str]] = None
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

# ── Pull Request Schemas ──

PullRequestStatus = Literal["OPEN", "MERGED", "CLOSED"]

class PullRequestCreate(BaseModel):
    source_branch_id: UUID
    target_branch_id: UUID
    title: str = Field(min_length=1, max_length=255)
    description: Optional[str] = None

class PullRequestResponse(BaseModel):
    id: UUID
    project_id: UUID
    source_branch_id: UUID
    target_branch_id: UUID
    title: str
    description: Optional[str] = None
    status: PullRequestStatus
    source_head_version_id: Optional[UUID] = None
    target_head_version_id: Optional[UUID] = None
    created_by: Optional[UUID] = None
    created_at: datetime
    closed_by: Optional[UUID] = None
    closed_at: Optional[datetime] = None
    is_deleted: bool
    deleted_at: Optional[datetime] = None

    class Config:
        from_attributes = True

# ── Version Schemas ──

class VersionResponse(BaseModel):
    id: UUID
    branch_id: UUID
    created_by: Optional[UUID] = None
    message: Optional[str] = None
    parent_version_id: Optional[UUID] = None
    source_daw: Optional[str] = None
    source_project_filename: Optional[str] = None
    manifest_json: Optional[dict[str, Any]] = None
    manifest_version: Optional[int] = None
    created_at: datetime
    is_deleted: bool
    deleted_at: Optional[datetime] = None

    class Config:
        from_attributes = True


# ── Content-Addressed Manifest (v1 and v2) ──
#
# See docs/content-addressed-storage.md. Every file reference is a hex
# SHA-256 that must already exist in the project's blob table (uploaded
# via PUT /projects/{pid}/blobs/{sha256}) before a version can reference it.
# The plugin writes v2; v1 is still accepted from older plugins. Stored
# manifests are read back through stemhub.manifests, never through these models.

_SHA256_HEX_RE = "^[0-9a-f]{64}$"
MAX_MANIFEST_ASSETS = 500
MAX_MANIFEST_PATH_LENGTH = 255

# The DAWs a version can come from, spelled as stored and shown.
SUPPORTED_SOURCE_DAWS = ("FL Studio", "Ableton Live")
_SOURCE_DAW_BY_KEY = {name.casefold(): name for name in SUPPORTED_SOURCE_DAWS}


def normalize_source_daw(value: Optional[str]) -> Optional[str]:
    """Map a DAW name to its supported spelling, ignoring case and extra spaces.

    Blank means unknown (None). Applied on write only: stored rows are never rewritten.
    """
    if value is None:
        return None
    key = " ".join(value.split()).casefold()
    if not key:
        return None
    if key not in _SOURCE_DAW_BY_KEY:
        raise ValueError(f"source_daw must be one of: {', '.join(SUPPORTED_SOURCE_DAWS)}")
    return _SOURCE_DAW_BY_KEY[key]


class _VersionManifestBase(BaseModel):
    source_daw: Optional[str] = Field(default=None, max_length=50)
    source_project_filename: Optional[str] = Field(default=None, max_length=255)

    @field_validator("source_daw")
    @classmethod
    def _supported_source_daw(cls, value: Optional[str]) -> Optional[str]:
        return normalize_source_daw(value)


def _reject_other_asset_list(data: Any, *, manifest_version: int, asset_list: str, other_asset_list: str) -> Any:
    # Other unknown keys are dropped, but a dropped asset list would let its files
    # skip the existence check and ref counting, so it is refused instead.
    if isinstance(data, dict) and other_asset_list in data:
        raise ValueError(
            f"A v{manifest_version} manifest lists its assets under "
            f"'{asset_list}', not '{other_asset_list}'."
        )
    return data


class ManifestFileRefV1(BaseModel):
    """Legacy v1 file entry: the path is under ``filename``."""

    sha256: str = Field(pattern=_SHA256_HEX_RE)
    size_bytes: int = Field(ge=0)
    filename: str = Field(min_length=1, max_length=MAX_MANIFEST_PATH_LENGTH)


class ManifestAssetV1(ManifestFileRefV1):
    """Legacy v1 asset entry, listed under ``tracks``. bpm/key/duration were never filled."""

    name: str = Field(min_length=1, max_length=255)
    bpm: Optional[int] = Field(default=None, ge=1, le=1000)
    key: Optional[str] = Field(default=None, max_length=10)
    duration_seconds: Optional[int] = Field(default=None, ge=0)


class VersionManifestV1(_VersionManifestBase):
    """Legacy manifest, still accepted from plugins built before v2."""

    manifest_version: Literal[1] = 1
    project_file: ManifestFileRefV1
    tracks: list[ManifestAssetV1] = Field(default_factory=list, max_length=MAX_MANIFEST_ASSETS)

    @model_validator(mode="before")
    @classmethod
    def _no_v2_asset_list(cls, data: Any) -> Any:
        return _reject_other_asset_list(data, manifest_version=1, asset_list="tracks", other_asset_list="assets")


class ManifestFileRef(BaseModel):
    """A v2 file entry: the project file or one asset, by path in the project folder."""

    sha256: str = Field(pattern=_SHA256_HEX_RE)
    size_bytes: int = Field(ge=0)
    path: str = Field(min_length=1, max_length=MAX_MANIFEST_PATH_LENGTH)


class VersionManifestV2(_VersionManifestBase):
    manifest_version: Literal[2] = 2
    project_file: ManifestFileRef
    assets: list[ManifestFileRef] = Field(default_factory=list, max_length=MAX_MANIFEST_ASSETS)

    @model_validator(mode="before")
    @classmethod
    def _no_v1_asset_list(cls, data: Any) -> Any:
        return _reject_other_asset_list(data, manifest_version=2, asset_list="assets", other_asset_list="tracks")


def _manifest_version_tag(value: Any) -> Optional[str]:
    # manifest_version defaulted to 1 before v2, so older clients may omit it.
    if isinstance(value, dict):
        return str(value.get("manifest_version", 1))
    manifest_version = getattr(value, "manifest_version", None)
    return None if manifest_version is None else str(manifest_version)


VersionManifest = Annotated[
    Union[
        Annotated[VersionManifestV1, Tag("1")],
        Annotated[VersionManifestV2, Tag("2")],
    ],
    Discriminator(_manifest_version_tag),
]


# What plugins built before the rename sent when the user wrote no message.
# Migration 316caf0fcfa9 cleared the stored rows holding it.
LEGACY_PLUGIN_PLACEHOLDER_MESSAGE = "Save from plugin"


class VersionFromManifestCreate(BaseModel):
    # "commit_message" is still read so plugins built before the rename keep working.
    message: Optional[str] = Field(
        default=None,
        max_length=500,
        validation_alias=AliasChoices("message", "commit_message"),
    )
    parent_version_id: Optional[UUID] = None
    manifest: VersionManifest

    @field_validator("message")
    @classmethod
    def _placeholder_means_no_message(cls, value: Optional[str]) -> Optional[str]:
        # Exact match, like the migration: anything else is the user's own text.
        return None if value == LEGACY_PLUGIN_PLACEHOLDER_MESSAGE else value


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
    total_versions: int
    total_contributors: int

class ContributorStats(BaseModel):
    user_id: UUID
    username: str
    initials: str
    versions: int

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
    message: Optional[str] = None
    created_at: datetime
    branch_name: str
    author: Optional[OwnerSummary] = None
    source_daw: Optional[str] = None
    source_project_filename: Optional[str] = None


class MixerDiffSummary(BaseModel):
    total_changes: int
    inserts_changed: int
    slots_changed: int
    parameter_changes: int


class MixerDiffChange(BaseModel):
    type: str
    # FL Studio numbering (Master = 0); None for project-level changes (project_file_changed).
    insert_index: Optional[int] = None
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


class AssetSummary(BaseModel):
    """One audio or MIDI file of a version (UI: "Audio & MIDI files").

    Read from the version's manifest (spec §7) through stemhub.manifests, in
    either manifest version. A version with no manifest yields an empty list;
    callers should treat that as "no file list available", not an error.

    `id` is "{sha256}:{position in the stored asset list}", counting skipped
    malformed entries: two assets may hold the same bytes. `name` is the file
    name without its extension, taken from `path` (v1 entries keep their stored
    `name`), and `file_type` comes from `path`. Both are display-only per
    spec §7 (filenames are not authoritative); nothing downstream should trust
    them for MIME dispatch or storage decisions.
    """

    id: str
    path: str
    name: str
    file_type: Optional[str] = None
    size_bytes: Optional[int] = None


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
    tempo_bpm: Optional[float] = None
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
