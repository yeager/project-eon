#!/usr/bin/env python3
"""Build a bounded, source-only 16-bit DOS control-flow candidate graph.

This is a reachability aid, not a disassembler proof: code/data is unclassified,
calls and interrupts may return, and computed transfers stop at opaque edges.
The report must remain in the external Project Eon analysis cache.
"""

from __future__ import annotations

import argparse
from collections import deque
import os
from pathlib import Path
import stat
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, CS_GRP_CALL, CS_GRP_JUMP, CS_GRP_RET, Cs
from capstone.x86_const import X86_OP_IMM

try:
    from .analyze_dos import read_verified_direct_file, verify_direct_directory
except ImportError:  # Direct script invocation from the tools directory.
    from analyze_dos import read_verified_direct_file, verify_direct_directory


ROOT = Path(__file__).resolve().parents[1]
DOS_BASE = 0x100
DEFAULT_LIMIT = 30_000


class ReachabilityError(ValueError):
    """Reject invalid entry points, bounds, or output locations."""


def _file_offset(ip: int, image_size: int) -> int | None:
    offset = ip - DOS_BASE
    return offset if 0 <= offset < image_size else None


def analyze_reachable(data: bytes, entry_ip: int,
                      instruction_limit: int = DEFAULT_LIMIT) -> dict[str, object]:
    """Explore linear instruction starts from a flat COM entry point.

    Both outcomes of conditional branches are retained. Direct in-image calls
    contribute a callee edge and an explicitly labelled return assumption.
    INT instructions likewise have a labelled possible-return edge. Indirect
    jumps terminate a path; indirect calls retain only an opaque return edge.
    """
    if not data or not 0 <= entry_ip <= 0xffff:
        raise ReachabilityError("image and 16-bit entry IP are required")
    if instruction_limit <= 0:
        raise ReachabilityError("instruction limit must be positive")

    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    decoder.detail = True
    pending = deque([entry_ip])
    queued = {entry_ip}
    instructions: dict[int, dict[str, object]] = {}
    edges: list[dict[str, object]] = []
    stops: list[dict[str, object]] = []

    def add_edge(source: int, target: int, kind: str) -> None:
        offset = _file_offset(target, len(data))
        edges.append({"source_ip": source, "target_ip": target,
                      "kind": kind, "in_image": offset is not None})
        if offset is not None and target not in queued:
            queued.add(target)
            pending.append(target)

    while pending:
        if len(instructions) >= instruction_limit:
            raise ReachabilityError("instruction limit reached before graph completed")
        ip = pending.popleft()
        offset = _file_offset(ip, len(data))
        if offset is None or ip in instructions:
            continue
        instruction = next(decoder.disasm(data[offset:offset + 15], ip, count=1), None)
        if (instruction is None or instruction.address != ip or instruction.size <= 0
                or instruction.size > len(data) - offset):
            stops.append({"ip": ip, "reason": "undecodable-or-truncated"})
            continue

        next_ip = (ip + instruction.size) & 0xffff
        row: dict[str, object] = {
            "ip": ip,
            "file_offset": offset,
            "mnemonic": instruction.mnemonic,
            "operands": instruction.op_str,
            "bytes_hex": instruction.bytes.hex(),
        }
        instructions[ip] = row

        if instruction.group(CS_GRP_RET):
            stops.append({"ip": ip, "reason": "return-no-caller-context"})
            continue
        if instruction.group(CS_GRP_CALL):
            immediate = next((operand.imm & 0xffff for operand in instruction.operands
                               if operand.type == X86_OP_IMM), None)
            if (instruction.mnemonic == "call" and immediate is not None
                    and _file_offset(immediate, len(data)) is not None):
                add_edge(ip, immediate, "direct-call-entry")
                add_edge(ip, next_ip, "direct-call-return-assumption")
            else:
                add_edge(ip, next_ip, "opaque-call-return-assumption")
                stops.append({"ip": ip, "reason": "opaque-call-target"})
            continue
        if instruction.group(CS_GRP_JUMP):
            immediate = next((operand.imm & 0xffff for operand in instruction.operands
                               if operand.type == X86_OP_IMM), None)
            if instruction.mnemonic in {"jmp", "ljmp"}:
                if instruction.mnemonic == "ljmp":
                    stops.append({"ip": ip, "reason": "opaque-far-jump"})
                    continue
                if immediate is None:
                    stops.append({"ip": ip, "reason": "opaque-indirect-jump"})
                elif _file_offset(immediate, len(data)) is None:
                    stops.append({"ip": ip, "reason": "direct-jump-outside-image",
                                  "target_ip": immediate})
                else:
                    add_edge(ip, immediate, "direct-jump")
            else:
                if immediate is not None:
                    add_edge(ip, immediate, "conditional-taken")
                else:
                    stops.append({"ip": ip, "reason": "opaque-conditional-target"})
                add_edge(ip, next_ip, "conditional-fallthrough")
            continue
        if instruction.mnemonic == "int":
            add_edge(ip, next_ip, "interrupt-return-assumption")
            continue
        if instruction.mnemonic in {"hlt", "ud2"}:
            stops.append({"ip": ip, "reason": "terminal-instruction"})
            continue
        add_edge(ip, next_ip, "fallthrough")

    outgoing: dict[int, list[dict[str, object]]] = {}
    for edge in edges:
        outgoing.setdefault(int(edge["source_ip"]), []).append(edge)

    def route_to(target: int, allow_assumptions: bool = True) -> list[dict[str, object]] | None:
        queue = deque([entry_ip])
        parents: dict[int, tuple[int, dict[str, object]] | None] = {entry_ip: None}
        while queue:
            current = queue.popleft()
            if current == target:
                route: list[dict[str, object]] = []
                cursor = current
                while parents[cursor] is not None:
                    previous, edge = parents[cursor]  # type: ignore[misc]
                    route.append(edge)
                    cursor = previous
                route.reverse()
                return route
            for edge in outgoing.get(current, []):
                if (not allow_assumptions
                        and str(edge["kind"]).endswith("assumption")):
                    continue
                destination = int(edge["target_ip"])
                if edge["in_image"] and destination not in parents:
                    parents[destination] = (current, edge)
                    queue.append(destination)
        return None

    return {
        "entry_ip": entry_ip,
        "instruction_count": len(instructions),
        "instructions": instructions,
        "edges": edges,
        "stops": stops,
        "route_to": route_to,
    }


def external_output(path: Path) -> Path:
    if not path.is_absolute() or path.exists() or path.is_symlink():
        raise ReachabilityError("output must be a new absolute path")
    normalized = Path(os.path.normpath(str(path)))
    if (normalized == Path("/tmp") or Path("/tmp") in normalized.parents
            or normalized == Path("/private/tmp")
            or Path("/private/tmp") in normalized.parents):
        raise ReachabilityError("output must not use /tmp")
    resolved = path.parent.resolve(strict=True) / path.name
    if resolved == ROOT or ROOT in resolved.parents:
        raise ReachabilityError("output must stay outside the repository")
    cache_root = (Path.home() / ".cache" / "project-eon-tools").resolve(strict=True)
    if cache_root not in resolved.parents:
        raise ReachabilityError("output must stay under the external Project Eon cache")
    return resolved


def run(args: argparse.Namespace) -> Path:
    output = external_output(args.output)
    media = verify_direct_directory(args.media_root, args.set_sha256)
    data = read_verified_direct_file(media / args.member, args.member_sha256)
    result = analyze_reachable(data, args.entry_ip, args.instruction_limit)
    targets = sorted(set(args.target_ip))
    if not targets or any(not 0 <= target <= 0xffff for target in targets):
        raise ReachabilityError("supply one or more 16-bit --target-ip values")

    digest = args.member_sha256
    descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "w", encoding="ascii", newline="\n") as stream:
        stream.write("# schema=project-eon.dos-reachability/v1\n")
        stream.write(f"# direct_media_set_sha256={args.set_sha256}\n")
        stream.write(f"# image={args.member};sha256={digest};base=0x{DOS_BASE:04x}\n")
        stream.write("# analysis=worklist-CFG-candidate;code-data-unclassified\n")
        stream.write("# edges=conditional-both-ways;near-calls-plus-return-assumption;INT-return-assumption;indirect-and-far-jumps-stop\n")
        stream.write(f"# entry_ip=0x{args.entry_ip:04x};instructions={result['instruction_count']}\n")
        for target in targets:
            route_fn = result["route_to"]
            direct_route = route_fn(target, False)  # type: ignore[operator]
            over_route = route_fn(target, True)  # type: ignore[operator]
            if direct_route is not None:
                route = direct_route
                path_class = "potential-direct-control-only"
            elif over_route is not None:
                route = over_route
                path_class = "potential-only-with-return-assumptions"
            else:
                stream.write(f"target\t{target:04x}\tunreached-in-overapprox\n")
                continue
            counts: dict[str, int] = {}
            for edge in route:
                kind = str(edge["kind"])
                if kind.endswith("assumption"):
                    counts[kind] = counts.get(kind, 0) + 1
            assumptions = ",".join(f"{name}:{count}" for name, count in sorted(counts.items())) or "none"
            opaque = any(str(edge["kind"]).startswith("opaque")
                         or edge["kind"] == "interrupt-return-assumption"
                         for edge in route)
            stream.write(f"target\t{target:04x}\t{path_class}\t"
                         f"opaque-or-interrupt-boundary={str(opaque).lower()}\t"
                         f"assumptions={assumptions}\n")
            control_edges = [edge for edge in route if edge["kind"] != "fallthrough"]
            stream.write("route-control-edges\t" + " ".join(
                f"{edge['source_ip']:04x}-{edge['kind']}->{edge['target_ip']:04x}"
                for edge in control_edges) + "\n")
        stream.write("ip\tfile_offset\tmnemonic\toperands\tbytes_hex\n")
        for ip, row in sorted(result["instructions"].items()):
            stream.write(f"{ip:04x}\t{row['file_offset']:04x}\t{row['mnemonic']}\t"
                         f"{row['operands']}\t{row['bytes_hex']}\n")
        stream.write("source_ip\ttarget_ip\tedge_kind\tin_image\n")
        for edge in result["edges"]:
            stream.write(f"{edge['source_ip']:04x}\t{edge['target_ip']:04x}\t"
                         f"{edge['kind']}\t{str(edge['in_image']).lower()}\n")
        stream.write("stop_ip\treason\toptional_target_ip\n")
        for stop in result["stops"]:
            target = stop.get("target_ip")
            target_text = f"{target:04x}" if target is not None else "-"
            stream.write(f"{stop['ip']:04x}\t{stop['reason']}\t{target_text}")
            stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    if stat.S_IMODE(output.stat().st_mode) != 0o600:
        output.chmod(0o600)
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--media-root", type=Path, required=True)
    parser.add_argument("--set-sha256", required=True)
    parser.add_argument("--member", required=True)
    parser.add_argument("--member-sha256", required=True)
    parser.add_argument("--entry-ip", type=lambda value: int(value, 0), default=0x100)
    parser.add_argument("--target-ip", action="append", type=lambda value: int(value, 0), default=[])
    parser.add_argument("--instruction-limit", type=int, default=DEFAULT_LIMIT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = run(args)
    except (OSError, ReachabilityError, ValueError) as error:
        print(f"DOS REACHABILITY SCAN REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"DOS REACHABILITY SCAN COMPLETE  external report: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
