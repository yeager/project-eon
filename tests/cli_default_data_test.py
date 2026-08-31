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
    executable = Path(sys.argv[1]).resolve()
    # Windows CI separately exercises the installed executable at the exact
    # `<install-dir>\\data` boundary in the Inno staging job.  A CMake build
    # target can share its output directory with generated build resources,
    # so this generic test cannot promise that its sibling `data` is absent.
    # Keep the read-only assertion on the final artifact rather than deleting
    # or perturbing a build output directory.
    if os.name == "nt":
        return 0
    home = temporary_root / "missing-default-data-home"
    if home.exists():
        raise SystemExit("test home must not already exist")
    # Windows deliberately uses a sibling data directory, not HOME. Keep this
    # test aligned with the installed artifact contract on every CI host.
    expected = executable.parent / "data" if os.name == "nt" else home / ".projecteon"
    if expected.exists():
        raise SystemExit("test default data path must not already exist")
    result = subprocess.run(
        (sys.argv[1], "--inspect"),
        env=os.environ | {"HOME": str(home), "XDG_CONFIG_HOME": str(temporary_root / "config")},
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 2 or not f'Data path does not exist: "{expected}"' in result.stderr:
        raise SystemExit("missing default data path was not rejected as a read-only lookup")
    if home.exists() or expected.exists():
        raise SystemExit("missing default data lookup created a directory")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
