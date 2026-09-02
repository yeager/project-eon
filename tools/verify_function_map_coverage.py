#!/usr/bin/env python3
"""Report strict, direct function-map coverage in external control-flow sidecars.

This is deliberately an evidence cross-check, not a disassembler or runtime
input.  A function-map row is reported as directly covered only when an
external v1 sidecar declares an exact matching release identity, CPU, address
space, range SHA-256, and an address within that range.  Missing declarations
are reported as such; they do not classify code or disprove a map row.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_disassembly_reports import ReportError, require_external_report


ROOT = Path(__file__).resolve().parents[1]
CLASSIFICATION = "static-candidate-unclassified"
ADDRESS_SPACES = {"runtime", "image-relative-unrelocated"}


def require_digest(value: object, label: str) -> str:
    if (not isinstance(value, str) or len(value) != 64
            or any(character not in "0123456789abcdef" for character in value)):
        raise ReportError(f"{label} must be a lower-case SHA-256")
    return value


def require_uint(value: object, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ReportError(f"{label} must be a non-negative integer")
    return value


def load_function_map(path: Path) -> list[dict]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ReportError(f"unable to read function map: {error}") from error
    if payload.get("schema") != "project-eon.function-map/v1" or not isinstance(payload.get("entries"), list):
        raise ReportError("function map has an unsupported schema")
    entries = payload["entries"]
    ids: set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("id"), str) or not entry["id"] or entry["id"] in ids:
            raise ReportError("function map has an invalid entry identity")
        ids.add(entry["id"])
        require_digest(entry.get("release_sha256"), "function release_sha256")
        require_digest(entry.get("source_asset_sha256"), "function source_asset_sha256")
        if "source_span_sha256" in entry:
            require_digest(entry["source_span_sha256"], "function source_span_sha256")
        if entry.get("cpu") not in {"i8086", "m68000"}:
            raise ReportError("function map has an unsupported CPU")
        space = entry.get("address_space", "runtime")
        if space not in ADDRESS_SPACES:
            raise ReportError("function map has an unsupported address space")
        address = entry.get("runtime_address")
        prefix = "$" if space == "runtime" else "+0x"
        try:
            if not isinstance(address, str) or not address.startswith(prefix) or not address[len(prefix):]:
                raise ValueError
            entry["_address"] = int(address[len(prefix):], 16)
        except ValueError as error:
            raise ReportError("function map has an invalid address") from error
        entry["_address_space"] = space
    return entries


def load_declared_ranges(sidecar_paths: list[Path]) -> set[tuple[str, str, str, str, int, int]]:
    """Return deduplicated (release, CPU, space, range hash, start, length) facts."""
    declared: set[tuple[str, str, str, str, int, int]] = set()
    for raw_path in sidecar_paths:
        path = require_external_report(raw_path)
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ReportError(f"control-flow sidecar is not JSON: {error}") from error
        if (payload.get("schema") != "project-eon.static-control-flow-set/v1"
                or payload.get("classification") != CLASSIFICATION
                or not isinstance(payload.get("documents"), list) or not payload["documents"]):
            raise ReportError("control-flow sidecar has an unsupported schema")
        for document in payload["documents"]:
            if not isinstance(document, dict) or document.get("schema") != "project-eon.static-control-flow/v1":
                raise ReportError("control-flow sidecar has an invalid document")
            if document.get("classification") != CLASSIFICATION or document.get("cpu") not in {"i8086", "m68000"}:
                raise ReportError("control-flow sidecar has invalid document classification or CPU")
            require_digest(document.get("archive_sha256"), "sidecar archive SHA-256")
            require_digest(document.get("source_sha256"), "sidecar source SHA-256")
            if not isinstance(document.get("source"), str) or not document["source"]:
                raise ReportError("control-flow sidecar has invalid source provenance")
            # Early extractor v1 sidecars predate the explicit source_kind
            # field. It is not a linkage dimension, so retain their bounded
            # hash/address facts without inventing a provenance category.
            if "source_kind" in document and (not isinstance(document["source_kind"], str) or not document["source_kind"]):
                raise ReportError("control-flow sidecar has invalid source kind")
            release = require_digest(document.get("carrier_archive_sha256", document.get("archive_sha256")),
                                     "sidecar effective release SHA-256")
            space = document.get("address_space", "runtime")
            if space not in ADDRESS_SPACES:
                raise ReportError("control-flow sidecar has an unsupported address space")
            key = "runtime_address" if space == "runtime" else "image_relative_address"
            ranges = document.get("ranges")
            if not isinstance(ranges, list) or not ranges:
                raise ReportError("control-flow sidecar has no declared ranges")
            for value in ranges:
                if not isinstance(value, dict):
                    raise ReportError("control-flow sidecar has an invalid declared range")
                start = require_uint(value.get(key), f"sidecar {key}")
                length = require_uint(value.get("length"), "sidecar range length")
                if length == 0 or start > sys.maxsize - length:
                    raise ReportError("control-flow sidecar has an overflowing declared range")
                digest = require_digest(value.get("sha256"), "sidecar range SHA-256")
                declared.add((release, document["cpu"], space, digest, start, length))
    return declared


def coverage(entries: list[dict], declared: set[tuple[str, str, str, str, int, int]]) -> tuple[list[str], list[str]]:
    bound: list[str] = []
    undeclared: list[str] = []
    for entry in entries:
        range_hash = entry.get("source_span_sha256", entry["source_asset_sha256"])
        matches = [row for row in declared if row[:4] == (
            entry["release_sha256"], entry["cpu"], entry["_address_space"], range_hash)]
        if any(start <= entry["_address"] < start + length for _, _, _, _, start, length in matches):
            bound.append(entry["id"])
        else:
            undeclared.append(entry["id"])
    return bound, undeclared


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--function-map", type=Path, default=ROOT / "docs" / "function-map.json")
    parser.add_argument("--sidecar", action="append", default=[], type=Path,
                        help="Absolute external static-control-flow sidecar; repeat as needed")
    parser.add_argument("--require-complete", action="store_true",
                        help="Reject if any function-map row lacks a direct declared-range binding")
    parser.add_argument("--json", action="store_true", help="Emit only aggregate IDs and counts")
    args = parser.parse_args(argv)
    try:
        if not args.sidecar:
            raise ReportError("at least one --sidecar is required")
        entries = load_function_map(args.function_map)
        bound, undeclared = coverage(entries, load_declared_ranges(args.sidecar))
        if args.require_complete and undeclared:
            raise ReportError("function-map rows are not directly declared by the supplied sidecars")
    except ReportError as error:
        print(f"FUNCTION MAP COVERAGE REJECTED  {error}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps({"schema": "project-eon.function-map-coverage/v1",
                          "classification": CLASSIFICATION,
                          "function_entries": len(entries),
                          "direct_range_bindings": len(bound),
                          "not_declared_by_supplied_sidecars": len(undeclared),
                          "bound_ids": bound, "not_declared_ids": undeclared}, sort_keys=True))
    else:
        print(f"FUNCTION MAP COVERAGE VERIFIED  {len(entries)} rows; {len(bound)} direct range bindings; "
              f"{len(undeclared)} not declared by supplied sidecars")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
