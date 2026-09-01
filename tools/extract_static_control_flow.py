#!/usr/bin/env python3
"""Extract hash-bound, direct static control-flow candidates from original media.

This tool is deliberately narrower than a disassembler.  It reads explicitly
named, user-supplied archive members in memory and writes a small external
JSON sidecar.  Every emitted edge is still a linear-decode candidate: it does
not prove that its source is code, that it is reachable, or that an external
call, interrupt, return, or branch has any particular result.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from io import BytesIO
from pathlib import Path
import stat
import sys
from zipfile import ZipFile

# This tool is executed directly from arbitrary working directories as well as
# imported by tests, so resolve its sibling helper from the tool directory.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_dos import read_fat12_members
from analyze_atari_st_prg import PRG_HEADER_BYTES, read_be16, read_be32, read_exact_program


ROOT = Path(__file__).resolve().parents[1]
CLASSIFICATION = "static-candidate-unclassified"


class ControlFlowError(ValueError):
    """Input provenance or sidecar destination is unsafe."""


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _require_sha256(value: str, label: str) -> None:
    if len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
        raise ControlFlowError(f"{label} must be a lower-case SHA-256")


def _require_external_output(path: Path) -> Path:
    if not path.is_absolute() or path.as_posix() == "/tmp" or path.as_posix().startswith("/tmp/"):
        raise ControlFlowError("output must be absolute and outside /tmp")
    resolved_parent = path.parent.resolve(strict=True)
    if resolved_parent == ROOT or ROOT in resolved_parent.parents:
        raise ControlFlowError("output must be outside the repository")
    try:
        info = path.lstat()
    except FileNotFoundError:
        return resolved_parent / path.name
    except OSError as error:
        raise ControlFlowError(f"unable to inspect output: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise ControlFlowError("output must be a new regular non-symlink path")
    raise ControlFlowError("output must not already exist")


def _read_zip_member(archive_path: Path, member: str, expected_archive_sha256: str) -> bytes:
    archive_bytes = archive_path.read_bytes()
    observed = _sha256(archive_bytes)
    if observed != expected_archive_sha256:
        raise ControlFlowError(f"archive SHA-256 mismatch: expected {expected_archive_sha256}, got {observed}")
    try:
        with ZipFile(BytesIO(archive_bytes)) as archive:
            return archive.read(member)
    except (KeyError, OSError, ValueError) as error:
        raise ControlFlowError(f"unable to read exact archive member {member!r}: {error}") from error


def _read_nested_zip_member(archive_path: Path, nested_member: str, member: str,
                            expected_archive_sha256: str) -> bytes:
    archive_bytes = archive_path.read_bytes()
    observed = _sha256(archive_bytes)
    if observed != expected_archive_sha256:
        raise ControlFlowError(f"archive SHA-256 mismatch: expected {expected_archive_sha256}, got {observed}")
    try:
        with ZipFile(BytesIO(archive_bytes)) as outer:
            nested = outer.read(nested_member)
        with ZipFile(BytesIO(nested)) as inner:
            return inner.read(member)
    except (KeyError, OSError, ValueError) as error:
        raise ControlFlowError(f"unable to read exact nested archive member: {error}") from error


def _verify_archive_sha256(archive_path: Path, expected_archive_sha256: str) -> None:
    observed = _sha256(archive_path.read_bytes())
    if observed != expected_archive_sha256:
        raise ControlFlowError(f"archive SHA-256 mismatch: expected {expected_archive_sha256}, got {observed}")


def _decode_one(decoder, data: bytes, offset: int, address: int):
    item = next(decoder.disasm(data[offset:], address + offset, count=1), None)
    if (item is None or item.address != address + offset or item.size <= 0
            or item.size > len(data) - offset):
        return None
    return item


def x86_direct_edges(data: bytes, *, source_offset: int, runtime_address: int) -> list[dict]:
    """Return direct 8086 CALL/JMP/Jcc/INT candidates without classifying code."""
    try:
        from capstone import CS_ARCH_X86, CS_MODE_16, Cs
        from capstone.x86_const import X86_OP_IMM
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    decoder.detail = True
    edges: list[dict] = []
    offset = 0
    while offset < len(data):
        item = _decode_one(decoder, data, offset, runtime_address)
        if item is None:
            offset += 1
            continue
        mnemonic = item.mnemonic.lower()
        kind = ("return" if mnemonic.startswith("ret") else "call" if mnemonic == "call"
                else "jump" if mnemonic == "jmp" else "conditional-jump" if mnemonic.startswith("j")
                else "interrupt" if mnemonic == "int" else None)
        if kind is not None:
            immediate = next((operand.imm for operand in item.operands if operand.type == X86_OP_IMM), None)
            if kind == "return":
                edges.append({"source_offset": source_offset + offset, "runtime_address": item.address,
                              "instruction_size": item.size, "kind": kind,
                              "classification": CLASSIFICATION, "target": "return-address-unproven"})
            elif kind == "interrupt" or immediate is not None:
                edge = {"source_offset": source_offset + offset,
                        "runtime_address": item.address,
                        "instruction_size": item.size,
                        "kind": kind,
                        "classification": CLASSIFICATION}
                if kind == "interrupt":
                    edge["interrupt_vector"] = immediate
                else:
                    edge["target_runtime_address"] = immediate
                edges.append(edge)
        offset += item.size
    return edges


def m68k_direct_edges(data: bytes, *, source_offset: int, runtime_address: int) -> list[dict]:
    """Return direct M68000 call/branch/trap/RTS candidates without execution claims."""
    try:
        from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000, Cs
        from capstone.m68k_const import (M68K_AM_ABSOLUTE_DATA_LONG, M68K_AM_ABSOLUTE_DATA_SHORT,
                                         M68K_AM_BRANCH_DISPLACEMENT,
                                         M68K_OP_IMM, M68K_OP_MEM, M68K_OP_BR_DISP)
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    decoder.detail = True
    edges: list[dict] = []
    offset = 0
    while offset < len(data):
        item = _decode_one(decoder, data, offset, runtime_address)
        if item is None:
            offset += 1
            continue
        mnemonic = item.mnemonic.lower().split(".", 1)[0]
        kind = ("return" if mnemonic == "rts" else "call" if mnemonic in {"jsr", "bsr"}
                else "jump" if mnemonic in {"bra", "jmp"} else "conditional-jump" if mnemonic.startswith("b")
                and mnemonic not in {"bra", "bsr"} else "trap" if mnemonic == "trap" else None)
        if kind is not None:
            immediate = next((operand.imm for operand in item.operands if operand.type == M68K_OP_IMM), None)
            target = None
            if item.operands:
                operand = item.operands[0]
                if operand.type == M68K_OP_BR_DISP and operand.address_mode == M68K_AM_BRANCH_DISPLACEMENT:
                    target = item.address + 2 + operand.br_disp.disp
                elif (operand.type == M68K_OP_MEM and operand.address_mode
                      in {M68K_AM_ABSOLUTE_DATA_SHORT, M68K_AM_ABSOLUTE_DATA_LONG}):
                    target = operand.imm
            if kind == "return":
                edges.append({"source_offset": source_offset + offset,
                              "runtime_address": item.address,
                              "instruction_size": item.size,
                              "kind": kind,
                              "classification": CLASSIFICATION,
                              "target": "return-address-unproven"})
            elif kind == "trap" or target is not None or immediate is not None:
                edge = {"source_offset": source_offset + offset,
                        "runtime_address": item.address,
                        "instruction_size": item.size,
                        "kind": kind,
                        "classification": CLASSIFICATION}
                if kind == "trap":
                    edge["trap_vector"] = immediate
                else:
                    edge["target_runtime_address"] = target if target is not None else immediate
                edges.append(edge)
        offset += item.size
    return edges


def parse_range(value: str) -> tuple[int, int, int, str]:
    try:
        raw_offset, raw_length, raw_address, digest = value.split(":")
        offset, length, address = int(raw_offset, 0), int(raw_length, 0), int(raw_address, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError("range must be OFFSET:LENGTH:RUNTIME_ADDRESS:SHA256") from error
    if offset < 0 or length <= 0 or address < 0:
        raise argparse.ArgumentTypeError("range offset/address must be non-negative and length positive")
    try:
        _require_sha256(digest, "range SHA-256")
    except ControlFlowError as error:
        raise argparse.ArgumentTypeError(str(error)) from error
    return offset, length, address, digest


def build_sidecar(cpu: str, archive_sha256: str, source_label: str, source: bytes,
                  ranges: list[tuple[int, int, int, str]], *, source_kind: str = "archive-member",
                  address_space: str = "runtime", container_sha256: str | None = None) -> dict:
    if address_space not in {"runtime", "image-relative-unrelocated"}:
        raise ControlFlowError("unsupported control-flow address space")
    address_key = "runtime_address" if address_space == "runtime" else "image_relative_address"
    target_key = "target_runtime_address" if address_space == "runtime" else "target_image_relative_address"
    records: list[dict] = []
    extractor = x86_direct_edges if cpu == "i8086" else m68k_direct_edges
    for offset, length, address, expected_digest in ranges:
        if offset > len(source) - length:
            raise ControlFlowError("requested range is outside the exact source member")
        span = source[offset:offset + length]
        observed_digest = _sha256(span)
        if observed_digest != expected_digest:
            raise ControlFlowError(f"range SHA-256 mismatch at +0x{offset:x}: expected {expected_digest}, got {observed_digest}")
        edges = extractor(span, source_offset=offset, runtime_address=address)
        if address_space != "runtime":
            for edge in edges:
                edge[address_key] = edge.pop("runtime_address")
                if "target_runtime_address" in edge:
                    edge[target_key] = edge.pop("target_runtime_address")
        records.append({"source_offset": offset, "length": length, address_key: address,
                        "sha256": observed_digest, "edges": edges})
    declared = [(record[address_key], record[address_key] + record["length"])
                for record in records]
    for record in records:
        for edge in record["edges"]:
            target = edge.get(target_key)
            if target is not None:
                edge["target_scope"] = ("within-declared-range" if any(start <= target < end
                                         for start, end in declared) else "outside-declared-range")
    return {"schema": "project-eon.static-control-flow/v1", "cpu": cpu,
            "archive_sha256": archive_sha256, "source": source_label, "source_kind": source_kind,
            "source_sha256": _sha256(source), "classification": CLASSIFICATION,
            "address_space": address_space, "ranges": records,
            **({"container_sha256": container_sha256} if container_sha256 is not None else {})}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--dos-archive", type=Path, help="Original DOS ZIP containing exact --member entries")
    source.add_argument("--fat12-archive", type=Path,
                        help="Original ZIP containing one exact FAT12 --fat12-member image")
    source.add_argument("--amiga-archive", "--m68k-archive", dest="m68k_archive", type=Path,
                        help="Original outer ZIP containing one exact nested M68000 disk image")
    source.add_argument("--atari-prg-archive", type=Path,
                        help="Original outer ZIP containing exact nested FAT12 Atari ST PRG media")
    parser.add_argument("--archive-sha256", required=True, help="Expected lower-case SHA-256 of the outer ZIP")
    parser.add_argument("--source-sha256", help="Expected lower-case SHA-256 of the exact disk/image source")
    parser.add_argument("--member", action="append", default=[], help="Exact DOS member; repeat as needed")
    parser.add_argument("--fat12-member", help="Exact FAT12 image member inside --fat12-archive")
    parser.add_argument("--nested-member", help="Exact nested ZIP member for --amiga-archive")
    parser.add_argument("--adf-member", "--disk-member", dest="disk_member",
                        help="Exact disk member inside --nested-member")
    parser.add_argument("--program", help="Exact FAT12 root PRG member for --atari-prg-archive")
    parser.add_argument("--program-sha256", help="Expected lower-case SHA-256 of --program")
    parser.add_argument("--range", action="append", type=parse_range, default=[],
                        help="OFFSET:LENGTH:RUNTIME_ADDRESS:SHA256; required for Amiga")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        _require_sha256(args.archive_sha256, "archive SHA-256")
        output = _require_external_output(args.output)
        documents: list[dict] = []
        if args.dos_archive is not None:
            if not args.member or args.range or args.nested_member or args.disk_member or args.fat12_member or args.source_sha256:
                raise ControlFlowError("DOS mode requires --member entries only; ranges are inferred from exact members")
            for member in args.member:
                media = _read_zip_member(args.dos_archive, member, args.archive_sha256)
                documents.append(build_sidecar("i8086", args.archive_sha256, member, media,
                                               [(0, len(media), 0x100, _sha256(media))]))
        elif args.fat12_archive is not None:
            if (not args.fat12_member or not args.member or args.range or args.nested_member
                    or args.disk_member or not args.source_sha256):
                raise ControlFlowError("FAT12 mode requires --fat12-member, --source-sha256 and exact --member entries")
            _require_sha256(args.source_sha256, "FAT12 image SHA-256")
            image = _read_zip_member(args.fat12_archive, args.fat12_member, args.archive_sha256)
            if _sha256(image) != args.source_sha256:
                raise ControlFlowError("FAT12 image SHA-256 mismatch")
            for member, media in read_fat12_members(image, args.member):
                documents.append(build_sidecar("i8086", args.archive_sha256,
                                               f"{args.fat12_member}:{member}", media,
                                               [(0, len(media), 0x100, _sha256(media))],
                                               source_kind="fat12-root-member"))
        elif args.m68k_archive is not None:
            if (not args.nested_member or not args.disk_member or not args.range or args.member
                    or args.fat12_member or not args.source_sha256):
                raise ControlFlowError("M68000 mode requires --nested-member, --disk-member, --source-sha256 and one or more --range values")
            _require_sha256(args.source_sha256, "disk SHA-256")
            media = _read_nested_zip_member(args.m68k_archive, args.nested_member, args.disk_member,
                                            args.archive_sha256)
            if _sha256(media) != args.source_sha256:
                raise ControlFlowError("disk SHA-256 mismatch")
            documents.append(build_sidecar("m68000", args.archive_sha256,
                                           f"{args.nested_member}!{args.disk_member}", media, args.range,
                                           source_kind="nested-disk-range"))
        else:
            if (not args.nested_member or not args.disk_member or not args.program or not args.program_sha256
                    or not args.source_sha256 or len(args.range) != 1 or args.member or args.fat12_member):
                raise ControlFlowError("Atari PRG mode requires exact nested/disk/program hashes and one range")
            _require_sha256(args.source_sha256, "disk SHA-256")
            _require_sha256(args.program_sha256, "program SHA-256")
            _verify_archive_sha256(args.atari_prg_archive, args.archive_sha256)
            disk, program = read_exact_program(args.atari_prg_archive, args.nested_member,
                                               args.disk_member, args.program)
            if _sha256(disk) != args.source_sha256 or _sha256(program) != args.program_sha256:
                raise ControlFlowError("Atari ST disk or PRG SHA-256 mismatch")
            if len(program) < PRG_HEADER_BYTES or read_be16(program, 0) != 0x601a:
                raise ControlFlowError("unsupported Atari ST PRG header")
            loadable = read_be32(program, 2) + read_be32(program, 6)
            symbols = read_be32(program, 14)
            if PRG_HEADER_BYTES + loadable + symbols > len(program):
                raise ControlFlowError("Atari ST PRG loadable range is outside the exact program")
            offset, length, _, _ = args.range[0]
            if offset < PRG_HEADER_BYTES or offset > PRG_HEADER_BYTES + loadable - length:
                raise ControlFlowError("Atari ST control-flow range must stay within PRG TEXT+DATA")
            documents.append(build_sidecar("m68000", args.archive_sha256,
                                           f"{args.nested_member}!{args.disk_member}:{args.program}", program,
                                           args.range, source_kind="nested-fat12-root-prg-text-data",
                                           address_space="image-relative-unrelocated",
                                           container_sha256=_sha256(disk)))
        payload = {"schema": "project-eon.static-control-flow-set/v1", "classification": CLASSIFICATION,
                   "documents": documents}
        output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (ControlFlowError, OSError) as error:
        print(f"STATIC CONTROL FLOW REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"STATIC CONTROL FLOW WRITTEN  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
