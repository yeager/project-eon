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
import re
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
    starts = [("start-menu", (str(executable), "--data", str(data_directory)))]
    for presentation in ("original", "modern"):
        for game, platform in (
            ("millennium", "dos"),
            ("millennium", "amiga"),
            ("millennium", "atari-st"),
            ("deuteros", "amiga"),
            ("deuteros", "atari-st"),
        ):
            starts.append((
                f"{game}/{platform}/{presentation}",
                (str(executable), "--data", str(data_directory), "--game", game,
                    "--platform", platform, "--presentation", presentation),
            ))
    for name, command in starts:
        try:
            completed = subprocess.run(
                command, env=environment, check=False, capture_output=True,
                text=True, timeout=2,
            )
        except subprocess.TimeoutExpired:
            continue
        raise SystemExit(
            f"{name} exited before its SDL loop (status {completed.returncode}):\n"
            f"{completed.stderr}"
        )

    # An individual original archive is a supported --data source too.  Ask
    # the program itself for the content-addressed identity first, then launch
    # that exact game/platform pair from the same read-only archive path.
    platform_names = {"DOS": "dos", "Amiga": "amiga", "Atari ST": "atari-st"}
    archive_starts: list[tuple[str, tuple[str, ...]]] = []
    for archive in sorted(data_directory.rglob("*.zip")):
        inspected = subprocess.run(
            (str(executable), "--data", str(archive), "--inspect"),
            env=environment, check=False, capture_output=True, text=True,
        )
        if inspected.returncode != 0:
            continue
        line = next((value for value in inspected.stdout.splitlines()
            if value.startswith("VERIFIED  ")), None)
        if not line:
            continue
        game = "millennium" if "Millennium 2.2" in line else "deuteros"
        platform = next((value for label, value in platform_names.items()
            if f" / {label} / " in line), None)
        if platform is None:
            raise SystemExit(f"Could not parse inspected platform for {archive}:\n{line}")
        expected_bootstrap = {
            ("millennium", "amiga"): (
                "bounded launcher bootstrap: resident entry 0x68000, raw resident SHA-256 "
                "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e"
            ),
            ("millennium", "atari-st"): (
                "bounded launcher bootstrap: target 0x77000, Fopen boundary MILL22A.inf"
            ),
            ("deuteros", "atari-st"): (
                "bounded launcher bootstrap: first/second raw stages SHA-256 "
                "dad3594c53375bd8285ef33e2d685bd38a5b38d930f2ea1305d117d63667f168/"
                "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7"
            ),
        }.get((game, platform))
        if expected_bootstrap and expected_bootstrap not in inspected.stdout:
            raise SystemExit(
                f"{game}/{platform} bounded launcher bootstrap did not match supplied media:\n"
                f"{inspected.stdout}"
            )
        if game == "deuteros" and platform == "atari-st":
            expected_state1 = (
                "Static state-1 raw-load plan: Disk 1 +0x55800 +0x5e400 "
                "-> RAM 0xb000 in 84 original reads; SHA-256 "
                "0d5ccb3a337fcbd4d34d34b3ad24f20c3bb2edca7e7b734b8abb14f6c0a30f47"
            )
            if expected_state1 not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST state-1 raw-load plan did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_state5 = (
                "Static vector-5 raw-load plans: Disk 1 +0x55800 +0xb400 "
                "-> RAM 0xb000 in 10 original reads; SHA-256 "
                "9659b21315e5c0528020be0b41eb75d57428f41b3b632fabfebe16d34038d298; "
                "copy RAM 0x57a00 +0x9393 -> 0xb006; Disk 1 +0x60c00 +0x4c800 "
                "-> RAM 0x16400 in 68 original reads; SHA-256 "
                "6b3e27702649ac201c4ecf92ad54f40656fd4d8633fadf5790014da34ce03ac6"
            )
            if expected_state5 not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST vector-5 raw-load plan did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        archive_starts.append((
            f"archive/{game}/{platform}/{archive.name}",
            (str(executable), "--data", str(archive), "--game", game,
                "--platform", platform, "--presentation", "original"),
        ))
    if len(archive_starts) < 5:
        raise SystemExit("Did not find every supported original archive as a direct --data input")
    for name, command in archive_starts:
        try:
            completed = subprocess.run(
                command, env=environment, check=False, capture_output=True,
                text=True, timeout=0.75,
            )
        except subprocess.TimeoutExpired:
            continue
        raise SystemExit(
            f"{name} exited before its SDL loop (status {completed.returncode}):\n"
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
