import uuid
from sqlalchemy import Column, String, Boolean, DateTime, ForeignKey, Text
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import relationship
from datetime import datetime, timezone
from .database import Base


class User(Base):
    __tablename__ = "users"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    email = Column(String, unique=True, index=True, nullable=False)
    username = Column(String, unique=True, index=True, nullable=False)
    password_hash = Column(String, nullable=False)
    avatar_url = Column(String, nullable=True)
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    is_active = Column(Boolean, default=True)

    # ── Relationships ──
    projects = relationship("Project", back_populates="owner")
    collaborations = relationship("Collaborator", back_populates="user")


class Project(Base):
    __tablename__ = "project"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    owner_id = Column(UUID(as_uuid=True), ForeignKey("users.id"), nullable=False)
    name = Column(String(255), nullable=False, index=True)
    description = Column(Text, nullable=True)
    category = Column(String(100), default="General")
    is_public = Column(Boolean, default=False)
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

    # ── Relationships ──
    owner = relationship("User", back_populates="projects")
    collaborators = relationship("Collaborator", back_populates="project")
    branches = relationship("Branch", back_populates="project")


class Collaborator(Base):
    __tablename__ = "collaborator"

    project_id = Column(UUID(as_uuid=True), ForeignKey("project.id"), primary_key=True)
    user_id = Column(UUID(as_uuid=True), ForeignKey("users.id"), primary_key=True)
    role = Column(String(50), nullable=False, default="Viewer")  # Admin, Editor, Viewer

    # ── Relationships ──
    project = relationship("Project", back_populates="collaborators")
    user = relationship("User", back_populates="collaborations")


class Branch(Base):
    __tablename__ = "branch"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    project_id = Column(UUID(as_uuid=True), ForeignKey("project.id"), nullable=False)
    name = Column(String(255), nullable=False)  # e.g. main, feature-fast-tempo
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

    # ── Relationships ──
    project = relationship("Project", back_populates="branches")
    versions = relationship("Version", back_populates="branch")


class Version(Base):
    __tablename__ = "version"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    branch_id = Column(UUID(as_uuid=True), ForeignKey("branch.id"), nullable=False)
    parent_version_id = Column(UUID(as_uuid=True), ForeignKey("version.id"), nullable=True)  # Git-like history
    commit_message = Column(String(500), nullable=True)
    storage_path = Column(String, nullable=True)  # .als, .flp pointer
    created_at = Column(DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

    # ── Relationships ──
    branch = relationship("Branch", back_populates="versions")
    parent = relationship("Version", remote_side="Version.id", backref="children")
    tracks = relationship("Track", back_populates="version")


class Track(Base):
    __tablename__ = "track"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4, index=True)
    version_id = Column(UUID(as_uuid=True), ForeignKey("version.id"), nullable=False)
    name = Column(String(255), nullable=False)  # e.g. Kick, Lead Synth
    file_type = Column(String(50), default=".json")
    storage_path = Column(String, nullable=True)

    # ── Relationships ──
    version = relationship("Version", back_populates="tracks")