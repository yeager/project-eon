"""Black-box, read-only admission test for a real installed Millennium DOS set."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys


EXPECTED_RELEASE = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"


def snapshot(root: Path) -> dict[str, tuple[int, str]]:
    result: dict[str, tuple[int, str]] = {}
    for item in sorted(root.rglob("*")):
        if item.is_symlink() or not item.is_file():
            continue
        digest = hashlib.sha256(item.read_bytes()).hexdigest()
        result[str(item.relative_to(root))] = (item.stat().st_size, digest)
    return result


def run(executable: str, root: Path, *arguments: str) -> str:
    completed = subprocess.run(
        [executable, "--data", str(root), "--game", "millennium", "--platform", "dos", *arguments],
        check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    return completed.stdout


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: cli_direct_directory_test.py <project-eon> <direct-media-root>")
    executable, root_text = sys.argv[1:]
    root = Path(root_text)
    if not root.is_absolute() or not root.is_dir():
        raise SystemExit("direct media root must be an absolute directory")
    before = snapshot(root)
    launch = run(executable, root, "--launch-check")
    inspection = run(executable, root, "--inspect", "--inventory")
    inspection_json = run(executable, root, "--inspect-json")
    after = snapshot(root)
    if before != after:
        raise SystemExit("Project Eon changed direct original media")
    if "LAUNCH CHECK  Millennium 2.2 / DOS / en / READY" not in launch:
        raise SystemExit("direct Millennium DOS set did not reach parser-only READY admission")
    if EXPECTED_RELEASE not in inspection:
        raise SystemExit("direct Millennium DOS logical release identity was not inspected")
    parsed = json.loads(inspection_json)
    releases = parsed.get("releases", [])
    if len(releases) != 1 or releases[0].get("media_layout") != "verified-directory":
        raise SystemExit("inspection did not label direct media as a verified directory")
    if releases[0].get("direct_set_sha256") != "d938cd6a611a83897a745b257a371613b73a7dddffb2d336ec2167a192803783":
        raise SystemExit("inspection omitted the verified direct-set identity")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
