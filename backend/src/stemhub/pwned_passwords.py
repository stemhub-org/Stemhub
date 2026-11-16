"""Refuse passwords already exposed in data breaches (NIST SP 800-63B),
using the HaveIBeenPwned "Pwned Passwords" range API.

k-anonymity: only the first 5 hex characters of the password's SHA-1 leave
the server; the API returns every breached hash with that prefix and the
match is made here. Padding hides how many hashes the prefix has.
"""
import hashlib
import logging

import httpx

logger = logging.getLogger(__name__)

RANGE_URL = "https://api.pwnedpasswords.com/range/{prefix}"
TIMEOUT_SECONDS = 3.0

BREACHED_PASSWORD_MESSAGE = (
    "This password has appeared in a data breach, so attackers try it first. Please choose another one."
)


async def is_password_pwned(password: str) -> bool:
    """Whether `password` is in a known breach. When the API cannot be
    reached the password is let through: an outage must not block sign-up."""
    digest = hashlib.sha1(password.encode("utf-8")).hexdigest().upper()
    prefix, suffix = digest[:5], digest[5:]
    try:
        async with httpx.AsyncClient(timeout=TIMEOUT_SECONDS) as client:
            response = await client.get(
                RANGE_URL.format(prefix=prefix),
                headers={"Add-Padding": "true", "User-Agent": "StemHub"},
            )
            response.raise_for_status()
    except httpx.HTTPError as exc:
        logger.warning("Pwned Passwords check skipped: %s", exc.__class__.__name__)
        return False

    for line in response.text.splitlines():
        candidate, _, count = line.strip().partition(":")
        # Padding entries have a count of 0.
        if candidate == suffix and count.strip() not in ("", "0"):
            return True
    return False
