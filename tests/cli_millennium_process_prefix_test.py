"""Read-only black-box check for the Millennium DOS process-prefix boundary."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys


RELEASE_SHA256 = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"


def snapshot(root: Path) -> dict[str, tuple[int, str]]:
    result: dict[str, tuple[int, str]] = {}
    for item in sorted(root.rglob("*")):
        if item.is_symlink() or not item.is_file():
            continue
        digest = hashlib.sha256()
        with item.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
        result[str(item.relative_to(root))] = (item.stat().st_size, digest.hexdigest())
    return result


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: cli_millennium_process_prefix_test.py <project-eon> <media-root>")
    executable, root_text = sys.argv[1:]
    root = Path(root_text)
    if not root.is_absolute() or not root.is_dir():
        raise SystemExit("direct media root must be an absolute directory")

    before = snapshot(root)
    for input_choice in ("0", "1"):
        completed = subprocess.run(
            [executable, "--data", str(root), "--game", "millennium", "--platform", "dos",
             "--release-language", "en", "--release-sha256", RELEASE_SHA256,
             "--native-step-diagnostics-json", "--native-startup-input", input_choice,
             "--native-step-limit", "2048"],
            check=False, text=True, capture_output=True,
        )
        try:
            payload = json.loads(completed.stdout)
        except json.JSONDecodeError as error:
            raise SystemExit(
                f"Millennium DOS did not emit JSON for startup input {input_choice}: {error}") from error
        result = payload.get("result")
        if (completed.returncode != 0
                or payload.get("release", {}).get("sha256") != RELEASE_SHA256
                or not isinstance(result, dict)
                or any(result.get(key) != value for key, value in {
                    "accepted": True,
                    "steps": 225,
                    "stop_reason": "external-observation",
                    "stop_before": "$1917",
                    "waiting_for_external_observation": True,
                }.items())
                or not isinstance(result.get("external_requirement"), str)
                or not result["external_requirement"].startswith(
                    "WAIT=DOS-PROCESS-PREFIX-BYTE @$1909 SRC=$")
                or not result["external_requirement"].endswith(":0 WIDTH=1")
                or payload.get("error")):
            raise SystemExit(
                "Millennium DOS did not reach its exact native process-prefix boundary:\n"
                f"{completed.stdout}\n{completed.stderr}")
    if before != snapshot(root):
        raise SystemExit("Project Eon changed direct original media")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
