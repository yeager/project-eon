#!/usr/bin/env python3
"""Exercise every explicit Project Eon platform start against real media.

This is intentionally configured only for developer builds that provide
EON_REAL_DATA_DIR.  CI never receives commercial media, while preservation
workstations gain a repeatable guard against a platform silently falling back
to another release or failing before its SDL event loop starts.
"""

from __future__ import annotations

import os
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile


def media_snapshot(directory: Path) -> dict[Path, str]:
    """Return a content snapshot without trusting timestamps or filenames."""
    snapshot: dict[Path, str] = {}
    for path in sorted(directory.rglob("*")):
        relative = path.relative_to(directory)
        if path.is_dir():
            snapshot[relative] = "directory"
            continue
        if not path.is_file():
            snapshot[relative] = "other"
            continue
        digest = hashlib.sha256()
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
        snapshot[relative] = f"file:{digest.hexdigest()}"
    return snapshot


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: cli_launch_test.py <project-eon> <real-data-dir>")

    executable = Path(sys.argv[1])
    data_directory = Path(sys.argv[2])
    if not executable.is_file():
        raise SystemExit(f"Project Eon executable not found: {executable}")
    if not data_directory.is_dir():
        raise SystemExit(f"Real data directory not found: {data_directory}")

    before = media_snapshot(data_directory)
    environment = os.environ | {"SDL_VIDEODRIVER": "dummy"}
    platforms = (
        ("millennium", "dos"),
        ("millennium", "amiga"),
        ("millennium", "atari-st"),
        ("deuteros", "amiga"),
        ("deuteros", "atari-st"),
    )
    for game, platform in platforms:
        command = (
            str(executable), "--data", str(data_directory), "--game", game,
            "--platform", platform, "--presentation", "original",
        )
        try:
            completed = subprocess.run(
                command, env=environment, check=False, capture_output=True,
                text=True, timeout=2,
            )
        except subprocess.TimeoutExpired:
            continue
        raise SystemExit(
            f"{game}/{platform} exited before its SDL loop (status {completed.returncode}):\n"
            f"{completed.stderr}"
        )
    after = media_snapshot(data_directory)
    if after != before:
        raise SystemExit("Project Eon changed the supplied game-data directory")

    # The Unix default is a read-only lookup at ~/.projecteon.  It must not
    # bootstrap a missing user-data directory as a side effect of inspection.
    if os.name != "nt":
        with tempfile.TemporaryDirectory() as temporary_home:
            isolated_environment = environment | {"HOME": temporary_home}
            missing_default = Path(temporary_home) / ".projecteon"
            completed = subprocess.run(
                (str(executable), "--inspect"), env=isolated_environment,
                check=False, capture_output=True, text=True,
            )
            if completed.returncode != 2 or missing_default.exists():
                raise SystemExit(
                    "Project Eon did not fail cleanly for a missing default data directory:\n"
                    f"{completed.stderr}"
                )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
