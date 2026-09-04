#!/usr/bin/env python3
"""Locate a reviewed external capture recorder by its pinned SHA-256.

This is a read-only recovery aid for external evidence tooling.  It never
builds, patches, copies, or launches an emulator; it only hashes regular,
executable candidates under explicitly supplied roots.  A normal DOSBox-X or
FS-UAE installation is deliberately not a match.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import sys


ROOT = Path(__file__).resolve().parents[1]
MAX_CANDIDATE_BYTES = 512 * 1024 * 1024
DEFAULT_MAX_FILES = 10_000


class LocatorError(RuntimeError):
    """A safe local lookup failure."""


def load_runner(filename: str):
    spec = importlib.util.spec_from_file_location(filename, ROOT / "tools" / f"{filename}.py")
    if not spec or not spec.loader:
        raise LocatorError(f"unable to load {filename}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def is_system_tmp_path(path: Path) -> bool:
    normalized = path.as_posix().replace("\\", "/")
    return normalized == "/tmp" or normalized.startswith("/tmp/")


def require_root(path: Path) -> Path:
    if is_system_tmp_path(path):
        raise LocatorError("search root must not use /tmp")
    if not path.is_absolute():
        raise LocatorError("search root must be absolute")
    try:
        info = path.lstat()
    except OSError as error:
        raise LocatorError(f"unable to stat search root: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISDIR(info.st_mode):
        raise LocatorError("search root must be a non-symlink directory")
    resolved = path.resolve()
    if resolved == ROOT or ROOT in resolved.parents:
        raise LocatorError("search root must stay outside the repository")
    return resolved


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def reviewed_hashes(kind: str, protocol: str | None) -> dict[str, str]:
    if kind == "millennium-dos":
        runner = load_runner("run_millennium_dos_capture")
        if protocol:
            if protocol not in runner.RECORDER_PROTOCOLS:
                raise LocatorError("recorder protocol is not in the reviewed finite set")
            return {protocol: runner.RECORDER_PROTOCOLS[protocol][1]}
        return {name: value[1] for name, value in sorted(runner.RECORDER_PROTOCOLS.items())}
    if protocol:
        raise LocatorError("--recorder-protocol is valid only for millennium-dos")
    runner = load_runner("run_deuteros_amiga_capture")
    return {"reviewed-fs-uae": runner.EXPECTED_RECORDER_SHA256}


def is_executable_candidate(path: Path, info: os.stat_result) -> bool:
    """Apply the host's executable-file convention without trusting content.

    POSIX executes regular files through mode bits. Windows does not preserve
    those bits, and uses executable suffixes instead; requiring ``S_IXUSR``
    there silently excludes every reviewed ``.exe`` recorder.
    """
    if os.name == "nt":
        executable_suffixes = {suffix.lower() for suffix in
                               os.environ.get("PATHEXT", ".COM;.EXE;.BAT;.CMD").split(";")
                               if suffix}
        return path.suffix.lower() in executable_suffixes
    return bool(info.st_mode & stat.S_IXUSR)


def iter_candidates(roots: list[Path], max_files: int):
    visited = 0
    pending = list(reversed(roots))
    while pending:
        directory = pending.pop()
        try:
            children = sorted(os.scandir(directory), key=lambda item: item.name, reverse=True)
        except OSError:
            continue
        for child in children:
            try:
                info = child.stat(follow_symlinks=False)
            except OSError:
                continue
            if stat.S_ISLNK(info.st_mode):
                continue
            path = Path(child.path)
            if stat.S_ISDIR(info.st_mode):
                pending.append(path)
                continue
            if not stat.S_ISREG(info.st_mode) or not is_executable_candidate(path, info):
                continue
            visited += 1
            if visited > max_files:
                raise LocatorError(f"search exceeded the {max_files}-file safety cap")
            if 0 < info.st_size <= MAX_CANDIDATE_BYTES:
                yield path, info.st_size


def locate(kind: str, roots: list[Path], protocol: str | None, max_files: int) -> list[dict[str, object]]:
    expected = reviewed_hashes(kind, protocol)
    matches: list[dict[str, object]] = []
    for path, size in iter_candidates(roots, max_files):
        digest = sha256_file(path)
        for name, reviewed_digest in expected.items():
            if digest == reviewed_digest:
                matches.append({"protocol": name, "sha256": digest, "bytes": size, "path": str(path)})
    return matches


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", choices=("millennium-dos", "deuteros-amiga"), required=True)
    parser.add_argument("--root", type=Path, action="append", required=True,
                        help="Absolute external directory to search; repeat as needed")
    parser.add_argument("--recorder-protocol", help="Reviewed Millennium DOS recorder protocol")
    parser.add_argument("--max-files", type=int, default=DEFAULT_MAX_FILES,
                        help=f"Maximum executable candidates to hash (1-{DEFAULT_MAX_FILES}; default: {DEFAULT_MAX_FILES})")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable match records")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_arguments(sys.argv[1:] if argv is None else argv)
    try:
        if not 1 <= args.max_files <= DEFAULT_MAX_FILES:
            raise LocatorError(f"max-files must be between 1 and {DEFAULT_MAX_FILES}")
        roots = [require_root(path) for path in args.root]
        matches = locate(args.kind, roots, args.recorder_protocol, args.max_files)
    except LocatorError as error:
        print(f"RECORDER LOCATOR REJECTED  {error}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps({"schema": "project-eon.recorder-locator/v1", "kind": args.kind,
                          "matches": matches}, sort_keys=True))
    elif matches:
        for match in matches:
            print(f"REVIEWED RECORDER FOUND  {match['protocol']}  {match['sha256']}  {match['path']}")
    else:
        print("REVIEWED RECORDER NOT FOUND  no supplied executable matched a pinned recorder hash")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
