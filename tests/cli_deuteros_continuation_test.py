"""Black-box, read-only Deuteros Amiga native bootstrap continuation check."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys


RELEASE_SHA256 = "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04"


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
        raise SystemExit("usage: cli_deuteros_continuation_test.py <project-eon> <media-root>")
    executable, root_text = sys.argv[1:]
    root = Path(root_text)
    if not root.is_absolute() or not root.is_dir():
        raise SystemExit("direct media root must be an absolute directory")

    before = snapshot(root)
    completed = subprocess.run(
        [executable, "--data", str(root), "--game", "deuteros", "--platform", "amiga",
         "--release-language", "en", "--release-sha256", RELEASE_SHA256,
         "--native-step-diagnostics-json", "--opening-ticks", "128",
         "--opening-input-held", "1", "--opening-continue"],
        check=False, text=True, capture_output=True,
    )
    after = snapshot(root)
    if before != after:
        raise SystemExit("Project Eon changed direct original media")
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(f"Deuteros continuation did not emit JSON: {error}") from error
    continuation = payload.get("title_continuation")
    if (completed.returncode != 0
            or payload.get("release", {}).get("sha256") != RELEASE_SHA256
            or payload.get("result", {}).get("stop_reason") != "title-handoff"
            or not isinstance(continuation, dict)
            or any(continuation.get(key) != value for key, value in {
                "accepted": True, "steps": 25,
                "stop_reason": "native-translation-boundary", "stop_before": "$fcca42",
                "waiting_for_external_observation": False,
                "external_requirement": None,
            }.items())
            or payload.get("error")):
        raise SystemExit(
            "Deuteros Amiga did not reach its expected native graphics-library boundary:\n"
            f"{completed.stdout}\n{completed.stderr}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
