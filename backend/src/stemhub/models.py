import uuid
from datetime import datetime, timezone
from typing import Any

from sqlalchemy import BigInteger, Boolean, DateTime, ForeignKey, String, Text
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
<<<<<<< HEAD
=======
    bio: Mapped[str | None] = mapped_column(Text, nullable=True)
    location: Mapped[str | None] = mapped_column(String, nullable=True)
    website: Mapped[str | None] = mapped_column(String, nullable=True)
>>>>>>> origin/dev
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)

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
<<<<<<< HEAD
=======
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
>>>>>>> origin/dev

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
<<<<<<< HEAD
=======
    created_by: Mapped[uuid.UUID | None] = mapped_column(UUID(as_uuid=True), ForeignKey("users.id"), nullable=True)
>>>>>>> origin/dev
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
    snapshot_manifest: Mapped[dict[str, Any] | None] = mapped_column(JSONB, nullable=True)

    # ── Relationships ──
    branch: Mapped["Branch"] = relationship("Branch", back_populates="versions")
<<<<<<< HEAD
=======
    author: Mapped["User | None"] = relationship("User", foreign_keys=[created_by])
>>>>>>> origin/dev
    parent: Mapped["Version | None"] = relationship("Version", remote_side="Version.id", backref="children")
    tracks: Mapped[list["Track"]] = relationship("Track", back_populates="version")


class Track(Base):
    __tablename__ = "track"

    id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    version_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), ForeignKey("version.id"), nullable=False)
    name: Mapped[str] = mapped_column(String(255), nullable=False)  # e.g. Kick, Lead Synth
    file_type: Mapped[str] = mapped_column(String(50), default=".json")
    storage_path: Mapped[str | None] = mapped_column(String, nullable=True)

    # ── Relationships ──
    version: Mapped["Version"] = relationship("Version", back_populates="tracks")
