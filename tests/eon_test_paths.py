"""Project-scoped scratch paths for tests that never need original media."""

from __future__ import annotations

import os
from pathlib import Path
import tempfile
from unittest import mock


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


def write_lf_text(path: Path, value: str, encoding: str = "ascii") -> None:
    """Write canonical recorder fixture bytes without host newline translation."""
    with path.open("w", encoding=encoding, newline="") as stream:
        stream.write(value)


class CanonicalReceiptPath:
    """Small test adapter for byte-exact recorder receipt fixture files."""

    def __init__(self, path: Path):
        self.path = path

    def write_text(self, value: str, encoding: str = "ascii") -> int:
        write_lf_text(self.path, value, encoding)
        return len(value)

    def __fspath__(self) -> str:
        return str(self.path)

    def __getattr__(self, name: str):
        return getattr(self.path, name)


class LfTextFixtureWrites:
    """Keep byte-exact text fixtures stable across Windows and POSIX."""

    def setUp(self) -> None:
        super().setUp()
        patcher = mock.patch.object(Path, "write_text", new=write_lf_path_text)
        patcher.start()
        self.addCleanup(patcher.stop)


def write_lf_path_text(path: Path, value: str, encoding=None, errors=None, newline=None) -> int:
    with path.open("w", encoding=encoding, errors=errors, newline="") as stream:
        return stream.write(value)
