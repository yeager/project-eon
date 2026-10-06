#!/usr/bin/env python3
"""Scan hash-verified flat DOS images for direct relative control-flow targets.

The linear decode is deliberately code/data-unclassified. Results are leads,
not a control-flow graph or proof that any instruction is reachable.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import stat
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, CS_GRP_CALL, CS_GRP_JUMP, Cs
from capstone.x86_const import X86_OP_IMM

try:
    from .analyze_dos import read_verified_direct_file, verify_direct_directory
except ImportError:  # Direct script invocation from the tools directory.
    from analyze_dos import read_verified_direct_file, verify_direct_directory


ROOT = Path(__file__).resolve().parents[1]
DOS_BASE = 0x100
MEMBER_IDENTITY = re.compile(r"([^/\\=]+)=([0-9a-f]{64})\Z")


class ScanError(ValueError):
    """Reject unsafe paths or unverified source media."""


def scan_member(name: str, digest: str, data: bytes, targets: set[int]) -> list[dict[str, object]]:
    """Return direct relative CALL/JMP candidate instructions on linear starts."""
    if not targets or any(target < 0 or target > 0xffff for target in targets):
        raise ScanError("target set must contain 16-bit instruction pointers")
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    decoder.detail = True
    candidates: list[dict[str, object]] = []
    cursor = 0
    while cursor < len(data):
        decoded = next(decoder.disasm(data[cursor:cursor + 15], DOS_BASE + cursor, count=1), None)
        if decoded is None or decoded.address != DOS_BASE + cursor or decoded.size <= 0:
            cursor += 1
            continue
        if decoded.group(CS_GRP_CALL) or decoded.group(CS_GRP_JUMP):
            for operand in decoded.operands:
                if operand.type == X86_OP_IMM and (operand.imm & 0xffff) in targets:
                    candidates.append({
                        "image": name,
                        "image_sha256": digest,
                        "file_offset": cursor,
                        "instruction_ip": (DOS_BASE + cursor) & 0xffff,
                        "mnemonic": decoded.mnemonic,
                        "target_ip": operand.imm & 0xffff,
                        "bytes_hex": decoded.bytes.hex(),
                    })
                    break
        cursor += decoded.size
    return candidates


def external_output(path: Path) -> Path:
    if not path.is_absolute() or path.exists() or path.is_symlink():
        raise ScanError("output must be a new absolute path")
    normalized = Path(os.path.normpath(str(path)))
    if (normalized == Path("/tmp") or Path("/tmp") in normalized.parents
            or normalized == Path("/private/tmp")
            or Path("/private/tmp") in normalized.parents):
        raise ScanError("output must not use /tmp")
    resolved = path.parent.resolve(strict=True) / path.name
    if resolved == ROOT or ROOT in resolved.parents:
        raise ScanError("output must stay outside the repository")
    return resolved


def parse_member_identities(values: list[str]) -> dict[str, str]:
    result: dict[str, str] = {}
    for value in values:
        match = MEMBER_IDENTITY.fullmatch(value)
        if not match:
            raise ScanError("members must use NAME=lowercase-sha256")
        name, digest = match.groups()
        if name in result:
            raise ScanError("duplicate member name")
        result[name] = digest
    if not result:
        raise ScanError("at least one exact member is required")
    return result


def run(args: argparse.Namespace) -> Path:
    output = external_output(args.output)
    media = verify_direct_directory(args.media_root, args.set_sha256)
    cache_root = (Path.home() / ".cache" / "project-eon-tools").resolve(strict=True)
    if cache_root not in output.parents:
        raise ScanError("output must stay under the external Project Eon cache")
    if output == media or media in output.parents:
        raise ScanError("output must stay outside supplied original media")
    members = parse_member_identities(args.member)
    targets = set(args.target_ip)
    if not targets or any(target < 0 or target > 0xffff for target in targets):
        raise ScanError("supply one or more 16-bit --target-ip values")
    candidates: list[dict[str, object]] = []
    for name, digest in members.items():
        data = read_verified_direct_file(media / name, digest)
        candidates.extend(scan_member(name, digest, data, targets))
    descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "w", encoding="ascii", newline="\n") as stream:
        stream.write("# schema=project-eon.dos-transfer-targets/v1\n")
        stream.write(f"# direct_media_set_sha256={args.set_sha256}\n")
        stream.write(f"# image_base=0x{DOS_BASE:04x}\n")
        stream.write("# decode=linear-instruction-starts; code-data-unclassified\n")
        stream.write("# target_ips=" + ",".join(f"0x{value:04x}" for value in sorted(targets)) + "\n")
        for name, digest in members.items():
            stream.write(f"# member={name};sha256={digest}\n")
        stream.write("image\timage_sha256\tfile_offset\tinstruction_ip\t"
                     "mnemonic\ttarget_ip\tbytes_hex\n")
        for row in candidates:
            stream.write(f"{row['image']}\t{row['image_sha256']}\t"
                         f"{row['file_offset']:04x}\t{row['instruction_ip']:04x}\t"
                         f"{row['mnemonic']}\t{row['target_ip']:04x}\t{row['bytes_hex']}\n")
        stream.flush()
        os.fsync(stream.fileno())
    if stat.S_IMODE(output.stat().st_mode) != 0o600:
        output.chmod(0o600)
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--media-root", type=Path, required=True)
    parser.add_argument("--set-sha256", required=True)
    parser.add_argument("--member", action="append", default=[], metavar="NAME=SHA256")
    parser.add_argument("--target-ip", action="append", default=[], type=lambda value: int(value, 0))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = run(args)
    except (OSError, ScanError, ValueError) as error:
        print(f"DOS TRANSFER SCAN REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"DOS TRANSFER SCAN COMPLETE  external metadata: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
