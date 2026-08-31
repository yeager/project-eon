#!/usr/bin/env python3
"""Verify external linear-disassembly reports without retaining their bytes.

The reports describe user-supplied commercial executable data and therefore
remain outside Git.  This tool checks their committed report identities and
line counts against the preservation inventory; it never opens game media,
creates an output file, or copies report contents into the repository.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import stat
import sys


ROOT = Path(__file__).resolve().parents[1]
MAX_REPORT_BYTES = 256 * 1024 * 1024
MAX_REPORT_DIRECTORY_ENTRIES = 64


class ReportError(ValueError):
    """An external report is absent, unsafe, or does not match the ledger."""


def load_expected_spans(inventory_path: Path) -> dict[str, tuple[str, int]]:
    try:
        inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ReportError(f"unable to read disassembly inventory: {error}") from error
    if inventory.get("schema") != "project-eon.disassembly-inventory/v2":
        raise ReportError("disassembly inventory schema is not supported")
    expected: dict[str, tuple[str, int]] = {}
    try:
        for release in inventory["releases"]:
            for span in release["static_spans"]:
                identifier = span["id"]
                digest = span["report_sha256"]
                lines = span["report_lines"]
                if (not isinstance(identifier, str) or not identifier
                        or identifier in expected
                        or not isinstance(digest, str) or len(digest) != 64
                        or any(byte not in "0123456789abcdef" for byte in digest)
                        or not isinstance(lines, int) or lines <= 0):
                    raise ReportError("disassembly inventory has an invalid static-report identity")
                expected[identifier] = (digest, lines)
    except (KeyError, TypeError) as error:
        raise ReportError("disassembly inventory has an incomplete static-report entry") from error
    if not expected:
        raise ReportError("disassembly inventory has no static reports")
    return expected


def parse_report_arguments(values: list[str]) -> dict[str, Path]:
    reports: dict[str, Path] = {}
    for value in values:
        identifier, separator, raw_path = value.partition("=")
        if not separator or not identifier or not raw_path or identifier in reports:
            raise ReportError("each --report must be one unique span-id=/absolute/report/path pair")
        reports[identifier] = Path(raw_path)
    return reports


def require_external_report(path: Path) -> Path:
    if not path.is_absolute() or path.as_posix() == "/tmp" or path.as_posix().startswith("/tmp/"):
        raise ReportError("report paths must be absolute and outside /tmp")
    try:
        info = path.lstat()
    except OSError as error:
        raise ReportError(f"unable to stat report {path}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise ReportError(f"report {path} must be a regular non-symlink file")
    if info.st_size == 0 or info.st_size > MAX_REPORT_BYTES:
        raise ReportError(f"report {path} is outside the bounded size limit")
    resolved = path.resolve(strict=True)
    if resolved == ROOT or ROOT in resolved.parents:
        raise ReportError("report paths must stay outside the repository")
    return resolved


def require_external_report_directory(path: Path) -> Path:
    """Admit one explicit, bounded directory of retained report files.

    This is intentionally not a recursive discovery route. A maintainer must
    point at the directory that contains their already-generated linear
    reports; the verifier then accepts only regular non-symlink files outside
    both the repository and /tmp. No game media is opened by this route.
    """
    if not path.is_absolute() or path.as_posix() == "/tmp" or path.as_posix().startswith("/tmp/"):
        raise ReportError("report directory must be absolute and outside /tmp")
    try:
        info = path.lstat()
    except OSError as error:
        raise ReportError(f"unable to stat report directory {path}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISDIR(info.st_mode):
        raise ReportError(f"report directory {path} must be a directory and not a symlink")
    resolved = path.resolve(strict=True)
    if resolved == ROOT or ROOT in resolved.parents:
        raise ReportError("report directory must stay outside the repository")
    return resolved


def report_identity(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    line_count = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
            line_count += block.count(b"\n")
    return digest.hexdigest(), line_count


def discover_reports_in_directory(expected: dict[str, tuple[str, int]], directory: Path) -> dict[str, Path]:
    """Match all expected span identities from one explicit report directory.

    Several static spans intentionally share one aggregate report. Matching on
    the committed hash-plus-line identity means report filenames cannot become
    a source of provenance or a fallback selection rule.
    """
    root = require_external_report_directory(directory)
    try:
        entries = sorted(root.iterdir(), key=lambda path: path.name)
    except OSError as error:
        raise ReportError(f"unable to enumerate report directory {root}: {error}") from error
    if len(entries) > MAX_REPORT_DIRECTORY_ENTRIES:
        raise ReportError("report directory exceeds the bounded entry limit")
    expected_identities = set(expected.values())
    available: dict[tuple[str, int], Path] = {}
    for entry in entries:
        try:
            report = require_external_report(entry)
        except ReportError:
            # The explicit directory can contain notes or subdirectories, but
            # none can become a report candidate. A matching report must be a
            # direct regular non-symlink file and will be rechecked below.
            continue
        identity = report_identity(report)
        if identity in expected_identities and identity not in available:
            available[identity] = report
    reports: dict[str, Path] = {}
    for identifier, identity in expected.items():
        report = available.get(identity)
        if report is None:
            raise ReportError(f"report directory has no matching report for {identifier}")
        reports[identifier] = report
    return reports


def verify_reports(expected: dict[str, tuple[str, int]], reports: dict[str, Path]) -> int:
    if set(reports) != set(expected):
        missing = sorted(set(expected) - set(reports))
        unexpected = sorted(set(reports) - set(expected))
        details = ([f"missing={','.join(missing)}"] if missing else []) + (
            [f"unexpected={','.join(unexpected)}"] if unexpected else [])
        raise ReportError("report span set does not match the inventory: " + "; ".join(details))
    resolved_reports: dict[Path, tuple[str, int]] = {}
    for identifier, raw_path in reports.items():
        path = require_external_report(raw_path)
        observed = resolved_reports.get(path)
        if observed is None:
            observed = report_identity(path)
            resolved_reports[path] = observed
        expected_digest, expected_lines = expected[identifier]
        if observed != (expected_digest, expected_lines):
            raise ReportError(f"report {identifier} does not match its committed hash/line identity")
    return len(resolved_reports)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=ROOT / "docs" / "disassembly-inventory.json",
                        help="Committed disassembly inventory (default: docs/disassembly-inventory.json)")
    parser.add_argument("--report", action="append", default=[], metavar="SPAN_ID=ABSOLUTE_PATH",
                        help="One external report for each static span; repeat for every inventory span")
    parser.add_argument("--report-directory", type=Path,
                        help="One absolute external directory containing retained reports; match by hash/line identity")
    args = parser.parse_args(argv)
    try:
        expected = load_expected_spans(args.inventory)
        if args.report_directory is not None and args.report:
            raise ReportError("--report-directory cannot be combined with --report")
        reports = (discover_reports_in_directory(expected, args.report_directory)
                   if args.report_directory is not None
                   else parse_report_arguments(args.report))
        unique_reports = verify_reports(expected, reports)
    except ReportError as error:
        print(f"DISASSEMBLY REPORTS REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"DISASSEMBLY REPORTS VERIFIED  {len(expected)} spans, {unique_reports} unique reports")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
