import importlib.util
from pathlib import Path
import sys


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "analyze_deuteros_response_entry_references.py"
SPEC = importlib.util.spec_from_file_location("response_refs", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def test_finds_68000_branch_and_pc_relative_call():
    stage = bytearray(16)
    stage[0:2] = bytes.fromhex("6006")  # BRA.S from $1002 to $1008
    stage[2:6] = bytes.fromhex("4eba0004")  # JSR (PC) from $1004 to $1008

    references = MODULE.find_direct_references(stage, 0x1000, 0x1008)

    assert [(item.source, item.kind) for item in references] == [
        (0x1000, "branch"),
        (0x1002, "PC-relative JSR"),
    ]


def test_finds_absolute_control_flow_and_pointer_literal():
    stage = bytearray(16)
    stage[0:6] = bytes.fromhex("4eb900001008")
    stage[8:14] = bytes.fromhex("4ef900001008")

    references = MODULE.find_direct_references(stage, 0x2000, 0x1008)

    assert [(item.source, item.kind) for item in references] == [
        (0x2000, "absolute JSR"),
        (0x2002, "absolute pointer literal"),
        (0x2008, "absolute JMP"),
        (0x200A, "absolute pointer literal"),
    ]


def test_ignores_nonmatching_destinations():
    stage = bytes.fromhex("60064e ba0002".replace(" ", ""))

    assert MODULE.find_direct_references(stage, 0x1000, 0x1010) == []
