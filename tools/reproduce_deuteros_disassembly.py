#!/usr/bin/env python3
"""Reproduce all currently mapped Deuteros code-image listings externally."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.reproduce_disassembly_reports import ReproductionError, require_output_directory


def run(arguments: list[str]) -> None:
    completed = subprocess.run((sys.executable, *arguments), cwd=ROOT,
                               text=True, capture_output=True, check=False)
    if completed.returncode:
        raise ReproductionError(completed.stderr.strip() or completed.stdout.strip())


def identity(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    return {"file": path.name, "sha256": hashlib.sha256(data).hexdigest(),
            "lines": data.count(b"\n")}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--amiga-archive", type=Path, required=True)
    parser.add_argument("--atari-replicants-archive", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = require_output_directory(args.output_directory)
        amiga = output / "deuteros-amiga-clean-loaded-spans.md"
        atari = output / "deuteros-atari-replicants-first-stage.md"
        atari_second = output / "deuteros-atari-replicants-second-stage.md"
        run(["tools/analyze_m68k.py", "--archive", str(args.amiga_archive),
             "--archive-sha256", "7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533",
             "--member", "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf",
             "--member-sha256", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
             "--complete-linear", "--output", str(amiga)])
        run(["tools/disassemble_m68k_range.py", "--archive",
             str(args.atari_replicants_archive), "--archive-sha256",
             "a9318feb83ff34b79f5a5ea1e5ffcb45828e4432ac75a859f55c3de97d724c93",
             "--member", "Deuteros (1991)(Activision)(M3)(Disk 1 of 2)[cr Replicants].st",
             "--member-sha256", "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee",
             "--offset", "0x4ec00", "--length", "0x1200", "--address", "0x1200",
             "--sha256", "d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc",
             "--output", str(atari)])
        run(["tools/disassemble_m68k_range.py", "--archive",
             str(args.atari_replicants_archive), "--archive-sha256",
             "a9318feb83ff34b79f5a5ea1e5ffcb45828e4432ac75a859f55c3de97d724c93",
             "--member", "Deuteros (1991)(Activision)(M3)(Disk 1 of 2)[cr Replicants].st",
             "--member-sha256", "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee",
             "--offset", "0x4800", "--length", "0x1200", "--address", "0x70000",
             "--sha256", "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7",
             "--output", str(atari_second)])
        index = {
            "schema": "project-eon.deuteros-full-disassembly/v1",
            "classification": "linear-candidate-unclassified",
            "reports": [identity(amiga), identity(atari), identity(atari_second)],
            "mapped_source_bytes": {"amiga": 467456, "atari_st": 9216},
            "gaps": [
                "Atari ST execution after the mapped 0x1200-byte second stage remains runtime-dependent",
                "Other recognized Amiga and Atari media variants have no independently proven code-image mapping",
            ],
        }
        (output / "index.json").write_text(
            json.dumps(index, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ReproductionError) as error:
        print(f"DEUTEROS DISASSEMBLY REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"DEUTEROS DISASSEMBLY COMPLETE  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
