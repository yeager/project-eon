#!/usr/bin/env python3
"""Verify the complete, hash-bound static-disassembly coverage manifest."""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SHA256 = re.compile(r"[0-9a-f]{64}")


def _generate_candidates(releases: dict, inventory: dict) -> dict:
    path = ROOT / "tools" / "generate_disassembly_candidate_inventory.py"
    spec = importlib.util.spec_from_file_location("eon_disassembly_candidates", path)
    if spec is None or spec.loader is None:
        raise ManifestError("unable to load disassembly candidate generator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    try:
        return module.generate(releases, inventory)
    except (KeyError, ValueError) as error:
        raise ManifestError(f"unable to derive scanner candidates: {error}") from error


class ManifestError(ValueError):
    """The committed coverage claim is incomplete or internally inconsistent."""


def _read(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ManifestError(f"unable to read {path}: {error}") from error
    if not isinstance(value, dict):
        raise ManifestError(f"{path} must contain one JSON object")
    return value


def _ranges(value: object, label: str) -> list[tuple[int, int]]:
    if not isinstance(value, list) or not value:
        raise ManifestError(f"{label} has no declared source ranges")
    ranges: list[tuple[int, int]] = []
    for row in value:
        if not isinstance(row, dict):
            raise ManifestError(f"{label} has an invalid source range")
        start, length = row.get("source_offset"), row.get("length")
        if not isinstance(start, int) or start < 0 or not isinstance(length, int) or length <= 0:
            raise ManifestError(f"{label} has an invalid source range")
        ranges.append((start, start + length))
    ordered = sorted(ranges)
    for previous, current in zip(ordered, ordered[1:]):
        if current[0] < previous[1]:
            raise ManifestError(f"{label} has overlapping source ranges")
    return ordered


def verify(manifest: dict, inventory: dict, releases: dict) -> dict:
    if manifest.get("schema") != "project-eon.complete-disassembly/v1":
        raise ManifestError("complete-disassembly schema is not supported")
    if inventory.get("schema") != "project-eon.disassembly-inventory/v2":
        raise ManifestError("disassembly-inventory schema is not supported")
    if releases.get("schema") != "project-eon.release-manifest/v1":
        raise ManifestError("release-manifest schema is not supported")
    release_rows = releases.get("releases")
    if not isinstance(release_rows, list):
        raise ManifestError("release manifest has no releases")
    recognized = {row.get("sha256"): row for row in release_rows if isinstance(row, dict)}
    if any(not isinstance(digest, str) or not SHA256.fullmatch(digest) for digest in recognized):
        raise ManifestError("release manifest contains an invalid release hash")
    inventory_rows = inventory.get("releases")
    if not isinstance(inventory_rows, list):
        raise ManifestError("disassembly inventory has no releases")
    inventory_by_release = {row.get("release_sha256"): row for row in inventory_rows
                            if isinstance(row, dict)}
    manifest_rows = manifest.get("releases")
    if not isinstance(manifest_rows, list):
        raise ManifestError("complete-disassembly manifest has no releases")
    manifest_by_release = {row.get("release_sha256"): row for row in manifest_rows
                           if isinstance(row, dict)}
    if set(manifest_by_release) != set(recognized) or set(inventory_by_release) != set(recognized):
        raise ManifestError("recognized release set is not enumerated exactly once")

    generated_candidates = {
        row["release_sha256"]: row["candidates"]
        for row in _generate_candidates(releases, inventory)["releases"]
    }
    image_count = range_count = byte_count = mapped_candidates = unmapped_candidates = 0
    for release_hash, release in recognized.items():
        declared = manifest_by_release[release_hash]
        inv = inventory_by_release[release_hash]
        for key in ("game", "platform", "language"):
            if declared.get(key) != release.get(key):
                raise ManifestError(f"{release_hash} {key} identity does not match release manifest")
        images = declared.get("images")
        if not isinstance(images, list):
            raise ManifestError(f"{release_hash} has no image enumeration")
        manifest_images = {row.get("span_id"): row for row in images if isinstance(row, dict)}
        inventory_images = {row.get("id"): row for row in inv.get("static_spans", [])
                            if isinstance(row, dict)}
        if len(manifest_images) != len(images) or set(manifest_images) != set(inventory_images):
            raise ManifestError(f"{release_hash} executable/code image set differs from inventory")
        if not images and not declared.get("unmapped_boundary"):
            raise ManifestError(f"{release_hash} has neither images nor an explicit unmapped boundary")
        candidates = generated_candidates[release_hash]
        generated_unmapped = sorted((row["profile_id"], row["code_candidate_kind"])
                                    for row in candidates
                                    if row["status"] == "discovered-unmapped")
        declared_unmapped_rows = declared.get("discovered_unmapped_candidates")
        if not isinstance(declared_unmapped_rows, list):
            raise ManifestError(f"{release_hash} has no discovered-unmapped candidate ledger")
        try:
            declared_unmapped = sorted((row["profile_id"], row["code_candidate_kind"])
                                       for row in declared_unmapped_rows)
        except (KeyError, TypeError) as error:
            raise ManifestError(f"{release_hash} has an invalid discovered-unmapped candidate") from error
        if declared_unmapped != generated_unmapped:
            raise ManifestError(f"{release_hash} discovered-unmapped candidate set differs from scanner ledgers")
        for candidate in candidates:
            if candidate["status"] == "mapped":
                if not candidate["mapped_span_ids"] or not set(candidate["mapped_span_ids"]) <= set(manifest_images):
                    raise ManifestError(f"{candidate['profile_id']} mapped candidate is detached from an image")
                mapped_candidates += 1
            else:
                unmapped_candidates += 1
        for span_id, image in manifest_images.items():
            span = inventory_images[span_id]
            if image.get("source_sha256") != span.get("leaf_sha256") or not SHA256.fullmatch(str(image.get("source_sha256", ""))):
                raise ManifestError(f"{span_id} source hash differs from inventory")
            if image.get("architecture") != span.get("cpu"):
                raise ManifestError(f"{span_id} architecture differs from inventory")
            decoder = image.get("decoder")
            if not isinstance(decoder, dict) or decoder.get("name") != "capstone" or decoder.get("version") != ">=5,<6":
                raise ManifestError(f"{span_id} decoder name/version is not pinned")
            basis = image.get("address_basis")
            if basis not in {"dos-com-linear-0x100", "runtime-absolute", "image-relative-unrelocated"}:
                raise ManifestError(f"{span_id} address basis is unsupported")
            if ((span.get("cpu") == "i8086" and basis != "dos-com-linear-0x100")
                    or (span.get("cpu") == "m68000" and basis == "dos-com-linear-0x100")):
                raise ManifestError(f"{span_id} address basis differs from its architecture")
            declared_ranges = _ranges(image.get("source_ranges"), span_id)
            covered_ranges = _ranges(span.get("segments"), span_id + " inventory coverage")
            if declared_ranges != covered_ranges:
                raise ManifestError(f"{span_id} has a coverage gap or undeclared range")
            for segment in span["segments"]:
                segment_basis = segment.get("address_space", "runtime")
                if basis == "image-relative-unrelocated" and segment_basis != basis:
                    raise ManifestError(f"{span_id} address basis differs from its segment")
                if basis != "image-relative-unrelocated" and segment_basis != "runtime":
                    raise ManifestError(f"{span_id} address basis differs from its segment")
            image_count += 1
            range_count += len(declared_ranges)
            byte_count += sum(end - start for start, end in declared_ranges)
    return {"releases": len(recognized), "images": image_count,
            "ranges": range_count, "bytes": byte_count,
            "mapped_candidates": mapped_candidates,
            "unmapped_candidates": unmapped_candidates}


def render_index(manifest: dict, totals: dict) -> str:
    lines = ["# Complete disassembly preservation index", "",
             (f"Verified coverage: {totals['releases']} recognized releases, "
              f"{totals['images']} code images, {totals['ranges']} source ranges, "
              f"{totals['bytes']} bytes; {totals['mapped_candidates']} mapped and "
              f"{totals['unmapped_candidates']} discovered-unmapped scanner candidates."), "",
             "Raw disassembly listings remain outside the repository.", ""]
    for release in manifest["releases"]:
        label = f"{release['game']} / {release['platform']} / {release['language']}"
        lines.append(f"## {label}")
        lines.append("")
        if release["images"]:
            for image in release["images"]:
                lines.append(f"- `{image['span_id']}` — {image['architecture']}, {image['address_basis']}")
        else:
            lines.append(f"- Unmapped preservation boundary: {release['unmapped_boundary']}")
        for candidate in release["discovered_unmapped_candidates"]:
            lines.append(f"- Discovered but unmapped: `{candidate['profile_id']}` "
                         f"({candidate['code_candidate_kind']})")
        lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=ROOT / "docs/complete-disassembly-manifest.json")
    parser.add_argument("--inventory", type=Path, default=ROOT / "docs/disassembly-inventory.json")
    parser.add_argument("--release-manifest", type=Path, default=ROOT / "docs/release-manifest.json")
    parser.add_argument("--index", action="store_true", help="print the verified English preservation index")
    args = parser.parse_args(argv)
    try:
        manifest = _read(args.manifest)
        totals = verify(manifest, _read(args.inventory), _read(args.release_manifest))
    except ManifestError as error:
        print(f"COMPLETE DISASSEMBLY REJECTED  {error}", file=sys.stderr)
        return 2
    if args.index:
        print(render_index(manifest, totals))
    else:
        print("COMPLETE DISASSEMBLY VERIFIED  " + ", ".join(
            f"{value} {key}" for key, value in totals.items()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
