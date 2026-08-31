#!/usr/bin/env python3
"""Prove that a missing default game-data directory remains read-only."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: cli_default_data_test.py <project-eon>")
    temporary_root = Path(os.environ.get("EON_TEST_TMPDIR", ""))
    if not temporary_root.is_dir():
        raise SystemExit("EON_TEST_TMPDIR must be an existing test-only directory")
    home = temporary_root / "missing-default-data-home"
    if home.exists():
        raise SystemExit("test home must not already exist")
    result = subprocess.run(
        (sys.argv[1], "--inspect"),
        env=os.environ | {"HOME": str(home), "XDG_CONFIG_HOME": str(temporary_root / "config")},
        capture_output=True,
        text=True,
        check=False,
    )
    expected = home / ".projecteon"
    if result.returncode != 2 or not f'Data path does not exist: "{expected}"' in result.stderr:
        raise SystemExit("missing default data path was not rejected as a read-only lookup")
    if home.exists() or expected.exists():
        raise SystemExit("missing default data lookup created a directory")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
