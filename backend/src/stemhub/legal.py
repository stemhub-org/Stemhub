"""Versions of the legal documents users must accept (GDPR: consent is
recorded with what was accepted and when). Bump a version when its text
changes; the frontend shows the same values (frontend/lib/legal.ts)."""
from .models import LegalAcceptance

TERMS_VERSION = "2026-11-15"
PRIVACY_POLICY_VERSION = "2026-11-15"

CURRENT_VERSIONS = {"terms": TERMS_VERSION, "privacy": PRIVACY_POLICY_VERSION}


def current_acceptances() -> list[LegalAcceptance]:
    """One acceptance per document, at its current version, timestamped now."""
    return [LegalAcceptance(document=document, version=version) for document, version in CURRENT_VERSIONS.items()]
