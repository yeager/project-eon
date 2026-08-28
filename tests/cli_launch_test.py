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
    # Keep the explicit spelling usable for scripts and package integrations;
    # it must select the same original data directory without creating or
    # transforming anything.
    data_dir_inspection = subprocess.run(
        (str(executable), "--data-dir", str(data_directory), "--inspect"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if data_dir_inspection.returncode != 0 or "VERIFIED  " not in data_dir_inspection.stdout:
        raise SystemExit(
            "--data-dir did not inspect the supplied original media:\n"
            f"{data_dir_inspection.stderr}"
        )
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
    detected_releases: set[tuple[str, str, str]] = set()
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
        language = line.rsplit(" / ", 1)[-1]
        detected_releases.add((game, platform, language))
        expected_bootstrap = {
            ("millennium", "amiga"): (
                "bounded launcher bootstrap: resident entry 0x68000, raw resident SHA-256 "
                "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e"
            ),
            ("millennium", "atari-st"): (
                "bounded launcher bootstrap: target 0x77000, Fopen boundary MILL22A.inf"
            ),
            ("deuteros", "amiga"): (
                "Channel-request static continuation: ADF 0x7092, entry 0x21892; BSR 0x2189a -> 0x2229c, "
                "0x2189e -> 0x224a2; bit 6 loop 0x218c6 -> 0x218be; final 0x218c8 -> 0x217f6; SHA-256 "
                "120fba90e0b4fa9e96d8a6cf95fbac512d67d7daa42c3776ce0d3066b3f02ee9"
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
        if game == "millennium" and platform == "amiga":
            expected_post_negative_d3 = (
                "post-negative-D3 terminal: entry 0x685fe; byte stores 0x7b3b5/0x7b3bc; "
                "BNE 0x68612 -> 0x68616, zero RTS 0x68614; BPL 0x68616 -> boundary 0x6861a, "
                "negative RTS 0x68618; SHA-256 "
                "a45ff5eca6e3594574b464574fa0aae3027bd2ea11472770708c96f4d21b56cc"
            )
            if expected_post_negative_d3 not in inspected.stdout:
                raise SystemExit(
                    "Millennium Amiga post-negative-D3 terminal did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        if game == "millennium" and platform == "dos" and language == "English":
            expected_gx_overlay = (
                "2200GX.EXE overlay evidence: name 0x11c2, loader 0x11ce reads segment cell 0x118; "
                "calls 0x11d1 -> 0x53a/0x11e4 -> 0x574/0x11ec -> 0x596; caller 0xd335 -> 0x11ce; "
                "adapter 0x6c52 RETF 0x6c68 to overlay offset 0x0; SHA-256 "
                "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb"
            )
            if expected_gx_overlay not in inspected.stdout:
                raise SystemExit(
                    "Millennium DOS GX overlay evidence did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_gx_dispatcher = (
                "2200GX.EXE dispatcher: entry +0x0, table +0x15; selector 0xe/0xf/0x12/0x14 "
                "-> 0x90/0x9f/0x97/0xa7; near RET then far RETF +0x14; SHA-256 "
                "f4d657fcbdda23d7f0fdf2bbf48405d0a04e8b8149df064607f49042525fbd55"
            )
            if expected_gx_dispatcher not in inspected.stdout:
                raise SystemExit(
                    "Millennium DOS GX dispatcher evidence did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        if game == "deuteros" and platform == "amiga":
            expected_callee = (
                "Channel-request first callee: ADF 0x7a9c, entry 0x2229c; bit 5 branch 0x222b4 -> 0x2232c; "
                "DBRA 0x222e0 -> 0x222be; vectors 0x222fc/0x22312; final services 0x2231e/0x22324 -> 0x21698; "
                "SHA-256 d1a162af50f92b60d03b1da4ab186a547e46d145b0599cfbbeff7fb5af324ac1"
            )
            if expected_callee not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga channel-request first callee did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_second_callee = (
                "Channel-request second callee: ADF 0x7ca2, entry 0x224a2; longword 0x224e6 -> 0x6c; "
                "clears 0xdff0a8/0xdff0b8/0xdff0c8/0xdff0d8; 0xf -> 0xdff096; RTS 0x224ca; SHA-256 "
                "d4e9a1ee0065537a627cdd9ee8827f11d5fa28e0f860aacb21bbdc7e11784bd1"
            )
            if expected_second_callee not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga channel-request second callee did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_following_service = (
                "Channel-request following service: ADF 0x825a, entry 0x22a5a; execution 0x22ab8, "
                "embedded table 0x22a6a, descriptors 0x22a6e stride 0xe; RTS 0x22b88; SHA-256 "
                "d5fdbdacd004d2cf377ea0dbaefb9d8b308ba23b568cfb3785456622bde49d19"
            )
            if expected_following_service not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga channel-request following service did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_adjacent_entry = (
                "Channel-request adjacent entry: ADF 0x838a, entry 0x22b8a; test 0x22a30, "
                "zero 0x22b90 -> 0x22b94, early RTS 0x22b92; descriptors 0x22a6e stride 0xe; "
                "final RTS 0x22be8; SHA-256 "
                "10ed8be15c107dbb56ca98eb8d17ffd2bce3910dd169d67ba058447c9031b1ff"
            )
            if expected_adjacent_entry not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga channel-request adjacent entry did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        if game == "millennium" and platform == "atari-st":
            expected_auxiliary_resource = "auxiliary resource-name evidence: MILL22B.INF cluster "
            if expected_auxiliary_resource not in inspected.stdout:
                raise SystemExit(
                    "Millennium Atari ST auxiliary resource-name evidence did not match supplied media:\n"
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
            expected_skipped_ascii = (
                "State-1 skipped ASCII evidence: Disk 1 +0x9d800 BRA.W displacement 0x9c2; "
                "block +0x9d80a +0x438 (18 printable runs), SHA-256 "
                "8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e"
            )
            if expected_skipped_ascii not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST skipped ASCII evidence did not match supplied media:\n"
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
    expected_releases = {
        ("millennium", "dos", "en"),
        ("millennium", "dos", "es"),
        ("millennium", "amiga", "en"),
        ("millennium", "atari-st", "en"),
        ("deuteros", "amiga", "en"),
        ("deuteros", "atari-st", "en"),
    }
    if detected_releases != expected_releases:
        raise SystemExit(
            "Did not find every supplied supported release as a direct --data input:\n"
            f"expected {sorted(expected_releases)}, got {sorted(detected_releases)}"
        )
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
