#!/usr/bin/env python3
"""Generate a metadata-only mapped/unmapped code-candidate inventory."""

from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


class CandidateError(ValueError):
    pass


def generate(releases: dict, inventory: dict) -> dict:
    profiles = {row["id"]: row for row in releases.get("parser_profiles", [])}
    result = {"schema": "project-eon.disassembly-candidates/v2", "releases": []}
    for release in inventory.get("releases", []):
        release_hash = release["release_sha256"]
        candidates = []
        spans = release.get("static_spans", [])
        for profile_id in release.get("coverage", []):
            if profile_id not in profiles:
                raise CandidateError(f"unknown parser profile {profile_id}")
            profile = profiles[profile_id]
            if profile.get("release_sha256") != release_hash:
                raise CandidateError(f"cross-release parser profile {profile_id}")
            start = profile.get("offset")
            length = profile.get("length")
            if not isinstance(start, int) or start < 0 or not isinstance(length, int) or length <= 0:
                raise CandidateError(f"invalid parser range {profile_id}")
            mapped = []
            member_spans = []
            for span in spans:
                if span.get("source_provenance_profile_id") == profile_id:
                    member_spans.append(span["id"])
                    continue
                if span.get("leaf_sha256") != profile.get("leaf_sha256"):
                    continue
                intervals = sorted((segment["source_offset"],
                                    segment["source_offset"] + segment["length"])
                                   for segment in span.get("segments", []))
                cursor = start
                for seg_start, seg_end in intervals:
                    if seg_end <= cursor:
                        continue
                    if seg_start > cursor:
                        break
                    cursor = max(cursor, seg_end)
                    if cursor >= start + length:
                        mapped.append(span["id"])
                        break
            if length == 512:
                kind = "boot"
            elif member_spans:
                kind = "container-with-mapped-members"
            else:
                kind = "raw-stage"
            candidates.append({"profile_id": profile_id,
                               "leaf_sha256": profile["leaf_sha256"],
                               "source_offset": start, "length": length,
                               "code_candidate_kind": kind,
                               "status": "mapped" if mapped else "discovered-unmapped",
                               "mapped_span_ids": sorted(set(mapped)),
                               "member_span_ids": sorted(set(member_spans))})
        result["releases"].append({"release_sha256": release_hash,
                                   "candidates": candidates})
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-manifest", type=Path, default=ROOT / "docs/release-manifest.json")
    parser.add_argument("--inventory", type=Path, default=ROOT / "docs/disassembly-inventory.json")
    args = parser.parse_args(argv)
    try:
        releases = json.loads(args.release_manifest.read_text(encoding="utf-8"))
        inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
        generated = generate(releases, inventory)
    except (OSError, json.JSONDecodeError, KeyError, CandidateError) as error:
        print(f"DISASSEMBLY CANDIDATES REJECTED  {error}", file=sys.stderr)
        return 2
    json.dump(generated, sys.stdout, indent=2, sort_keys=True)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
