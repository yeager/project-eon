#!/usr/bin/env python3
"""Cross-validate Project Eon's hash-addressed preservation ledgers.

This verifier reads repository metadata only.  It never opens user game media,
captures, saves, or generated reports.  Its purpose is to stop a release,
parser profile, linear disassembly span, or parity boundary from becoming
orphaned when the preservation records evolve independently.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SHA256 = re.compile(r"^[0-9a-f]{64}$")
EXPECTED_FIELDS = {
    "recognition", "bootstrap", "gameplay", "input", "rendering", "audio", "saves", "completion",
}


class LedgerError(ValueError):
    """A preservation record is incomplete, detached, or internally unsafe."""


def load_json(root: Path, name: str) -> dict:
    path = root / "docs" / name
    try:
        with path.open(encoding="utf-8") as stream:
            value = json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        raise LedgerError(f"cannot read {path}: {error}") from error
    if not isinstance(value, dict):
        raise LedgerError(f"{path} must contain a JSON object")
    return value


def require(condition: bool, message: str) -> None:
    if not condition:
        raise LedgerError(message)


def unique_index(rows: list[dict], field: str, label: str) -> dict[str, dict]:
    index: dict[str, dict] = {}
    for row in rows:
        value = row.get(field)
        require(isinstance(value, str) and value, f"{label} has no {field}")
        require(value not in index, f"duplicate {label} {field}: {value}")
        index[value] = row
    return index


def verify(root: Path) -> dict[str, int]:
    manifest = load_json(root, "release-manifest.json")
    recovery = load_json(root, "recovery-map.json")
    parity = load_json(root, "parity-matrix.json")
    disassembly = load_json(root, "disassembly-inventory.json")
    require(manifest.get("schema") == "project-eon.release-manifest/v1", "unexpected release manifest schema")
    require(recovery.get("schema") == "project-eon.recovery-map/v1", "unexpected recovery-map schema")
    require(parity.get("schema") == "project-eon.parity-matrix/v1", "unexpected parity-matrix schema")
    require(disassembly.get("schema") == "project-eon.disassembly-inventory/v2", "unexpected disassembly-inventory schema")

    releases = unique_index(manifest.get("releases", []), "sha256", "release")
    require(releases, "release manifest is empty")
    for release_hash, release in releases.items():
        require(bool(SHA256.fullmatch(release_hash)), f"release identity is not SHA-256: {release_hash}")
        require(release.get("game") in {"millennium", "deuteros"}, f"release {release_hash} has unknown game")
        require(release.get("platform") in {"dos", "amiga", "atari_st"}, f"release {release_hash} has unknown platform")
        require(isinstance(release.get("language"), str) and release["language"], f"release {release_hash} has no language")
        require(isinstance(release.get("size"), int) and release["size"] > 0, f"release {release_hash} has invalid size")

    profiles = unique_index(manifest.get("parser_profiles", []), "id", "parser profile")
    profile_pairs: set[tuple[str, str]] = set()
    leaf_sizes: dict[tuple[str, str], int] = {}
    for profile_id, profile in profiles.items():
        release_hash = profile.get("release_sha256")
        leaf_hash = profile.get("leaf_sha256")
        leaf_size = profile.get("leaf_size")
        offset = profile.get("offset")
        length = profile.get("length")
        require(release_hash in releases, f"profile {profile_id} references an unknown release")
        require(isinstance(leaf_hash, str) and SHA256.fullmatch(leaf_hash), f"profile {profile_id} has invalid leaf hash")
        require(isinstance(leaf_size, int) and leaf_size > 0, f"profile {profile_id} has invalid leaf size")
        require(isinstance(offset, int) and isinstance(length, int) and 0 <= offset < leaf_size
                and 0 < length <= leaf_size - offset, f"profile {profile_id} has unbounded source range")
        pair = (release_hash, profile_id)
        require(pair not in profile_pairs, f"duplicate profile identity: {profile_id}")
        profile_pairs.add(pair)
        key = (release_hash, leaf_hash)
        leaf_sizes[key] = max(leaf_sizes.get(key, 0), leaf_size)

    recovery_entries = unique_index(recovery.get("entries", []), "id", "recovery-map entry")
    recovery_pairs: set[tuple[str, str]] = set()
    for entry_id, entry in recovery_entries.items():
        release_hash = entry.get("release_sha256")
        profile_id = entry.get("parser_profile_id")
        require(release_hash in releases and profile_id in profiles, f"recovery entry {entry_id} has unknown release/profile")
        require(profiles[profile_id]["release_sha256"] == release_hash,
                f"recovery entry {entry_id} crosses release identities")
        pair = (release_hash, profile_id)
        require(pair not in recovery_pairs, f"duplicate recovery profile mapping: {profile_id}")
        recovery_pairs.add(pair)
    require(recovery_pairs == profile_pairs, "recovery map does not cover parser profiles exactly")

    parity_rows = unique_index(parity.get("releases", []), "release_sha256", "parity row")
    require(set(parity_rows) == set(releases), "parity matrix does not cover releases exactly")
    states = set(parity.get("states", []))
    require(states == {"verified", "boundary", "unrecovered"}, "parity matrix state vocabulary changed")
    for release_hash, row in parity_rows.items():
        require(set(row) - {"release_sha256"} == EXPECTED_FIELDS,
                f"parity row {release_hash} has incomplete status fields")
        require(row["recognition"] == "verified", f"recognised release {release_hash} is not recognised in parity")
        require(all(row[field] in states for field in EXPECTED_FIELDS),
                f"parity row {release_hash} uses an invalid status")

    inventory_rows = unique_index(disassembly.get("releases", []), "release_sha256", "disassembly release")
    require(set(inventory_rows) == set(releases), "disassembly inventory does not cover releases exactly")
    span_ids: set[str] = set()
    span_count = 0
    for release_hash, row in inventory_rows.items():
        start_profile = row.get("start_profile_id")
        coverage = row.get("coverage")
        require(start_profile in profiles and profiles[start_profile]["release_sha256"] == release_hash,
                f"disassembly row {release_hash} has an invalid start profile")
        require(isinstance(coverage, list) and coverage and start_profile in coverage,
                f"disassembly row {release_hash} has invalid coverage")
        require(isinstance(row.get("unresolved"), str) and row["unresolved"],
                f"disassembly row {release_hash} has no unresolved boundary")
        for profile_id in coverage:
            require(profile_id in profiles and profiles[profile_id]["release_sha256"] == release_hash,
                    f"disassembly row {release_hash} crosses profile identity {profile_id}")
        for span in row.get("static_spans", []):
            span_count += 1
            span_id = span.get("id")
            leaf_hash = span.get("leaf_sha256")
            require(isinstance(span_id, str) and span_id and span_id not in span_ids,
                    f"duplicate or missing static span id: {span_id}")
            span_ids.add(span_id)
            require(span.get("cpu") == row.get("cpu") and span.get("cpu") in {"i8086", "m68000"},
                    f"static span {span_id} has incompatible CPU")
            require(span.get("coverage_kind") == "linear-candidate-unclassified",
                    f"static span {span_id} claims non-linear coverage")
            require(isinstance(leaf_hash, str) and SHA256.fullmatch(leaf_hash),
                    f"static span {span_id} has invalid leaf hash")
            leaf_size = leaf_sizes.get((release_hash, leaf_hash))
            if leaf_size is None:
                provenance_id = span.get("source_provenance_profile_id")
                require(provenance_id in profiles and profiles[provenance_id]["release_sha256"] == release_hash,
                        f"static span {span_id} has no same-release source provenance")
                leaf_size = profiles[provenance_id]["leaf_size"]
            require(isinstance(span.get("report_sha256"), str) and SHA256.fullmatch(span["report_sha256"]),
                    f"static span {span_id} has invalid report hash")
            require(isinstance(span.get("report_lines"), int) and span["report_lines"] > 0,
                    f"static span {span_id} has invalid report line count")
            require(isinstance(span.get("boundary"), str) and span["boundary"],
                    f"static span {span_id} has no preservation boundary")
            segments = span.get("segments")
            require(isinstance(segments, list) and segments, f"static span {span_id} has no segments")
            previous_end = 0
            for segment in segments:
                offset = segment.get("source_offset")
                length = segment.get("length")
                require(isinstance(offset, int) and isinstance(length, int) and 0 <= offset
                        and 0 < length <= leaf_size - offset and offset >= previous_end,
                        f"static span {span_id} has an unbounded or overlapping segment")
                previous_end = offset + length

    return {"releases": len(releases), "profiles": len(profiles), "spans": span_count}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT,
                        help="Repository root containing docs/ (default: this repository)")
    args = parser.parse_args()
    try:
        result = verify(args.root.resolve())
    except LedgerError as error:
        print(f"PRESERVATION LEDGER REJECTED  {error}")
        return 2
    print("PRESERVATION LEDGER VERIFIED  "
          f"{result['releases']} releases, {result['profiles']} profiles, {result['spans']} static spans")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
