from __future__ import annotations

import re
import shutil
import subprocess
from pathlib import Path

import pytest

from stemhub.parser_version import PYFLP_COMMIT


PYFLP_SUBMODULE_PATH = Path(__file__).resolve().parents[1] / "vendor" / "PyFLP_v2"


def _submodule_head() -> str:
    # Without its own .git entry, `git rev-parse` would walk up and answer
    # with the superproject's HEAD, so only a real submodule checkout counts.
    if not (PYFLP_SUBMODULE_PATH / ".git").exists():
        pytest.skip("backend/vendor/PyFLP_v2 is not a git checkout")
    git = shutil.which("git")
    if git is None:
        pytest.skip("git is not installed")
    result = subprocess.run(
        [git, "-C", str(PYFLP_SUBMODULE_PATH), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        # For example inside the backend image, where the submodule's .git
        # file points at a superproject directory that is not there.
        pytest.skip(f"cannot read the submodule HEAD: {result.stderr.strip()}")
    return result.stdout.strip()


def test_pyflp_commit_is_a_full_sha() -> None:
    assert re.fullmatch(r"[0-9a-f]{40}", PYFLP_COMMIT)


def test_pyflp_commit_matches_the_checked_out_submodule() -> None:
    assert PYFLP_COMMIT == _submodule_head(), (
        "backend/vendor/PyFLP_v2 moved: set PYFLP_COMMIT in "
        "backend/src/stemhub/parser_version.py to the new submodule commit"
    )
