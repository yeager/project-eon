#!/usr/bin/env python3
"""Bounded call-stack-aware static reachability for English DOS TITLES.EXE.

This companion analysis matches near CALL/RET pairs and stops at software
interrupts and unresolved transfers. It does not classify code versus data or
model DOS, BIOS, private handlers, registers, or runtime behavior. Reports must
remain under the external Project Eon analysis cache.
"""

from __future__ import annotations

import argparse
from collections import Counter, deque
import hashlib
import os
from pathlib import Path
import stat
import sys
from typing import Optional

from capstone import CS_ARCH_X86, CS_MODE_16, CS_GRP_CALL, CS_GRP_JUMP, CS_GRP_RET, Cs
from capstone.x86_const import (
    X86_OP_IMM, X86_OP_MEM, X86_OP_REG, X86_REG_AH, X86_REG_AL, X86_REG_AX,
    X86_REG_CS, X86_REG_DS, X86_REG_SI, X86_REG_SP, X86_REG_SS,
)

try:
    from .analyze_dos import read_verified_direct_file, verify_direct_directory
except ImportError:  # Direct script invocation from the tools directory.
    from analyze_dos import read_verified_direct_file, verify_direct_directory


ROOT = Path(__file__).resolve().parents[1]
CACHE_ROOT = Path.home() / ".cache" / "project-eon-tools"
DIRECT_MEDIA_SET_SHA256 = "d938cd6a611a83897a745b257a371613b73a7dddffb2d336ec2167a192803783"
TITLES_SHA256 = "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"
TITLES_SIZE = 7_022
DOS_BASE = 0x0100
ENTRY_IP = 0x1B80
TARGETS = (0x0134, 0x125C)
DEFAULT_STATE_LIMIT = 100_000
DEFAULT_CALL_DEPTH = 64


class ContextReachabilityError(ValueError):
    """Reject malformed inputs, exceeded bounds, or unsafe output locations."""


Frame = tuple[int, int]  # return IP and expected SP delta at the callee's RET
State = tuple[int, tuple[Frame, ...], int, Optional[int], Optional[int],
              Optional[bool], int, Optional[bool], Optional[bool], Optional[int],
              bool, Optional[bool], Optional[tuple[int, ...]], Optional[bool]]


def _memory_may_alias_return_word(
    operand, call_stack: tuple[Frame, ...], *, ss_equals_cs: bool | None,
    stack_pointer_base: int | None, ds_equals_cs: bool | None,
    si_values: tuple[int, ...] | None,
) -> bool:
    """Prove an aliased CS/DS operand disjoint from return words."""
    if not call_stack:
        return False
    memory = operand.mem
    if memory.base or memory.index:
        if (memory.base != X86_REG_SI or memory.index
                or memory.segment not in {0, X86_REG_DS}
                or ds_equals_cs is not True or si_values is None):
            return True
        if ss_equals_cs is None or stack_pointer_base is None:
            return True
        if not ss_equals_cs:
            return False
        access = {((value + memory.disp + index) & 0xFFFF)
                  for value in si_values for index in range(max(1, operand.size))}
    elif memory.segment == X86_REG_CS or (
            memory.segment in {0, X86_REG_DS} and ds_equals_cs is True):
        if ss_equals_cs is None or stack_pointer_base is None:
            return True
        if not ss_equals_cs:
            return False
        start = memory.disp & 0xFFFF
        access = {(start + index) & 0xFFFF for index in range(max(1, operand.size))}
    else:
        return True
    for _, return_delta in call_stack:
        return_start = (stack_pointer_base + return_delta) & 0xFFFF
        return_word = {return_start, (return_start + 1) & 0xFFFF}
        if access & return_word:
            return True
    return False


def _file_offset(ip: int, image_size: int) -> int | None:
    offset = ip - DOS_BASE
    return offset if 0 <= offset < image_size else None


def analyze_context_reachable(
    data: bytes,
    entry_ip: int = ENTRY_IP,
    *,
    state_limit: int = DEFAULT_STATE_LIMIT,
    call_depth_limit: int = DEFAULT_CALL_DEPTH,
    int91_return_ax_sequence: tuple[int, ...] = (),
    return_from_bios_int10_at_046d: bool = False,
    preserve_int91_wrapper_stack: bool = False,
    preserve_bios_ds_at_046d: bool = False,
    return_from_dos_int21_at_1b26: bool = False,
) -> dict[str, object]:
    """Explore candidate instruction paths with a matched near-call stack.

    Conditional branches fork both ways because flags are unknown. Near calls
    enter only exact in-image targets and push their return IP. A near RET may
    return only to that top frame with the expected SP-relative depth. Explicit
    PUSH/POP and immediate SP arithmetic update a bounded stack delta; unknown
    SP writes stop. INT instructions, far calls/returns/jumps, and indirect
    transfers stop at explicit opaque boundaries. The optional INT 91h AX
    sequence and BIOS INT 10h return are named scenarios at exact TITLES.EXE
    sites; neither is an emulator or runtime claim. The BIOS scenario assumes
    an IRET to $046f, restoration of the caller stack, and preservation of
    active near-call return words. All other tracked registers/flags and
    memory/device effects remain unknown or unmodeled.
    """
    if not data or not 0 <= entry_ip <= 0xFFFF:
        raise ContextReachabilityError("image and 16-bit entry IP are required")
    if state_limit <= 0 or call_depth_limit <= 0:
        raise ContextReachabilityError("state and call-depth limits must be positive")
    if (len(int91_return_ax_sequence) > 2
            or any(not 0 <= value <= 0xFFFF for value in int91_return_ax_sequence)):
        raise ContextReachabilityError("INT 91h scenario must contain at most two 16-bit AX values")
    int91_wrapper = bytes.fromhex("1e56575506cd91075d5f5e1fc3")
    if preserve_int91_wrapper_stack and not int91_return_ax_sequence:
        raise ContextReachabilityError(
            "INT 91h wrapper-stack scenario requires an INT 91h return scenario")
    if (preserve_int91_wrapper_stack
            and data[0x22:0x22 + len(int91_wrapper)] != int91_wrapper):
        raise ContextReachabilityError(
            "INT 91h wrapper-stack scenario requires the exact TITLES.EXE wrapper bytes")
    if preserve_bios_ds_at_046d and not return_from_bios_int10_at_046d:
        raise ContextReachabilityError(
            "BIOS DS-preservation scenario requires the exact INT 10h return scenario")

    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    decoder.detail = True
    start: State = (entry_ip, (), 0, None, None, None, 0, None, None, None,
                    False, None, None, None)
    pending: deque[State] = deque([start])
    parents: dict[State, tuple[State, str] | None] = {start: None}
    instructions: dict[int, dict[str, object]] = {}
    edges: list[tuple[int, int, str, int]] = []
    stops: list[dict[str, object]] = []
    first_blocked: dict[str, object] | None = None

    def enqueue(source: State, target_ip: int, stack: tuple[Frame, ...],
                stack_delta: int, al_constant: int | None, ah_constant: int | None,
                zf_constant: bool | None, int91_return_index: int,
                ax_is_cs: bool | None, ss_equals_cs: bool | None,
                stack_pointer_base: int | None, push_cs_pending: bool,
                ds_equals_cs: bool | None, si_values: tuple[int, ...] | None,
                direction_decrement: bool | None, kind: str) -> None:
        target_ip &= 0xFFFF
        target: State = (target_ip, stack, stack_delta, al_constant, ah_constant,
                         zf_constant, int91_return_index, ax_is_cs, ss_equals_cs,
                         stack_pointer_base, push_cs_pending, ds_equals_cs,
                         si_values, direction_decrement)
        edges.append((source[0], target_ip, kind, len(stack)))
        if _file_offset(target_ip, len(data)) is None:
            return
        if target not in parents:
            if len(parents) >= state_limit:
                raise ContextReachabilityError("context state limit reached before graph completed")
            parents[target] = (source, kind)
            pending.append(target)

    def block(state: State, reason: str, *, vector: int | None = None,
              target: int | None = None) -> None:
        nonlocal first_blocked
        row: dict[str, object] = {
            "ip": state[0], "reason": reason,
            "call_stack": [[frame[0], frame[1]] for frame in state[1]],
            "stack_delta": state[2],
            "ax_constant": ((state[4] << 8) | state[3]
                            if state[3] is not None and state[4] is not None else None),
            "al_constant": state[3],
            "ah_constant": state[4],
            "zf_constant": state[5],
            "int91_return_index": state[6],
            "ax_is_cs": state[7],
            "ss_equals_cs": state[8],
            "stack_pointer_base": state[9],
            "push_cs_pending": state[10],
            "ds_equals_cs": state[11],
            "si_values": state[12],
            "direction_decrement": state[13],
        }
        if vector is not None:
            row["interrupt_vector"] = vector
        if target is not None:
            row["target_ip"] = target
        stops.append(row)
        if first_blocked is None:
            first_blocked = row

    while pending:
        state = pending.popleft()
        (ip, call_stack, stack_delta, al_constant, ah_constant, zf_constant,
         int91_index, ax_is_cs, ss_equals_cs, stack_pointer_base,
         push_cs_pending, ds_equals_cs, si_values, direction_decrement) = state
        offset = _file_offset(ip, len(data))
        if offset is None:
            block(state, "target-outside-image")
            continue
        instruction = next(decoder.disasm(data[offset:offset + 15], ip, count=1), None)
        if (instruction is None or instruction.address != ip or instruction.size <= 0
                or instruction.size > len(data) - offset):
            block(state, "undecodable-or-truncated")
            continue

        next_ip = (ip + instruction.size) & 0xFFFF
        instructions.setdefault(ip, {
            "ip": ip, "file_offset": offset, "mnemonic": instruction.mnemonic,
            "operands": instruction.op_str, "bytes_hex": instruction.bytes.hex(),
        })
        if push_cs_pending and not (
                instruction.mnemonic == "pop" and instruction.operands
                and instruction.operands[0].type == X86_OP_REG
                and instruction.operands[0].reg == X86_REG_DS):
            push_cs_pending = False

        # A matching SP delta does not prove that the return word still holds
        # the pushed IP. Only a CS-absolute access proven disjoint from every
        # active return word may pass; no memory effect is otherwise modeled.
        if any(operand.type == X86_OP_MEM and _memory_may_alias_return_word(
                operand, call_stack, ss_equals_cs=ss_equals_cs,
                stack_pointer_base=stack_pointer_base,
                ds_equals_cs=ds_equals_cs, si_values=si_values)
               for operand in instruction.operands):
            block(state, "memory-operand-with-active-call-frame")
            continue

        if instruction.group(CS_GRP_RET):
            if instruction.mnemonic == "ret" and call_stack:
                return_ip, expected_sp_delta = call_stack[-1]
                if stack_delta != expected_sp_delta:
                    block(state, "near-ret-stack-delta-mismatch", target=return_ip)
                else:
                    cleanup = next((operand.imm & 0xFFFF
                                    for operand in instruction.operands
                                    if operand.type == X86_OP_IMM), 0)
                    enqueue(state, return_ip, call_stack[:-1],
                            stack_delta + 2 + cleanup, al_constant, ah_constant,
                            zf_constant, int91_index, ax_is_cs, ss_equals_cs,
                            stack_pointer_base, push_cs_pending, ds_equals_cs,
                            si_values, direction_decrement, "matched-near-ret")
            elif instruction.mnemonic == "ret":
                block(state, "near-ret-without-call-frame")
            else:
                block(state, "far-or-interrupt-return-opaque")
            continue

        if instruction.group(CS_GRP_CALL):
            immediate = next((operand.imm & 0xFFFF for operand in instruction.operands
                               if operand.type == X86_OP_IMM), None)
            if instruction.mnemonic != "call" or immediate is None:
                block(state, "far-or-indirect-call-opaque")
            elif _file_offset(immediate, len(data)) is None:
                block(state, "direct-call-target-outside-image", target=immediate)
            elif len(call_stack) >= call_depth_limit:
                block(state, "near-call-depth-limit", target=immediate)
            else:
                call_sp_delta = stack_delta - 2
                next_stack = call_stack + ((next_ip, call_sp_delta),)
                enqueue(state, immediate, next_stack, call_sp_delta,
                        al_constant, ah_constant, zf_constant, int91_index,
                        ax_is_cs, ss_equals_cs, stack_pointer_base, push_cs_pending,
                        ds_equals_cs, si_values, direction_decrement,
                        "near-call-entry")
            continue

        if instruction.group(CS_GRP_JUMP):
            immediate = next((operand.imm & 0xFFFF for operand in instruction.operands
                               if operand.type == X86_OP_IMM), None)
            if instruction.mnemonic in {"jmp", "ljmp"}:
                if instruction.mnemonic == "ljmp":
                    block(state, "far-jump-opaque", target=immediate)
                elif immediate is None:
                    block(state, "indirect-jump-opaque")
                elif _file_offset(immediate, len(data)) is None:
                    block(state, "direct-jump-target-outside-image", target=immediate)
                else:
                    enqueue(state, immediate, call_stack, stack_delta, al_constant,
                            ah_constant, zf_constant, int91_index, ax_is_cs,
                            ss_equals_cs, stack_pointer_base, push_cs_pending,
                            ds_equals_cs, si_values, direction_decrement,
                            "direct-near-jump")
            elif immediate is None:
                block(state, "conditional-target-opaque")
            else:
                known_zf_branch = instruction.mnemonic in {"je", "jz", "jne", "jnz"}
                if known_zf_branch and zf_constant is not None:
                    taken = zf_constant if instruction.mnemonic in {"je", "jz"} else not zf_constant
                    target = immediate if taken else next_ip
                    enqueue(state, target, call_stack, stack_delta, al_constant,
                            ah_constant, zf_constant, int91_index, ax_is_cs,
                            ss_equals_cs, stack_pointer_base, push_cs_pending,
                            ds_equals_cs, si_values, direction_decrement,
                            "conditional-taken-known-zf" if taken else "conditional-fallthrough-known-zf")
                else:
                    enqueue(state, immediate, call_stack, stack_delta, al_constant,
                            ah_constant, zf_constant, int91_index, ax_is_cs,
                            ss_equals_cs, stack_pointer_base, push_cs_pending,
                            ds_equals_cs, si_values, direction_decrement,
                            "conditional-taken-flags-unknown")
                    enqueue(state, next_ip, call_stack, stack_delta, al_constant,
                            ah_constant, zf_constant, int91_index, ax_is_cs,
                            ss_equals_cs, stack_pointer_base, push_cs_pending,
                            ds_equals_cs, si_values, direction_decrement,
                            "conditional-fallthrough-flags-unknown")
            continue

        if instruction.mnemonic == "int":
            vector = next((operand.imm & 0xFF for operand in instruction.operands
                           if operand.type == X86_OP_IMM), None)
            if (ip == 0x0127 and vector == 0x91
                    and int91_index < len(int91_return_ax_sequence)):
                observed_ax = int91_return_ax_sequence[int91_index]
                enqueue(state, next_ip, call_stack, stack_delta,
                        observed_ax & 0xFF, (observed_ax >> 8) & 0xFF,
                        None, int91_index + 1, None, ss_equals_cs,
                        stack_pointer_base, False, ds_equals_cs, si_values, None,
                        f"scenario-int91-return-ax-{observed_ax:04x}")
                continue
            if ip == 0x046D and vector == 0x10 and return_from_bios_int10_at_046d:
                enqueue(state, next_ip, call_stack, stack_delta,
                        None, None, None, int91_index, None, ss_equals_cs,
                        stack_pointer_base, False,
                        ds_equals_cs if preserve_bios_ds_at_046d else None,
                        None, None,
                        "scenario-bios-int10-iret-at-046d;effects-opaque")
                continue
            if ip == 0x1B26 and vector == 0x21 and return_from_dos_int21_at_1b26:
                enqueue(state, next_ip, call_stack, stack_delta,
                        None, None, None, int91_index, None, ss_equals_cs,
                        stack_pointer_base, False, None, None, None,
                        "scenario-dos-int21-return-at-1b26;effects-opaque")
                continue
            block(state, "software-interrupt-opaque", vector=vector)
            continue

        if instruction.mnemonic in {"hlt", "ud2"}:
            block(state, "terminal-instruction")
            continue
        def operand_constant(operand) -> int | None:
            if operand.type == X86_OP_IMM:
                return operand.imm & 0xFFFF
            if operand.type != X86_OP_REG:
                return None
            if operand.reg == X86_REG_AX:
                return ((ah_constant << 8) | al_constant
                        if al_constant is not None and ah_constant is not None else None)
            if operand.reg == X86_REG_AL:
                return al_constant
            if operand.reg == X86_REG_AH:
                return ah_constant
            return None

        if instruction.mnemonic == "mov" and len(instruction.operands) == 2:
            destination, source = instruction.operands
            if destination.type == X86_OP_REG:
                if destination.reg == X86_REG_AX:
                    value = operand_constant(source)
                    ax_is_cs = (True if source.type == X86_OP_REG
                                and source.reg == X86_REG_CS else
                                (ax_is_cs if source.type == X86_OP_REG
                                 and source.reg == X86_REG_AX else None))
                    if value is None:
                        al_constant = ah_constant = None
                    else:
                        al_constant, ah_constant = value & 0xFF, (value >> 8) & 0xFF
                elif destination.reg == X86_REG_AL:
                    ax_is_cs = None
                    al_constant = operand_constant(source)
                    if al_constant is not None:
                        al_constant &= 0xFF
                elif destination.reg == X86_REG_AH:
                    ax_is_cs = None
                    ah_constant = operand_constant(source)
                    if ah_constant is not None:
                        ah_constant &= 0xFF
                elif destination.reg == X86_REG_SI:
                    if source.type == X86_OP_IMM:
                        si_values = (source.imm & 0xFFFF,)
                    elif source.type == X86_OP_REG and source.reg == X86_REG_SI:
                        pass
                    else:
                        si_values = None
                elif destination.reg == X86_REG_DS:
                    if source.type == X86_OP_REG and source.reg == X86_REG_CS:
                        ds_equals_cs = True
                    elif source.type == X86_OP_REG and source.reg == X86_REG_DS:
                        pass
                    elif source.type == X86_OP_REG and source.reg == X86_REG_AX \
                            and ax_is_cs is True:
                        ds_equals_cs = True
                    else:
                        ds_equals_cs = None
                elif destination.reg == X86_REG_SS:
                    if source.type == X86_OP_REG and source.reg == X86_REG_AX:
                        ss_equals_cs = True if ax_is_cs is True else None
                    elif source.type == X86_OP_REG and source.reg == X86_REG_CS:
                        ss_equals_cs = True
                    else:
                        ss_equals_cs = None
        elif instruction.mnemonic in {"cmp", "test"} and len(instruction.operands) == 2:
            left, right = (operand_constant(operand) for operand in instruction.operands)
            if left is None or right is None:
                zf_constant = None
            elif instruction.mnemonic == "cmp":
                zf_constant = (left & ((1 << (instruction.operands[0].size * 8)) - 1)) == (
                    right & ((1 << (instruction.operands[0].size * 8)) - 1))
            else:
                zf_constant = (left & right) == 0
        else:
            # Preserve ZF only through instructions that do not modify flags.
            if instruction.mnemonic not in {
                "mov", "lea", "push", "pushf", "pop", "pusha", "pushaw",
                "popa", "popaw", "xchg", "nop", "cld", "std", "cli", "sti",
            } and not instruction.group(CS_GRP_JUMP) and not instruction.group(CS_GRP_CALL) \
                    and not instruction.group(CS_GRP_RET):
                zf_constant = None
            if instruction.mnemonic == "xchg" and any(
                    operand.type == X86_OP_REG and operand.reg in
                    {X86_REG_AX, X86_REG_AL, X86_REG_AH}
                    for operand in instruction.operands):
                al_constant = ah_constant = None
                ax_is_cs = None
            if (instruction.mnemonic != "push" and instruction.operands
                    and instruction.operands[0].type == X86_OP_REG):
                destination_reg = instruction.operands[0].reg
                if destination_reg == X86_REG_AX:
                    al_constant = ah_constant = None
                    ax_is_cs = None
                elif destination_reg == X86_REG_AL:
                    al_constant = None
                    ax_is_cs = None
                elif destination_reg == X86_REG_AH:
                    ah_constant = None
                    ax_is_cs = None
                elif destination_reg == X86_REG_SI:
                    si_values = None
                elif destination_reg == X86_REG_DS:
                    ds_equals_cs = None
                elif destination_reg == X86_REG_SS:
                    ss_equals_cs = None

        if instruction.mnemonic in {"enter", "leave"}:
            block(state, "frame-pointer-stack-operation-opaque")
            continue
        if instruction.mnemonic in {"push", "pushf"}:
            stack_delta -= 2
        elif instruction.mnemonic in {"pusha", "pushaw"}:
            stack_delta -= 16
        elif instruction.mnemonic == "pop" and instruction.operands \
                and instruction.operands[0].type == X86_OP_REG \
                and instruction.operands[0].reg == X86_REG_SP:
            block(state, "stack-pointer-write-opaque")
            continue
        elif instruction.mnemonic in {"pop", "popf"}:
            stack_delta += 2
        elif instruction.mnemonic in {"popa", "popaw"}:
            stack_delta += 16
        elif instruction.mnemonic == "mov" and len(instruction.operands) == 2 \
                and instruction.operands[0].type == X86_OP_REG \
                and instruction.operands[0].reg == X86_REG_SP:
            source = instruction.operands[1]
            if ((source.type == X86_OP_IMM or
                 (source.type == X86_OP_REG and source.reg == X86_REG_AX
                  and al_constant is not None and ah_constant is not None)) and not call_stack):
                # The program entry's immediate SP initialization establishes
                # a new abstract baseline without creating a call frame.
                stack_delta = 0
                stack_pointer_base = ((source.imm & 0xFFFF)
                                      if source.type == X86_OP_IMM else
                                      ((ah_constant << 8) | al_constant))
            else:
                block(state, "stack-pointer-write-opaque")
                continue
        elif instruction.mnemonic in {"add", "sub"} and len(instruction.operands) == 2 \
                and instruction.operands[0].type == X86_OP_REG \
                and instruction.operands[0].reg == X86_REG_SP:
            source = instruction.operands[1]
            if source.type != X86_OP_IMM or source.imm < 0 or source.imm > 0x7FFF:
                block(state, "stack-pointer-arithmetic-opaque")
                continue
            stack_delta += source.imm if instruction.mnemonic == "add" else -source.imm
        elif any(operand.type == X86_OP_REG and operand.reg == X86_REG_SP
                 for operand in instruction.operands[:1]):
            block(state, "stack-pointer-write-opaque")
            continue
        if (instruction.mnemonic in {"mov", "pop"} and instruction.operands
                and instruction.operands[0].type == X86_OP_REG
                and instruction.operands[0].reg == X86_REG_SS and call_stack):
            block(state, "stack-segment-write-with-active-call-frame")
            continue
        if (instruction.mnemonic == "push" and instruction.operands
                and instruction.operands[0].type == X86_OP_REG
                and instruction.operands[0].reg == X86_REG_CS):
            push_cs_pending = True
        elif instruction.mnemonic == "pop" and instruction.operands \
                and instruction.operands[0].type == X86_OP_REG:
            if instruction.operands[0].reg == X86_REG_DS:
                if push_cs_pending:
                    ds_equals_cs = True
                elif (preserve_int91_wrapper_stack and ip == 0x012D
                      and int91_index > 0 and call_stack
                      and state[2] == call_stack[-1][1] - 2
                      and state[11] is True):
                    ds_equals_cs = True
                else:
                    ds_equals_cs = None
            push_cs_pending = False
        else:
            push_cs_pending = False
        if instruction.mnemonic == "cld":
            direction_decrement = False
        elif instruction.mnemonic == "std":
            direction_decrement = True
        elif instruction.mnemonic in {"popf", "iret"}:
            direction_decrement = None
        if instruction.mnemonic == "lodsb":
            if si_values is not None:
                deltas = (-1, 1) if direction_decrement is None else (
                    (-1,) if direction_decrement else (1,))
                updated_si = sorted({(value + delta) & 0xFFFF
                                     for value in si_values for delta in deltas})
                si_values = tuple(updated_si) if len(updated_si) <= 64 else None
        enqueue(state, next_ip, call_stack, stack_delta, al_constant, ah_constant,
                zf_constant, int91_index, ax_is_cs, ss_equals_cs,
                stack_pointer_base, push_cs_pending, ds_equals_cs, si_values,
                direction_decrement, "fallthrough")

    def route_for(target_ip: int) -> tuple[State | None, list[tuple[int, int, str, int]]]:
        found = next((state for state in parents if state[0] == target_ip), None)
        if found is None:
            return None, []
        route: list[tuple[int, int, str, int]] = []
        cursor = found
        while parents[cursor] is not None:
            previous, kind = parents[cursor]  # type: ignore[misc]
            route.append((previous[0], cursor[0], kind, len(cursor[1])))
            cursor = previous
        route.reverse()
        return found, route

    def route_to_stop(stop: dict[str, object]) -> list[tuple[int, int, str, int]]:
        stop_state = (int(stop["ip"]),
                      tuple((int(frame[0]), int(frame[1])) for frame in stop["call_stack"]),
                      int(stop["stack_delta"]), stop.get("al_constant"),
                      stop.get("ah_constant"), stop.get("zf_constant"),
                      int(stop["int91_return_index"]), stop.get("ax_is_cs"),
                      stop.get("ss_equals_cs"), stop.get("stack_pointer_base"),
                      bool(stop.get("push_cs_pending")), stop.get("ds_equals_cs"),
                      (tuple(int(value) for value in stop["si_values"])
                       if stop.get("si_values") is not None else None),
                      stop.get("direction_decrement"))
        route: list[tuple[int, int, str, int]] = []
        cursor = stop_state
        while cursor in parents and parents[cursor] is not None:
            previous, kind = parents[cursor]  # type: ignore[misc]
            route.append((previous[0], cursor[0], kind, len(cursor[1])))
            cursor = previous
        route.reverse()
        return route

    # Avoid building a quadratic state-to-edge index for the externally
    # reported graph; target and first-blocked routes come directly from parents.
    target_results: dict[int, dict[str, object]] = {}
    for target_ip in TARGETS:
        state, route = route_for(target_ip)
        target_results[target_ip] = {
            "reachable": state is not None,
            "call_stack": [frame[0] for frame in state[1]] if state is not None else None,
            "stack_delta": state[2] if state is not None else None,
            "route": route,
        }
    first_blocked_route = route_to_stop(first_blocked) if first_blocked else []
    return {
        "entry_ip": entry_ip,
        "int91_return_ax_sequence": list(int91_return_ax_sequence),
        "int91_return_count_consumed": max((state[6] for state in parents), default=0),
        "bios_int10_return_at_046d": return_from_bios_int10_at_046d,
        "int91_wrapper_stack_preserved": preserve_int91_wrapper_stack,
        "bios_ds_preserved_at_046d": preserve_bios_ds_at_046d,
        "dos_int21_return_at_1b26": return_from_dos_int21_at_1b26,
        "state_count": len(parents),
        "instruction_count": len(instructions),
        "instructions": instructions,
        "edges": edges,
        "stops": stops,
        "stop_counts": Counter(str(stop["reason"]) for stop in stops),
        "targets": target_results,
        "first_blocked": first_blocked,
        "first_blocked_route": first_blocked_route,
    }


def static_reference_candidates(data: bytes, target: int) -> list[dict[str, object]]:
    """Find candidate instruction and literal-word references at every byte start.

    Every decoded row is explicitly a candidate because code/data boundaries
    are not established. Literal little-endian word matches are emitted
    separately and make no instruction claim.
    """
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    decoder.detail = True
    rows: list[dict[str, object]] = []
    for offset in range(len(data)):
        raw = data[offset:offset + 15]
        instruction = next(decoder.disasm(raw, DOS_BASE + offset, count=1), None)
        if instruction is None or instruction.address != DOS_BASE + offset:
            continue
        for operand_index, operand in enumerate(instruction.operands):
            if operand.type == X86_OP_IMM and (operand.imm & 0xFFFF) == target:
                rows.append({"kind": "byte-start-immediate-candidate", "offset": offset,
                             "ip": DOS_BASE + offset, "operand_index": operand_index,
                             "mnemonic": instruction.mnemonic, "operands": instruction.op_str,
                             "bytes_hex": instruction.bytes.hex()})
            elif (operand.type == X86_OP_MEM and operand.mem.base == 0
                  and operand.mem.index == 0 and (operand.mem.disp & 0xFFFF) == target):
                rows.append({"kind": "byte-start-absolute-memory-candidate", "offset": offset,
                             "ip": DOS_BASE + offset, "operand_index": operand_index,
                             "mnemonic": instruction.mnemonic, "operands": instruction.op_str,
                             "bytes_hex": instruction.bytes.hex()})
    literal = target.to_bytes(2, "little")
    start = 0
    while True:
        offset = data.find(literal, start)
        if offset < 0:
            break
        rows.append({"kind": "literal-little-endian-word", "offset": offset,
                     "bytes_hex": literal.hex()})
        start = offset + 1
    return sorted(rows, key=lambda row: (int(row["offset"]), str(row["kind"])))


def external_output(path: Path) -> Path:
    if not path.is_absolute() or path.exists() or path.is_symlink():
        raise ContextReachabilityError("output must be a new absolute path")
    normalized = Path(os.path.normpath(str(path)))
    if (normalized == Path("/tmp") or Path("/tmp") in normalized.parents
            or normalized == Path("/private/tmp") or Path("/private/tmp") in normalized.parents):
        raise ContextReachabilityError("output must not use /tmp")
    resolved = path.parent.resolve(strict=True) / path.name
    repo = ROOT.resolve()
    cache = CACHE_ROOT.resolve(strict=True)
    if resolved == repo or repo in resolved.parents or cache not in resolved.parents:
        raise ContextReachabilityError("output must stay under the external Project Eon cache")
    return resolved


def write_report(path: Path, data: bytes, result: dict[str, object]) -> str:
    output = external_output(path)
    lines = [
        "# schema=project-eon.dos-context-reachability/v2",
        f"# direct_media_set_sha256={DIRECT_MEDIA_SET_SHA256}",
        f"# image=TITLES.EXE;size={TITLES_SIZE};sha256={TITLES_SHA256};base=0x{DOS_BASE:04x}",
        f"# input_sha256={hashlib.sha256(data).hexdigest()}",
        "# analysis=bounded-context-sensitive-static;code-data-unclassified;no-runtime-claim",
        ("# scenario-int91=opaque" if not result["int91_return_ax_sequence"] else
         "# scenario=observed-TITLES-INT91-return-AX-at-0127:" +
         ",".join(f"0x{int(value):04x}" for value in result["int91_return_ax_sequence"]) +
         f";max_consumed={result['int91_return_count_consumed']}"
         + (";wrapper-saved-DS-word-preserved"
            if result["int91_wrapper_stack_preserved"] else "")
         + ";scenario-assumption-only;not-a-runtime-route"),
        ("# scenario-bios-int10-046d=opaque" if not result["bios_int10_return_at_046d"] else
         "# scenario-bios-int10-046d=IRET-to-046f;restore-caller-CS-IP-SS-SP-and-preserve-active-return-words;"
         + ("preserve-DS-value;" if result["bios_ds_preserved_at_046d"] else "")
         + "other-registers-flags-memory-device-effects-unknown;assumption-only;not-runtime-proof"),
        ("# scenario-dos-int21-1b26=opaque" if not result["dos_int21_return_at_1b26"] else
         "# scenario-dos-int21-1b26=return-to-1b28;restore-caller-CS-IP-SS-SP-and-preserve-active-return-words;"
         "registers-flags-and-memory-effects-unknown;assumption-only;not-runtime-proof"),
        f"# entry_ip=0x{ENTRY_IP:04x};state_count={result['state_count']};instructions={result['instruction_count']}",
        f"# limits=call_depth:{DEFAULT_CALL_DEPTH};states:{DEFAULT_STATE_LIMIT}",
        "# memory_alias=CS-absolute-or-DS-absolute-when-DS-equals-CS-or-DS:SI;"
        "only-when-provably-disjoint-from-return-words;"
        "memory-values-and-effects-unmodeled;LODSB-SI-direction-forks-when-DF-unknown",
        "# semantics=near-call-return-candidates;active-frame-memory-opaque-except-proven-disjoint-CS-absolute-or-DS-absolute-when-DS=CS-or-DS:SI;memory-effects-not-modeled;"
        "INT91-0127-AX,INT10-046D-IRET,and-INT21-1B26-return-only-when-scenario-provided;"
        "BIOS-DOS-register-flags-memory-device-effects-unknown;"
        "INT91-wrapper-saved-DS-and-BIOS-DS-preserved-only-when-scenario-provided;"
        "AL-AH-constants;CMP-TEST-ZF-only;"
        "unknown-conditional-branches-both;indirect-and-far-transfers-opaque",
    ]
    for target in TARGETS:
        entry = result["targets"][target]  # type: ignore[index]
        lines.append(f"target\t{target:04x}\treachable={str(entry['reachable']).lower()}"
                     f"\tcall_stack={entry['call_stack']}")
        for source, destination, kind, depth in entry["route"]:
            lines.append(f"route\t{target:04x}\t{source:04x}\t{kind}\t{destination:04x}\tdepth={depth}")
    first = result["first_blocked"]
    if first is None:
        lines.append("first_blocked\tnone")
    else:
        vector = first.get("interrupt_vector")
        vector_text = f"0x{int(vector):02x}" if vector is not None else "-"
        frames = ",".join(
            f"{int(frame[0]):04x}@{int(frame[1]):+d}" for frame in first["call_stack"])
        lines.append(f"first_blocked\t{int(first['ip']):04x}\t{first['reason']}"
                     f"\tvector={vector_text}\tstack={frames}"
                     f"\tstack_delta={int(first['stack_delta']):+d}")
        for source, destination, kind, depth in result["first_blocked_route"]:
            lines.append(f"blocked_route\t{source:04x}\t{kind}\t{destination:04x}\tdepth={depth}")
    lines.append("stop_counts\t" + ",".join(
        f"{key}:{value}" for key, value in sorted(result["stop_counts"].items())))
    lines.append("target\treference_kind\tfile_offset\tinstruction_ip\tmnemonic\toperands\tbytes")
    for target in TARGETS:
        for ref in static_reference_candidates(data, target):
            lines.append(f"{target:04x}\t{ref['kind']}\t{int(ref['offset']):04x}\t"
                         f"{int(ref.get('ip', 0)):04x}\t{ref.get('mnemonic', '-')}\t"
                         f"{ref.get('operands', '-')}\t{ref['bytes_hex']}")
    lines.append("ip\tfile_offset\tmnemonic\toperands\tbytes")
    for ip, row in sorted(result["instructions"].items()):  # type: ignore[union-attr]
        lines.append(f"{ip:04x}\t{int(row['file_offset']):04x}\t{row['mnemonic']}\t"
                     f"{row['operands']}\t{row['bytes_hex']}")
    lines.append("source_ip\ttarget_ip\tedge_kind\tcall_depth")
    for source, target, kind, depth in result["edges"]:  # type: ignore[union-attr]
        lines.append(f"{source:04x}\t{target:04x}\t{kind}\t{depth}")
    content = ("\n".join(lines) + "\n").encode("ascii")
    fd = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())
    if stat.S_IMODE(output.stat().st_mode) != 0o600:
        output.chmod(0o600)
    return hashlib.sha256(content).hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--media-root", type=Path, required=True,
                        help="Existing direct-media directory; media is read in place")
    parser.add_argument("--output", type=Path, required=True,
                        help="New report path under ~/.cache/project-eon-tools")
    parser.add_argument("--scenario-observed-int91-returns", action="store_true",
                        help="Explore only the two captured TITLES.EXE INT 91h AX values at 0x0127, in observed order (0x0101, 0x0000); this is a static scenario, not runtime proof")
    parser.add_argument("--scenario-return-from-bios-int10-at-046d", action="store_true",
                        help="Assume BIOS INT 10h at runtime 0x046d IRETs to 0x046f, restores caller CS:IP/SS:SP, and preserves active call return words; all other effects stay unknown")
    parser.add_argument("--scenario-preserve-int91-wrapper-stack", action="store_true",
                        help="Assume the exact TITLES.EXE INT 91h wrapper's saved stack words are unchanged, allowing POP DS at 0x012d to restore a previously known DS=CS")
    parser.add_argument("--scenario-preserve-bios-ds-at-046d", action="store_true",
                        help="Additionally assume BIOS INT 10h at 0x046d preserves the incoming DS value")
    parser.add_argument("--scenario-return-from-dos-int21-at-1b26", action="store_true",
                        help="Assume DOS INT 21h at TITLES.EXE runtime 0x1b26 returns to 0x1b28 with the caller stack and active return words intact; all other effects stay unknown")
    args = parser.parse_args(argv)
    try:
        media = verify_direct_directory(args.media_root, DIRECT_MEDIA_SET_SHA256)
        data = read_verified_direct_file(media / "TITLES.EXE", TITLES_SHA256)
        if len(data) != TITLES_SIZE:
            raise ContextReachabilityError("hash-matched TITLES.EXE has unexpected size")
        scenario_returns = (0x0101, 0x0000) if args.scenario_observed_int91_returns else ()
        result = analyze_context_reachable(
            data, int91_return_ax_sequence=scenario_returns,
            return_from_bios_int10_at_046d=args.scenario_return_from_bios_int10_at_046d,
            preserve_int91_wrapper_stack=args.scenario_preserve_int91_wrapper_stack,
            preserve_bios_ds_at_046d=args.scenario_preserve_bios_ds_at_046d,
            return_from_dos_int21_at_1b26=args.scenario_return_from_dos_int21_at_1b26)
        digest = write_report(args.output, data, result)
    except (OSError, ContextReachabilityError, ValueError) as error:
        print(f"DOS CONTEXT REACHABILITY REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"DOS CONTEXT REACHABILITY COMPLETE  sha256={digest}  output={args.output}")
    if result["int91_return_ax_sequence"]:
        print("  scenario INT 91h AX returns at TITLES.EXE:0x0127=" +
              ",".join(f"0x{int(value):04x}" for value in result["int91_return_ax_sequence"]) +
              f"; max consumed={result['int91_return_count_consumed']}")
    if result["int91_wrapper_stack_preserved"]:
        print("  scenario INT 91h at TITLES.EXE:0x0127 preserves the exact wrapper save area")
    if result["bios_int10_return_at_046d"]:
        print("  scenario BIOS INT 10h at TITLES.EXE:0x046d returns to 0x046f; "
              "caller stack and live return words preserved, other effects unknown")
    if result["bios_ds_preserved_at_046d"]:
        print("  scenario BIOS INT 10h at 0x046d also preserves DS")
    if result["dos_int21_return_at_1b26"]:
        print("  scenario DOS INT 21h at TITLES.EXE:0x1b26 returns to 0x1b28; "
              "register and memory effects unknown")
    for target in TARGETS:
        reach = result["targets"][target]  # type: ignore[index]
        print(f"  {target:04x}: reachable={str(reach['reachable']).lower()}")
    first = result["first_blocked"]
    if first:
        print(f"  first blocked boundary={int(first['ip']):04x} {first['reason']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
