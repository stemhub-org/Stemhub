import uuid
from datetime import datetime, timezone
from typing import Any

from sqlalchemy import BigInteger, Boolean, CheckConstraint, DateTime, ForeignKey, Index, Integer, String, Text, text
from sqlalchemy.dialects.postgresql import JSONB, UUID
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .database import Base


class User(Base):
    __tablename__ = "users"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    email: Mapped[str] = mapped_column(String, unique=True, index=True, nullable=False)
    username: Mapped[str] = mapped_column(String, unique=True, index=True, nullable=False)
    password_hash: Mapped[str] = mapped_column(String, nullable=False)
    avatar_url: Mapped[str | None] = mapped_column(String, nullable=True)
    bio: Mapped[str | None] = mapped_column(Text, nullable=True)
    location: Mapped[str | None] = mapped_column(String, nullable=True)
    website: Mapped[str | None] = mapped_column(String, nullable=True)
    genres: Mapped[list[str] | None] = mapped_column(JSONB, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    is_admin: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    is_deleted: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False, index=True)
    deleted_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    # ── Relationships ──
    projects: Mapped[list["Project"]] = relationship("Project", back_populates="owner")
    collaborations: Mapped[list["Collaborator"]] = relationship("Collaborator", back_populates="user")


class Project(Base):
    __tablename__ = "project"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    owner_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id"), nullable=False)
    name: Mapped[str] = mapped_column(String(255), nullable=False, index=True)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)
    category: Mapped[str] = mapped_column(String(100), default="General")
    tags: Mapped[list[str] | None] = mapped_column(JSONB, nullable=True)
    like_count: Mapped[int] = mapped_column(Integer, default=0)
    is_public: Mapped[bool] = mapped_column(Boolean, default=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    is_deleted: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False, index=True)
    deleted_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    # ── Relationships ──
    owner: Mapped["User"] = relationship("User", back_populates="projects")
    collaborators: Mapped[list["Collaborator"]] = relationship("Collaborator", back_populates="project")
    branches: Mapped[list["Branch"]] = relationship("Branch", back_populates="project")


class Collaborator(Base):
    __tablename__ = "collaborator"

    project_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("project.id"), primary_key=True)
    user_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id"), primary_key=True)
    role: Mapped[str] = mapped_column(String(50), nullable=False, default="Viewer")  # Admin, Editor, Viewer
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

    # ── Relationships ──
    project: Mapped["Project"] = relationship("Project", back_populates="collaborators")
    user: Mapped["User"] = relationship("User", back_populates="collaborations")


class Branch(Base):
    __tablename__ = "branch"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    project_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("project.id"), nullable=False)
    name: Mapped[str] = mapped_column(String(255), nullable=False)  # e.g. main, feature-fast-tempo
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    is_deleted: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False, index=True)
    deleted_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    # ── Relationships ──
    project: Mapped["Project"] = relationship("Project", back_populates="branches")
    versions: Mapped[list["Version"]] = relationship("Version", back_populates="branch")


class Version(Base):
    __tablename__ = "version"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    branch_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("branch.id"), nullable=False)
    created_by: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id"), nullable=True)
    parent_version_id: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("version.id"), nullable=True)  # Git-like history
    commit_message: Mapped[str | None] = mapped_column(String(500), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    is_deleted: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False, index=True)
    deleted_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    artifact_path: Mapped[str | None] = mapped_column(String, nullable=True)  # .als, .flp pointer
    artifact_size_bytes: Mapped[int | None] = mapped_column(BigInteger, nullable=True)
    artifact_checksum: Mapped[str | None] = mapped_column(String(128), nullable=True)  # SHA-256 for integrity verification
    source_daw: Mapped[str | None] = mapped_column(String(50), nullable=True)
    source_project_filename: Mapped[str | None] = mapped_column(String(255), nullable=True)
    # ── Version payload ──
    # snapshot_manifest is the legacy field: it stores the DAW mixer state that
    # was extracted from the (whole) artifact bundle uploaded via the old flow.
    # It will keep being written by the legacy POST /versions endpoint until
    # the plugin migrates to the manifest-based flow.
    snapshot_manifest: Mapped[dict[str, Any] | None] = mapped_column(JSONB, nullable=True)
    # manifest_json is the content-addressed manifest (see
    # docs/content-addressed-storage.md). Populated only by the new
    # manifest-based version-create endpoint. When set, artifact_path /
    # artifact_size_bytes / artifact_checksum are unused. manifest_version
    # is the schema version of this JSON blob so future readers can migrate.
    manifest_json: Mapped[dict[str, Any] | None] = mapped_column(JSONB, nullable=True)
    manifest_version: Mapped[int | None] = mapped_column(Integer, nullable=True)

    # ── Relationships ──
    branch: Mapped["Branch"] = relationship("Branch", back_populates="versions")
    author: Mapped["User | None"] = relationship("User", foreign_keys=[created_by])
    parent: Mapped["Version | None"] = relationship("Version", remote_side="Version.id", backref="children")
    tracks: Mapped[list["Track"]] = relationship("Track", back_populates="version")


class PullRequest(Base):
    """Proposal to merge one branch into another within the same project.

    Lifecycle: OPEN → MERGED (merge engine, issue #253) or OPEN → CLOSED
    (closed without merge). Both MERGED and CLOSED are terminal — a closed PR
    is not reopened, users open a new one (SPECIFICATION.md §7, §19).
    """
    __tablename__ = "pull_request"
    __table_args__ = (
        # Kept as a plain String + CHECK rather than a native Postgres ENUM so
        # adding a status later is a one-line migration, not an ALTER TYPE.
        CheckConstraint("status IN ('OPEN', 'MERGED', 'CLOSED')", name="ck_pull_request_status"),
        # Same invariant as the API-level 400: a branch cannot be merged into itself.
        CheckConstraint("source_branch_id <> target_branch_id", name="ck_pull_request_distinct_branches"),
        # At most one OPEN pull request per ordered (source, target) pair, as on
        # GitHub. Partial so closed/merged/soft-deleted PRs never block a new one.
        Index(
            "uq_pull_request_open_pair",
            "source_branch_id",
            "target_branch_id",
            unique=True,
            postgresql_where=text("status = 'OPEN' AND is_deleted = false"),
        ),
    )

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    # Redundant with source/target branch's project but stored so the per-project
    # listing needs no join and cross-project PRs are impossible by construction.
    project_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("project.id"), nullable=False, index=True)
    source_branch_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("branch.id"), nullable=False)
    target_branch_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("branch.id"), nullable=False)
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    description: Mapped[str | None] = mapped_column(Text, nullable=True)
    status: Mapped[str] = mapped_column(String(16), nullable=False, default="OPEN")  # OPEN, MERGED, CLOSED
    # Head (latest live version) of each branch when the PR was opened. Branch
    # has no head pointer, so this is the only record of what was proposed; the
    # merge engine (issue #253) compares target_head against the live head to
    # refuse a stale promotion. NULL when the branch had no version yet.
    source_head_version_id: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("version.id"), nullable=True)
    target_head_version_id: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("version.id"), nullable=True)
    created_by: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id"), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    closed_by: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id"), nullable=True)
    closed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    is_deleted: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False, index=True)
    deleted_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    # ── Relationships ──
    # Two FKs point at branch, so each relationship must name its own FK column
    # or SQLAlchemy raises AmbiguousForeignKeysError at mapper configuration.
    project: Mapped["Project"] = relationship("Project")
    source_branch: Mapped["Branch"] = relationship("Branch", foreign_keys=[source_branch_id])
    target_branch: Mapped["Branch"] = relationship("Branch", foreign_keys=[target_branch_id])
    # Same story for users: created_by and closed_by both point at users.id.
    author: Mapped["User | None"] = relationship("User", foreign_keys=[created_by])
    closer: Mapped["User | None"] = relationship("User", foreign_keys=[closed_by])
    source_head_version: Mapped["Version | None"] = relationship("Version", foreign_keys=[source_head_version_id])
    target_head_version: Mapped["Version | None"] = relationship("Version", foreign_keys=[target_head_version_id])


class Blob(Base):
    """Content-addressed blob. Project-scoped for privacy — see docs/content-addressed-storage.md."""
    __tablename__ = "blob"

    project_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("project.id", ondelete="CASCADE"), primary_key=True)
    sha256: Mapped[str] = mapped_column(String(64), primary_key=True)
    size_bytes: Mapped[int] = mapped_column(BigInteger, nullable=False)
    mime_type: Mapped[str | None] = mapped_column(String(80), nullable=True)
    storage_uri: Mapped[str] = mapped_column(Text, nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    ref_count: Mapped[int] = mapped_column(Integer, default=0, nullable=False)


class Track(Base):
    __tablename__ = "track"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    version_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("version.id"), nullable=False)
    name: Mapped[str] = mapped_column(String(255), nullable=False)  # e.g. Kick, Lead Synth
    file_type: Mapped[str] = mapped_column(String(50), default=".json")
    bpm: Mapped[int | None] = mapped_column(Integer, nullable=True)
    key: Mapped[str | None] = mapped_column(String(10), nullable=True)
    duration: Mapped[int | None] = mapped_column(Integer, nullable=True)  # in seconds
    storage_path: Mapped[str | None] = mapped_column(String, nullable=True)
    created_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), nullable=True)

    # ── Relationships ──
    version: Mapped["Version"] = relationship("Version", back_populates="tracks")


# ── Social & Feed Models ──

class UserFollow(Base):
    __tablename__ = "user_follow"

    follower_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE"), primary_key=True)
    followed_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE"), primary_key=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


class PlatformUpdate(Base):
    __tablename__ = "platform_update"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    version_string: Mapped[str] = mapped_column(String(50), nullable=False)
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    description: Mapped[str] = mapped_column(Text, nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


# ── Community Tab Models ──

class Challenge(Base):
    __tablename__ = "challenge"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    description: Mapped[str] = mapped_column(Text, nullable=False)
    level: Mapped[str] = mapped_column(String(50), nullable=False)  # e.g., Beginner, Intermediate, Advanced
    prize: Mapped[str] = mapped_column(String(255), nullable=False)
    ends_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


class ChallengeParticipant(Base):
    __tablename__ = "challenge_participant"

    user_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE"), primary_key=True)
    challenge_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("challenge.id", ondelete="CASCADE"), primary_key=True)
    joined_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


class Event(Base):
    __tablename__ = "event"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    type: Mapped[str] = mapped_column(String(50), nullable=False)  # Workshop, Stream
    title: Mapped[str] = mapped_column(String(255), nullable=False)
    host_name: Mapped[str] = mapped_column(String(255), nullable=False)
    event_date: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


class EventAttendee(Base):
    __tablename__ = "event_attendee"

    user_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE"), primary_key=True)
    event_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("event.id", ondelete="CASCADE"), primary_key=True)
    registered_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
