"""Read-only CLI check for the owned Millennium Atari TOS startup services."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys


RELEASE_SHA256 = "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69"


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
        raise SystemExit(
            "usage: cli_millennium_atari_native_service_test.py <project-eon> <media-root>"
        )
    executable, root_text = sys.argv[1:]
    root = Path(root_text)
    if not root.is_absolute() or not root.is_dir():
        raise SystemExit("direct media root must be an absolute directory")

    before = snapshot(root)
    completed = subprocess.run(
        [executable, "--data", str(root), "--game", "millennium", "--platform", "atari-st",
         "--release-language", "en", "--release-sha256", RELEASE_SHA256,
         "--native-step-diagnostics-json", "--native-step-limit", "65536"],
        check=False, text=True, capture_output=True,
    )
    after = snapshot(root)
    if before != after:
        raise SystemExit("Project Eon changed direct original media")
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(f"Millennium Atari diagnostic did not emit JSON: {error}") from error

    result = payload.get("result")
    if (completed.returncode != 0
            or payload.get("release", {}).get("sha256") != RELEASE_SHA256
            or payload.get("driver") != "millennium-atari"
            or any(result.get(key) != value for key, value in {
                "accepted": True,
                "steps": 9596,
                "state": "game-init-second-config-xbios-38",
                "stop_before": "$2ab24",
                "waiting_for_external_observation": True,
                "step_limit_reached": False,
                "status_register_origin": "eon-tos-compatibility",
                "line_a_origin": "eon-tos-compatibility",
                "selector_21_origin": "eon-tos-compatibility",
                "selector_6_origin": "eon-tos-compatibility",
                "selector_38_origin": "eon-tos-compatibility",
                "selector_6_palette_loaded": True,
                "movem_registers_preserved_opaque": True,
                "abstract_movem_rts_stack_delta": 64,
            }.items())
            or not all(isinstance(result.get(key), int) for key in (
                "last_sequence", "local_instruction_count", "loop_iteration", "loop_d7_value",
                "palette_helper_entry_count", "state_id"))
            or payload.get("error")):
        raise SystemExit(
                "Millennium Atari did not reach its expected bounded game-init checkpoint "
            f"after the owned TOS services and real config decode:\n{completed.stdout}\n{completed.stderr}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
