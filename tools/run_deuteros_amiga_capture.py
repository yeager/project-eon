#!/usr/bin/env python3
"""Run a read-only, operator-driven Deuteros Amiga recorder session.

This helper prepares external evidence around a separately reviewed FS-UAE
recorder. It is not an emulator distribution, input injector, trace
assembler, or Project Eon runtime component. A physical operator must use the
visible FS-UAE window; headless SDL, debugger routes, and playback are not
admitted.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import threading
import time


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_RELEASE_SHA256 = "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04"
EXPECTED_RELEASE_SIZE = 4_066_771
EXPECTED_KICKSTART_SHA256 = "c9521c114900633c09317ca6ff979db7b9df34d3cb537de062f5d51811c42c04"
# This is the supplied ZIP size, not the 262,144-byte ROM payload size.
EXPECTED_KICKSTART_SIZE = 143_269
EXPECTED_RECORDER_SHA256 = "0e0bfb1fe73a6f37dc38992b39e34e355564adc516106c399c8be86fb38232ec"
# Independently reviewed x86_64 restoration; see CAPTURE_RECORDER_RESTORATION.md.
TRV2_RECORDER_V10_SHA256 = "c6422037df6cadeb50ffaee3bb1c1b56d21722a7c687287d6058d4802943f54b"
TRV2_RECORDER_V11_SHA256 = "18378aacf2a4bbe4fe3a84c295a1f53a3edd08c1cd4f5dca90f2c4af87c2181d"
TRV2_RECORDER_SHA256 = "7b3779771dd705aeb313f71c355e06fe6d4f77b836ea95f7b9748baba4eb4f64"
TRV2_RECORDER_V13_SHA256 = "e32f337dafdfb30e655f0aa8eb06e37a5dc445a225ec8473da92cad23cf5eb24"
TRV2_RECORDER_V14_SHA256 = "701d11b705dd36934712ab37df4e105a2d68bde4dea2642214f45012d6768acf"
TRV2_RECORDER_V15_SHA256 = "7160dfafbfe67b17db931065ab6f9853874591ea4af6c33ad51059b2b0f703df"
TRV2_RECORDER_V16_SHA256 = "aa4c797fc2e580c887ab6be88a57abe93f6b442ac822870e4840c514898518b4"
TRV2_RECORDER_V16_SIZE = 62_015_016
TRV2_RECORDER_V17_SHA256 = "8d7255b20a6f9867a9329541cdf5e0a590d2d507f639da313dd8964c7f02fd5e"
TRV2_RECORDER_V17_SIZE = 62_016_168
TRV2_RECORDER_V18_SHA256 = "44477a0f41025a6e3f68098eb093fb7a32aebf3b2d1577fb294337a617154a54"
TRV2_RECORDER_V18_SIZE = 62_016_152
TRV2_RECORDER_V19_SHA256 = "7b46501fc3cf774938fc8ca4e02788586ba22ef440462ea7ecd00396311b73a2"
TRV2_RECORDER_V19_SIZE = 62_020_024
TRV2_RECORDER_V20_SHA256 = "071f1c949409be9ff3faa128d0acc98fcde9136c0aa09ab6a6edb058e7fbc397"
TRV2_RECORDER_V20_SIZE = 62_020_224
EXPECTED_DISK1_SHA256 = "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"
EXPECTED_DISK2_SHA256 = "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a"
EXPECTED_DISK1_ARCHIVE_SHA256 = "7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533"
EXPECTED_DISK1_ARCHIVE_SIZE = 449_666
EXPECTED_DISK2_ARCHIVE_SHA256 = "b98ee3c36141773485c5e03dd8bb4aa59784eaf08a1363fa6a2951a5eb5fdc0a"
EXPECTED_DISK2_ARCHIVE_SIZE = 490_962
EXPECTED_ROM_SHA256 = "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53"
DISK1_ARCHIVE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).zip"
DISK2_ARCHIVE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 2 of 2).zip"
DISK1_IMAGE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf"
DISK2_IMAGE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 2 of 2).adf"
KICKSTART_IMAGE = "Kickstart v1.3 r34.005 (1987-12)(Commodore)(A500-A1000-A2000-CDTV)[!].rom"
MIN_DURATION_SECONDS = 15
MAX_DURATION_SECONDS = 600
MAX_FOCUS_SETTLE_SECONDS = 120
# A finite timing profile makes a reachability diagnostic reproducible without
# confusing it with time-faithful capture evidence. Warp never establishes
# original timing, gameplay, or title-screen behaviour.
TIMING_PROFILES = {"realtime": "0", "warp": "1"}
CAPTURE_INTENTS = {"diagnostic-no-input", "physical-input"}
# The reviewed delivery observer writes a bounded physical-input receipt.
# Never hash an arbitrary-size file merely because a recorder path was set.
MAX_INPUT_RECEIPT_BYTES = 64 * 1024
MAX_INPUT_RECEIPT_RECORDS = 256
# Raw recorder output and console diagnostics are external evidence, not
# unbounded host storage.  A broken emulator must not be able to exhaust the
# operator's terminal, disk, or cache while a capture is being reviewed.
MAX_RAW_OBSERVATION_BYTES = 8 * 1024 * 1024
# v17 emits one small, separate receipt for the two byte cells read by the
# selector dispatch. Keep it independent from historical raw-PC grammars.
MAX_SELECTOR_DISPATCH_BYTES = 64 * 1024
MAX_SELECTOR_DISPATCH_RECORDS = 256
MAX_LATE_RAW_RECORDS_PER_SITE = 96
MAX_LATE_RAW_RECORDS_V19 = 26 * MAX_LATE_RAW_RECORDS_PER_SITE
MAX_LATE_RAW_RECORDS_V20 = 28 * MAX_LATE_RAW_RECORDS_PER_SITE
# Retain the historical v19 name for callers and tests that exercise schema 29.
MAX_LATE_RAW_RECORDS = MAX_LATE_RAW_RECORDS_V19
MAX_LATE_RAW_BYTES = 1024 * 1024
MAX_LATE_SELECTOR_BYTES = 32 * 1024
MAX_ZERO_ROUTE_OBSERVATION_BYTES = 1024 * 1024
MAX_ZERO_ROUTE_OBSERVATION_RECORDS = 4096
MAX_ZERO_ROUTE_OBSERVATION_INVOCATIONS = 16
MAX_LATE_DISPLAY_RECEIPT_BYTES = 512 * 1024
MAX_LATE_DISPLAY_WRITES = 2048
MAX_LATE_DISPLAY_WRITES_PER_REGISTER = 64
MAX_LATE_DISPLAY_RECEIPT_BYTES_V24 = 2 * 1024 * 1024
MAX_LATE_DISPLAY_WRITES_V24 = 8192
MAX_LATE_DISPLAY_WINDOWS_V24 = 4
MAX_LATE_DISPLAY_WRITES_PER_WINDOW_V24 = 2048
MAX_LATCH_WRITE_BYTES = 16 * 1024
MAX_LATCH_WRITE_RECORDS = 64
MAX_NEGATIVE_SELECTOR_BYTES = 256 * 1024
MAX_NEGATIVE_SELECTOR_ROUTES = 96
MAX_NEGATIVE_SELECTOR_PHASES_PER_ROUTE = 8
MAX_CALLBACK_MEMORY_BYTES = 512 * 1024
MAX_CALLBACK_MEMORY_RECORDS_PER_SITE = 24
MAX_SERVICE_BOUNDARY_BYTES = 512 * 1024
MAX_SERVICE_BOUNDARY_SEQUENCES = 12
MAX_SERVICE_BOUNDARY_SEQUENCES_V33 = 32
MAX_RESPONSE_QUEUE_BYTES = 128 * 1024
MAX_RESPONSE_QUEUE_RECORDS_PER_SITE = 24
MAX_RESPONSE_QUEUE_SEQUENCES = 12
MAX_TITLE_CALLBACK_BYTES_V32 = 256 * 1024
MAX_TITLE_CALLBACK_SEQUENCES_V32 = 12
TITLE_CALLBACK_STAGES_V32 = (
    ("gate-test", 0x1F094), ("gate-branch", 0x1F09A),
    ("selector-load", 0x1F09E), ("selector-compare", 0x1F0A2),
    ("selector-branch", 0x1F0A6), ("copy-x", 0x1F0A8),
    ("copy-y", 0x1F0B0), ("service-call", 0x1F0B8),
    ("service-entry", 0x20118), ("move-sprite-call", 0x201B6),
    ("move-sprite-return", 0x201BA), ("service-return", 0x201BE),
    ("callback-continuation", 0x1F0BE), ("callback-cleanup", 0x1F0EC),
    ("callback-rts", 0x1F0EE),
)
CALLBACK_MEMORY_TARGETS = (0x1F056, 0x1F14E, 0x4077E, 0x4078C, 0x407BA, 0x407E4)
CALLBACK_MEMORY_CALLER_SEQUENCE = (0x4077E, 0x4078C, 0x407BA, 0x407E4)
# The 21-byte range includes the queue's $1eed4 look-ahead after its 20-byte shift.
CALLBACK_MEMORY_FIXED_RANGES = ((0x1EEC0, 21), (0x1EED6, 2), (0x1EF2E, 1), (0x407E6, 2))
# V27's later display observer is limited to the two reviewed click ordinals.
LATE_DISPLAY_TARGET_ORDINALS_V27 = (48, 52)
LATE_DISPLAY_TARGET_ORDINALS_V29 = (76,)
LATE_DISPLAY_TARGET_ORDINALS_V30 = (76,)
SERVICE_BOUNDARY_TARGETS_V29 = (0x20118, 0x201B6, 0x201BA, 0x201BE, 0x1F0BE, 0x20202, 0x20216)
SERVICE_BOUNDARY_CALLER_RETURNS_V29 = (0x1F0BE, 0x20202, 0x20216)
SERVICE_BOUNDARY_FIXED_RANGES_V29 = (
    (0x1FFC8, 2), (0x1FFCA, 2), (0x1FFCC, 2), (0x1FFCE, 2),
    (0x1FFD0, 2), (0x1FFD2, 2), (0x1EE10, 2), (0x1EE12, 2))
SERVICE_BOUNDARY_WINDOW_REGISTERS_V29 = (0, 1, 6)
RESPONSE_QUEUE_SITES_V31 = (0x4077E, 0x407B4, 0x1F230, 0x1F23E,
                            0x1F258, 0x407BA, 0x407E4)
SERVICE_BOUNDARY_TARGETS_V30 = SERVICE_BOUNDARY_TARGETS_V29
SERVICE_BOUNDARY_CALLER_RETURNS_V30 = SERVICE_BOUNDARY_CALLER_RETURNS_V29
SERVICE_BOUNDARY_FIXED_RANGES_V30 = SERVICE_BOUNDARY_FIXED_RANGES_V29
SERVICE_BOUNDARY_WINDOW_REGISTERS_V30 = SERVICE_BOUNDARY_WINDOW_REGISTERS_V29
MAX_LATE_SELECTOR_RECORDS = MAX_LATE_RAW_RECORDS_PER_SITE
LATE_INPUT_START_ORDINAL = 9
# 2,048 fixed-format writes can exceed 128 KiB; this remains a strict cap
# above the reviewed finite grammar without rejecting a complete observation.
MAX_TITLE_DISPLAY_RECEIPT_BYTES = 512 * 1024
MAX_RECORDER_CONSOLE_LOG_BYTES = 1024 * 1024
# Retaining only a prefix protects disk, but a pathological recorder can still
# consume host CPU and pipe bandwidth indefinitely while the runner hashes it.
# Signal the owner at this generous cap so it can terminate the child without
# mistaking a partial runaway diagnostic for a normal timed-out capture.
MAX_RECORDER_CONSOLE_TOTAL_BYTES = 64 * 1024 * 1024
# The reviewed recorder's raw-PC observer is intentionally finite and only
# exposes these investigation sites.  This list is a grammar boundary, not an
# interpretation of the observed instructions or their ABI effects.
RAW_PC_SITES = (
    0x000210D4, 0x00021822, 0x00040450, 0x0004046C, 0x0004069A, 0x0001ED80,
    0x0001EDA6, 0x0001EDAC, 0x0001EF74, 0x0001F056, 0x0001F182, 0x0001FE7A,
    0x0001FE84, 0x0001FE88, 0x0001FE92, 0x0001FE96, 0x0001FBE6,
    0x0002182A, 0x0002182C, 0x00021834, 0x00021850, 0x0002185E, 0x00021892,
)
RAW_PC_V16_SITES = (*RAW_PC_SITES, 0x000218CC)
RAW_PC_V18_SITES = (*RAW_PC_V16_SITES, 0x00021866)
RAW_PC_V19_SITES = (*RAW_PC_V18_SITES, 0x0001FEA8)
LATE_RAW_PC_V19_SITES = RAW_PC_V19_SITES
LATE_RAW_PC_V20_SITES = (*LATE_RAW_PC_V19_SITES, 0x0001FC22, 0x0001FC9C)
MAX_RAW_RECORDS = 4096
MAX_RAW_RECORDS_PER_SITE = 128
MAX_TITLE_DISPLAY_WRITES = 2048
MAX_TITLE_DISPLAY_WRITES_PER_REGISTER = 64
RAW_PC_LEGACY_LINE = re.compile(
    r"raw-pc ([1-9][0-9]*) cycles=([0-9]+) pc=0x([0-9a-f]{8}) "
    r"opcode=0x([0-9a-f]{4}) d0=0x([0-9a-f]{8}) a0=0x([0-9a-f]{8}) "
    r"a6=0x([0-9a-f]{8}) sr=0x([0-9a-f]{4})\n")
# The cycle-exact core keeps a prefetched IR word separately from the memory
# word at its current PC. Receipt v7 records both values and never presents
# the former as though it were a direct original-media byte read.
RAW_PC_V7_LINE = re.compile(
    r"raw-pc ([1-9][0-9]*) cycles=([0-9]+) pc=0x([0-9a-f]{8}) "
    r"ir_opcode=0x([0-9a-f]{4}) memory_opcode=0x([0-9a-f]{4}) "
    r"d0=0x([0-9a-f]{8}) a0=0x([0-9a-f]{8}) a6=0x([0-9a-f]{8}) sr=0x([0-9a-f]{4})\n")
# v9 binds a raw CPU sample only to the most recent recorder-confirmed
# host-to-core delivery. It never states that the guest polled or accepted it.
RAW_PC_V9_LINE = re.compile(
    r"raw-pc ([1-9][0-9]*) cycles=([0-9]+) pc=0x([0-9a-f]{8}) "
    r"ir_opcode=0x([0-9a-f]{4}) memory_opcode=0x([0-9a-f]{4}) "
    r"d0=0x([0-9a-f]{8}) a0=0x([0-9a-f]{8}) a6=0x([0-9a-f]{8}) sr=0x([0-9a-f]{4}) "
    r"input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
# v10 has one explicit arm at the recovered title site, followed by bounded
# writes to a deliberately small custom-chip display register allow-list.  It
# records neither bitplane contents nor a claim that a title frame was shown.
TITLE_DISPLAY_ARM_LINE = re.compile(
    r"display-arm 1 cycles=([0-9]+) site=0x0001eda6 "
    r"input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
TITLE_DISPLAY_WRITE_LINE = re.compile(
    r"display-write ([1-9][0-9]*) cycles=([0-9]+) vpos=([0-9]+) hpos=([0-9]+) "
    r"origin=(cpu|copper) register=0x([0-9a-f]{4}) value=0x([0-9a-f]{4}) "
    r"input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
LATE_DISPLAY_WRITE_LINE = re.compile(
    r"late-display-write ([1-9][0-9]*) cycles=([0-9]+) vpos=([0-9]+) hpos=([0-9]+) "
    r"origin=(cpu|copper) register=0x([0-9a-f]{4}) value=0x([0-9a-f]{4}) "
    r"input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
LATCH_WRITE_LINE = re.compile(
    r"latch-write ([1-9][0-9]*) cycles=([0-9]+) site=0x00021868 next_pc=0x00021870 "
    r"opcode=0x13fc target=0x00021720 before=0x00 after=0x01 "
    r"input_ordinal=([1-9][0-9]*) input_frame=(-?[0-9]+)\n")
TITLE_DISPLAY_REGISTERS = frozenset((
    0x0080, 0x0082, 0x008E, 0x0090, 0x0092, 0x0094,
    *range(0x00E0, 0x00F0, 2), 0x0100, 0x0108, 0x010A,
    *range(0x0180, 0x01C0, 2),
))
# The reviewed FS-UAE host-delivery observer prints raw signed integer action
# fields. They remain opaque delivery observations; this grammar proves only
# that an external receipt has not been hand-edited into an arbitrary file.
HOST_INPUT_LINE = re.compile(
    r"host-input ([1-9][0-9]*) frame=(-?[0-9]+) line=(-?[0-9]+) "
    r"action=(-?[0-9]+) state=(-?[0-9]+)\n")
SELECTOR_DISPATCH_LINE = re.compile(
    r"selector-dispatch ([1-9][0-9]*) raw_ordinal=([1-9][0-9]*) "
    r"cycles=([0-9]+) pc=0x0001fbe6 cell_1f98c=0x([0-9a-f]{2}) "
    r"cell_1f98e=0x([0-9a-f]{2}) input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
LATE_RAW_PC_LINE = re.compile(
    r"late-pc ([1-9][0-9]*) cycles=([0-9]+) pc=0x([0-9a-f]{8}) "
    r"ir_opcode=0x([0-9a-f]{4}) memory_opcode=0x([0-9a-f]{4}) "
    r"d0=0x([0-9a-f]{8}) a0=0x([0-9a-f]{8}) a6=0x([0-9a-f]{8}) sr=0x([0-9a-f]{4}) "
    r"input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
LATE_SELECTOR_DISPATCH_LINE = re.compile(
    r"late-selector-dispatch ([1-9][0-9]*) late_raw_ordinal=([1-9][0-9]*) "
    r"cycles=([0-9]+) pc=0x0001fbe6 cell_1f98c=0x([0-9a-f]{2}) "
    r"cell_1f98e=0x([0-9a-f]{2}) input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
NEGATIVE_SELECTOR_LINE = re.compile(
    r"negative-selector ([1-9][0-9]*) route=([1-9][0-9]*) cycles=([0-9]+) "
    r"phase=(dispatch|branch|gate|call|callee|return|skip|epilogue|caller) "
    r"pc=0x([0-9a-f]{8}) opcode0=0x([0-9a-f]{8}) opcode1=0x([0-9a-f]{8}) opcode_valid=(0|1) "
    r"d0=0x([0-9a-f]{8}) d1=0x([0-9a-f]{8}) sr=0x([0-9a-f]{4}) "
    r"selector=0x([0-9a-f]{2}) secondary=0x([0-9a-f]{2}) "
    r"input_ordinal=([1-9][0-9]*) input_frame=(-?[0-9]+)\n")
NEGATIVE_SELECTOR_PHASES = (
    "dispatch", "branch", "gate", "call", "callee", "return", "epilogue", "caller")
NEGATIVE_SELECTOR_PHASE_PC = {
    "dispatch": 0x1FBE6, "branch": 0x1FBEE, "gate": 0x1FBFE,
    "call": 0x1FC08, "callee": 0x3FBF8, "return": 0x1FC0E,
    "skip": 0x1FC12, "epilogue": 0x1FC20, "caller": 0x1FAC6,
}
CALLBACK_MEMORY_SUMMARY_LINE = re.compile(
    r"callback-snapshot ([1-9][0-9]*) target=0x([0-9a-f]{6}) cycles=([0-9]+) "
    r"pc=0x([0-9a-f]{8}) opcode=0x([0-9a-f]{4}) sr=0x([0-9a-f]{4}) "
    r"input_ordinal=([1-9][0-9]*) input_frame=(-?[0-9]+) "
    + " ".join(f"d{index}=0x([0-9a-f]{{8}})" for index in range(8)) + " "
    + " ".join(f"a{index}=0x([0-9a-f]{{8}})" for index in range(8)) + r"\n")
CALLBACK_MEMORY_WINDOW_LINE = re.compile(
    r"callback-window input_ordinal=([1-9][0-9]*) pc=0x([0-9a-f]{6}) "
    r"register=a([0-7]) start=0x([0-9a-f]{8}) valid=(0|1) bytes=([0-9a-f]{64})?\n")
CALLBACK_MEMORY_FIXED_LINE = re.compile(
    r"callback-fixed input_ordinal=([1-9][0-9]*) pc=0x([0-9a-f]{6}) "
    r"address=0x([0-9a-f]{8}) width=(1|2|20|21) valid=(0|1) bytes=([0-9a-f]{2,42})?\n")
SERVICE_BOUNDARY_SUMMARY_LINE = re.compile(
    r"service-snapshot ([1-9][0-9]*) sequence=([1-9][0-9]*) target=0x([0-9a-f]{6}) "
    r"cycles=([0-9]+) pc=0x([0-9a-f]{8}) opcode=0x([0-9a-f]{4}) sr=0x([0-9a-f]{4}) "
    r"input_ordinal=([1-9][0-9]*) input_frame=(-?[0-9]+) caller_return_valid=(0|1) "
    r"caller_return_pc=0x([0-9a-f]{8}) "
    + " ".join(f"d{index}=0x([0-9a-f]{{8}})" for index in range(8)) + " "
    + " ".join(f"a{index}=0x([0-9a-f]{{8}})" for index in range(8)) + r"\n")
SERVICE_BOUNDARY_WINDOW_LINE = re.compile(
    r"service-window sequence=([1-9][0-9]*) input_ordinal=([1-9][0-9]*) pc=0x([0-9a-f]{6}) "
    r"register=a([0-7]) start=0x([0-9a-f]{8}) valid=(0|1) bytes=([0-9a-f]{64})?\n")
SERVICE_BOUNDARY_FIXED_LINE = re.compile(
    r"service-fixed sequence=([1-9][0-9]*) input_ordinal=([1-9][0-9]*) pc=0x([0-9a-f]{6}) "
    r"address=0x([0-9a-f]{8}) width=2 valid=(0|1) bytes=([0-9a-f]{4})?\n")
RESPONSE_QUEUE_LINE_V31 = re.compile(
    r"response-queue ([1-9][0-9]*) sequence=([0-9]+) stage=([0-6]) cycles=([0-9]+) "
    r"pc=0x([0-9a-f]{6}) opcode=0x([0-9a-f]{4}) sr=0x([0-9a-f]{4}) "
    r"input_ordinal=([1-9][0-9]*) input_frame=(-?[0-9]+) d0=0x([0-9a-f]{8}) "
    r"a7=0x([0-9a-f]{8}) stack_valid=(0|1) stack_return=0x([0-9a-f]{8}) "
    r"queue_valid=(0|1) queue=([0-9a-f]{42})? pending_valid=(0|1) pending=([0-9a-f]{4})?\n")
TITLE_CALLBACK_LINE_V32 = re.compile(
    r"title-callback ([1-9][0-9]*) sequence=([1-9][0-9]*) stage=([a-z-]+) "
    r"cycles=([0-9]+) pc=0x([0-9a-f]{6}) input_ordinal=([1-9][0-9]*) "
    r"input_frame=(-?[0-9]+) value_valid=(0|1) value=0x([0-9a-f]{4}) "
    r"source=0x([0-9a-f]{8}) destination=0x([0-9a-f]{8}) width=(0|1|2) "
    r"cell_x_valid=(0|1) cell_x=0x([0-9a-f]{4}) "
    r"cell_y_valid=(0|1) cell_y=0x([0-9a-f]{4})\n")
ZERO_ROUTE_OBSERVATION_LINE = re.compile(
    r"zero-route-observation ([1-9][0-9]*) cycles=([0-9]+) pc=0x([0-9a-f]{8}) "
    r"input_ordinal=([1-9][0-9]*) input_frame=(-?[0-9]+) "
    + " ".join(f"d{index}=0x([0-9a-f]{{8}})" for index in range(8)) + r" "
    + r"a0=0x([0-9a-f]{8}) a1=0x([0-9a-f]{8}) a2=0x([0-9a-f]{8}) a4=0x([0-9a-f]{8}) "
    + r"cell_1f98c=0x([0-9a-f]{2}) cell_1f98e=0x([0-9a-f]{2}) cells_valid=(0|1) "
    + r"mem_addr=0x([0-9a-f]{8}) mem_width=(0|1|2|4) mem_value=0x([0-9a-f]{8}) mem_valid=(0|1)\n")
ZERO_ROUTE_OBSERVATION_SITES = frozenset((
    0x0001FC22, 0x0001FC28, 0x0001FC2C, 0x0001FC42, 0x0001FC4A, 0x0001FC50,
    0x0001FC56, 0x0001FC5E, 0x0001FC6A, 0x0001FC6C, 0x0001FC74,
    0x0001FC76, 0x0001FC88, 0x0001FC8E, 0x0001FC94, 0x0001FC9A,
))
# Receipt v6 additionally binds the finite recorder timing profile. Older
# evidence remains verifiable without pretending it has the newer field.
CAPTURE_RECEIPT_VERSION = "23"
CAPTURE_RECEIPT_V16_PHASED_VERSION = "26"
CAPTURE_RECEIPT_V17_VERSION = "27"
CAPTURE_RECEIPT_V18_VERSION = "28"
CAPTURE_RECEIPT_V19_VERSION = "29"
CAPTURE_RECEIPT_V20_VERSION = "30"
CAPTURE_RECEIPT_V21_VERSION = "31"
CAPTURE_RECEIPT_V22_VERSION = "32"
CAPTURE_RECEIPT_V23_VERSION = "33"
CAPTURE_RECEIPT_V24_VERSION = "34"
CAPTURE_RECEIPT_V25_VERSION = "35"
CAPTURE_RECEIPT_V26_VERSION = "36"
CAPTURE_RECEIPT_V27_VERSION = "37"
CAPTURE_RECEIPT_V29_VERSION = "38"
CAPTURE_RECEIPT_V30_VERSION = "39"
CAPTURE_RECEIPT_V31_VERSION = "40"
CAPTURE_RECEIPT_V32_VERSION = "41"
CAPTURE_RECEIPT_V33_VERSION = "42"
TRV2_RECORDER_V21_SHA256 = "2fc7f47425d0fa005bb59bf41eaeccf32d1cba284dee4f227e7e723b853e1b35"
TRV2_RECORDER_V21_SIZE = 62_030_288
TRV2_RECORDER_V22_SHA256 = "eb0995c70f7f355f674d448b08c0f3e647430562ffde7d179e5aeb12e5abca71"
TRV2_RECORDER_V22_SIZE = 62_031_592
TRV2_RECORDER_V23_SHA256 = "e4e46e84cd75eceffb28d26882fef0062582c251b0ef26baa0fccf6aff2e8dd1"
TRV2_RECORDER_V23_SIZE = 62_039_312
TRV2_RECORDER_V24_SHA256 = "ddd6e2df32c9d785e93b96c0a55b6244fab034212b6842e031ef09648d7765ac"
TRV2_RECORDER_V24_SIZE = 62_039_936
TRV2_RECORDER_V25_SHA256 = "89f66139d3dfd47d59addde5901eebcc844825a331e8f0746ca96ae8f09de593"
TRV2_RECORDER_V25_SIZE = 62_053_824
TRV2_RECORDER_V26_CALLBACK_MEMORY_SHA256 = "2d61d03c631d049960a02cef852a580b5348d2a5808f286a6110c6d8bc5143b7"
TRV2_RECORDER_V26_CALLBACK_MEMORY_SIZE = 62_066_216
TRV2_RECORDER_V27_CALLBACK_MEMORY_TARGETED_DISPLAY_SHA256 = "be10c4fd2d9b615d3b68658dfa5a8f7a96d95d0ced67ea7ac203d14fcb4ab2ac"
TRV2_RECORDER_V27_CALLBACK_MEMORY_TARGETED_DISPLAY_SIZE = 62_066_504
TRV2_RECORDER_V29_SERVICE_BOUNDARY_DISPLAY_SHA256 = "f2c5c7634aa88598c127fb8d2cb81e8b3863ca0b81e59cb1f2ff54f4e64c0040"
TRV2_RECORDER_V29_SERVICE_BOUNDARY_DISPLAY_SIZE = 62_088_128
TRV2_RECORDER_V30_SERVICE_RETURN_DISPLAY_SHA256 = "45b0fde4ffd354793c16cf72fa19891e08b9aee3f2d21b837ac40b46c3323877"
TRV2_RECORDER_V30_SERVICE_RETURN_DISPLAY_SIZE = 62_087_912
TRV2_RECORDER_V31_RESPONSE_QUEUE_SHA256 = "3081ff4b8d12aa550ae065fc83df9f88ff3bfce5ba9ff6af6141bcdce70b74b0"
TRV2_RECORDER_V31_RESPONSE_QUEUE_SIZE = 62_093_928
TRV2_RECORDER_V32_TITLE_CALLBACK_SHA256 = "UNPINNED"
TRV2_RECORDER_V32_TITLE_CALLBACK_SIZE = 0
TRV2_RECORDER_V33_SERVICE_BOUNDARY_SHA256 = "f3701b29d1f3b507f7ca759838b04ff4a3fb2927167fedf1e12cabcf8a7f5ceb"
TRV2_RECORDER_V33_SERVICE_BOUNDARY_SIZE = 62_094_200
RECORDER_ZERO_ROUTE_OBSERVATION_HASHES = frozenset((
    TRV2_RECORDER_V21_SHA256, TRV2_RECORDER_V22_SHA256, TRV2_RECORDER_V23_SHA256,
    TRV2_RECORDER_V24_SHA256, TRV2_RECORDER_V25_SHA256,
    TRV2_RECORDER_V26_CALLBACK_MEMORY_SHA256,
    TRV2_RECORDER_V27_CALLBACK_MEMORY_TARGETED_DISPLAY_SHA256,
    TRV2_RECORDER_V29_SERVICE_BOUNDARY_DISPLAY_SHA256,
    TRV2_RECORDER_V30_SERVICE_RETURN_DISPLAY_SHA256,
    TRV2_RECORDER_V31_RESPONSE_QUEUE_SHA256,
    TRV2_RECORDER_V32_TITLE_CALLBACK_SHA256,
    TRV2_RECORDER_V33_SERVICE_BOUNDARY_SHA256,
))
SOURCE_LAYOUT_RELEASE = "nested-release-zip"
SOURCE_LAYOUT_STANDALONE = "standalone-zip-pair"
SOURCE_LAYOUTS = {SOURCE_LAYOUT_RELEASE, SOURCE_LAYOUT_STANDALONE}


class CaptureError(RuntimeError):
    """A local preflight failure that must not create an admitted capture."""


class RecorderConsoleStatus:
    """Hash-bound identity for a bounded external console transcript."""

    def __init__(self, total_bytes: int, sha256: str, retained_bytes: int,
                 retained_sha256: str, over_limit: bool) -> None:
        self.total_bytes = total_bytes
        self.sha256 = sha256
        self.retained_bytes = retained_bytes
        self.retained_sha256 = retained_sha256
        self.over_limit = over_limit

    @property
    def truncated(self) -> bool:
        return self.total_bytes != self.retained_bytes


def sha256_file(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    retained_digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
            size += len(block)
    return digest.hexdigest(), size


def require_absolute_regular_file(path: Path, label: str, *, executable: bool = False) -> Path:
    if not path.is_absolute():
        raise CaptureError(f"{label} path must be absolute")
    try:
        info = path.lstat()
    except OSError as error:
        raise CaptureError(f"Unable to stat {label}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError(f"{label} must be a non-symlink regular file")
    if executable and not os.access(path, os.X_OK):
        raise CaptureError(f"{label} is not executable")
    return path


def validate_identity(path: Path, label: str, expected_hash: str, expected_size: int) -> tuple[str, int]:
    digest, size = sha256_file(path)
    if digest != expected_hash or size != expected_size:
        raise CaptureError(f"{label} is not the exact recognised source")
    return digest, size


def reviewed_recorder_hashes() -> dict[str, str]:
    """Keep historical evidence valid when another host build is reviewed."""
    hashes = {"reviewed-fs-uae": EXPECTED_RECORDER_SHA256,
            "reviewed-fs-uae-trv2-v10": TRV2_RECORDER_V10_SHA256,
            "reviewed-fs-uae-trv2-v11": TRV2_RECORDER_V11_SHA256,
            "reviewed-fs-uae-trv2-v12": TRV2_RECORDER_SHA256,
            "reviewed-fs-uae-trv2-v13": TRV2_RECORDER_V13_SHA256,
            "reviewed-fs-uae-trv2-v14": TRV2_RECORDER_V14_SHA256,
            "reviewed-fs-uae-trv2-v15": TRV2_RECORDER_V15_SHA256,
            "reviewed-fs-uae-trv2-v16": TRV2_RECORDER_V16_SHA256,
            "reviewed-fs-uae-trv2-v17": TRV2_RECORDER_V17_SHA256,
            "reviewed-fs-uae-trv2-v18": TRV2_RECORDER_V18_SHA256,
            "reviewed-fs-uae-trv2-v19": TRV2_RECORDER_V19_SHA256,
            "reviewed-fs-uae-trv2-v20": TRV2_RECORDER_V20_SHA256}
    hashes["reviewed-fs-uae-trv2-v21"] = TRV2_RECORDER_V21_SHA256
    hashes["reviewed-fs-uae-trv2-v22"] = TRV2_RECORDER_V22_SHA256
    hashes["reviewed-fs-uae-trv2-v23"] = TRV2_RECORDER_V23_SHA256
    hashes["reviewed-fs-uae-trv2-v24"] = TRV2_RECORDER_V24_SHA256
    hashes["reviewed-fs-uae-trv2-v25"] = TRV2_RECORDER_V25_SHA256
    hashes["reviewed-fs-uae-trv2-v26-callback-memory"] = TRV2_RECORDER_V26_CALLBACK_MEMORY_SHA256
    hashes["reviewed-fs-uae-trv2-v27-callback-memory-targeted-display"] = (
        TRV2_RECORDER_V27_CALLBACK_MEMORY_TARGETED_DISPLAY_SHA256)
    hashes["reviewed-fs-uae-trv2-v29-service-boundary-display"] = (
        TRV2_RECORDER_V29_SERVICE_BOUNDARY_DISPLAY_SHA256)
    hashes["reviewed-fs-uae-trv2-v30-service-return-display"] = (
        TRV2_RECORDER_V30_SERVICE_RETURN_DISPLAY_SHA256)
    hashes["reviewed-fs-uae-trv2-v31-response-queue"] = TRV2_RECORDER_V31_RESPONSE_QUEUE_SHA256
    hashes["reviewed-fs-uae-trv2-v32-title-callback"] = TRV2_RECORDER_V32_TITLE_CALLBACK_SHA256
    hashes["reviewed-fs-uae-trv2-v33-service-boundary"] = TRV2_RECORDER_V33_SERVICE_BOUNDARY_SHA256
    return hashes


def raw_pc_sites_for_format(raw_format: str) -> tuple[int, ...]:
    """Keep the historical v15 grammar separate from the v16 observer site."""
    if raw_format == "v9-v18-phased":
        return RAW_PC_V18_SITES
    if raw_format == "v9-v19-phased":
        return RAW_PC_V19_SITES
    return RAW_PC_V16_SITES if raw_format in {"v9-v16", "v9-v16-phased"} else RAW_PC_SITES


def validate_recorder(path: Path) -> tuple[str, int]:
    digest, size = sha256_file(path)
    if digest not in reviewed_recorder_hashes().values():
        expected = ", ".join(sorted(reviewed_recorder_hashes().values()))
        raise CaptureError(
            "recorder hash does not match a reviewed FS-UAE binary "
            f"(expected SHA-256 {expected}, got {digest}); select "
            "the reviewed external recorder rather than a normal FS-UAE installation")
    if digest == TRV2_RECORDER_V16_SHA256 and size != TRV2_RECORDER_V16_SIZE:
        raise CaptureError("v16 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V17_SHA256 and size != TRV2_RECORDER_V17_SIZE:
        raise CaptureError("v17 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V18_SHA256 and size != TRV2_RECORDER_V18_SIZE:
        raise CaptureError("v18 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V19_SHA256 and size != TRV2_RECORDER_V19_SIZE:
        raise CaptureError("v19 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V20_SHA256 and size != TRV2_RECORDER_V20_SIZE:
        raise CaptureError("v20 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V21_SHA256 and size != TRV2_RECORDER_V21_SIZE:
        raise CaptureError("v21 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V22_SHA256 and size != TRV2_RECORDER_V22_SIZE:
        raise CaptureError("v22 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V23_SHA256 and size != TRV2_RECORDER_V23_SIZE:
        raise CaptureError("v23 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V24_SHA256 and size != TRV2_RECORDER_V24_SIZE:
        raise CaptureError("v24 recorder size does not match the reviewed binary")
    if digest == TRV2_RECORDER_V25_SHA256 and size != TRV2_RECORDER_V25_SIZE:
        raise CaptureError("v25 recorder size does not match the reviewed binary")
    if (digest == TRV2_RECORDER_V26_CALLBACK_MEMORY_SHA256
            and size != TRV2_RECORDER_V26_CALLBACK_MEMORY_SIZE):
        raise CaptureError("v26 callback-memory recorder size does not match the reviewed binary")
    if (digest == TRV2_RECORDER_V27_CALLBACK_MEMORY_TARGETED_DISPLAY_SHA256
            and size != TRV2_RECORDER_V27_CALLBACK_MEMORY_TARGETED_DISPLAY_SIZE):
        raise CaptureError("v27 targeted-display recorder size does not match the reviewed binary")
    if (digest == TRV2_RECORDER_V29_SERVICE_BOUNDARY_DISPLAY_SHA256
            and size != TRV2_RECORDER_V29_SERVICE_BOUNDARY_DISPLAY_SIZE):
        raise CaptureError("v29 service-boundary recorder size does not match the reviewed binary")
    if (digest == TRV2_RECORDER_V30_SERVICE_RETURN_DISPLAY_SHA256
            and size != TRV2_RECORDER_V30_SERVICE_RETURN_DISPLAY_SIZE):
        raise CaptureError("v30 service-return recorder size does not match the reviewed binary")
    if (digest == TRV2_RECORDER_V32_TITLE_CALLBACK_SHA256
            and size != TRV2_RECORDER_V32_TITLE_CALLBACK_SIZE):
        raise CaptureError("v32 title-callback recorder size does not match the reviewed binary")
    if (digest == TRV2_RECORDER_V33_SERVICE_BOUNDARY_SHA256
            and size != TRV2_RECORDER_V33_SERVICE_BOUNDARY_SIZE):
        raise CaptureError("v33 service-boundary recorder size does not match the reviewed binary")
    if (digest == TRV2_RECORDER_V31_RESPONSE_QUEUE_SHA256
            and size != TRV2_RECORDER_V31_RESPONSE_QUEUE_SIZE):
        raise CaptureError("v31 response-queue recorder size does not match the reviewed binary")
    return digest, size


def is_system_tmp_path(path: Path) -> bool:
    """Reject the operator's `/tmp` spelling before host path resolution.

    `/tmp` becomes `/private/tmp` on macOS and a drive-rooted path on Windows,
    so a resolved-path-only test would make the external-evidence contract
    host dependent.
    """
    normalized = path.as_posix().replace("\\", "/")
    if normalized == "/tmp" or normalized.startswith("/tmp/"):
        return True
    parts = path.parts
    return bool(path.anchor and len(parts) > 1 and parts[1].casefold() == "tmp")


def reject_unsafe_output(*sources: Path, output: Path) -> Path:
    # Reject this contractual spelling before Windows can classify its
    # POSIX-looking form as relative to the current drive.
    if is_system_tmp_path(output):
        raise CaptureError("output must not use /tmp; use a Project Eon cache path")
    if not output.is_absolute():
        raise CaptureError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise CaptureError("output directory must not exist")
    resolved = output.resolve(strict=False)
    if resolved == ROOT or ROOT in resolved.parents:
        raise CaptureError("output must stay outside the repository")
    if any(source.parent == resolved or source.parent in resolved.parents for source in sources):
        raise CaptureError("output must stay outside supplied-media directories")
    if not resolved.parent.is_dir() or resolved.parent.is_symlink():
        raise CaptureError("output parent must be an existing non-symlink directory")
    return resolved


def require_visible_operator_input(environment: dict[str, str]) -> None:
    if environment.get("SDL_VIDEODRIVER", "").lower() == "dummy":
        raise CaptureError("headless SDL is forbidden; a physical operator must use the visible emulator window")
    if not environment.get("DISPLAY") and not environment.get("WAYLAND_DISPLAY"):
        raise CaptureError("a visible X11 or Wayland display is required for physical input capture")
    # DISPLAY can be set while X11 authentication still prevents SDL from
    # creating a window. When the standard probe is installed, verify the
    # connection before mounting media or starting the reviewed recorder.
    display = environment.get("DISPLAY")
    probe = shutil.which("xdpyinfo") if display else None
    if probe:
        try:
            result = subprocess.run([probe, "-display", display], env=environment,
                capture_output=True, text=True, timeout=5, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise CaptureError("unable to verify visible X11 display access") from error
        if result.returncode != 0:
            raise CaptureError("X11 display is inaccessible; check DISPLAY and XAUTHORITY")


def mount_options(mountpoint: Path) -> set[str]:
    completed = subprocess.run(
        ["findmnt", "-T", str(mountpoint), "-no", "OPTIONS"], check=True,
        capture_output=True, text=True)
    return set(completed.stdout.strip().split(","))


def mountpoint_is_active(mountpoint: Path) -> bool:
    """Return whether this exact path, not an ancestor, is still mounted."""
    try:
        completed = subprocess.run(
            ["findmnt", "-n", "--mountpoint", str(mountpoint), "-o", "TARGET"],
            check=False, capture_output=True, text=True)
    except FileNotFoundError:
        # This Linux-specific FUSE cleanup probe has no Windows equivalent.
        # A physical Amiga capture cannot run there, while test preflight must
        # remain a safe negative result rather than a host-tool crash.
        return False
    return completed.returncode == 0 and completed.stdout.strip() == str(mountpoint)


def unmount(mountpoint: Path) -> None:
    """Unmount and prove the FUSE view is gone before reusing the evidence root."""
    if not mountpoint_is_active(mountpoint):
        return
    completed = subprocess.run(["fusermount", "-u", str(mountpoint)], check=False,
                               capture_output=True, text=True)
    if completed.returncode != 0 or mountpoint_is_active(mountpoint):
        raise CaptureError(f"unable to unmount read-only capture view: {mountpoint}")


def mount_read_only(source: Path, mountpoint: Path) -> None:
    try:
        subprocess.run(["archivemount", "-o", "ro", str(source), str(mountpoint)], check=True)
        required = {"ro", "nosuid", "nodev", "default_permissions"}
        if not required <= mount_options(mountpoint):
            raise CaptureError("archivemount did not report required read-only safety options")
    except Exception:
        unmount(mountpoint)
        raise


def write_exclusive(path: Path, content: str) -> None:
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())


def input_receipt_status(path: Path) -> str:
    """Describe a recorder-created host-input receipt without creating one.

    An absent receipt proves neither a user action nor a game-input outcome;
    it only means the recorder did not observe a host input event.  Keeping
    that distinction in the external status prevents a no-input preflight
    from looking like an interactive capture.
    """
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "host_input_receipt=absent\n"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("host-input receipt is not a regular non-symlink file")
    if info.st_size > MAX_INPUT_RECEIPT_BYTES:
        raise CaptureError("host-input receipt exceeds the bounded recorder contract")
    if info.st_size == 0:
        return "host_input_receipt=empty\n"
    record_count = parse_host_input_receipt(path)
    digest, size = sha256_file(path)
    return ("host_input_receipt=present\n"
            f"host_input_receipt_sha256={digest}\n"
            f"host_input_receipt_bytes={size}\n"
            f"host_input_receipt_records={record_count}\n")


def capture_intent_status(intent: str, receipt_status: str, observed_during_capture: bool) -> str:
    """Fail closed when the stated physical-input/no-input session disagrees.

    The receipt only proves FS-UAE's host-to-core delivery observation.  It
    does not infer that Deuteros polled, accepted, or interpreted that input.
    """
    if intent not in CAPTURE_INTENTS:
        raise CaptureError("capture intent is not in the reviewed finite set")
    fields = dict(line.split("=", 1) for line in receipt_status.splitlines())
    state = fields.get("host_input_receipt")
    if intent == "physical-input":
        if state != "present" or not observed_during_capture:
            raise CaptureError("physical-input capture requires an observed non-empty host-input receipt")
        requirement = "required"
    else:
        if state not in {"absent", "empty"} or observed_during_capture:
            raise CaptureError("diagnostic-no-input capture must not retain host input")
        requirement = "forbidden"
    return (f"capture_intent={intent}\n"
            f"capture_intent_input_requirement={requirement}\n")


def capture_operator_instructions(intent: str, *, late_sampling: bool = False) -> tuple[str, str, str]:
    """Return visible instructions that cannot contradict the capture intent."""
    if intent == "physical-input":
        return (
            "CAPTURE PREPARED  read-only original media; physical operator input required",
            "Focus the visible FS-UAE window by clicking it yourself; do not use terminal or automation input. Advance the visible intro with ordinary mapped keys (press and release Return or Space only while the game is visibly waiting). Choose 1: ENGLISH only when the language selector is on screen. At the visible Disk 2 prompt, press F10 once to request insertion of the already-mounted, read-only second image into DF0, then continue only in response to visible prompts." +
            (" Continue beyond the language selector through the later visible route; the separate late probe begins after host-input ordinal 8 and records at most 96 distinct deliveries per site." if late_sampling else ""),
            "No debugger, playback, injected host event, or guest-memory edit is permitted.",
        )
    if intent == "diagnostic-no-input":
        return (
            "CAPTURE PREPARED  read-only original media; diagnostic no-input capture",
            "The FS-UAE window remains visible for observation only. Do not click it or press any key in it.",
            "No host input, debugger, playback, injected host event, or guest-memory edit is permitted.",
        )
    raise CaptureError("capture intent is not in the reviewed finite set")


def parse_host_input_records(path: Path) -> list[tuple[int, int]]:
    """Validate finite host-to-core delivery records and retain ordinal/frame."""
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("host-input receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("host-input receipt has a truncated final record")
    records: list[tuple[int, int]] = []
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = HOST_INPUT_LINE.fullmatch(line)
        if not match:
            raise CaptureError("host-input receipt contains an invalid recorder record")
        if int(match.group(1)) != expected:
            raise CaptureError("host-input receipt record ordinals are not contiguous")
        records.append((int(match.group(1)), int(match.group(2))))
        if len(records) > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("host-input receipt exceeds the recorder record cap")
    return records


def parse_host_input_receipt(path: Path) -> int:
    """Validate only the recorder's finite host-to-core delivery grammar."""
    return len(parse_host_input_records(path))


def raw_observation_status(path: Path, name: str, raw_format: str = "legacy") -> str:
    """Bind strict raw-PC observations without assigning runtime semantics."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        return f"{name}=absent\n"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError(f"{name} is not a regular non-symlink file")
    if info.st_size > MAX_RAW_OBSERVATION_BYTES:
        raise CaptureError(f"{name} exceeds the bounded recorder contract")
    if info.st_size == 0:
        return f"{name}=empty\n"
    site_counts, site_opcode_pairs, input_links, phase_counts = _parse_raw_pc(path, raw_format)
    digest, size = sha256_file(path)
    sites = raw_pc_sites_for_format(raw_format)
    ordered_counts = ",".join(
        f"0x{site:08x}:{site_counts[site]}" for site in sites if site in site_counts)
    status = (f"{name}=present\n{name}_sha256={digest}\n{name}_bytes={size}\n"
              f"{name}_format={raw_format}\n{name}_records={sum(site_counts.values())}\n"
              f"{name}_site_counts={ordered_counts}\n")
    if raw_format in {"v7", "v9", "v9-v16", "v9-v16-phased", "v9-v18-phased", "v9-v19-phased"}:
        ordered_pairs = ",".join(
            f"0x{site:08x}:" + "+".join(
                f"{ir:04x}/{memory:04x}" for ir, memory in sorted(site_opcode_pairs[site]))
            for site in sites if site in site_opcode_pairs)
        status += f"{name}_opcode_pairs={ordered_pairs}\n"
    if raw_format in {"v9", "v9-v16", "v9-v16-phased", "v9-v18-phased", "v9-v19-phased"}:
        status += (f"{name}_input_links={sum(ordinal != 0 for ordinal, _ in input_links)}\n"
                   f"{name}_last_input_ordinal={input_links[-1][0] if input_links else 0}\n")
        for phase, label in ((0, "pre_input"), (1, "post_input")):
            ordered_phase_counts = ",".join(
                f"0x{site:08x}:{phase_counts[phase][site]}"
                for site in sites if phase_counts[phase].get(site, 0))
            status += f"{name}_{label}_site_counts={ordered_phase_counts}\n"
    return status


def parse_raw_pc_observations(path: Path, raw_format: str = "legacy") -> dict[int, int]:
    """Validate the recorder grammar and return reachability counts only.

    This deliberately does not use register values as recovered ABI facts.
    It makes malformed, reordered, over-cap, or unreviewed-site recorder
    output a failed external capture instead of an opaque hash-bound blob.
    """
    return parse_raw_pc_summary(path, raw_format)[0]


def _parse_raw_pc(
    path: Path, raw_format: str
) -> tuple[
    dict[int, int],
    dict[int, set[tuple[int, int]]],
    list[tuple[int, int]],
    tuple[dict[int, int], dict[int, int]],
]:
    """Validate raw records and retain opaque v7/v9 fields per probe site."""
    if raw_format == "legacy":
        matcher = RAW_PC_LEGACY_LINE
    elif raw_format == "v7":
        matcher = RAW_PC_V7_LINE
    elif raw_format in {"v9", "v9-v16", "v9-v16-phased", "v9-v18-phased", "v9-v19-phased"}:
        matcher = RAW_PC_V9_LINE
    else:
        raise CaptureError("raw_pc format is not a reviewed recorder grammar")
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("raw_pc is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("raw_pc has a truncated final record")
    counts: dict[int, int] = {}
    phase_counts: tuple[dict[int, int], dict[int, int]] = ({}, {})
    opcode_pairs: dict[int, set[tuple[int, int]]] = {}
    input_links: list[tuple[int, int]] = []
    previous_cycle = -1
    previous_v18_sample: tuple[int, int, int] | None = None
    for expected_ordinal, line in enumerate(text.splitlines(keepends=True), start=1):
        match = matcher.fullmatch(line)
        if not match:
            raise CaptureError("raw_pc contains an invalid recorder record")
        ordinal, cycle, site = int(match.group(1)), int(match.group(2)), int(match.group(3), 16)
        if ordinal != expected_ordinal:
            raise CaptureError("raw_pc record ordinals are not contiguous")
        if cycle < previous_cycle:
            raise CaptureError("raw_pc cycles are not monotonic")
        previous_cycle = cycle
        if site not in raw_pc_sites_for_format(raw_format):
            raise CaptureError("raw_pc uses an unreviewed probe site")
        counts[site] = counts.get(site, 0) + 1
        if raw_format in {"v7", "v9", "v9-v16", "v9-v16-phased", "v9-v18-phased", "v9-v19-phased"}:
            if raw_format in {"v9-v18-phased", "v9-v19-phased"} and site == 0x00021866 and int(match.group(5), 16) != 0x6608:
                raise CaptureError("v18 input-branch site has an unexpected memory opcode")
            opcode_pairs.setdefault(site, set()).add((int(match.group(4), 16), int(match.group(5), 16)))
        if raw_format in {"v9", "v9-v16", "v9-v16-phased", "v9-v18-phased", "v9-v19-phased"}:
            input_ordinal, input_frame = int(match.group(10)), int(match.group(11))
            if input_ordinal > MAX_INPUT_RECEIPT_RECORDS:
                raise CaptureError("raw_pc exceeds the input-recorder ordinal cap")
            if input_links and input_ordinal < input_links[-1][0]:
                raise CaptureError("raw_pc input ordinals are not monotonic")
            if input_ordinal == 0 and input_frame != 0:
                raise CaptureError("raw_pc no-input snapshot must use frame zero")
            if raw_format in {"v9-v18-phased", "v9-v19-phased"} and site == 0x00021866:
                if previous_v18_sample != (0x0002185E, input_ordinal, input_frame):
                    raise CaptureError("v18 branch sample is not paired with the same input-linked bit-test")
            if raw_format in {"v9-v18-phased", "v9-v19-phased"}:
                previous_v18_sample = (site, input_ordinal, input_frame)
            input_links.append((input_ordinal, input_frame))
            phase = 1 if input_ordinal else 0
            phase_counts[phase][site] = phase_counts[phase].get(site, 0) + 1
            if phase_counts[phase][site] > MAX_RAW_RECORDS_PER_SITE:
                raise CaptureError("raw_pc exceeds the per-site phase cap")
            if raw_format == "v9-v16" and counts[site] > MAX_RAW_RECORDS_PER_SITE:
                raise CaptureError("raw_pc exceeds the per-site recorder cap")
            if raw_format in {"v9-v16-phased", "v9-v18-phased", "v9-v19-phased"} and counts[site] > MAX_RAW_RECORDS_PER_SITE * 2:
                raise CaptureError("raw_pc exceeds the per-site recorder cap")
        elif counts[site] > MAX_RAW_RECORDS_PER_SITE:
            raise CaptureError("raw_pc exceeds the per-site recorder cap")
        if expected_ordinal > MAX_RAW_RECORDS:
            raise CaptureError("raw_pc exceeds the recorder record cap")
    return counts, opcode_pairs, input_links, phase_counts


def parse_raw_pc_summary(path: Path, raw_format: str = "legacy") -> tuple[dict[int, int], dict[int, set[tuple[int, int]]]]:
    """Validate raw records and retain opaque IR/memory pairs per probe site."""
    counts, opcode_pairs, _, _ = _parse_raw_pc(path, raw_format)
    return counts, opcode_pairs


def parse_raw_pc_input_links(path: Path, raw_format: str = "v9") -> list[tuple[int, int]]:
    """Return v9 delivery chronology without promoting it to guest input proof."""
    return _parse_raw_pc(path, raw_format)[2]


def parse_raw_pc_phase_counts(path: Path, raw_format: str = "v9") -> tuple[dict[int, int], dict[int, int]]:
    """Return bounded pre/post-delivery reachability counts for v9 records."""
    return _parse_raw_pc(path, raw_format)[3]


def _late_raw_pc_contract(late_version: str) -> tuple[tuple[int, ...], int]:
    if late_version == "v19":
        return LATE_RAW_PC_V19_SITES, MAX_LATE_RAW_RECORDS_V19
    if late_version == "v20":
        return LATE_RAW_PC_V20_SITES, MAX_LATE_RAW_RECORDS_V20
    raise CaptureError("late raw-PC version is not a reviewed recorder contract")


def late_raw_pc_version_for_recorder(recorder_sha256: str) -> str:
    """Select only the late-site grammar reviewed for this recorder build."""
    if recorder_sha256 == TRV2_RECORDER_V19_SHA256:
        return "v19"
    if recorder_sha256 in {
        TRV2_RECORDER_V20_SHA256, TRV2_RECORDER_V21_SHA256,
        TRV2_RECORDER_V22_SHA256, TRV2_RECORDER_V23_SHA256,
        TRV2_RECORDER_V24_SHA256, TRV2_RECORDER_V25_SHA256,
        TRV2_RECORDER_V26_CALLBACK_MEMORY_SHA256,
        TRV2_RECORDER_V27_CALLBACK_MEMORY_TARGETED_DISPLAY_SHA256,
        TRV2_RECORDER_V29_SERVICE_BOUNDARY_DISPLAY_SHA256,
        TRV2_RECORDER_V30_SERVICE_RETURN_DISPLAY_SHA256,
        TRV2_RECORDER_V31_RESPONSE_QUEUE_SHA256,
        TRV2_RECORDER_V32_TITLE_CALLBACK_SHA256,
        TRV2_RECORDER_V33_SERVICE_BOUNDARY_SHA256,
    }:
        return "v20"
    raise CaptureError("recorder has no reviewed late raw-PC contract")


def parse_late_raw_pc_receipt(
    path: Path, late_version: str = "v19"
) -> list[tuple[int, int, int, int, int]]:
    """Validate the finite post-ordinal-eight raw-PC window, preserving opaque values."""
    allowed_sites, max_records = _late_raw_pc_contract(late_version)
    try:
        info = path.lstat()
    except OSError as error:
        raise CaptureError(f"late raw-PC receipt is unavailable: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("late raw-PC receipt is not a regular non-symlink file")
    if info.st_size > MAX_LATE_RAW_BYTES:
        raise CaptureError("late raw-PC receipt exceeds the bounded recorder contract")
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("late raw-PC receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("late raw-PC receipt has a truncated final record")
    records: list[tuple[int, int, int, int, int]] = []
    site_counts: dict[int, int] = {}
    last_site_input: dict[int, int] = {}
    previous_cycle = -1
    previous_input = 0
    previous_input_site: tuple[int, int, int] | None = None
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = LATE_RAW_PC_LINE.fullmatch(line)
        if not match:
            raise CaptureError("late raw-PC receipt contains an invalid recorder record")
        record, cycles, site = int(match.group(1)), int(match.group(2)), int(match.group(3), 16)
        input_ordinal, input_frame = int(match.group(10)), int(match.group(11))
        if record != expected:
            raise CaptureError("late raw-PC record ordinals are not contiguous")
        if cycles < previous_cycle:
            raise CaptureError("late raw-PC cycles are not monotonic")
        if site not in allowed_sites:
            raise CaptureError("late raw-PC uses an unreviewed probe site")
        if not LATE_INPUT_START_ORDINAL <= input_ordinal <= MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("late raw-PC input ordinal is outside the reviewed late window")
        if input_ordinal < previous_input:
            raise CaptureError("late raw-PC input ordinals are not monotonic")
        if input_ordinal <= last_site_input.get(site, 0):
            raise CaptureError("late raw-PC repeats an input ordinal at one probe site")
        if site == 0x00021866 and int(match.group(5), 16) != 0x6608:
            raise CaptureError("late v19 input-branch site has an unexpected memory opcode")
        if site == 0x00021866 and previous_input_site != (0x0002185E, input_ordinal, input_frame):
            raise CaptureError("late v19 branch sample is not paired with its input-linked bit-test")
        if site == 0x0002185E:
            previous_input_site = (site, input_ordinal, input_frame)
        elif site == 0x00021866:
            previous_input_site = (site, input_ordinal, input_frame)
        else:
            previous_input_site = None
        site_counts[site] = site_counts.get(site, 0) + 1
        if site_counts[site] > MAX_LATE_RAW_RECORDS_PER_SITE:
            raise CaptureError("late raw-PC receipt exceeds the per-site sample cap")
        if expected > max_records:
            raise CaptureError("late raw-PC receipt exceeds the total sample cap")
        records.append((record, cycles, site, input_ordinal, input_frame))
        previous_cycle = cycles
        previous_input = input_ordinal
        last_site_input[site] = input_ordinal
    return records


def _optional_bounded_file(path: Path, label: str, limit: int) -> tuple[str, tuple[str, int] | None]:
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "absent", None
    except OSError as error:
        raise CaptureError(f"{label} is unavailable: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError(f"{label} is not a regular non-symlink file")
    if info.st_size > limit:
        raise CaptureError(f"{label} exceeds the bounded recorder contract")
    if info.st_size == 0:
        return "empty", None
    return "present", sha256_file(path)


def late_raw_pc_status(path: Path, input_path: Path, late_version: str = "v19") -> str:
    """Hash and link the late sidecar to host deliveries without interpreting actions."""
    allowed_sites, _ = _late_raw_pc_contract(late_version)
    state, identity = _optional_bounded_file(path, "late raw-PC receipt", MAX_LATE_RAW_BYTES)
    if state == "absent":
        return ("late_input_pc=absent\nlate_input_pc_format=v1\nlate_input_pc_records=0\n"
                "late_input_pc_site_counts=\nlate_input_pc_input_links=0\n"
                "late_input_pc_last_input_ordinal=0\nlate_input_pc_input_chronology=none\n"
                "late_input_pc_input_chronology_records=0\n")
    if state == "empty":
        return ("late_input_pc=empty\nlate_input_pc_format=v1\nlate_input_pc_records=0\n"
                "late_input_pc_site_counts=\nlate_input_pc_input_links=0\n"
                "late_input_pc_last_input_ordinal=0\nlate_input_pc_input_chronology=none\n"
                "late_input_pc_input_chronology_records=0\n")
    records = parse_late_raw_pc_receipt(path, late_version)
    events = dict(parse_host_input_records(input_path))
    for _, _, _, input_ordinal, input_frame in records:
        if events.get(input_ordinal) != input_frame:
            raise CaptureError("late raw-PC chronology does not match the host-input receipt")
    counts: dict[int, int] = {}
    for _, _, site, _, _ in records:
        counts[site] = counts.get(site, 0) + 1
    assert identity is not None
    digest, size = identity
    links = len(records)
    return ("late_input_pc=present\nlate_input_pc_format=v1\n"
            f"late_input_pc_sha256={digest}\nlate_input_pc_bytes={size}\n"
            f"late_input_pc_records={len(records)}\n"
            "late_input_pc_site_counts=" + ",".join(
                f"0x{site:08x}:{counts[site]}" for site in allowed_sites if site in counts) + "\n"
            f"late_input_pc_input_links={links}\n"
            f"late_input_pc_last_input_ordinal={records[-1][3]}\n"
            f"late_input_pc_input_chronology={'linked' if links else 'none'}\n"
            f"late_input_pc_input_chronology_records={links}\n")


def parse_late_selector_dispatch(path: Path) -> list[tuple[int, int, int, int, int, int, int]]:
    """Parse later selector-cell reads joined to late raw-PC records."""
    state, _ = _optional_bounded_file(path, "late selector-dispatch receipt", MAX_LATE_SELECTOR_BYTES)
    if state != "present":
        return []
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("late selector-dispatch receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("late selector-dispatch receipt has a truncated final record")
    records: list[tuple[int, int, int, int, int, int, int]] = []
    previous_cycle = -1
    previous_raw = 0
    previous_input = LATE_INPUT_START_ORDINAL - 1
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = LATE_SELECTOR_DISPATCH_LINE.fullmatch(line)
        if not match:
            raise CaptureError("late selector-dispatch receipt contains an invalid recorder record")
        ordinal, raw_ordinal, cycles = int(match.group(1)), int(match.group(2)), int(match.group(3))
        cell_a, cell_b = int(match.group(4), 16), int(match.group(5), 16)
        input_ordinal, input_frame = int(match.group(6)), int(match.group(7))
        if ordinal != expected or ordinal > MAX_LATE_SELECTOR_RECORDS:
            raise CaptureError("late selector-dispatch ordinals exceed the reviewed sample cap")
        if raw_ordinal <= previous_raw or cycles < previous_cycle:
            raise CaptureError("late selector-dispatch order is not monotonic")
        if not LATE_INPUT_START_ORDINAL <= input_ordinal <= MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("late selector-dispatch input ordinal is outside the late window")
        if input_ordinal <= previous_input:
            raise CaptureError("late selector-dispatch repeats an input ordinal")
        records.append((ordinal, raw_ordinal, cycles, cell_a, cell_b, input_ordinal, input_frame))
        previous_raw, previous_cycle, previous_input = raw_ordinal, cycles, input_ordinal
    return records


def late_selector_dispatch_status(
    dispatch_path: Path, raw_path: Path, input_path: Path, late_version: str = "v19"
) -> str:
    """Require a one-to-one join from late selector PCs to late cell reads."""
    state, identity = _optional_bounded_file(
        dispatch_path, "late selector-dispatch receipt", MAX_LATE_SELECTOR_BYTES)
    raw_state, _ = _optional_bounded_file(raw_path, "late raw-PC receipt", MAX_LATE_RAW_BYTES)
    raw_records = parse_late_raw_pc_receipt(raw_path, late_version) if raw_state == "present" else []
    raw_links = [(record, cycles, input_ordinal, input_frame)
                 for record, cycles, site, input_ordinal, input_frame in raw_records
                 if site == 0x0001FBE6]
    if state != "present":
        if raw_links:
            raise CaptureError("late selector-dispatch receipt omits a reached raw-PC site")
        return f"late_selector_dispatch={state}\nlate_selector_dispatch_format=v1\nlate_selector_dispatch_records=0\n"
    records = parse_late_selector_dispatch(dispatch_path)
    dispatch_links = [(record[1], record[2], record[5], record[6]) for record in records]
    if dispatch_links != raw_links:
        raise CaptureError("late selector-dispatch rows do not match late raw-PC chronology")
    events = dict(parse_host_input_records(input_path))
    for _, _, _, _, _, input_ordinal, input_frame in records:
        if events.get(input_ordinal) != input_frame:
            raise CaptureError("late selector-dispatch chronology does not match host input")
    if not records:
        return "late_selector_dispatch=empty\nlate_selector_dispatch_format=v1\nlate_selector_dispatch_records=0\n"
    assert identity is not None
    digest, size = identity
    return ("late_selector_dispatch=present\nlate_selector_dispatch_format=v1\n"
            f"late_selector_dispatch_sha256={digest}\nlate_selector_dispatch_bytes={size}\n"
            f"late_selector_dispatch_records={len(records)}\n"
            f"late_selector_dispatch_raw_pc_links={len(dispatch_links)}\n"
            f"late_selector_dispatch_input_links={len(records)}\n"
            f"late_selector_dispatch_last_input_ordinal={records[-1][5]}\n"
            "late_selector_dispatch_input_chronology=linked\n"
            f"late_selector_dispatch_input_chronology_records={len(records)}\n")


def parse_negative_selector_receipt(path: Path) -> list[dict[str, int | str]]:
    """Parse schema 35's independent negative-selector trace grammar."""
    state, _ = _optional_bounded_file(
        path, "negative-selector receipt", MAX_NEGATIVE_SELECTOR_BYTES)
    if state != "present":
        return []
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("negative-selector receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("negative-selector receipt has a truncated final record")
    records: list[dict[str, int | str]] = []
    routes: dict[int, list[dict[str, int | str]]] = {}
    previous_cycle = -1
    previous_route = 0
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = NEGATIVE_SELECTOR_LINE.fullmatch(line)
        if not match:
            raise CaptureError("negative-selector receipt contains an invalid recorder record")
        (ordinal, route, cycles, phase, pc, opcode0, opcode1, opcode_valid, d0, d1, sr,
         selector, secondary, input_ordinal, input_frame) = (
            int(match.group(1)), int(match.group(2)), int(match.group(3)), match.group(4),
            int(match.group(5), 16), int(match.group(6), 16), int(match.group(7), 16),
            int(match.group(8)), int(match.group(9), 16), int(match.group(10), 16),
            int(match.group(11), 16), int(match.group(12), 16), int(match.group(13), 16),
            int(match.group(14)), int(match.group(15)))
        if opcode_valid != 1:
            raise CaptureError("negative-selector opcode words are not valid")
        if ordinal != expected or route > MAX_NEGATIVE_SELECTOR_ROUTES:
            raise CaptureError("negative-selector ordinal or route exceeds its reviewed cap")
        if cycles > 0xFFFFFFFFFFFFFFFF or cycles <= previous_cycle:
            raise CaptureError("negative-selector cycles are not strictly increasing")
        if pc != NEGATIVE_SELECTOR_PHASE_PC[phase]:
            raise CaptureError("negative-selector phase has an unexpected PC")
        if selector < 0x80:
            raise CaptureError("negative-selector row is not on the signed-negative route")
        if phase == "branch":
            # The observed instruction is BPL.W $1fc9c. The negative cell sets
            # N, so the branch is not taken and execution eventually reaches
            # the local helper/call path.
            if opcode0 != 0x6A0000AC or not (sr & 0x0008):
                raise CaptureError("negative-selector branch opcode, target, or N flag is invalid")
            target = (pc + 2 + ((opcode0 & 0xFFFF) - 0x10000
                                if opcode0 & 0x8000 else opcode0 & 0xFFFF)) & 0xFFFFFFFF
            if target != 0x1FC9C:
                raise CaptureError("negative-selector branch target is invalid")
            if (opcode1 >> 16) != 0x2F00 and (opcode1 & 0xFFFF) != 0x6100:
                raise CaptureError("negative-selector branch fallthrough opcode is invalid")
        if phase == "gate":
            # CMPI.W #$20,D0; BEQ.B $1fc12. The compare's Z flag alone
            # selects the skipped-service path.
            gate_word = opcode0 >> 16
            gate_displacement = gate_word & 0xFF
            if gate_displacement & 0x80:
                gate_displacement -= 0x100
            if ((gate_word & 0xFF00) != 0x6700
                    or ((pc + 2 + gate_displacement) & 0xFFFFFFFF) != 0x1FC12):
                raise CaptureError("negative-selector gate opcode or branch target is invalid")
            if (opcode0 & 0xFFFF) != 0x7013 or (opcode1 >> 16) != 0x720C:
                raise CaptureError("negative-selector gate context opcodes are invalid")
            zero = bool(sr & 0x0004)
            if zero != ((d0 & 0xFFFF) == 0x20):
                raise CaptureError("negative-selector service gate flags do not match D0")
        if phase == "call":
            if opcode0 != 0x4EB90003 or (opcode1 >> 16) != 0xFBF8:
                raise CaptureError("negative-selector call opcode or target is invalid")
            if d0 != 0x13 or d1 != 0x0C:
                raise CaptureError("negative-selector service arguments are invalid")
        if phase == "callee":
            if d0 != 0x13 or d1 != 0x0C:
                raise CaptureError("negative-selector service arguments are invalid")
        if phase == "skip" and ((opcode0 >> 16) != 0x203C or (opcode1 >> 16) != 0x4E20):
            raise CaptureError("negative-selector skip path opcode context is invalid")
        if phase == "epilogue" and (opcode0 >> 16) != 0x4E75:
            raise CaptureError("negative-selector epilogue opcode is invalid")
        if phase == "caller" and opcode0 != 0x4A390001:
            raise CaptureError("negative-selector caller continuation opcode is invalid")
        if route < previous_route or route > previous_route + 1 or (not routes and route != 1):
            raise CaptureError("negative-selector routes are not contiguous")
        if route not in routes:
            if phase != "dispatch":
                raise CaptureError("negative-selector route does not begin at dispatch")
            routes[route] = []
        route_rows = routes[route]
        if len(route_rows) >= MAX_NEGATIVE_SELECTOR_PHASES_PER_ROUTE:
            raise CaptureError("negative-selector route exceeds its phase cap")
        phase_path = NEGATIVE_SELECTOR_PHASES
        if len(route_rows) >= 3:
            gate_row = route_rows[2]
            suppressed = bool(int(gate_row["sr"]) & 0x0004)
            if suppressed:
                phase_path = ("dispatch", "branch", "gate", "skip", "epilogue", "caller")
        if phase != phase_path[len(route_rows)]:
            raise CaptureError("negative-selector route phases are out of order")
        if route_rows:
            previous = route_rows[-1]
            if (input_ordinal, input_frame, selector, secondary) != (
                    int(previous["input_ordinal"]), int(previous["input_frame"]),
                    int(previous["selector"]), int(previous["secondary"])):
                raise CaptureError("negative-selector route fields changed between phases")
        elif phase != "dispatch":
            raise CaptureError("negative-selector route is incomplete before dispatch")
        route_rows.append({"ordinal": ordinal, "route": route, "cycles": cycles,
                           "phase": phase, "pc": pc, "opcode0": opcode0,
                           "opcode1": opcode1, "opcode_valid": opcode_valid,
                           "d0": d0, "d1": d1, "sr": sr,
                           "selector": selector, "secondary": secondary,
                           "input_ordinal": input_ordinal, "input_frame": input_frame})
        records.append(route_rows[-1])
        previous_cycle, previous_route = cycles, route

    return records


def negative_selector_status(path: Path, input_path: Path) -> str:
    """Hash and strictly join schema 35 rows to physical host input delivery."""
    state, identity = _optional_bounded_file(
        path, "negative-selector receipt", MAX_NEGATIVE_SELECTOR_BYTES)
    if state != "present":
        return (f"negative_selector={state}\nnegative_selector_format=v1\n"
                "negative_selector_records=0\nnegative_selector_routes=0\n")
    records = parse_negative_selector_receipt(path)
    host_events = dict(parse_host_input_records(input_path))
    for record in records:
        ordinal, frame = int(record["input_ordinal"]), int(record["input_frame"])
        if host_events.get(ordinal) != frame:
            raise CaptureError("negative-selector input ordinal/frame does not match host input")
    assert identity is not None
    digest, size = identity
    return ("negative_selector=present\nnegative_selector_format=v1\n"
            f"negative_selector_sha256={digest}\nnegative_selector_bytes={size}\n"
            f"negative_selector_records={len(records)}\n"
            f"negative_selector_routes={len({int(row['route']) for row in records})}\n"
            f"negative_selector_input_links={len(records)}\n")


def _read_callback_memory_receipt(path: Path) -> tuple[str, bytes, tuple[str, int] | None]:
    """Read the v26 sidecar through a bounded no-follow descriptor."""
    try:
        before = path.lstat()
    except FileNotFoundError:
        return "absent", b"", None
    except OSError as error:
        raise CaptureError(f"callback-memory receipt is unavailable: {error}") from error
    if stat.S_ISLNK(before.st_mode) or not stat.S_ISREG(before.st_mode):
        raise CaptureError("callback-memory receipt is not a regular non-symlink file")
    if before.st_size > MAX_CALLBACK_MEMORY_BYTES:
        raise CaptureError("callback-memory receipt exceeds the bounded recorder contract")
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            opened = os.fstat(stream.fileno())
            if (not stat.S_ISREG(opened.st_mode)
                    or (opened.st_dev, opened.st_ino) != (before.st_dev, before.st_ino)
                    or opened.st_size != before.st_size):
                raise CaptureError("callback-memory receipt changed during bounded open")
            payload = stream.read(MAX_CALLBACK_MEMORY_BYTES + 1)
    except OSError as error:
        raise CaptureError(f"callback-memory receipt is unavailable: {error}") from error
    if len(payload) > MAX_CALLBACK_MEMORY_BYTES:
        raise CaptureError("callback-memory receipt exceeds the bounded recorder contract")
    if len(payload) != before.st_size:
        raise CaptureError("callback-memory receipt changed while being read")
    if not payload:
        return "empty", payload, None
    return "present", payload, (hashlib.sha256(payload).hexdigest(), len(payload))


def _parse_callback_memory_payload(payload: bytes, input_path: Path) -> list[dict[str, int | str]]:
    """Validate ordered entry/exit and caller samples without naming their meaning."""
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("callback-memory receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("callback-memory receipt has a truncated final record")
    host_events = dict(parse_host_input_records(input_path))
    counts = {site: 0 for site in CALLBACK_MEMORY_TARGETS}
    previous_cycles = -1
    active_sequence = 0
    previous_input_ordinal = 0
    callback_entries: set[int] = set()
    callback_exits: set[int] = set()
    caller_stages: dict[int, int] = {}
    records: list[dict[str, int | str]] = []
    lines = text.splitlines(keepends=True)
    index = 0
    while index < len(lines):
        match = CALLBACK_MEMORY_SUMMARY_LINE.fullmatch(lines[index])
        if not match:
            raise CaptureError("callback-memory receipt contains an invalid sample record")
        groups = match.groups()
        record_ordinal, target, cycles, pc, opcode, sr, input_ordinal, input_frame = (
            int(groups[0]), int(groups[1], 16), int(groups[2]), int(groups[3], 16),
            int(groups[4], 16), int(groups[5], 16), int(groups[6]), int(groups[7]))
        if target not in counts or target != (pc & 0xFFFFFF):
            raise CaptureError("callback-memory receipt uses an unreviewed probe site")
        if record_ordinal != counts[target] + 1:
            raise CaptureError("callback-memory per-site ordinals are not contiguous")
        counts[target] += 1
        if counts[target] > MAX_CALLBACK_MEMORY_RECORDS_PER_SITE:
            raise CaptureError("callback-memory receipt exceeds its per-site sample cap")
        if len(records) >= MAX_CALLBACK_MEMORY_RECORDS_PER_SITE * len(CALLBACK_MEMORY_TARGETS):
            raise CaptureError("callback-memory receipt exceeds its total sample cap")
        if cycles < previous_cycles or input_ordinal < previous_input_ordinal:
            raise CaptureError("callback-memory chronology is not monotonic")
        if input_ordinal > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("callback-memory input ordinal exceeds the host-input cap")
        if host_events.get(input_ordinal) != input_frame:
            raise CaptureError("callback-memory input ordinal/frame does not match host input")
        registers = [int(value, 16) for value in groups[8:24]]
        if target == 0x1F056:
            if input_ordinal in callback_entries:
                raise CaptureError("callback-memory receipt repeats a callback entry for one input")
            callback_entries.add(input_ordinal)
        elif target == 0x1F14E:
            if input_ordinal not in callback_entries or input_ordinal in callback_exits:
                raise CaptureError("callback-memory exit is not ordered after one same-input entry")
            callback_exits.add(input_ordinal)
        elif target in CALLBACK_MEMORY_CALLER_SEQUENCE:
            stage = caller_stages.get(input_ordinal, 0)
            if stage >= len(CALLBACK_MEMORY_CALLER_SEQUENCE) or target != CALLBACK_MEMORY_CALLER_SEQUENCE[stage]:
                raise CaptureError("callback-memory caller samples are not in the reviewed order")
            caller_stages[input_ordinal] = stage + 1

        index += 1
        if index + 12 > len(lines):
            raise CaptureError("callback-memory sample omits bounded memory rows")
        valid_windows = 0
        for register in range(8):
            window = CALLBACK_MEMORY_WINDOW_LINE.fullmatch(lines[index])
            if not window:
                raise CaptureError("callback-memory register window is malformed")
            ordinal_value, pc_value, register_value, start, valid, raw = window.groups()
            ordinal_value, pc_value, register_value = int(ordinal_value), int(pc_value, 16), int(register_value)
            valid, start = int(valid), int(start, 16)
            expected_start = registers[8 + register] - 16 if registers[8 + register] >= 16 else registers[8 + register]
            if (ordinal_value, pc_value, register_value, start) != (input_ordinal, target, register, expected_start):
                raise CaptureError("callback-memory register window does not match its sample")
            if (valid == 1 and (raw is None or registers[8 + register] < 16)) or (valid == 0 and raw is not None):
                raise CaptureError("callback-memory register window validity does not match its bytes")
            valid_windows += valid
            index += 1
        valid_fixed = 0
        for address, width in CALLBACK_MEMORY_FIXED_RANGES:
            fixed = CALLBACK_MEMORY_FIXED_LINE.fullmatch(lines[index])
            if not fixed:
                raise CaptureError("callback-memory fixed range is malformed")
            ordinal_value, pc_value, address_value, width_value, valid, raw = fixed.groups()
            ordinal_value, pc_value = int(ordinal_value), int(pc_value, 16)
            address_value, width_value, valid = int(address_value, 16), int(width_value), int(valid)
            if (ordinal_value, pc_value, address_value, width_value) != (input_ordinal, target, address, width):
                raise CaptureError("callback-memory fixed range does not match its sample")
            if (valid == 1 and (raw is None or len(raw) != width * 2)) or (valid == 0 and raw is not None):
                raise CaptureError("callback-memory fixed range validity does not match its bytes")
            valid_fixed += valid
            index += 1
        records.append({"ordinal": record_ordinal, "target": target, "cycles": cycles,
                        "pc": pc, "opcode": opcode, "sr": sr,
                        "input_ordinal": input_ordinal, "input_frame": input_frame,
                        "valid_windows": valid_windows, "valid_fixed": valid_fixed})
        previous_cycles, previous_input_ordinal = cycles, input_ordinal
    return records


def callback_memory_status(path: Path, input_path: Path) -> str:
    """Validate, hash, and summarize the separately versioned callback observer."""
    state, payload, identity = _read_callback_memory_receipt(path)
    if state != "present":
        return (f"callback_memory={state}\ncallback_memory_format=v1\n"
                "callback_memory_records=0\ncallback_memory_target_counts=none\n"
                "callback_memory_callback_pairs=0\ncallback_memory_caller_samples=0\n"
                "callback_memory_callback_entries=0\ncallback_memory_callback_exits=0\n"
                "callback_memory_unmatched_callback_entries=0\n"
                "callback_memory_unmatched_callback_exits=0\n"
                "callback_memory_complete_caller_sequences=0\n"
                "callback_memory_entries_by_input_ordinal=none\n"
                "callback_memory_exits_by_input_ordinal=none\n"
                "callback_memory_unmatched_entries_by_input_ordinal=none\n"
                "callback_memory_unmatched_exits_by_input_ordinal=none\n"
                "callback_memory_complete_caller_sequences_by_input_ordinal=none\n"
                "callback_memory_callback_caller_link=not-established\n"
                "callback_memory_ram_windows_valid=0\ncallback_memory_fixed_ranges_valid=0\n"
                "callback_memory_input_links=0\ncallback_memory_input_chronology=none\n")
    records = _parse_callback_memory_payload(payload, input_path)
    counts = {site: 0 for site in CALLBACK_MEMORY_TARGETS}
    for record in records:
        counts[int(record["target"])] += 1
    assert identity is not None
    digest, size = identity
    entry_ordinals = {int(record["input_ordinal"]) for record in records
                      if int(record["target"]) == 0x1F056}
    exit_ordinals = {int(record["input_ordinal"]) for record in records
                     if int(record["target"]) == 0x1F14E}
    pairs = len(entry_ordinals & exit_ordinals)
    caller_sequences: dict[int, list[int]] = {}
    for record in records:
        target = int(record["target"])
        if target in CALLBACK_MEMORY_CALLER_SEQUENCE:
            caller_sequences.setdefault(int(record["input_ordinal"]), []).append(target)
    complete_caller_sequences = sum(
        tuple(sites) == CALLBACK_MEMORY_CALLER_SEQUENCE
        for sites in caller_sequences.values())
    complete_caller_ordinals = [ordinal for ordinal, sites in caller_sequences.items()
                                if tuple(sites) == CALLBACK_MEMORY_CALLER_SEQUENCE]
    caller_samples = sum(counts[site] for site in CALLBACK_MEMORY_CALLER_SEQUENCE)
    valid_windows = sum(int(record["valid_windows"]) for record in records)
    valid_fixed = sum(int(record["valid_fixed"]) for record in records)
    target_counts = ",".join(f"0x{site:08x}:{counts[site]}" for site in CALLBACK_MEMORY_TARGETS)
    entries_by_input = ",".join(f"{ordinal}:1" for ordinal in sorted(entry_ordinals)) or "none"
    exits_by_input = ",".join(f"{ordinal}:1" for ordinal in sorted(exit_ordinals)) or "none"
    unmatched_entries_by_input = ",".join(
        f"{ordinal}:1" for ordinal in sorted(entry_ordinals - exit_ordinals)) or "none"
    unmatched_exits_by_input = ",".join(
        f"{ordinal}:1" for ordinal in sorted(exit_ordinals - entry_ordinals)) or "none"
    complete_callers_by_input = ",".join(
        f"{ordinal}:1" for ordinal in sorted(complete_caller_ordinals)) or "none"
    return ("callback_memory=present\ncallback_memory_format=v1\n"
            f"callback_memory_sha256={digest}\ncallback_memory_bytes={size}\n"
            f"callback_memory_records={len(records)}\n"
            f"callback_memory_target_counts={target_counts}\n"
            f"callback_memory_callback_pairs={pairs}\n"
            f"callback_memory_callback_entries={len(entry_ordinals)}\n"
            f"callback_memory_callback_exits={len(exit_ordinals)}\n"
            f"callback_memory_unmatched_callback_entries={len(entry_ordinals - exit_ordinals)}\n"
            f"callback_memory_unmatched_callback_exits={len(exit_ordinals - entry_ordinals)}\n"
            f"callback_memory_complete_caller_sequences={complete_caller_sequences}\n"
            f"callback_memory_entries_by_input_ordinal={entries_by_input}\n"
            f"callback_memory_exits_by_input_ordinal={exits_by_input}\n"
            f"callback_memory_unmatched_entries_by_input_ordinal={unmatched_entries_by_input}\n"
            f"callback_memory_unmatched_exits_by_input_ordinal={unmatched_exits_by_input}\n"
            f"callback_memory_complete_caller_sequences_by_input_ordinal={complete_callers_by_input}\n"
            "callback_memory_callback_caller_link=not-established\n"
            f"callback_memory_caller_samples={caller_samples}\n"
            f"callback_memory_ram_windows_valid={valid_windows}\n"
            f"callback_memory_fixed_ranges_valid={valid_fixed}\n"
            f"callback_memory_input_links={len(records)}\n"
            "callback_memory_input_chronology=linked\n")


def _read_service_boundary_payload(path: Path) -> bytes:
    try:
        before = path.lstat()
    except FileNotFoundError:
        return b""
    except OSError as error:
        raise CaptureError(f"service-boundary receipt is unavailable: {error}") from error
    if stat.S_ISLNK(before.st_mode) or not stat.S_ISREG(before.st_mode):
        raise CaptureError("service-boundary receipt is not a regular non-symlink file")
    if before.st_size > MAX_SERVICE_BOUNDARY_BYTES:
        raise CaptureError("service-boundary receipt exceeds its bounded cap")
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            opened = os.fstat(stream.fileno())
            if (not stat.S_ISREG(opened.st_mode) or (opened.st_dev, opened.st_ino) !=
                    (before.st_dev, before.st_ino) or opened.st_size != before.st_size):
                raise CaptureError("service-boundary receipt changed during bounded open")
            payload = stream.read(MAX_SERVICE_BOUNDARY_BYTES + 1)
    except OSError as error:
        raise CaptureError(f"service-boundary receipt is unavailable: {error}") from error
    if len(payload) > MAX_SERVICE_BOUNDARY_BYTES or len(payload) != before.st_size:
        raise CaptureError("service-boundary receipt changed or exceeded its bounded cap")
    return payload


def _parse_service_boundary_payload(payload: bytes, input_path: Path, *,
                                    allow_any_input: bool = False,
                                    exact_move_sprite_windows: bool = False,
                                    max_sequences: int = MAX_SERVICE_BOUNDARY_SEQUENCES) -> list[dict[str, object]]:
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("service-boundary receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("service-boundary receipt has a truncated final record")
    host_events = dict(parse_host_input_records(input_path))
    counts = {site: 0 for site in SERVICE_BOUNDARY_TARGETS_V30}
    sequences: dict[int, dict[str, int | list[int]]] = {}
    current_sequence = 0
    last_sequence_input = 0
    previous_cycles = -1
    records: list[dict[str, object]] = []
    lines = text.splitlines(keepends=True)
    index = 0
    while index < len(lines):
        match = SERVICE_BOUNDARY_SUMMARY_LINE.fullmatch(lines[index])
        if not match:
            raise CaptureError("service-boundary receipt contains an invalid sample")
        groups = match.groups()
        record_ordinal, sequence, target = int(groups[0]), int(groups[1]), int(groups[2], 16)
        cycles, pc = int(groups[3]), int(groups[4], 16)
        input_ordinal, input_frame = int(groups[7]), int(groups[8])
        return_valid, return_pc = int(groups[9]), int(groups[10], 16)
        if target not in counts or target != (pc & 0xffffff):
            raise CaptureError("service-boundary sample uses an unreviewed PC")
        if record_ordinal != counts[target] + 1:
            raise CaptureError("service-boundary per-site ordinals are not contiguous")
        counts[target] += 1
        if counts[target] > max_sequences:
            raise CaptureError("service-boundary per-site cap was exceeded")
        if len(records) >= max_sequences * len(SERVICE_BOUNDARY_TARGETS_V30):
            raise CaptureError("service-boundary total record cap was exceeded")
        if not allow_any_input and input_ordinal != LATE_DISPLAY_TARGET_ORDINALS_V30[0]:
            raise CaptureError("service-boundary sample is outside input ordinal 76")
        if host_events.get(input_ordinal) != input_frame:
            raise CaptureError("service-boundary sample does not match host input chronology")
        if cycles < previous_cycles:
            raise CaptureError("service-boundary cycles are not monotonic")
        registers = [int(value, 16) for value in groups[11:27]]
        if target == 0x20118:
            if (sequence != current_sequence + 1 or sequence > max_sequences
                    or (allow_any_input and input_ordinal <= last_sequence_input)):
                raise CaptureError("service-boundary sequence IDs are not contiguous")
            if ((return_valid and return_pc not in SERVICE_BOUNDARY_CALLER_RETURNS_V29)
                    or (not return_valid and return_pc != 0)):
                raise CaptureError("service-boundary entry has an unreviewed caller return")
            sequences[sequence] = {"input_ordinal": input_ordinal, "input_frame": input_frame,
                                   "return_valid": return_valid, "return_pc": return_pc,
                                   "targets": [target]}
            current_sequence = sequence
            last_sequence_input = input_ordinal
        else:
            if sequence != current_sequence or sequence not in sequences:
                raise CaptureError("service-boundary sample has no matching entry sequence")
            state = sequences[sequence]
            if (state["input_ordinal"], state["input_frame"], state["return_valid"],
                    state["return_pc"]) != (input_ordinal, input_frame, return_valid, return_pc):
                raise CaptureError("service-boundary sequence metadata changed")
            prior_targets = state["targets"]
            assert isinstance(prior_targets, list)
            stage = len(prior_targets)
            expected_target = ({1: 0x201B6, 2: 0x201BA, 3: 0x201BE}.get(stage))
            if stage == 4:
                if not return_valid or return_pc not in SERVICE_BOUNDARY_CALLER_RETURNS_V30:
                    raise CaptureError("service-boundary caller return is invalid for stage four")
                if target != return_pc or target != (pc & 0xffffff):
                    raise CaptureError("service-boundary stage-four target does not match the saved caller PC")
                expected_target = return_pc
            if target != expected_target:
                raise CaptureError("service-boundary samples violate the reviewed call/return order")
            prior_targets.append(target)
        index += 1
        if index + 11 > len(lines):
            raise CaptureError("service-boundary sample omits bounded RAM rows")
        valid_windows = 0
        windows: dict[int, tuple[int, int, str | None]] = {}
        for register in SERVICE_BOUNDARY_WINDOW_REGISTERS_V29:
            window = SERVICE_BOUNDARY_WINDOW_LINE.fullmatch(lines[index])
            if not window:
                raise CaptureError("service-boundary register window is malformed")
            seq_v, ord_v, pc_v, reg_v, start_v, valid_v, raw = window.groups()
            seq_v, ord_v, pc_v = int(seq_v), int(ord_v), int(pc_v, 16)
            reg_v, start_v, valid_v = int(reg_v), int(start_v, 16), int(valid_v)
            exact_object_window = (exact_move_sprite_windows and target == 0x201B6
                                   and reg_v in {0, 1})
            expected_start = (registers[8 + reg_v] if exact_object_window else
                              registers[8 + reg_v] - 16 if registers[8 + reg_v] >= 16 else
                              registers[8 + reg_v])
            if (seq_v, ord_v, pc_v, reg_v, start_v) != (sequence, input_ordinal, target, register, expected_start):
                raise CaptureError("service-boundary register window does not match its sample")
            if ((valid_v and (raw is None or (registers[8 + reg_v] < 16
                                              and not exact_object_window)))
                    or (not valid_v and raw is not None)):
                raise CaptureError("service-boundary register window validity is inconsistent")
            valid_windows += valid_v
            windows[reg_v] = (start_v, valid_v, raw)
            index += 1
        valid_fixed = 0
        fixed_ranges: dict[int, tuple[int, str | None]] = {}
        for address, width in SERVICE_BOUNDARY_FIXED_RANGES_V29:
            fixed = SERVICE_BOUNDARY_FIXED_LINE.fullmatch(lines[index])
            if not fixed:
                raise CaptureError("service-boundary fixed RAM row is malformed")
            seq_v, ord_v, pc_v, addr_v, valid_v, raw = fixed.groups()
            seq_v, ord_v, pc_v = int(seq_v), int(ord_v), int(pc_v, 16)
            addr_v, valid_v = int(addr_v, 16), int(valid_v)
            if (seq_v, ord_v, pc_v, addr_v, 2) != (sequence, input_ordinal, target, address, width):
                raise CaptureError("service-boundary fixed RAM row does not match its reviewed range")
            if (valid_v and raw is None) or (not valid_v and raw is not None):
                raise CaptureError("service-boundary fixed RAM validity is inconsistent")
            valid_fixed += valid_v
            fixed_ranges[address] = (valid_v, raw)
            index += 1
        records.append({"sequence": sequence, "target": target, "cycles": cycles,
                        "input_ordinal": input_ordinal, "input_frame": input_frame,
                        "return_valid": return_valid, "return_pc": return_pc,
                        "valid_windows": valid_windows, "valid_fixed": valid_fixed,
                        "registers": tuple(registers), "windows": windows,
                        "fixed_ranges": fixed_ranges})
        previous_cycles = cycles
    return records


def service_boundary_status(path: Path, input_path: Path, *,
                            allow_any_input: bool = False,
                            exact_move_sprite_windows: bool = False,
                            max_sequences: int = MAX_SERVICE_BOUNDARY_SEQUENCES) -> str:
    """Validate bounded service-boundary samples and their host-input links."""
    receipt_format = "v2" if allow_any_input or exact_move_sprite_windows else "v1"
    scope_fields = ("service_boundary_input_scope=all-host-inputs\n"
                    "service_boundary_window_policy=exact-a0-a1-at-0x201b6\n"
                    if allow_any_input or exact_move_sprite_windows else "")
    try:
        path.lstat()
    except FileNotFoundError:
        state, payload = "absent", b""
    else:
        payload = _read_service_boundary_payload(path)
        state = "present" if payload else "empty"
    if state != "present":
        return (f"service_boundary={state}\nservice_boundary_format={receipt_format}\n"
                + scope_fields
                + "service_boundary_records=0\nservice_boundary_sequence_count=0\n"
                "service_boundary_site_counts=none\nservice_boundary_caller_return_counts=none\n"
                "service_boundary_valid_windows=0\nservice_boundary_valid_fixed_ranges=0\n"
                "service_boundary_input_links=0\nservice_boundary_input_chronology=none\n")
    records = _parse_service_boundary_payload(payload, input_path,
        allow_any_input=allow_any_input,
        exact_move_sprite_windows=exact_move_sprite_windows,
        max_sequences=max_sequences)
    counts = {site: 0 for site in SERVICE_BOUNDARY_TARGETS_V30}
    sequences: dict[int, list[int]] = {}
    caller_counts = {pc: 0 for pc in SERVICE_BOUNDARY_CALLER_RETURNS_V30}
    for record in records:
        counts[record["target"]] += 1
        sequences.setdefault(record["sequence"], []).append(record["target"])
        if record["target"] == 0x20118 and record["return_valid"]:
            caller_counts[record["return_pc"]] += 1
    site_counts = ",".join(f"0x{site:08x}:{counts[site]}" for site in SERVICE_BOUNDARY_TARGETS_V30)
    return_counts = ",".join(f"0x{pc:08x}:{caller_counts[pc]}" for pc in SERVICE_BOUNDARY_CALLER_RETURNS_V30)
    move_sprite_fields = ""
    if exact_move_sprite_windows:
        call_rows = [record for record in records if record["target"] == 0x201B6]
        entry_rows = [record for record in records if record["target"] == 0x20118]
        def captured_value(value: str | None) -> str:
            return value if value is not None else "unavailable"
        args = []
        for record in call_rows:
            registers = record["registers"]
            assert isinstance(registers, tuple)
            args.append(
                f"{record['sequence']}:{record['input_ordinal']}"
                f":d0={registers[0]:08x}:d1={registers[1]:08x}"
                f":a0={registers[8]:08x}:a1={registers[9]:08x}"
                f":a6={registers[14]:08x}")
        inputs = []
        for record in entry_rows:
            fixed = record["fixed_ranges"]
            assert isinstance(fixed, dict)
            fields = []
            for address, _ in SERVICE_BOUNDARY_FIXED_RANGES_V29:
                valid, raw = fixed[address]
                fields.append(f"{address:05x}:valid={valid}:bytes={captured_value(raw)}")
            inputs.append(f"{record['sequence']}:{record['input_ordinal']}:" + ":".join(fields))
        object_windows = []
        for record in call_rows:
            windows = record["windows"]
            assert isinstance(windows, dict)
            fields = []
            for register in (0, 1):
                start, valid, raw = windows[register]
                fields.append(
                    f"a{register}@{start:08x}:valid={valid}:bytes={captured_value(raw)}")
            object_windows.append(
                f"{record['sequence']}:{record['input_ordinal']}:" + ":".join(fields))
        entry_by_sequence = {record["sequence"]: record for record in entry_rows}
        call_by_sequence = {record["sequence"]: record for record in call_rows}

        def signed_word(value: int) -> int:
            return value if value < 0x8000 else value - 0x10000

        def evaluate_axis(original: int, delta: int, minimum: int, maximum: int) -> int:
            value = (original + delta) & 0xFFFF
            if signed_word(delta) < 0:
                if signed_word(value) < 0 or value < minimum:
                    return minimum
            elif value > maximum:
                return maximum
            return value

        comparisons = []
        for sequence in sorted(set(entry_by_sequence) | set(call_by_sequence)):
            entry = entry_by_sequence.get(sequence)
            call = call_by_sequence.get(sequence)
            if entry is None or call is None:
                comparisons.append(f"{sequence}:incomplete:entry-or-call-missing")
                continue
            fixed = entry["fixed_ranges"]
            registers = call["registers"]
            assert isinstance(fixed, dict) and isinstance(registers, tuple)
            word_addresses = (0x1FFC8, 0x1EE10, 0x1FFCA, 0x1FFCC,
                              0x1FFCE, 0x1EE12, 0x1FFD0, 0x1FFD2)
            values: dict[int, int] = {}
            for address in word_addresses:
                valid, raw = fixed[address]
                if not valid or raw is None:
                    break
                values[address] = int(raw, 16)
            if len(values) != len(word_addresses):
                comparisons.append(f"{sequence}:incomplete:coordinate-preimage-unavailable")
                continue
            x = evaluate_axis(values[0x1FFC8], values[0x1EE10],
                              values[0x1FFCA], values[0x1FFCC])
            y = evaluate_axis(values[0x1FFCE], values[0x1EE12],
                              values[0x1FFD0], values[0x1FFD2])
            expected_d0 = (((x - 0x10) & 0xFFFF) >> 1)
            expected_d1 = ((y - 0x06) & 0xFFFF)
            observed_d0, observed_d1 = registers[0] & 0xFFFF, registers[1] & 0xFFFF
            observed_a0, observed_a1 = registers[8], registers[9]
            matches = (observed_d0 == expected_d0 and observed_d1 == expected_d1
                       and observed_a0 == 0x12E12 and observed_a1 == 0x1FFDA)
            route_targets = sequences.get(sequence, [])
            route_complete = route_targets == [0x20118, 0x201B6, 0x201BA,
                                                0x201BE, entry["return_pc"]]
            comparisons.append(
                f"{sequence}:{'match' if matches else 'mismatch'}"
                f":tail={'complete' if route_complete else 'partial'}"
                f":expected_d0={expected_d0:04x}:observed_d0={observed_d0:04x}"
                f":expected_d1={expected_d1:04x}:observed_d1={observed_d1:04x}"
                f":observed_a0={observed_a0:08x}:observed_a1={observed_a1:08x}")
        move_sprite_fields = (
            f"service_boundary_move_sprite_args={'|'.join(args) if args else 'none'}\n"
            f"service_boundary_move_sprite_inputs={'|'.join(inputs) if inputs else 'none'}\n"
            f"service_boundary_move_sprite_windows={'|'.join(object_windows) if object_windows else 'none'}\n"
            f"service_boundary_move_sprite_comparison={'|'.join(comparisons) if comparisons else 'none'}\n")
    return (f"service_boundary=present\nservice_boundary_format={receipt_format}\n"
            + scope_fields
            + move_sprite_fields
            + f"service_boundary_sha256={hashlib.sha256(payload).hexdigest()}\n"
            f"service_boundary_bytes={len(payload)}\nservice_boundary_records={len(records)}\n"
            f"service_boundary_sequence_count={len(sequences)}\n"
            f"service_boundary_site_counts={site_counts}\n"
            f"service_boundary_caller_return_counts={return_counts}\n"
            f"service_boundary_valid_windows={sum(record['valid_windows'] for record in records)}\n"
            f"service_boundary_valid_fixed_ranges={sum(record['valid_fixed'] for record in records)}\n"
            f"service_boundary_input_links={len(records)}\n"
            "service_boundary_input_chronology=linked\n")


def response_queue_status(path: Path, input_path: Path) -> str:
    """Validate v31's bounded ordered queue-helper trace without naming its bytes."""
    try:
        before = path.lstat()
    except FileNotFoundError:
        return ("response_queue=absent\nresponse_queue_format=v1\n"
                "response_queue_records=0\nresponse_queue_sequences=0\n"
                "response_queue_complete_sequences=0\nresponse_queue_dequeue_results=0\n"
                "response_queue_empty_returns=0\nresponse_queue_input_links=0\n"
                "response_queue_input_chronology=none\n")
    except OSError as error:
        raise CaptureError(f"response-queue receipt is unavailable: {error}") from error
    if stat.S_ISLNK(before.st_mode) or not stat.S_ISREG(before.st_mode):
        raise CaptureError("response-queue receipt is not a regular non-symlink file")
    if before.st_size > MAX_RESPONSE_QUEUE_BYTES:
        raise CaptureError("response-queue receipt exceeds its bounded recorder contract")
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            opened = os.fstat(stream.fileno())
            if (not stat.S_ISREG(opened.st_mode)
                    or (opened.st_dev, opened.st_ino) != (before.st_dev, before.st_ino)
                    or opened.st_size != before.st_size):
                raise CaptureError("response-queue receipt changed during bounded open")
            payload = stream.read(MAX_RESPONSE_QUEUE_BYTES + 1)
    except OSError as error:
        raise CaptureError(f"response-queue receipt is unavailable: {error}") from error
    if len(payload) > MAX_RESPONSE_QUEUE_BYTES or len(payload) != before.st_size:
        raise CaptureError("response-queue receipt changed or exceeded its bounded cap")
    if not payload:
        return ("response_queue=empty\nresponse_queue_format=v1\n"
                "response_queue_records=0\nresponse_queue_sequences=0\n"
                "response_queue_complete_sequences=0\nresponse_queue_dequeue_results=0\n"
                "response_queue_empty_returns=0\nresponse_queue_input_links=0\n"
                "response_queue_input_chronology=none\n")
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("response-queue receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("response-queue receipt has a truncated final record")

    host_events = dict(parse_host_input_records(input_path))
    counts = {site: 0 for site in RESPONSE_QUEUE_SITES_V31}
    sequences: dict[int, list[dict[str, int | str]]] = {}
    previous_cycles = -1
    records: list[dict[str, int | str]] = []
    for line in text.splitlines(keepends=True):
        match = RESPONSE_QUEUE_LINE_V31.fullmatch(line)
        if not match:
            raise CaptureError("response-queue receipt contains an invalid sample")
        g = match.groups()
        ordinal, sequence, stage, cycles = int(g[0]), int(g[1]), int(g[2]), int(g[3])
        pc, opcode, sr = int(g[4], 16), int(g[5], 16), int(g[6], 16)
        input_ordinal, input_frame = int(g[7]), int(g[8])
        d0, a7, stack_valid, stack_return = int(g[9], 16), int(g[10], 16), int(g[11]), int(g[12], 16)
        queue_valid, queue_hex, pending_valid, pending_hex = int(g[13]), g[14], int(g[15]), g[16]
        if pc not in counts or ordinal != counts[pc] + 1:
            raise CaptureError("response-queue sample uses an unreviewed PC or ordinal")
        counts[pc] += 1
        if counts[pc] > MAX_RESPONSE_QUEUE_RECORDS_PER_SITE:
            raise CaptureError("response-queue per-site sample cap was exceeded")
        if len(records) >= MAX_RESPONSE_QUEUE_RECORDS_PER_SITE * len(RESPONSE_QUEUE_SITES_V31):
            raise CaptureError("response-queue total sample cap was exceeded")
        if cycles < previous_cycles:
            raise CaptureError("response-queue cycles are not monotonic")
        if host_events.get(input_ordinal) != input_frame:
            raise CaptureError("response-queue sample is detached from host-input chronology")
        if (queue_valid != (queue_hex is not None)
                or pending_valid != (pending_hex is not None)
                or (stack_valid == 0 and stack_return != 0)):
            raise CaptureError("response-queue validity flags do not match their bytes")
        queue = bytes.fromhex(queue_hex) if queue_hex is not None else None
        pending = int(pending_hex, 16) if pending_hex is not None else None
        if stage == 0:
            if (sequence != 0 or pc != 0x4077E):
                raise CaptureError("response-queue standalone caller sample is detached")
        elif stage == 1:
            if (pc != 0x407B4 or sequence != len(sequences) + 1
                    or sequence > MAX_RESPONSE_QUEUE_SEQUENCES):
                raise CaptureError("response-queue sequence start is out of order")
            if queue is None or pending is None:
                raise CaptureError("response-queue call preimage is incomplete")
            sequences[sequence] = []
        else:
            if sequence not in sequences or pc != RESPONSE_QUEUE_SITES_V31[stage]:
                raise CaptureError("response-queue stage has no matching caller sequence")
            prior = sequences[sequence]
            if stage != len(prior) + 2:
                raise CaptureError("response-queue sequence skipped or repeated a stage")
            if stage <= 4 and (not stack_valid or stack_return != 0x407BA):
                raise CaptureError("response-queue helper stack does not return to $407ba")
            if queue is None or pending is None:
                raise CaptureError("response-queue helper RAM snapshot is incomplete")
            if prior and input_ordinal < int(prior[-1]["input_ordinal"]):
                raise CaptureError("response-queue input ordinal moved backward within a sequence")
            if stage == 2 and pending == 0:
                raise CaptureError("response-queue helper entry did not observe a nonempty queue")
            if stage == 3:
                if (d0 & 0xFFFF) != pending or bool(sr & 0x0004) != ((d0 & 0xFFFF) == 0):
                    raise CaptureError("response-queue second read disagrees with D0 or Z flag")
            if stage == 4:
                if not prior or int(prior[-1]["stage"]) != 3:
                    raise CaptureError("response-queue RTS has no second-read sample")
                branch = prior[-1]
                branch_count = int(branch["d0"]) & 0xFFFF
                branch_queue = bytes.fromhex(str(branch["queue"]))
                branch_pending = int(str(branch["pending"]), 16)
                expected_queue = bytearray(branch_queue)
                expected_pending = branch_pending
                if branch_count:
                    expected_queue[:20] = branch_queue[1:21]
                    expected_pending -= 1
                    if (d0 & 0xFF) != branch_queue[0]:
                        raise CaptureError("response-queue RTS byte does not match the dequeued queue head")
                if queue != bytes(expected_queue) or pending != expected_pending:
                    raise CaptureError("response-queue RTS memory does not match the observed local path")
            if stage in {5, 6}:
                if not prior or queue != bytes.fromhex(str(prior[-1]["queue"])) \
                        or pending != int(str(prior[-1]["pending"]), 16):
                    raise CaptureError("response-queue caller memory changed across a return boundary")
            if prior and int(prior[-1]["cycles"]) >= cycles:
                raise CaptureError("response-queue sequence cycles are not strictly increasing")
        record = {"sequence": sequence, "stage": stage, "cycles": cycles,
                  "input_ordinal": input_ordinal, "input_frame": input_frame,
                  "pc": pc, "opcode": opcode, "sr": sr, "d0": d0,
                  "stack_valid": stack_valid, "stack_return": stack_return,
                  "queue": queue.hex() if queue is not None else "",
                  "pending": f"{pending:04x}" if pending is not None else ""}
        records.append(record)
        if stage > 0:
            sequences[sequence].append(record)
        previous_cycles = cycles

    complete = 0
    dequeues = 0
    empty_returns = 0
    for sequence_records in sequences.values():
        stages = [int(record["stage"]) for record in sequence_records]
        if stages == [1, 2, 3, 4, 5, 6]:
            complete += 1
        if len(stages) >= 5 and stages[:5] == [1, 2, 3, 4, 5]:
            if (int(sequence_records[2]["d0"]) & 0xFFFF) == 0:
                empty_returns += 1
            else:
                dequeues += 1
    digest, size = hashlib.sha256(payload).hexdigest(), len(payload)
    site_counts = ",".join(f"0x{site:06x}:{counts[site]}" for site in RESPONSE_QUEUE_SITES_V31)
    return ("response_queue=present\nresponse_queue_format=v1\n"
            f"response_queue_sha256={digest}\nresponse_queue_bytes={size}\n"
            f"response_queue_records={len(records)}\nresponse_queue_site_counts={site_counts}\n"
            f"response_queue_sequences={len(sequences)}\n"
            f"response_queue_complete_sequences={complete}\n"
            f"response_queue_dequeue_results={dequeues}\n"
            f"response_queue_empty_returns={empty_returns}\n"
            f"response_queue_input_links={len(records)}\n"
            "response_queue_input_chronology=linked\n")


def title_callback_status_v32(path: Path, input_path: Path) -> str:
    """Validate v32's ordered, input-linked $00ff title-callback observations."""
    try:
        before = path.lstat()
    except FileNotFoundError:
        return ("title_callback=absent\ntitle_callback_format=v1\n"
                "title_callback_records=0\ntitle_callback_sequences=0\n"
                "title_callback_complete_sequences=0\ntitle_callback_input_links=0\n"
                "title_callback_input_chronology=none\n"
                "title_callback_outer_returns=0\n"
                "title_callback_runtime_abi=unknown\n")
    except OSError as error:
        raise CaptureError(f"title-callback receipt is unavailable: {error}") from error
    if stat.S_ISLNK(before.st_mode) or not stat.S_ISREG(before.st_mode):
        raise CaptureError("title-callback receipt is not a regular non-symlink file")
    if before.st_size > MAX_TITLE_CALLBACK_BYTES_V32:
        raise CaptureError("title-callback receipt exceeds its bounded recorder contract")
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            opened = os.fstat(stream.fileno())
            if (not stat.S_ISREG(opened.st_mode)
                    or (opened.st_dev, opened.st_ino) != (before.st_dev, before.st_ino)
                    or opened.st_size != before.st_size):
                raise CaptureError("title-callback receipt changed during bounded open")
            payload = stream.read(MAX_TITLE_CALLBACK_BYTES_V32 + 1)
    except OSError as error:
        raise CaptureError(f"title-callback receipt is unavailable: {error}") from error
    if len(payload) > MAX_TITLE_CALLBACK_BYTES_V32 or len(payload) != before.st_size:
        raise CaptureError("title-callback receipt changed or exceeded its bounded cap")
    if not payload:
        return ("title_callback=empty\ntitle_callback_format=v1\n"
                "title_callback_records=0\ntitle_callback_sequences=0\n"
                "title_callback_complete_sequences=0\ntitle_callback_input_links=0\n"
                "title_callback_input_chronology=none\n"
                "title_callback_outer_returns=0\n"
                "title_callback_runtime_abi=unknown\n")
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("title-callback receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("title-callback receipt has a truncated final record")

    host_events = dict(parse_host_input_records(input_path))
    records: list[dict[str, int | str]] = []
    sequences: dict[int, list[dict[str, int | str]]] = {}
    active_sequence = 0
    previous_cycles = -1
    counts = {stage: 0 for stage, _ in TITLE_CALLBACK_STAGES_V32}
    stages_by_name = dict(TITLE_CALLBACK_STAGES_V32)
    for line in text.splitlines(keepends=True):
        match = TITLE_CALLBACK_LINE_V32.fullmatch(line)
        if not match:
            raise CaptureError("title-callback receipt contains an invalid sample")
        (ordinal_s, sequence_s, stage, cycles_s, pc_s, input_ordinal_s,
         input_frame_s, valid_s, value_s, source_s, destination_s, width_s,
         cell_x_valid_s, cell_x_s, cell_y_valid_s, cell_y_s) = match.groups()
        ordinal, sequence, cycles = int(ordinal_s), int(sequence_s), int(cycles_s)
        pc, input_ordinal, input_frame = int(pc_s, 16), int(input_ordinal_s), int(input_frame_s)
        valid, value = int(valid_s), int(value_s, 16)
        source, destination, width = int(source_s, 16), int(destination_s, 16), int(width_s)
        cell_x_valid, cell_x = int(cell_x_valid_s), int(cell_x_s, 16)
        cell_y_valid, cell_y = int(cell_y_valid_s), int(cell_y_s, 16)
        if stage not in stages_by_name or pc != stages_by_name[stage]:
            raise CaptureError("title-callback sample uses an unreviewed stage or PC")
        if ordinal != len(records) + 1:
            raise CaptureError("title-callback record ordinals are not contiguous")
        counts[stage] += 1
        if counts[stage] > MAX_TITLE_CALLBACK_SEQUENCES_V32:
            raise CaptureError("title-callback per-site cap was exceeded")
        if records and cycles <= previous_cycles:
            raise CaptureError("title-callback cycles are not strictly increasing")
        if host_events.get(input_ordinal) != input_frame:
            raise CaptureError("title-callback sample is detached from host-input chronology")
        expected_mem = {
            "gate-test": (0x1EE16, 0, 1),
            "copy-x": (None, 0x1EE10, 2),
            "copy-y": (None, 0x1EE12, 2),
        }.get(stage)
        if stage == "selector-compare":
            if not valid or value != 0x00FF or source == 0 or destination != 0 or width != 0:
                raise CaptureError("title-callback selector sample does not establish the $00ff route")
        elif expected_mem is None:
            if (valid, value, source, destination, width) != (0, 0, 0, 0, 0):
                raise CaptureError("title-callback non-memory stage claims unsupported memory data")
        else:
            expected_source, expected_destination, expected_width = expected_mem
            if (not valid or width != expected_width or destination != expected_destination
                    or (expected_source is not None and source != expected_source)
                    or (expected_source is None and source == 0)):
                raise CaptureError("title-callback gate/copy memory sample is incomplete or misplaced")
            if stage == "gate-test" and value != 0:
                raise CaptureError("title-callback gate sample is not the observed zero-route fallthrough")
        if stage == "service-entry":
            if cell_x_valid != 1 or cell_y_valid != 1:
                raise CaptureError("title-callback service entry omits copied destination cells")
        elif (cell_x_valid, cell_x, cell_y_valid, cell_y) != (0, 0, 0, 0):
            raise CaptureError("title-callback non-entry stage claims destination-cell data")
        if sequence not in sequences:
            if sequence != len(sequences) + 1 or sequence != active_sequence + 1 or stage != "gate-test":
                raise CaptureError("title-callback sequence IDs are not contiguous")
            active_sequence = sequence
        elif sequence != active_sequence:
            raise CaptureError("title-callback sequences are interleaved")
        sequence_rows = sequences.setdefault(sequence, [])
        expected_stage = len(sequence_rows)
        if (sequence > MAX_TITLE_CALLBACK_SEQUENCES_V32
                or expected_stage >= len(TITLE_CALLBACK_STAGES_V32)
                or stage != TITLE_CALLBACK_STAGES_V32[expected_stage][0]):
            raise CaptureError("title-callback stages are skipped, repeated, or out of order")
        if sequence_rows and (input_ordinal != int(sequence_rows[0]["input_ordinal"])
                              or input_frame != int(sequence_rows[0]["input_frame"])):
            raise CaptureError("title-callback sequence moved across host-input events")
        record: dict[str, int | str] = {
            "ordinal": ordinal, "sequence": sequence, "stage": stage, "pc": pc,
            "cycles": cycles, "input_ordinal": input_ordinal, "input_frame": input_frame,
            "value_valid": valid, "value": value, "source": source,
            "destination": destination, "width": width,
            "cell_x_valid": cell_x_valid, "cell_x": cell_x,
            "cell_y_valid": cell_y_valid, "cell_y": cell_y,
        }
        records.append(record)
        sequence_rows.append(record)
        previous_cycles = cycles
    for rows in sequences.values():
        if len(rows) != len(TITLE_CALLBACK_STAGES_V32):
            raise CaptureError("title-callback receipt contains an incomplete ordered sequence")
        copy_x = rows[5]
        copy_y = rows[6]
        entry = rows[8]
        if (int(copy_x["value"]) != int(entry["cell_x"])
                or int(copy_y["value"]) != int(entry["cell_y"])):
            raise CaptureError("title-callback copied values disagree with service-entry destination cells")
    digest, size = hashlib.sha256(payload).hexdigest(), len(payload)
    return ("title_callback=present\ntitle_callback_format=v1\n"
            f"title_callback_sha256={digest}\ntitle_callback_bytes={size}\n"
            f"title_callback_records={len(records)}\n"
            f"title_callback_sequences={len(sequences)}\n"
            f"title_callback_complete_sequences={len(sequences)}\n"
            f"title_callback_input_links={len(records)}\n"
            "title_callback_input_chronology=linked\n"
            "title_callback_outer_returns=0\n"
            "title_callback_runtime_abi=unknown\n")


def _read_zero_route_observation(path: Path) -> tuple[str, bytes]:
    """Read the small schema-31 sidecar without following links or trusting stat alone."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "absent", b""
    except OSError as error:
        raise CaptureError(f"zero-route observation is unavailable: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("zero-route observation is not a regular non-symlink file")
    if info.st_size > MAX_ZERO_ROUTE_OBSERVATION_BYTES:
        raise CaptureError("zero-route observation exceeds the bounded recorder contract")
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise CaptureError("zero-route observation must remain a regular file")
            payload = stream.read(MAX_ZERO_ROUTE_OBSERVATION_BYTES + 1)
    except OSError as error:
        raise CaptureError(f"zero-route observation is unavailable: {error}") from error
    if len(payload) > MAX_ZERO_ROUTE_OBSERVATION_BYTES:
        raise CaptureError("zero-route observation exceeds the bounded recorder contract")
    return ("empty" if not payload else "present"), payload


def _zero_route_observation_records(payload: bytes) -> list[dict[str, int]]:
    """Parse exact v21 rows, retaining opaque machine state without gameplay claims."""
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("zero-route observation is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("zero-route observation has a truncated final record")
    records: list[dict[str, int]] = []
    previous_cycles = -1
    previous_input_ordinal = 0
    previous_input_frame = -1
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = ZERO_ROUTE_OBSERVATION_LINE.fullmatch(line)
        if not match:
            raise CaptureError("zero-route observation contains an invalid recorder record")
        # Record, cycles, PC, input ordinal, input frame, D0..D7, A0/A1/A2/A4,
        # selector cells, memory address, width, value and valid flag.
        names = ("record", "cycles", "pc", "input_ordinal", "input_frame",
                 *(f"d{i}" for i in range(8)), "a0", "a1", "a2", "a4",
                 "cell_1f98c", "cell_1f98e", "cells_valid", "mem_addr", "mem_width", "mem_value", "mem_valid")
        # Width and validity are decimal fields, unlike the surrounding hex fields.
        values = []
        try:
            for index, value in enumerate(match.groups()):
                if index in {0, 1, 3, 4, 19, 21, 23}:
                    values.append(int(value))
                else:
                    values.append(int(value, 16))
        except ValueError as error:
            raise CaptureError("zero-route observation contains an out-of-range integer") from error
        if len(names) != len(values):
            raise CaptureError("zero-route observation field count is invalid")
        record = dict(zip(names, values))
        if record["record"] != expected:
            raise CaptureError("zero-route observation ordinals are not contiguous")
        if record["cycles"] > 0xFFFFFFFFFFFFFFFF:
            raise CaptureError("zero-route observation cycle count exceeds the reviewed range")
        if record["input_ordinal"] > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("zero-route observation input ordinal exceeds the recorder cap")
        if not -(1 << 63) <= record["input_frame"] < (1 << 63):
            raise CaptureError("zero-route observation input frame exceeds the signed-64 range")
        if record["cycles"] < previous_cycles:
            raise CaptureError("zero-route observation cycles are not monotonic")
        if record["input_ordinal"] < previous_input_ordinal:
            raise CaptureError("zero-route observation input ordinals are not monotonic")
        if record["input_frame"] < previous_input_frame:
            raise CaptureError("zero-route observation input frames are not monotonic")
        if record["pc"] not in ZERO_ROUTE_OBSERVATION_SITES:
            raise CaptureError("zero-route observation uses an unreviewed probe site")
        if record["cells_valid"] != int(record["pc"] in {
                0x0001FC22, 0x0001FC28, 0x0001FC2C}):
            raise CaptureError("zero-route selector-cell validity does not match the probe site")
        if record["mem_valid"] not in {0, 1}:
            raise CaptureError("zero-route observation memory-valid flag is invalid")
        if record["mem_width"] and record["mem_value"] >= 1 << (record["mem_width"] * 8):
            raise CaptureError("zero-route observation value exceeds its memory width")
        if (record["mem_width"] == 0) != (record["mem_valid"] == 0):
            # $1fc74 is an intended CPU write: width is one but the bus-side
            # memory readback is intentionally marked invalid there.
            if record["pc"] != 0x0001FC74:
                raise CaptureError("zero-route observation memory validity/width mismatch")
        if record["mem_width"] == 0 and (record["mem_addr"] or record["mem_value"]):
            raise CaptureError("zero-route no-access record carries memory data")
        if len(records) >= MAX_ZERO_ROUTE_OBSERVATION_RECORDS:
            raise CaptureError("zero-route observation exceeds the recorder record cap")
        records.append(record)
        previous_cycles = record["cycles"]
        previous_input_ordinal = record["input_ordinal"]
        previous_input_frame = record["input_frame"]
    return records


def _validate_zero_route_memory(record: dict[str, int], last_write: tuple[int, int] | None) -> None:
    """Enforce the exact observed memory shape at each reviewed instruction PC."""
    pc = record["pc"]
    address, width, value, valid = (record["mem_addr"], record["mem_width"],
                                    record["mem_value"], record["mem_valid"])
    fixed_reads = {
        0x0001FC22: (0x0001F98E, 1),
        0x0001FC42: (0x0001F99C, 4),
        0x0001FC4A: (0x0001F974, 4),
        0x0001FC50: (0x0001F96C, 4),
        0x0001FC56: (0x0001F970, 4),
        0x0001FC88: (0x0001F9A0, 4),
        0x0001FC8E: (0x0001F974, 4),
        0x0001FC94: (0x0001F974, 4),
    }
    if pc in {0x0001FC28, 0x0001FC2C, 0x0001FC9A}:
        if (address, width, value, valid) != (0, 0, 0, 0):
            raise CaptureError("zero-route branch/return site must have no memory access")
    elif pc in fixed_reads:
        expected_address, expected_width = fixed_reads[pc]
        if (address, width, valid) != (expected_address, expected_width, 1):
            raise CaptureError("zero-route fixed-cell read has the wrong address or width")
        if width == 1 and pc == 0x0001FC22 and value != record["cell_1f98e"]:
            raise CaptureError("zero-route selector-cell read does not match its sample")
    elif pc == 0x0001FC5E:
        if (address, width, valid) != (record["a0"], 1, 1):
            raise CaptureError("zero-route glyph read does not match A0")
    elif pc == 0x0001FC6A:
        if (address, width, valid) != (record["a1"], 2, 1):
            raise CaptureError("zero-route first mask read does not match A1")
    elif pc == 0x0001FC6C:
        if (address, width, valid) != (record["a2"], 2, 1):
            raise CaptureError("zero-route second mask read does not match A2")
    elif pc == 0x0001FC74:
        if (address, width, value, valid) != (record["a4"], 1, record["d2"] & 0xFF, 0):
            raise CaptureError("zero-route intended byte write does not match A4 and D2")
    elif pc == 0x0001FC76:
        if (address, width, valid) != (record["a4"], 1, 1):
            raise CaptureError("zero-route destination readback does not match A4")
        if last_write != (address, value):
            raise CaptureError("zero-route destination readback differs from the preceding intended write")


def _read_zero_route_host_input(path: Path) -> dict[int, int]:
    """Read only the bounded regular receipt needed to verify sample chronology."""
    try:
        info = path.lstat()
        if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
            raise CaptureError("host-input receipt is not a regular non-symlink file")
        if info.st_size > MAX_INPUT_RECEIPT_BYTES:
            raise CaptureError("host-input receipt exceeds the bounded recorder contract")
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise CaptureError("host-input receipt must remain a regular file")
            payload = stream.read(MAX_INPUT_RECEIPT_BYTES + 1)
    except OSError as error:
        raise CaptureError(f"host-input receipt is unavailable: {error}") from error
    if len(payload) > MAX_INPUT_RECEIPT_BYTES:
        raise CaptureError("host-input receipt exceeds the bounded recorder contract")
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("host-input receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("host-input receipt has a truncated final record")
    events: dict[int, int] = {}
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = HOST_INPUT_LINE.fullmatch(line)
        if not match:
            raise CaptureError("host-input receipt contains an invalid recorder record")
        try:
            ordinal, frame = int(match.group(1)), int(match.group(2))
        except ValueError as error:
            raise CaptureError("host-input receipt contains an out-of-range integer") from error
        if ordinal != expected:
            raise CaptureError("host-input receipt contains an invalid recorder record")
        events[expected] = frame
        if len(events) > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("host-input receipt exceeds the recorder record cap")
    return events


def parse_zero_route_observation(path: Path, input_path: Path) -> list[list[dict[str, int]]]:
    """Validate complete zero-route invocations and their recorder-confirmed input chronology."""
    state, payload = _read_zero_route_observation(path)
    if state != "present":
        return []
    records = _zero_route_observation_records(payload)
    input_events = _read_zero_route_host_input(input_path)
    return _validate_zero_route_invocations(records, input_events)


def _validate_zero_route_invocations(
    records: list[dict[str, int]], input_events: dict[int, int]
) -> list[list[dict[str, int]]]:
    """Join one bounded sidecar parse to host input and verify complete route shape."""
    for record in records:
        if input_events.get(record["input_ordinal"]) != record["input_frame"]:
            raise CaptureError("zero-route observation chronology does not match the host-input receipt")
    row_group = [0x0001FC5E]
    for _ in range(4):
        row_group.extend((0x0001FC6A, 0x0001FC6C, 0x0001FC74, 0x0001FC76))
    invocation_pcs = [0x0001FC22, 0x0001FC28, 0x0001FC2C, 0x0001FC42, 0x0001FC4A,
                      0x0001FC50, 0x0001FC56]
    for _ in range(8):
        invocation_pcs.extend(row_group)
    invocation_pcs.extend((0x0001FC88, 0x0001FC8E, 0x0001FC94, 0x0001FC9A))
    invocation_size = len(invocation_pcs)
    if len(records) % invocation_size:
        raise CaptureError("zero-route observation does not contain a complete start-through-return invocation")
    invocation_count = len(records) // invocation_size
    if invocation_count > MAX_ZERO_ROUTE_OBSERVATION_INVOCATIONS:
        raise CaptureError("zero-route observation exceeds the invocation cap")
    invocations: list[list[dict[str, int]]] = []
    for offset in range(0, len(records), invocation_size):
        invocation = records[offset:offset + invocation_size]
        if [record["pc"] for record in invocation] != invocation_pcs:
            raise CaptureError("zero-route observation PC sequence is not a complete reviewed invocation")
        last_write: tuple[int, int] | None = None
        for record in invocation:
            if record["pc"] in {0x0001FC22, 0x0001FC28, 0x0001FC2C}:
                if record["cell_1f98c"] != 0 or record["cell_1f98e"] != 0:
                    raise CaptureError("zero-route entry does not sample both selector cells as zero")
            _validate_zero_route_memory(record, last_write)
            if record["pc"] == 0x0001FC74:
                last_write = (record["mem_addr"], record["mem_value"])
            elif record["pc"] == 0x0001FC76:
                last_write = None
        invocations.append(invocation)
    return invocations


def zero_route_observation_status(path: Path, input_path: Path) -> str:
    """Bind the diagnostic-only sidecar's exact bytes, rows and complete invocations."""
    state, payload = _read_zero_route_observation(path)
    if state == "absent":
        return ("zero_route_observation=absent\nzero_route_observation_format=v1\n"
                "zero_route_observation_records=0\nzero_route_observation_invocations=0\n")
    if state == "empty":
        return ("zero_route_observation=empty\nzero_route_observation_format=v1\n"
                "zero_route_observation_records=0\nzero_route_observation_invocations=0\n")
    records = _zero_route_observation_records(payload)
    invocations = _validate_zero_route_invocations(records, _read_zero_route_host_input(input_path))
    digest = hashlib.sha256(payload).hexdigest()
    records = sum(len(invocation) for invocation in invocations)
    return ("zero_route_observation=present\nzero_route_observation_format=v1\n"
            f"zero_route_observation_sha256={digest}\nzero_route_observation_bytes={len(payload)}\n"
            f"zero_route_observation_records={records}\n"
            f"zero_route_observation_invocations={len(invocations)}\n")


def raw_pc_input_chronology_status(raw_path: Path, input_path: Path, raw_format: str = "v9") -> str:
    """Cross-bind v9 CPU samples to prior recorder-confirmed host delivery."""
    links = parse_raw_pc_input_links(raw_path, raw_format)
    if not any(ordinal for ordinal, _ in links):
        return "raw_pc_input_chronology=none\n"
    by_ordinal = dict(parse_host_input_records(input_path))
    for ordinal, frame in links:
        if ordinal and by_ordinal.get(ordinal) != frame:
            raise CaptureError("raw_pc input chronology does not match the host-input receipt")
    return ("raw_pc_input_chronology=linked\n"
            f"raw_pc_input_chronology_records={sum(ordinal != 0 for ordinal, _ in links)}\n")


def parse_selector_dispatch_receipt(
    path: Path,
) -> list[tuple[int, int, int, int, int, int, int]]:
    """Validate v17's bounded byte-cell samples without assigning meaning."""
    try:
        info = path.lstat()
    except OSError as error:
        raise CaptureError(f"selector-dispatch receipt is unavailable: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("selector-dispatch receipt is not a regular non-symlink file")
    if info.st_size > MAX_SELECTOR_DISPATCH_BYTES:
        raise CaptureError("selector-dispatch receipt exceeds the bounded recorder contract")
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("selector-dispatch receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("selector-dispatch receipt has a truncated final record")
    records: list[tuple[int, int, int, int, int, int, int]] = []
    previous_cycle = -1
    previous_raw_ordinal = 0
    previous_input_ordinal = 0
    phase_counts = [0, 0]
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = SELECTOR_DISPATCH_LINE.fullmatch(line)
        if not match:
            raise CaptureError("selector-dispatch receipt contains an invalid recorder record")
        (ordinal, raw_ordinal, cycles, cell_1f98c, cell_1f98e,
         input_ordinal, input_frame) = (int(match.group(1)), int(match.group(2)),
                                       int(match.group(3)), int(match.group(4), 16),
                                       int(match.group(5), 16), int(match.group(6)),
                                       int(match.group(7)))
        if ordinal != expected:
            raise CaptureError("selector-dispatch receipt ordinals are not contiguous")
        if cycles < previous_cycle or raw_ordinal <= previous_raw_ordinal:
            raise CaptureError("selector-dispatch order is not monotonic")
        if input_ordinal < previous_input_ordinal or input_ordinal > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("selector-dispatch input ordinal is invalid")
        if input_ordinal == 0 and input_frame != 0:
            raise CaptureError("selector-dispatch no-input sample must use frame zero")
        phase = 1 if input_ordinal else 0
        phase_counts[phase] += 1
        if phase_counts[phase] > 128:
            raise CaptureError("selector-dispatch exceeds the per-phase sample cap")
        records.append((ordinal, raw_ordinal, cycles, cell_1f98c, cell_1f98e,
                        input_ordinal, input_frame))
        previous_cycle = cycles
        previous_raw_ordinal = raw_ordinal
        previous_input_ordinal = input_ordinal
        if len(records) > MAX_SELECTOR_DISPATCH_RECORDS:
            raise CaptureError("selector-dispatch exceeds the recorder record cap")
    return records


def selector_dispatch_status(
    dispatch_path: Path,
    raw_path: Path,
    input_path: Path,
    raw_format: str = "v9-v16-phased",
) -> str:
    """Hash and cross-bind v17 selector cells to raw-PC and host chronology."""
    try:
        info = dispatch_path.lstat()
    except FileNotFoundError:
        info = None
    if info is not None and (stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode)):
        raise CaptureError("selector-dispatch receipt is not a regular non-symlink file")
    if info is not None and info.st_size > MAX_SELECTOR_DISPATCH_BYTES:
        raise CaptureError("selector-dispatch receipt exceeds the bounded recorder contract")

    raw_site_links: list[tuple[int, int, int, int]] = []
    if raw_path.exists():
        try:
            raw_text = raw_path.read_bytes().decode("ascii")
        except UnicodeDecodeError as error:
            raise CaptureError("raw_pc is not ASCII recorder output") from error
        for line in raw_text.splitlines(keepends=True):
            match = RAW_PC_V9_LINE.fullmatch(line)
            if not match:
                raise CaptureError("raw_pc contains an invalid recorder record")
            if int(match.group(3), 16) == 0x1FBE6:
                raw_site_links.append((int(match.group(1)), int(match.group(2)),
                                       int(match.group(10)), int(match.group(11))))

    if info is None:
        if raw_site_links:
            raise CaptureError("selector-dispatch receipt is missing samples present in raw_pc")
        return "selector_dispatch=absent\n"
    if info.st_size == 0:
        if raw_site_links:
            raise CaptureError("selector-dispatch receipt omits reached raw_pc samples")
        return "selector_dispatch=empty\n"

    records = parse_selector_dispatch_receipt(dispatch_path)
    dispatch_links = [(record[1], record[2], record[5], record[6]) for record in records]
    if dispatch_links != raw_site_links:
        raise CaptureError("selector-dispatch records do not match raw_pc chronology")
    host_events = dict(parse_host_input_records(input_path))
    for _, _, _, _, _, input_ordinal, input_frame in records:
        if input_ordinal and host_events.get(input_ordinal) != input_frame:
            raise CaptureError("selector-dispatch chronology does not match host input")
    digest, size = sha256_file(dispatch_path)
    linked = sum(record[5] != 0 for record in records)
    return ("selector_dispatch=present\n"
            f"selector_dispatch_sha256={digest}\n"
            f"selector_dispatch_bytes={size}\n"
            f"selector_dispatch_records={len(records)}\n"
            f"selector_dispatch_raw_pc_links={len(dispatch_links)}\n"
            f"selector_dispatch_input_links={linked}\n"
            f"selector_dispatch_last_input_ordinal={records[-1][5]}\n"
            f"selector_dispatch_input_chronology={'linked' if linked else 'none'}\n")


def _validate_title_display_input_links(links: list[tuple[int, int]], input_path: Path) -> str:
    """Bind opaque display records to prior recorder delivery, never guest input."""
    if not any(ordinal for ordinal, _ in links):
        return "none"
    by_ordinal = dict(parse_host_input_records(input_path))
    for ordinal, frame in links:
        if ordinal and by_ordinal.get(ordinal) != frame:
            raise CaptureError("title-display input chronology does not match the host-input receipt")
    return "linked"


def parse_title_display_receipt(path: Path) -> tuple[int, dict[int, int], list[tuple[int, int]], int]:
    """Validate v10's title-armed custom-chip write receipt without display inference."""
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("title-display receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("title-display receipt has a truncated final record")
    lines = text.splitlines(keepends=True)
    if not lines:
        raise CaptureError("title-display receipt has no arm record")
    arm = TITLE_DISPLAY_ARM_LINE.fullmatch(lines[0])
    if not arm:
        raise CaptureError("title-display receipt must begin with the reviewed title arm")
    arm_cycles = int(arm.group(1))
    links = [(int(arm.group(2)), int(arm.group(3)))]
    if links[0][0] > MAX_INPUT_RECEIPT_RECORDS:
        raise CaptureError("title-display receipt exceeds the input-recorder ordinal cap")
    if links[0][0] == 0 and links[0][1] != 0:
        raise CaptureError("title-display no-input snapshot must use frame zero")
    counts: dict[int, int] = {}
    previous_cycles = arm_cycles
    for expected_ordinal, line in enumerate(lines[1:], start=2):
        match = TITLE_DISPLAY_WRITE_LINE.fullmatch(line)
        if not match:
            raise CaptureError("title-display receipt contains an invalid recorder record")
        ordinal, cycles, vpos, hpos, register = (int(match.group(1)), int(match.group(2)),
                                                   int(match.group(3)), int(match.group(4)),
                                                   int(match.group(6), 16))
        input_link = (int(match.group(8)), int(match.group(9)))
        if ordinal != expected_ordinal:
            raise CaptureError("title-display receipt record ordinals are not contiguous")
        if cycles < previous_cycles:
            raise CaptureError("title-display receipt cycles are not monotonic")
        if vpos > 0xffff or hpos > 0xffff:
            raise CaptureError("title-display receipt position exceeds the reviewed unsigned-16 range")
        if register not in TITLE_DISPLAY_REGISTERS:
            raise CaptureError("title-display receipt writes an unreviewed display register")
        if input_link[0] > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("title-display receipt exceeds the input-recorder ordinal cap")
        if input_link[0] == 0 and input_link[1] != 0:
            raise CaptureError("title-display no-input snapshot must use frame zero")
        if input_link[0] < links[-1][0]:
            raise CaptureError("title-display input ordinals are not monotonic")
        counts[register] = counts.get(register, 0) + 1
        if counts[register] > MAX_TITLE_DISPLAY_WRITES_PER_REGISTER:
            raise CaptureError("title-display receipt exceeds the per-register recorder cap")
        previous_cycles = cycles
        links.append(input_link)
        if expected_ordinal > MAX_TITLE_DISPLAY_WRITES + 1:
            raise CaptureError("title-display receipt exceeds the recorder record cap")
    return arm_cycles, counts, links, len(lines) - 1


def title_display_receipt_status(path: Path, input_path: Path) -> str:
    """Hash-bind v10 display observations while admitting no display conclusion."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "title_display=absent\n"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("title-display receipt is not a regular non-symlink file")
    if info.st_size > MAX_TITLE_DISPLAY_RECEIPT_BYTES:
        raise CaptureError("title-display receipt exceeds the bounded recorder contract")
    if info.st_size == 0:
        return "title_display=empty\n"
    arm_cycles, counts, links, writes = parse_title_display_receipt(path)
    chronology = _validate_title_display_input_links(links, input_path)
    digest, size = sha256_file(path)
    register_counts = ",".join(
        f"0x{register:04x}:{counts[register]}" for register in sorted(counts))
    return ("title_display=present\n"
            f"title_display_sha256={digest}\n"
            f"title_display_bytes={size}\n"
            "title_display_format=v10\n"
            f"title_display_records={writes + 1}\n"
            f"title_display_arm_cycles={arm_cycles}\n"
            f"title_display_writes={writes}\n"
            f"title_display_register_counts={register_counts}\n"
            f"title_display_input_links={sum(ordinal != 0 for ordinal, _ in links)}\n"
            f"title_display_last_input_ordinal={links[-1][0]}\n"
            f"title_display_input_chronology={chronology}\n"
            f"title_display_input_chronology_records={sum(ordinal != 0 for ordinal, _ in links)}\n")


def parse_late_display_receipt(
    path: Path, *, windowed: bool = False,
    target_ordinals: tuple[int, ...] | None = None,
) -> list[tuple[int, int, int, int, int, int, int]]:
    """Validate a separately bounded post-intro display-write sidecar."""
    byte_limit = (MAX_LATE_DISPLAY_RECEIPT_BYTES_V24 if windowed
                  else MAX_LATE_DISPLAY_RECEIPT_BYTES)
    record_limit = MAX_LATE_DISPLAY_WRITES_V24 if windowed else MAX_LATE_DISPLAY_WRITES
    try:
        info = path.lstat()
    except FileNotFoundError:
        return []
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("late-display receipt is not a regular non-symlink file")
    if info.st_size > byte_limit:
        raise CaptureError("late-display receipt exceeds the bounded recorder contract")
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
                     | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise CaptureError("late-display receipt must remain a regular file")
            payload = stream.read(byte_limit + 1)
    except OSError as error:
        raise CaptureError(f"late-display receipt is unavailable: {error}") from error
    if len(payload) > byte_limit:
        raise CaptureError("late-display receipt exceeds the bounded recorder contract")
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("late-display receipt is not ASCII recorder output") from error
    if not text:
        return []
    if not text.endswith("\n"):
        raise CaptureError("late-display receipt has a truncated final record")
    records: list[tuple[int, int, int, int, int, int, int]] = []
    counts: dict[int, int] = {}
    window_counts: dict[int, int] = {}
    window_register_counts: dict[tuple[int, int], int] = {}
    previous_cycles = -1
    previous_input = 20
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = LATE_DISPLAY_WRITE_LINE.fullmatch(line)
        if not match:
            raise CaptureError("late-display receipt contains an invalid recorder record")
        ordinal, cycles, vpos, hpos = (int(match.group(i)) for i in range(1, 5))
        register = int(match.group(6), 16)
        input_ordinal, input_frame = int(match.group(8)), int(match.group(9))
        if ordinal != expected:
            raise CaptureError("late-display receipt ordinals are not contiguous")
        if cycles < previous_cycles:
            raise CaptureError("late-display receipt cycles are not monotonic")
        if vpos > 0xffff or hpos > 0xffff:
            raise CaptureError("late-display position exceeds the reviewed range")
        if register not in TITLE_DISPLAY_REGISTERS:
            raise CaptureError("late-display receipt writes an unreviewed display register")
        if not 20 < input_ordinal <= MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("late-display receipt is outside the later-input window")
        if target_ordinals is not None and input_ordinal not in target_ordinals:
            raise CaptureError("late-display receipt uses an unreviewed target input ordinal")
        if input_ordinal < previous_input:
            raise CaptureError("late-display input ordinals are not monotonic")
        if windowed:
            if input_ordinal not in window_counts:
                window_limit = (len(target_ordinals) if target_ordinals is not None
                                else MAX_LATE_DISPLAY_WINDOWS_V24)
                if len(window_counts) >= window_limit:
                    raise CaptureError("late-display receipt exceeds the recorder window cap")
                window_counts[input_ordinal] = 0
            window_counts[input_ordinal] += 1
            if window_counts[input_ordinal] > MAX_LATE_DISPLAY_WRITES_PER_WINDOW_V24:
                raise CaptureError("late-display receipt exceeds the per-window recorder cap")
            key = (input_ordinal, register)
            window_register_counts[key] = window_register_counts.get(key, 0) + 1
            if window_register_counts[key] > MAX_LATE_DISPLAY_WRITES_PER_REGISTER:
                raise CaptureError("late-display receipt exceeds the per-register window cap")
        else:
            counts[register] = counts.get(register, 0) + 1
            if counts[register] > MAX_LATE_DISPLAY_WRITES_PER_REGISTER:
                raise CaptureError("late-display receipt exceeds the per-register recorder cap")
        records.append((ordinal, cycles, vpos, hpos, register, input_ordinal, input_frame))
        if len(records) > record_limit:
            raise CaptureError("late-display receipt exceeds the recorder record cap")
        previous_cycles, previous_input = cycles, input_ordinal
    return records


def late_display_receipt_status(
    path: Path, input_path: Path, *, windowed: bool = False,
    target_ordinals: tuple[int, ...] | None = None,
) -> str:
    """Bind post-ordinal-20 display writes to exact host-delivery chronology."""
    byte_limit = (MAX_LATE_DISPLAY_RECEIPT_BYTES_V24 if windowed
                  else MAX_LATE_DISPLAY_RECEIPT_BYTES)
    format_version = ("v3-targeted-ordinals" if target_ordinals is not None else
                      "v2" if windowed else "v1")
    target_suffix = (f"late_display_target_ordinals={','.join(str(value) for value in target_ordinals)}\n"
                     if target_ordinals is not None else "")
    window_suffix = ("late_display_windows=0\nlate_display_window_records=none\n"
                     if windowed else "")
    try:
        info = path.lstat()
    except FileNotFoundError:
        return (f"late_display=absent\nlate_display_format={format_version}\n"
                f"late_display_records=0\n{target_suffix}{window_suffix}")
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("late-display receipt is not a regular non-symlink file")
    if info.st_size > byte_limit:
        raise CaptureError("late-display receipt exceeds the bounded recorder contract")
    if info.st_size == 0:
        return (f"late_display=empty\nlate_display_format={format_version}\n"
                f"late_display_records=0\n{target_suffix}{window_suffix}")
    records = parse_late_display_receipt(
        path, windowed=windowed, target_ordinals=target_ordinals)
    links = [(record[5], record[6]) for record in records]
    _validate_title_display_input_links(links, input_path)
    counts: dict[int, int] = {}
    window_counts: dict[int, int] = {}
    for record in records:
        counts[record[4]] = counts.get(record[4], 0) + 1
        window_counts[record[5]] = window_counts.get(record[5], 0) + 1
    digest, size = sha256_file(path)
    register_counts = ",".join(f"0x{reg:04x}:{counts[reg]}" for reg in sorted(counts))
    return (f"late_display=present\nlate_display_format={format_version}\n"
            f"late_display_sha256={digest}\nlate_display_bytes={size}\n"
            f"late_display_records={len(records)}\n"
            f"late_display_register_counts={register_counts}\n"
            + target_suffix
            + (f"late_display_windows={len(window_counts)}\n"
               "late_display_window_records="
               + ",".join(f"{ordinal}:{count}" for ordinal, count in window_counts.items())
               + "\n" if windowed else "")
            +
            f"late_display_first_input_ordinal={records[0][5]}\n"
            f"late_display_last_input_ordinal={records[-1][5]}\n"
            "late_display_input_chronology=linked\n"
            f"late_display_input_chronology_records={len(records)}\n")


def latch_write_receipt_status(path: Path, input_path: Path) -> str:
    """Validate exact 0→1 writes at the recovered main-stage latch site."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "latch_write=absent\nlatch_write_format=v1\nlatch_write_records=0\n"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("latch-write receipt is not a regular non-symlink file")
    if info.st_size > MAX_LATCH_WRITE_BYTES:
        raise CaptureError("latch-write receipt exceeds the bounded recorder contract")
    if info.st_size == 0:
        return "latch_write=empty\nlatch_write_format=v1\nlatch_write_records=0\n"
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise CaptureError("latch-write receipt must remain a regular file")
            payload = stream.read(MAX_LATCH_WRITE_BYTES + 1)
        if len(payload) > MAX_LATCH_WRITE_BYTES:
            raise CaptureError("latch-write receipt exceeds the bounded recorder contract")
        text = payload.decode("ascii")
    except (OSError, UnicodeDecodeError) as error:
        raise CaptureError("latch-write receipt is not readable ASCII") from error
    input_events = _read_zero_route_host_input(input_path)
    previous_cycles = -1
    previous_input = 0
    first_input = 0
    records = 0
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = LATCH_WRITE_LINE.fullmatch(line)
        if not match:
            raise CaptureError("latch-write receipt contains an invalid recorder record")
        ordinal, cycles, input_ordinal, input_frame = map(int, match.groups())
        if ordinal != expected:
            raise CaptureError("latch-write receipt ordinals are not contiguous")
        if cycles < previous_cycles:
            raise CaptureError("latch-write receipt cycles are not monotonic")
        if input_ordinal not in input_events or input_events[input_ordinal] != input_frame:
            raise CaptureError("latch-write receipt is not linked to exact host-input chronology")
        if input_ordinal < previous_input:
            raise CaptureError("latch-write input ordinals are not monotonic")
        if expected == 1:
            first_input = input_ordinal
        previous_cycles, previous_input = cycles, input_ordinal
        records += 1
        if records > MAX_LATCH_WRITE_RECORDS:
            raise CaptureError("latch-write receipt exceeds the recorder record cap")
    digest, size = hashlib.sha256(payload).hexdigest(), len(payload)
    return ("latch_write=present\nlatch_write_format=v1\n"
            f"latch_write_sha256={digest}\nlatch_write_bytes={size}\n"
            f"latch_write_records={records}\n"
            f"latch_write_first_input_ordinal={first_input}\n"
            f"latch_write_last_input_ordinal={previous_input}\n"
            "latch_write_input_chronology=linked\n")


def capture_bounded_console(stream, path: Path, over_limit: threading.Event) -> RecorderConsoleStatus:
    """Drain a console into fixed storage and signal if its total safety cap trips.

    Continue reading after the signal until the owner kills the child: otherwise
    the child could block on a full stdout pipe before the runner can preserve
    a coherent, bounded diagnostic receipt.
    """
    digest = hashlib.sha256()
    retained_digest = hashlib.sha256()
    total = 0
    retained = 0
    with path.open("xb") as destination:
        while True:
            chunk = stream.read(64 * 1024)
            if not chunk:
                break
            total += len(chunk)
            digest.update(chunk)
            if total > MAX_RECORDER_CONSOLE_TOTAL_BYTES:
                over_limit.set()
            if retained < MAX_RECORDER_CONSOLE_LOG_BYTES:
                keep = chunk[:MAX_RECORDER_CONSOLE_LOG_BYTES - retained]
                destination.write(keep)
                retained_digest.update(keep)
                retained += len(keep)
        destination.flush()
        os.fsync(destination.fileno())
    return RecorderConsoleStatus(total, digest.hexdigest(), retained, retained_digest.hexdigest(),
                                 over_limit.is_set())


def recorder_console_status(status: RecorderConsoleStatus) -> str:
    return ("recorder_console=present\n"
            f"recorder_console_sha256={status.sha256}\n"
            f"recorder_console_total_bytes={status.total_bytes}\n"
            f"recorder_console_retained_bytes={status.retained_bytes}\n"
            f"recorder_console_retained_sha256={status.retained_sha256}\n"
            f"recorder_console_truncated={'true' if status.truncated else 'false'}\n"
            f"recorder_console_over_limit={'true' if status.over_limit else 'false'}\n")


def identity_status(name: str, identity: tuple[str, int]) -> str:
    """Place both pre- and post-capture identities in the external receipt."""
    digest, size = identity
    return f"{name}_sha256={digest}\n{name}_bytes={size}\n"


def recorder_config(disk1: Path, disk2: Path, kickstart: Path, output: Path,
                    timing_profile: str = "realtime",
                    diagnostic_disk2_in_drive1: bool = False) -> str:
    try:
        warp_mode = TIMING_PROFILES[timing_profile]
    except KeyError as error:
        raise CaptureError("timing profile is not in the reviewed finite profile set") from error
    posix = lambda path: path.as_posix()
    return "\n".join((
        "# Ephemeral physical-input capture configuration; no debugger or playback.",
        "amiga_model = A500",
        f"kickstart_file = {posix(kickstart)}",
        f"floppy_drive_0 = {posix(disk1)}",
        (f"floppy_drive_1 = {posix(disk2)}" if diagnostic_disk2_in_drive1 else
         "# Keep DF1 empty so disk 2 remains available for the game's explicit"
         " disk-in-DF0 prompt through FS-UAE's ordinary removable-media menu."),
        f"floppy_image_0 = {posix(disk1)}",
        f"floppy_image_1 = {posix(disk2)}",
        "floppy_write_protect = 1",
        f"base_dir = {posix(output / 'runtime')}",
        f"logs_dir = {posix(output / 'logs')}",
        f"save_states_dir = {posix(output / 'states')}",
        "fullscreen = 0",
        "window_width = 640",
        "window_height = 512",
        "console_debugger = 0",
        "use_debugger = 0",
        # A visible physical F10 press requests the mounted, read-only second
        # disk. The host receipt records the mapped swapper action; acceptance
        # by the guest drive still requires separate visible/log evidence.
        "keyboard_key_f10 = action_drive_0_insert_floppy_1",
        "uae_sound_output = interrupts",
        f"warp_mode = {warp_mode}",
        "",
    ))


def input_delivery_file_observed(path: Path) -> bool:
    """Return whether the recorder has begun an input-delivery receipt.

    This is deliberately only a live operator aid. The file remains subject to
    complete bounded grammar and hash validation after FS-UAE has stopped;
    parsing it while the recorder is appending could race a partial line.
    """
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    if not stat.S_ISREG(info.st_mode) or stat.S_ISLNK(info.st_mode):
        raise CaptureError("live host-input receipt is not a regular non-symlink file")
    return info.st_size > 0


def run_capture(args: argparse.Namespace) -> Path:
    release_arg = getattr(args, "source_release", None)
    disk1_arg = getattr(args, "disk1_archive", None)
    disk2_arg = getattr(args, "disk2_archive", None)
    if release_arg:
        if disk1_arg or disk2_arg:
            raise CaptureError("select either one source release or both standalone disk archives")
        source_layout = SOURCE_LAYOUT_RELEASE
        release = require_absolute_regular_file(Path(release_arg), "source release")
        disk1_source = disk2_source = None
        output_sources = (release,)
    else:
        if not disk1_arg or not disk2_arg:
            raise CaptureError("standalone capture requires both --disk1-archive and --disk2-archive")
        source_layout = SOURCE_LAYOUT_STANDALONE
        release = None
        disk1_source = require_absolute_regular_file(Path(disk1_arg), "Deuteros disk 1 archive")
        disk2_source = require_absolute_regular_file(Path(disk2_arg), "Deuteros disk 2 archive")
        output_sources = (disk1_source, disk2_source)
    kickstart = require_absolute_regular_file(Path(args.kickstart_archive), "Kickstart archive")
    recorder = require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    output = reject_unsafe_output(*output_sources, kickstart, output=Path(args.output))
    diagnostic_disk2_in_drive1 = getattr(args, "diagnostic_disk2_in_drive1", False)
    if diagnostic_disk2_in_drive1 and args.capture_intent != "diagnostic-no-input":
        raise CaptureError("Disk 2 in DF1 is allowed only for a no-input diagnostic")
    if not MIN_DURATION_SECONDS <= args.duration_seconds <= MAX_DURATION_SECONDS:
        raise CaptureError(f"duration must be between {MIN_DURATION_SECONDS} and {MAX_DURATION_SECONDS} seconds")
    if not 0 <= args.focus_settle_seconds <= MAX_FOCUS_SETTLE_SECONDS:
        raise CaptureError(f"focus-settle duration must be between 0 and {MAX_FOCUS_SETTLE_SECONDS} seconds")
    environment = dict(os.environ)
    environment.pop("PROJECT_EON_FS_UAE_CALLBACK_MEMORY_RECORD", None)
    environment.pop("PROJECT_EON_FS_UAE_SERVICE_BOUNDARY_RECORD", None)
    environment.pop("PROJECT_EON_FS_UAE_RESPONSE_QUEUE_RECORD", None)
    require_visible_operator_input(environment)
    release_before = (validate_identity(release, "source release", EXPECTED_RELEASE_SHA256, EXPECTED_RELEASE_SIZE)
                      if release is not None else None)
    disk1_archive_before = (validate_identity(disk1_source, "Deuteros disk 1 archive",
        EXPECTED_DISK1_ARCHIVE_SHA256, EXPECTED_DISK1_ARCHIVE_SIZE)
        if disk1_source is not None else None)
    disk2_archive_before = (validate_identity(disk2_source, "Deuteros disk 2 archive",
        EXPECTED_DISK2_ARCHIVE_SHA256, EXPECTED_DISK2_ARCHIVE_SIZE)
        if disk2_source is not None else None)
    kickstart_before = validate_identity(kickstart, "Kickstart archive", EXPECTED_KICKSTART_SHA256, EXPECTED_KICKSTART_SIZE)
    recorder_identity = validate_recorder(recorder)
    is_v17 = recorder_identity[0] == TRV2_RECORDER_V17_SHA256
    is_v18 = recorder_identity[0] == TRV2_RECORDER_V18_SHA256
    is_v19 = recorder_identity[0] == TRV2_RECORDER_V19_SHA256
    is_v20 = recorder_identity[0] == TRV2_RECORDER_V20_SHA256
    is_v21 = recorder_identity[0] == TRV2_RECORDER_V21_SHA256
    is_v22 = recorder_identity[0] == TRV2_RECORDER_V22_SHA256
    is_v23 = recorder_identity[0] == TRV2_RECORDER_V23_SHA256
    is_v24 = recorder_identity[0] == TRV2_RECORDER_V24_SHA256
    is_v25 = recorder_identity[0] == TRV2_RECORDER_V25_SHA256
    is_v26 = recorder_identity[0] == TRV2_RECORDER_V26_CALLBACK_MEMORY_SHA256
    is_v27 = recorder_identity[0] == TRV2_RECORDER_V27_CALLBACK_MEMORY_TARGETED_DISPLAY_SHA256
    is_v29 = recorder_identity[0] == TRV2_RECORDER_V29_SERVICE_BOUNDARY_DISPLAY_SHA256
    is_v31 = recorder_identity[0] == TRV2_RECORDER_V31_RESPONSE_QUEUE_SHA256
    is_v32 = recorder_identity[0] == TRV2_RECORDER_V32_TITLE_CALLBACK_SHA256
    is_v33 = recorder_identity[0] == TRV2_RECORDER_V33_SERVICE_BOUNDARY_SHA256
    is_v30 = recorder_identity[0] == TRV2_RECORDER_V30_SERVICE_RETURN_DISPLAY_SHA256
    has_zero_route_observer = recorder_identity[0] in RECORDER_ZERO_ROUTE_OBSERVATION_HASHES
    has_late_input_sidecars = is_v19 or is_v20 or is_v21 or is_v22 or is_v23 or is_v24 or is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33
    is_v16 = recorder_identity[0] == TRV2_RECORDER_V16_SHA256
    phased_raw = is_v16 or is_v17 or is_v18 or is_v19 or is_v20 or is_v21 or is_v22 or is_v23 or is_v24 or is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33
    raw_format = ("v9-v19-phased" if is_v19 or is_v20 or is_v21 or is_v22 or is_v23 or is_v24 or is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33 else
                  "v9-v18-phased" if is_v18 else
                  "v9-v16-phased" if phased_raw else "v9")
    receipt_version = (CAPTURE_RECEIPT_V33_VERSION if is_v33 else
                       CAPTURE_RECEIPT_V32_VERSION if is_v32 else
                       CAPTURE_RECEIPT_V31_VERSION if is_v31 else
                       CAPTURE_RECEIPT_V30_VERSION if is_v30 else
                       CAPTURE_RECEIPT_V29_VERSION if is_v29 else
                       CAPTURE_RECEIPT_V27_VERSION if is_v27 else
                       CAPTURE_RECEIPT_V26_VERSION if is_v26 else
                       CAPTURE_RECEIPT_V25_VERSION if is_v25 else
                       CAPTURE_RECEIPT_V24_VERSION if is_v24 else
                       CAPTURE_RECEIPT_V23_VERSION if is_v23 else
                       CAPTURE_RECEIPT_V22_VERSION if is_v22 else
                       CAPTURE_RECEIPT_V21_VERSION if is_v21 else
                       CAPTURE_RECEIPT_V20_VERSION if is_v20 else
                       CAPTURE_RECEIPT_V19_VERSION if is_v19 else
                       CAPTURE_RECEIPT_V18_VERSION if is_v18 else
                       CAPTURE_RECEIPT_V17_VERSION if is_v17 else
                       CAPTURE_RECEIPT_V16_PHASED_VERSION if is_v16 else
                       CAPTURE_RECEIPT_VERSION)
    output.mkdir(mode=0o700)
    mounts = [output / name for name in ("release-outer-ro", "disk1-ro", "disk2-ro", "kickstart-ro")]
    for index, mountpoint in enumerate(mounts):
        if index != 0 or release is not None:
            mountpoint.mkdir(mode=0o700)
    mounted: list[Path] = []
    try:
        if release is not None:
            mount_read_only(release, mounts[0])
            mounted.append(mounts[0])
            disk1_archive = mounts[0] / DISK1_ARCHIVE
            disk2_archive = mounts[0] / DISK2_ARCHIVE
            disk1_archive_identity = validate_identity(
                disk1_archive, "Deuteros disk 1 nested archive", EXPECTED_DISK1_ARCHIVE_SHA256,
                EXPECTED_DISK1_ARCHIVE_SIZE)
            disk2_archive_identity = validate_identity(
                disk2_archive, "Deuteros disk 2 nested archive", EXPECTED_DISK2_ARCHIVE_SHA256,
                EXPECTED_DISK2_ARCHIVE_SIZE)
        else:
            assert disk1_source is not None and disk2_source is not None
            disk1_archive, disk2_archive = disk1_source, disk2_source
            assert disk1_archive_before is not None and disk2_archive_before is not None
            disk1_archive_identity, disk2_archive_identity = disk1_archive_before, disk2_archive_before
        mount_read_only(disk1_archive, mounts[1])
        mounted.append(mounts[1])
        mount_read_only(disk2_archive, mounts[2])
        mounted.append(mounts[2])
        mount_read_only(kickstart, mounts[3])
        mounted.append(mounts[3])
        disk1 = mounts[1] / DISK1_IMAGE
        disk2 = mounts[2] / DISK2_IMAGE
        rom = mounts[3] / KICKSTART_IMAGE
        validate_identity(disk1, "clean Deuteros disk 1", EXPECTED_DISK1_SHA256, 901_120)
        validate_identity(disk2, "clean Deuteros disk 2", EXPECTED_DISK2_SHA256, 901_120)
        validate_identity(rom, "Kickstart ROM", EXPECTED_ROM_SHA256, 262_144)
        configuration = output / "deuteros-amiga-capture.fs-uae"
        write_exclusive(configuration, recorder_config(
            disk1, disk2, rom, output, args.timing_profile,
            diagnostic_disk2_in_drive1))
        configuration_identity = sha256_file(configuration)
        command = [str(recorder), str(configuration)]
        write_exclusive(output / "command-tail.txt", " ".join(command) + "\n")
        environment.update({
            "PROJECT_EON_FS_UAE_RAW_RECORD": str(output / "raw-pc.txt"),
            "PROJECT_EON_FS_UAE_INPUT_RECORD": str(output / "host-input-receipt.txt"),
            "PROJECT_EON_FS_UAE_DISPLAY_RECORD": str(output / "title-display.txt"),
        })
        environment.pop("PROJECT_EON_FS_UAE_SELECTOR_DISPATCH_RECORD", None)
        environment.pop("PROJECT_EON_FS_UAE_LATE_RAW_RECORD", None)
        environment.pop("PROJECT_EON_FS_UAE_LATE_SELECTOR_RECORD", None)
        environment.pop("PROJECT_EON_FS_UAE_NEGATIVE_SELECTOR_RECORD", None)
        if is_v17 or is_v18:
            environment["PROJECT_EON_FS_UAE_SELECTOR_DISPATCH_RECORD"] = str(
                output / "selector-dispatch.txt")
        if has_late_input_sidecars:
            environment["PROJECT_EON_FS_UAE_LATE_RAW_RECORD"] = str(
                output / "late-input-pc.txt")
            environment["PROJECT_EON_FS_UAE_LATE_SELECTOR_RECORD"] = str(
                output / "late-selector-dispatch.txt")
        if is_v22 or is_v23 or is_v24 or is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33:
            environment["PROJECT_EON_FS_UAE_LATE_DISPLAY_RECORD"] = str(
                output / "late-display.txt")
        else:
            environment.pop("PROJECT_EON_FS_UAE_LATE_DISPLAY_RECORD", None)
        if has_zero_route_observer:
            environment["PROJECT_EON_FS_UAE_ZERO_ROUTE_RECORD"] = str(
                output / "zero-route-observation.txt")
        if is_v23 or is_v24 or is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33:
            environment["PROJECT_EON_FS_UAE_LATCH_WRITE_RECORD"] = str(
                output / "latch-write.txt")
        else:
            environment.pop("PROJECT_EON_FS_UAE_LATCH_WRITE_RECORD", None)
        if is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33:
            environment["PROJECT_EON_FS_UAE_NEGATIVE_SELECTOR_RECORD"] = str(
                output / "negative-selector.txt")
        if is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33:
            environment["PROJECT_EON_FS_UAE_CALLBACK_MEMORY_RECORD"] = str(
                output / "callback-memory.txt")
        if is_v29 or is_v30 or is_v31 or is_v32 or is_v33:
            environment["PROJECT_EON_FS_UAE_SERVICE_BOUNDARY_RECORD"] = str(
            output / "service-boundary.txt")
        if is_v31 or is_v32 or is_v33:
            environment["PROJECT_EON_FS_UAE_RESPONSE_QUEUE_RECORD"] = str(
                output / "response-queue.txt")
        if is_v32:
            environment["PROJECT_EON_FS_UAE_TITLE_CALLBACK_RECORD"] = str(
                output / "title-callback.txt")
        for instruction in capture_operator_instructions(
                args.capture_intent, late_sampling=has_late_input_sidecars):
            print(instruction)
        print(f"The {args.focus_settle_seconds}-second focus-settle window begins now; the {args.duration_seconds}-second capture window follows.")
        started = time.time()
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        assert process.stdout is not None
        console_result: list[RecorderConsoleStatus] = []
        console_errors: list[BaseException] = []
        console_over_limit = threading.Event()

        def drain_console() -> None:
            try:
                console_result.append(capture_bounded_console(
                    process.stdout, output / "recorder-console.log", console_over_limit))
            except BaseException as error:  # Report it after process cleanup.
                console_errors.append(error)

        console_thread = threading.Thread(target=drain_console,
                                          name="project-eon-fs-uae-console", daemon=True)
        console_thread.start()
        settle_deadline = time.monotonic() + args.focus_settle_seconds
        deadline: float | None = None
        live_input_observed = False
        while True:
            exit_status = process.poll()
            if exit_status is not None:
                break
            now = time.monotonic()
            if deadline is None and now >= settle_deadline:
                deadline = now + args.duration_seconds
                requirement = ("physical input remains required" if args.capture_intent == "physical-input"
                               else "no host input is permitted")
                print(f"CAPTURE WINDOW ACTIVE  {args.duration_seconds} seconds; {requirement}.")
            if not live_input_observed and input_delivery_file_observed(output / "host-input-receipt.txt"):
                live_input_observed = True
                print("HOST INPUT DELIVERY OBSERVED  receipt will be fully validated after capture.")
            remaining = (settle_deadline - now if deadline is None else deadline - now)
            if console_over_limit.is_set():
                process.kill()
                process.wait()
                exit_status = 125
                break
            if remaining <= 0:
                process.kill()
                process.wait()
                exit_status = 124
                break
            console_over_limit.wait(timeout=min(0.1, remaining))
        console_thread.join()
        if console_errors:
            raise CaptureError(f"unable to retain bounded recorder console: {console_errors[0]}")
        if len(console_result) != 1:
            raise CaptureError("bounded recorder console did not produce exactly one receipt")
        ended = time.time()
        release_after = None
        if release is not None:
            release_after = validate_identity(release, "source release", EXPECTED_RELEASE_SHA256, EXPECTED_RELEASE_SIZE)
            if release_before != release_after:
                raise CaptureError("source release changed during capture; evidence is rejected")
        if disk1_source is not None and disk2_source is not None:
            disk1_archive_after = validate_identity(disk1_source, "Deuteros disk 1 archive",
                EXPECTED_DISK1_ARCHIVE_SHA256, EXPECTED_DISK1_ARCHIVE_SIZE)
            disk2_archive_after = validate_identity(disk2_source, "Deuteros disk 2 archive",
                EXPECTED_DISK2_ARCHIVE_SHA256, EXPECTED_DISK2_ARCHIVE_SIZE)
            if (disk1_archive_before, disk2_archive_before) != (disk1_archive_after, disk2_archive_after):
                raise CaptureError("standalone disk archives changed during capture; evidence is rejected")
            disk1_archive_identity, disk2_archive_identity = disk1_archive_after, disk2_archive_after
        kickstart_after = validate_identity(kickstart, "Kickstart archive", EXPECTED_KICKSTART_SHA256, EXPECTED_KICKSTART_SIZE)
        if kickstart_before != kickstart_after:
            raise CaptureError("Kickstart archive changed during capture; evidence is rejected")
        receipt_status = input_receipt_status(output / "host-input-receipt.txt")
        intent_status = capture_intent_status(args.capture_intent, receipt_status,
                                              live_input_observed)
        raw_path = output / "raw-pc.txt"
        input_path = output / "host-input-receipt.txt"
        observation_status = raw_observation_status(raw_path, "raw_pc", raw_format)
        chronology_status = (raw_pc_input_chronology_status(raw_path, input_path, raw_format)
                             if "raw_pc=present\n" in observation_status else "")
        display_status = title_display_receipt_status(output / "title-display.txt", input_path)
        selector_status = (selector_dispatch_status(
            output / "selector-dispatch.txt", raw_path, input_path, raw_format)
            if is_v17 or is_v18 else "")
        late_version = late_raw_pc_version_for_recorder(recorder_identity[0])
        late_status = (late_raw_pc_status(
                           output / "late-input-pc.txt", input_path, late_version)
                       + late_selector_dispatch_status(
                           output / "late-selector-dispatch.txt",
                           output / "late-input-pc.txt", input_path, late_version)
                       if has_late_input_sidecars else "")
        zero_route_status = (zero_route_observation_status(
            output / "zero-route-observation.txt", input_path) if has_zero_route_observer else "")
        late_display_status = (late_display_receipt_status(
            output / "late-display.txt", input_path,
            windowed=is_v24 or is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33,
            target_ordinals=(LATE_DISPLAY_TARGET_ORDINALS_V30 if is_v30 or is_v31 or is_v32 or is_v33 else
                             LATE_DISPLAY_TARGET_ORDINALS_V29 if is_v29 else
                             LATE_DISPLAY_TARGET_ORDINALS_V27 if is_v27 else None))
            if is_v22 or is_v23 or is_v24 or is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33 else "")
        latch_write_status = (latch_write_receipt_status(
            output / "latch-write.txt", input_path) if is_v23 or is_v24 or is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33 else "")
        negative_selector_receipt_status = (negative_selector_status(
            output / "negative-selector.txt", input_path) if is_v25 or is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33 else "")
        callback_memory_receipt_status = (callback_memory_status(
            output / "callback-memory.txt", input_path) if is_v26 or is_v27 or is_v29 or is_v30 or is_v31 or is_v32 or is_v33 else "")
        service_boundary_receipt_status = (service_boundary_status(
            output / "service-boundary.txt", input_path,
            allow_any_input=is_v33, exact_move_sprite_windows=is_v33,
            max_sequences=(MAX_SERVICE_BOUNDARY_SEQUENCES_V33 if is_v33
                           else MAX_SERVICE_BOUNDARY_SEQUENCES))
            if is_v29 or is_v30 or is_v31 or is_v32 or is_v33 else "")
        response_queue_receipt_status = (response_queue_status(
            output / "response-queue.txt", input_path) if is_v31 or is_v32 or is_v33 else "")
        title_callback_receipt_status = (title_callback_status_v32(
            output / "title-callback.txt", input_path) if is_v32 else "")
        write_exclusive(output / "run-status.txt",
                        f"capture_receipt_version={receipt_version}\n"
                        + ("recorder_protocol=deuteros-amiga-fsuae-v33-service-boundary\n" if is_v33 else
                           "recorder_protocol=deuteros-amiga-fsuae-v32-title-callback\n" if is_v32 else
                           "recorder_protocol=deuteros-amiga-fsuae-v31-response-queue\n" if is_v31 else
                           "recorder_protocol=deuteros-amiga-fsuae-v30-service-return-display\n" if is_v30 else
                           "recorder_protocol=deuteros-amiga-fsuae-v29-service-boundary-display\n" if is_v29 else
                           "recorder_protocol=deuteros-amiga-fsuae-v27-callback-memory-targeted-display\n" if is_v27 else
                           "recorder_protocol=deuteros-amiga-fsuae-v26-callback-memory\n" if is_v26 else
                           "recorder_protocol=deuteros-amiga-fsuae-v25\n" if is_v25 else
                           "recorder_protocol=deuteros-amiga-fsuae-v24\n" if is_v24 else
                           "recorder_protocol=deuteros-amiga-fsuae-v23\n" if is_v23 else
                           "recorder_protocol=deuteros-amiga-fsuae-v22\n" if is_v22 else
                           "recorder_protocol=deuteros-amiga-fsuae-v21\n" if is_v21 else
                           "recorder_protocol=deuteros-amiga-fsuae-v20\n" if is_v20 else
                           "recorder_protocol=deuteros-amiga-fsuae-v19\n" if is_v19 else
                           "recorder_protocol=deuteros-amiga-fsuae-v18\n" if is_v18 else
                           "recorder_protocol=deuteros-amiga-fsuae-v17\n" if is_v17 else
                           "recorder_protocol=deuteros-amiga-fsuae-v16\n" if is_v16 else
                           "recorder_protocol=deuteros-amiga-fsuae-v15\n"
                           if recorder_identity[0] == TRV2_RECORDER_V15_SHA256 else "")
                        + f"source_layout={source_layout}\n"
                        f"source_container={'single-outer-zip' if release is not None else 'two-independent-zip-files'}\n"
                        f"content_release_sha256={EXPECTED_RELEASE_SHA256}\n"
                        f"timing_profile={args.timing_profile}\n"
                        f"focus_settle_seconds={args.focus_settle_seconds}\n"
                        f"host_input_observed_during_capture={'true' if live_input_observed else 'false'}\n"
                        f"exit_status={exit_status}\nstart_unix={started:.6f}\nend_unix={ended:.6f}\n"
                        + (identity_status("source_release", release_after) if release_after is not None else "")
                        + identity_status("kickstart_archive", kickstart_after)
                        + identity_status("disk1_archive", disk1_archive_identity)
                        + identity_status("disk2_archive", disk2_archive_identity)
                        + identity_status("recorder", recorder_identity)
                        + identity_status("configuration", configuration_identity)
                        + intent_status + receipt_status + observation_status + chronology_status + display_status
                        + selector_status + late_status + zero_route_status + late_display_status
                        + latch_write_status
                        + negative_selector_receipt_status
                        + callback_memory_receipt_status
                        + service_boundary_receipt_status
                        + response_queue_receipt_status
                        + title_callback_receipt_status
                        + recorder_console_status(console_result[0]))
        if console_result[0].over_limit:
            raise CaptureError(
                "recorder console exceeded the 64 MiB safety cap; evidence was retained but is not admitted")
        print("CAPTURE FINISHED  external evidence only; host-input receipt status is in run-status.txt")
        return output
    finally:
        unmount_errors: list[BaseException] = []
        for mountpoint in reversed(mounted):
            try:
                unmount(mountpoint)
            except Exception as error:
                unmount_errors.append(error)
        if unmount_errors:
            raise CaptureError(f"unable to clean up {len(unmount_errors)} read-only capture view(s): {unmount_errors[0]}")


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--source-release", help="Absolute recognised English Deuteros Amiga ZIP path")
    source.add_argument("--disk1-archive", help="Absolute recognised standalone Deuteros disk 1 ZIP path")
    parser.add_argument("--disk2-archive", help="Absolute recognised standalone Deuteros disk 2 ZIP path")
    parser.add_argument("--kickstart-archive", required=True, help="Absolute recognised Kickstart 1.3 ZIP path")
    parser.add_argument("--recorder", required=True, help="Absolute reviewed FS-UAE recorder binary path")
    parser.add_argument("--output", required=True, help="New absolute cache directory for external capture evidence")
    parser.add_argument("--duration-seconds", type=int, default=120, help="Visible operator window duration (15-600; default: 120)")
    parser.add_argument("--focus-settle-seconds", type=int, default=10,
                        help="Manual visible-window focus time before capture (0-120; default: 10)")
    parser.add_argument("--capture-intent", choices=tuple(sorted(CAPTURE_INTENTS)), required=True,
                        help="Required operator declaration: physical-input or diagnostic-no-input")
    parser.add_argument("--timing-profile", choices=tuple(sorted(TIMING_PROFILES)), default="realtime",
                        help="Recorder timing profile (default: realtime; warp is diagnostic only)")
    parser.add_argument("--diagnostic-disk2-in-df1", dest="diagnostic_disk2_in_drive1",
                        action="store_true",
                        help="No-input diagnostic only: also insert Disk 2 in DF1")
    arguments = parser.parse_args(argv)
    if arguments.source_release:
        if arguments.disk2_archive:
            parser.error("--disk2-archive cannot be used with --source-release")
    elif not arguments.disk1_archive or not arguments.disk2_archive:
        parser.error("provide --source-release or both --disk1-archive and --disk2-archive")
    if arguments.diagnostic_disk2_in_drive1 and arguments.capture_intent != "diagnostic-no-input":
        parser.error("--diagnostic-disk2-in-df1 requires --capture-intent diagnostic-no-input")
    return arguments


def main(argv: list[str] | None = None) -> int:
    try:
        output = run_capture(parse_arguments(sys.argv[1:] if argv is None else argv))
    except (CaptureError, OSError, subprocess.SubprocessError) as error:
        print(f"Deuteros Amiga capture preflight rejected: {error}", file=sys.stderr)
        return 2
    print(f"EXTERNAL CAPTURE DIRECTORY  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
