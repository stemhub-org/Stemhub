import uuid
from datetime import datetime, timezone
from typing import Any

from sqlalchemy import BigInteger, Boolean, CheckConstraint, DateTime, ForeignKey, ForeignKeyConstraint, Index, Integer, String, Text, UniqueConstraint, func, text
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

    __table_args__ = (
        # Usernames are unique regardless of case, so "Hubert" cannot be
        # registered next to "hubert". Covers soft-deleted users too, so a
        # deleted account's name cannot be taken over.
        Index("uq_users_username_lower", text("lower(username)"), unique=True),
        # Emails are stored lowercase (schemas.normalize_email); this keeps a
        # row written another way from duplicating an account.
        Index("uq_users_email_lower", text("lower(email)"), unique=True),
    )

    # ── Relationships ──
    projects: Mapped[list["Project"]] = relationship("Project", back_populates="owner")
    collaborations: Mapped[list["Collaborator"]] = relationship("Collaborator", back_populates="user")
    identities: Mapped[list["UserIdentity"]] = relationship("UserIdentity", back_populates="user")
    sessions: Mapped[list["UserSession"]] = relationship("UserSession", back_populates="user")
    legal_acceptances: Mapped[list["LegalAcceptance"]] = relationship("LegalAcceptance", back_populates="user")


class UserIdentity(Base):
    """An external account (Google, ...) a user signs in with.

    Social sign-in looks the user up by the provider's stable account id, not
    by email: the email of an external account can change or be reused, its
    id cannot.
    """
    __tablename__ = "user_identity"
    __table_args__ = (
        UniqueConstraint("provider", "provider_user_id", name="uq_user_identity_provider_account"),
        # One account per provider and user, as on GitHub.
        UniqueConstraint("user_id", "provider", name="uq_user_identity_user_provider"),
    )

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    user_id: Mapped[uuid.UUID] = mapped_column(
        UUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE"), nullable=False, index=True
    )
    provider: Mapped[str] = mapped_column(String(32), nullable=False)
    provider_user_id: Mapped[str] = mapped_column(String(255), nullable=False)
    # The provider's email when the identity was linked, for support and auditing.
    email: Mapped[str | None] = mapped_column(String, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

    user: Mapped["User"] = relationship("User", back_populates="identities")


class UserSession(Base):
    """A signed-in browser or plugin. Its id travels in the access token
    ("sid"), and a token is only accepted while its session is neither
    revoked (logout) nor expired, so signing out really ends it."""
    __tablename__ = "user_session"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    user_id: Mapped[uuid.UUID] = mapped_column(
        UUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE"), nullable=False, index=True
    )
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    revoked_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    user: Mapped["User"] = relationship("User", back_populates="sessions")


class LegalAcceptance(Base):
    """A user accepting one version of a legal document (legal.py), kept as
    proof of consent: which document, which version, when."""
    __tablename__ = "legal_acceptance"
    __table_args__ = (
        CheckConstraint("document IN ('terms', 'privacy')", name="ck_legal_acceptance_document"),
        UniqueConstraint("user_id", "document", "version", name="uq_legal_acceptance_user_document_version"),
    )

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    user_id: Mapped[uuid.UUID] = mapped_column(
        UUID(as_uuid=True), ForeignKey("users.id", ondelete="CASCADE"), nullable=False, index=True
    )
    document: Mapped[str] = mapped_column(String(32), nullable=False)
    version: Mapped[str] = mapped_column(String(32), nullable=False)
    accepted_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), nullable=False, server_default=func.now()
    )

    user: Mapped["User"] = relationship("User", back_populates="legal_acceptances")


class LoginAttempt(Base):
    """One email + password sign-in attempt, kept to throttle brute force
    (see login_throttle.py). The email is stored whether or not it has an
    account, so throttling never reveals which emails do."""
    __tablename__ = "login_attempt"
    __table_args__ = (
        Index("ix_login_attempt_email_created_at", "email", "created_at"),
        Index("ix_login_attempt_ip_created_at", "ip", "created_at"),
    )

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    email: Mapped[str] = mapped_column(String, nullable=False)
    ip: Mapped[str | None] = mapped_column(String(45), nullable=True)
    succeeded: Mapped[bool] = mapped_column(Boolean, nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), nullable=False, server_default=func.now()
    )


class RateLimitHit(Base):
    """One use of a rate-limited action (`action`) by one client (`key`,
    e.g. an IP). See rate_limit.py."""
    __tablename__ = "rate_limit_hit"
    __table_args__ = (Index("ix_rate_limit_hit_action_key_created_at", "action", "key", "created_at"),)

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    action: Mapped[str] = mapped_column(String(64), nullable=False)
    key: Mapped[str] = mapped_column(String(255), nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), nullable=False, server_default=func.now()
    )


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
    # ON DELETE SET NULL: erasing a user's account must not be blocked by, nor delete, what they did here.
    created_by: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id", ondelete="SET NULL"), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    is_deleted: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False, index=True)
    deleted_by: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id", ondelete="SET NULL"), nullable=True)
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
    message: Mapped[str | None] = mapped_column(String(500), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    is_deleted: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False, index=True)
    deleted_by: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id", ondelete="SET NULL"), nullable=True)
    deleted_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    source_daw: Mapped[str | None] = mapped_column(String(50), nullable=True)
    source_project_filename: Mapped[str | None] = mapped_column(String(255), nullable=True)
    # Content-addressed manifest — see docs/content-addressed-storage.md.
    # manifest_version is the JSON schema version so future readers can migrate.
    manifest_json: Mapped[dict[str, Any] | None] = mapped_column(JSONB, nullable=True)
    manifest_version: Mapped[int | None] = mapped_column(Integer, nullable=True)

    # ── Relationships ──
    branch: Mapped["Branch"] = relationship("Branch", back_populates="versions")
    author: Mapped["User | None"] = relationship("User", foreign_keys=[created_by])
    parent: Mapped["Version | None"] = relationship("Version", remote_side="Version.id", backref="children")


class PullRequest(Base):
    """Proposal to bring one branch into another within the same project.

    Lifecycle: OPEN → MERGED (accepted, which promotes the source branch with
    no content merge, issue #253) or OPEN → CLOSED (closed without accepting).
    Both MERGED and CLOSED are terminal — a closed PR is not reopened, users
    open a new one (SPECIFICATION.md §19).
    """
    __tablename__ = "pull_request"
    __table_args__ = (
        # Kept as a plain String + CHECK rather than a native Postgres ENUM so
        # adding a status later is a one-line migration, not an ALTER TYPE.
        CheckConstraint("status IN ('OPEN', 'MERGED', 'CLOSED')", name="ck_pull_request_status"),
        # Same invariant as the API-level 400: a branch cannot be proposed into itself.
        CheckConstraint("source_branch_id <> target_branch_id", name="ck_pull_request_distinct_branches"),
        # At most one OPEN pull request per ordered (source, target) pair, as on
        # GitHub. Partial so CLOSED/MERGED/soft-deleted PRs never block a new one.
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
    # has no head pointer, so this is the only record of what was proposed;
    # accepting (issue #253) compares target_head against the live head to
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


# ── Social & Feed Models ──

class CachedProjectModel(Base):
    """Derived data owned by a blob; never keeps unreferenced bytes alive."""
    __tablename__ = "project_model"
    __table_args__ = (
        ForeignKeyConstraint(["project_id", "project_file_sha256"], ["blob.project_id", "blob.sha256"], ondelete="CASCADE"),
    )
    project_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True)
    project_file_sha256: Mapped[str] = mapped_column(String(64), primary_key=True)
    extractor_version: Mapped[str] = mapped_column(String(96), primary_key=True)
    model_json: Mapped[dict[str, Any] | None] = mapped_column(JSONB, nullable=True)
    error: Mapped[str | None] = mapped_column(String(255), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))


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
