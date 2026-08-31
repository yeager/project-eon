"""Project-scoped scratch paths for tests that never need original media."""

from __future__ import annotations

import os
from pathlib import Path
import tempfile


def temporary_directory(prefix: str = "eon-test-") -> tempfile.TemporaryDirectory[str]:
    """Create a short-lived directory beneath Eon's cache, never ``/tmp``.

    CI can set ``EON_TEST_TMPDIR`` to an external scratch directory. Local
    tests otherwise use a user-owned cache outside both the checkout and game
    media. The caller still owns normal ``TemporaryDirectory`` cleanup.
    """
    configured = os.environ.get("EON_TEST_TMPDIR")
    root = Path(configured) if configured else Path.home() / ".cache" / "project-eon-tools" / "tests"
    root.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(prefix=prefix, dir=root)
