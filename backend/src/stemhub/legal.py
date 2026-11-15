"""Versions of the legal documents users must accept (GDPR: consent is
recorded with what was accepted and when). Bump a version when its text
changes; the frontend shows the same values (frontend/lib/legal.ts)."""
import uuid

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from .models import LegalAcceptance

TERMS_VERSION = "2026-11-15"
PRIVACY_POLICY_VERSION = "2026-11-15"

CURRENT_VERSIONS = {"terms": TERMS_VERSION, "privacy": PRIVACY_POLICY_VERSION}


def current_acceptances() -> list[LegalAcceptance]:
    """One acceptance per document, at its current version, timestamped now."""
    return [LegalAcceptance(document=document, version=version) for document, version in CURRENT_VERSIONS.items()]


async def _accepted(db: AsyncSession, user_id: uuid.UUID) -> set[tuple[str, str]]:
    rows = await db.execute(
        select(LegalAcceptance.document, LegalAcceptance.version).where(LegalAcceptance.user_id == user_id)
    )
    return {(document, version) for document, version in rows.all()}


async def has_accepted_current_versions(db: AsyncSession, user_id: uuid.UUID) -> bool:
    return set(CURRENT_VERSIONS.items()) <= await _accepted(db, user_id)


async def accept_current_versions(db: AsyncSession, user_id: uuid.UUID) -> None:
    """Record the current versions the user has not accepted yet; the caller commits."""
    accepted = await _accepted(db, user_id)
    for document, version in CURRENT_VERSIONS.items():
        if (document, version) not in accepted:
            db.add(LegalAcceptance(user_id=user_id, document=document, version=version))
