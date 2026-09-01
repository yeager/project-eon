#!/usr/bin/env python3
"""Verify external static-control-flow sidecars against the preservation ledger."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

# Keep direct execution and test-module loading independent of the caller's
# import path; this helper never installs or imports project runtime code.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_disassembly_reports import ReportError, report_identity, require_external_report


ROOT = Path(__file__).resolve().parents[1]


def expected_sidecars(inventory_path: Path) -> dict[str, tuple[str, int]]:
    try:
        inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ReportError(f"unable to read disassembly inventory: {error}") from error
    if inventory.get("schema") != "project-eon.disassembly-inventory/v2":
        raise ReportError("disassembly inventory schema is not supported")
    expected: dict[str, tuple[str, int]] = {}
    known_spans = {span.get("id") for release in inventory.get("releases", [])
                   for span in release.get("static_spans", [])}
    for row in inventory.get("control_flow_sidecars", []):
        digest, lines, identifiers = row.get("sha256"), row.get("lines"), row.get("span_ids")
        if (not isinstance(digest, str) or len(digest) != 64
                or any(character not in "0123456789abcdef" for character in digest)
                or not isinstance(lines, int) or lines <= 0 or row.get("classification")
                != "static-candidate-unclassified" or not isinstance(identifiers, list) or not identifiers):
            raise ReportError("disassembly inventory has an invalid control-flow identity")
        for identifier in identifiers:
            if not isinstance(identifier, str) or identifier not in known_spans or identifier in expected:
                raise ReportError("disassembly inventory has an invalid control-flow span reference")
            expected[identifier] = (digest, lines)
    if not expected:
        raise ReportError("disassembly inventory has no control-flow sidecars")
    return expected


def parse_sidecars(values: list[str]) -> dict[str, Path]:
    sidecars: dict[str, Path] = {}
    for value in values:
        identifier, separator, raw_path = value.partition("=")
        if not separator or not identifier or not raw_path or identifier in sidecars:
            raise ReportError("each --sidecar must be one unique span-id=/absolute/sidecar.json pair")
        sidecars[identifier] = Path(raw_path)
    return sidecars


def verify_sidecars(expected: dict[str, tuple[str, int]], sidecars: dict[str, Path]) -> int:
    if set(expected) != set(sidecars):
        raise ReportError("control-flow span set does not match the inventory")
    observed_paths: dict[Path, tuple[str, int]] = {}
    for identifier, raw_path in sidecars.items():
        path = require_external_report(raw_path)
        identity = observed_paths.setdefault(path, report_identity(path))
        if identity != expected[identifier]:
            raise ReportError(f"control-flow sidecar {identifier} does not match its committed hash/line identity")
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ReportError(f"control-flow sidecar {identifier} is not JSON: {error}") from error
        if payload.get("schema") != "project-eon.static-control-flow-set/v1":
            raise ReportError(f"control-flow sidecar {identifier} has an unsupported schema")
    return len(observed_paths)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=ROOT / "docs" / "disassembly-inventory.json")
    parser.add_argument("--sidecar", action="append", default=[], metavar="SPAN_ID=ABSOLUTE_PATH")
    args = parser.parse_args(argv)
    try:
        expected = expected_sidecars(args.inventory)
        unique = verify_sidecars(expected, parse_sidecars(args.sidecar))
    except ReportError as error:
        print(f"STATIC CONTROL FLOW REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"STATIC CONTROL FLOW VERIFIED  {len(expected)} spans, {unique} unique sidecars")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
