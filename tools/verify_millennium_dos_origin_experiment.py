#!/usr/bin/env python3
"""Verify an external DOSBox-X origin-observer experiment, never game evidence."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

from millennium_dos_origin_experiment import ExperimentError, verify_run


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", help="External experiment output directory")
    args = parser.parse_args(argv)
    try:
        fields = verify_run(Path(args.capture_directory) / "experiment-status.txt")
    except (ExperimentError, OSError) as error:
        print(f"origin experiment verification rejected: {error}", file=sys.stderr)
        return 1
    print("origin experiment verified; experimental-only, not recovery-admissible")
    print("record meaning: last-fetch context only; no causal transfer is established")
    print(f"raw receipt SHA-256: {fields['raw_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
