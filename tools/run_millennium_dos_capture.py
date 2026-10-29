#!/usr/bin/env python3
"""Run a read-only, operator-driven Millennium DOS recorder session.

This helper is not an emulator distribution, input injector, trace assembler,
or runtime component. It prepares a fresh external evidence directory around
an already reviewed DOSBox-X recorder. The operator must interact with the
visible emulator window; headless SDL and DOSBox-X AUTOTYPE are rejected.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import threading
import time


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_RELEASE_SHA256 = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
EXPECTED_RELEASE_SIZE = 328_383
# V21's successor receives these immutable identities in the recorder config
# before guest execution. A future reviewed observer must still recompute the
# loaded image fingerprints itself; configured values are expected identities,
# never evidence from the guest or a substitute for the runner's source hash.
EXPECTED_DIRECT_MEDIA_SET_SHA256 = "d938cd6a611a83897a745b257a371613b73a7dddffb2d336ec2167a192803783"
EXPECTED_TITLES_EXE_SHA256 = "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"
EXPECTED_2200AD_EXE_SHA256 = "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"
EXPECTED_TITLES_EXE_SIZE = 7_022
EXPECTED_2200AD_EXE_SIZE = 54_391
# The capture helper never accepts a merely executable emulator as a recorder:
# its hooks and input-receipt contract are part of the provenance boundary.
EXPECTED_RECORDER_SHA256 = "7b959f7aee3d2db0513db4f14e3075f306e798e25adaeeebd96aedd81aef65da"
RECORDER_PROTOCOLS = {
    # v11 is the existing exact callback-loop receipt. Its shape remains the
    # default so a new diagnostic cannot silently change prior evidence.
    "v11": ("22", EXPECTED_RECORDER_SHA256),
    # This separately built recorder retains the immediately preceding normal
    # core tuple at its own callback boundary. It is observational only.
    "v12-predecessor": ("22", "20a5ec331ca71e541d2f6d42c1ab49eca0fec5dabf298b6faf51fa45c63c24ed"),
    # v13 correlates only observer ordinals: a visible SDL key event and the
    # original title's next documented INT 21h/AH=06h poll. It neither reads
    # the DOS result nor captures/derives a title frame, audio, or action.
    "v13-title-poll": ("22", "07d80df74d303b519884d37dd474da071b414e98396e8ae030ad89256432521b"),
    # v14 retains a fixed normal-core instruction ring only when the known
    # DOSBox-X INT 6 callback boundary is reached. It is a separate recorder
    # file, leaving the v13 result grammar and prior evidence unchanged.
    "v14-normal-core-history": ("22", "748c1c934a78a28baef083fc352b552644f9665bc27fc032db0fdd7463ee5c63"),
    "v15-anomaly-entry": ("22", "0f74d8350ef61249d9ede9b11baa60133f407eaa863c60b66d18e86671bbc65e"),
    "v16-anomaly-entry": ("22", "ab5b1f42a486a7f768f33978c7504cc59fd86a4eb29f80a9041205d383ec91fa"),
    "v17-anomaly-entry": ("22", "d03e11ea48710e00d97c127f9b3d3ba5bc6f4227fafb7764e2b5950e50704ae6"),
    "v18-ivt-entry": ("22", "74420b4f8a0e2009f08b278ead1f9b36804404808b27895f327f204836d65e11"),
    "v19-int93-vector": ("22", "487cbc3292b6b279ff0cfe444dbded64ae86c5cb174af5161ddbd052d53022f0"),
    "v20-title-entry-transfer": ("22", "a07aa94abd5e7a38c52b81e2080a4161dc33a5bd15a63be9b6316219e65b2ef5"),
    # V21 observes one original DOS Set Interrupt Vector transaction after
    # checking its instruction preimage.  It remains diagnostics-only.
    "v21-int93-installation": ("22", "18ec0ead7d08deeca694fbbe8155d5f5e6a99562adaea22fe914a691961fe1f1"),
}
# This binary has completed the independent *static* observer review but has
# not completed the release/pin process.  Keeping it separate from
# RECORDER_PROTOCOLS is deliberate: a normal capture cannot accidentally
# treat an experimental run as recovery-admissible evidence.
EXPERIMENTAL_OBSERVER_SHA256 = "26acf29a06ef53abb876b04d155540e38370daf5beb85fc8c51ffcd08bb98fce"
EXPERIMENTAL_OBSERVER_PROTOCOLS = {
    "v21-int93-installation": frozenset({EXPERIMENTAL_OBSERVER_SHA256}),
    "millennium-dos-en-driver-load-return-v1": frozenset({
        # Separately built platform binaries from the same pinned source and
        # patch. Both remain experimental-only and diagnostic-no-input.
        "57020c1138879a6f53394f592b6f88bcd69875bfc06c98f7cd9b9a64372e8404",
        "942f30f2199350a51d0ff1e7c024f4e226d29b5576dc6ea0d61b7556c12468e8",
    }),
    "millennium-dos-en-driver-load-return-v2": frozenset({
        "a03a43372a31b89514007c5053a8890c2b13a653c1562c379c3eaee6edc030e6",
    }),
    # Bounded DOS INT 21h return-register pairs at four hash-bound TITLES.EXE
    # sites. This is an external diagnostic only; it stops before guest IRET.
    "millennium-dos-ah48-handler-return-v1": frozenset({
        "3c3b6d9312d59cfada0f4b75466f90da5193e61e0450271cbe268fbb2b85c65c",
    }),
    # Separately built observer bound only to the third allocator call at
    # TITLES.EXE:$1b4f. Its binary and v3 sidecar grammar do not widen v1.
    "millennium-dos-ah48-site-1b4f-v2": frozenset({
        "75ddc62f68817bfb37cb59d8a283ea06e89cf8d46104bf3458c0d99fcfa2b6ef",
    }),
    "millennium-dos-ah48-sites-1bca-1bd5-v3": frozenset({
        "c8d0795e32e566a7a998ef9f26542fd85652f27b957144a06858181ae9379d41",
    }),
    # The separately pinned v4 recorder adds a bounded, hash-linked SDL host
    # input receipt. It remains experimental and recovery-inadmissible.
    "millennium-dos-ah48-sites-1bca-1bd5-v4-host-input": frozenset({
        "6e32911220bfa480c4890c59729ebb4ef399e7b8d1da6841d959817f19e312f9",
    }),
    "millennium-dos-ah48-sites-1bca-1bd5-v5-after-iret": frozenset({
        "0ebf895c90fe0fbaddadf5303e96aaf3cb5bd1dea45b6650a9024add4b23ec2c",
    }),
    "millennium-dos-title-library-v6": frozenset({
        "a67ea94cf02abb786ad070b28a41b168c3217a9a9893059b179f3d528b42eeef",
    }),
}
EXPERIMENTAL_OBSERVER_SIZES = {
    "millennium-dos-en-driver-load-return-v1": {
        "57020c1138879a6f53394f592b6f88bcd69875bfc06c98f7cd9b9a64372e8404": 15_809_768,
        "942f30f2199350a51d0ff1e7c024f4e226d29b5576dc6ea0d61b7556c12468e8": 132_933_552,
    },
    "millennium-dos-en-driver-load-return-v2": {
        "a03a43372a31b89514007c5053a8890c2b13a653c1562c379c3eaee6edc030e6": 132_934_416,
    },
    "millennium-dos-ah48-handler-return-v1": {
        "3c3b6d9312d59cfada0f4b75466f90da5193e61e0450271cbe268fbb2b85c65c": 122_502_752,
    },
    "millennium-dos-ah48-site-1b4f-v2": {
        "75ddc62f68817bfb37cb59d8a283ea06e89cf8d46104bf3458c0d99fcfa2b6ef": 122_505_616,
    },
    "millennium-dos-ah48-sites-1bca-1bd5-v3": {
        "c8d0795e32e566a7a998ef9f26542fd85652f27b957144a06858181ae9379d41": 134_030_152,
    },
    "millennium-dos-ah48-sites-1bca-1bd5-v4-host-input": {
        "6e32911220bfa480c4890c59729ebb4ef399e7b8d1da6841d959817f19e312f9": 19_598_136,
    },
    "millennium-dos-ah48-sites-1bca-1bd5-v5-after-iret": {
        "0ebf895c90fe0fbaddadf5303e96aaf3cb5bd1dea45b6650a9024add4b23ec2c": 22_842_648,
    },
    "millennium-dos-title-library-v6": {
        "a67ea94cf02abb786ad070b28a41b168c3217a9a9893059b179f3d528b42eeef": 123_683_760,
    },
}
EXPERIMENTAL_OBSERVER_RECEIPT_VERSIONS = {
    "millennium-dos-en-driver-load-return-v1": "22",
    "millennium-dos-en-driver-load-return-v2": "22",
    "millennium-dos-ah48-handler-return-v1": "22",
    "millennium-dos-ah48-site-1b4f-v2": "22",
    "millennium-dos-ah48-sites-1bca-1bd5-v3": "22",
    "millennium-dos-ah48-sites-1bca-1bd5-v4-host-input": "22",
    "millennium-dos-ah48-sites-1bca-1bd5-v5-after-iret": "22",
    "millennium-dos-title-library-v6": "22",
}
EXPERIMENTAL_ONLY_PROTOCOLS = frozenset(EXPERIMENTAL_OBSERVER_RECEIPT_VERSIONS)
DRIVER_LOAD_RETURN_PROTOCOLS = frozenset({
    "millennium-dos-en-driver-load-return-v1",
    "millennium-dos-en-driver-load-return-v2",
})
DRIVER_LOAD_RETURN_GRACEFUL_CLOSE_PROTOCOL = "millennium-dos-en-driver-load-return-v2"
AH48_HANDLER_RETURN_PROTOCOL = "millennium-dos-ah48-handler-return-v1"
AH48_SITE_1B4F_RETURN_PROTOCOL = "millennium-dos-ah48-site-1b4f-v2"
AH48_SITES_1BCA_1BD5_RETURN_PROTOCOL = "millennium-dos-ah48-sites-1bca-1bd5-v3"
AH48_SITES_1BCA_1BD5_V4_HOST_INPUT_PROTOCOL = (
    "millennium-dos-ah48-sites-1bca-1bd5-v4-host-input")
AH48_SITES_1BCA_1BD5_V5_AFTER_IRET_PROTOCOL = (
    "millennium-dos-ah48-sites-1bca-1bd5-v5-after-iret")
AH48_TITLE_LIBRARY_V6_PROTOCOL = "millennium-dos-title-library-v6"
BUFFERED_HOST_INPUT_RECEIPT_PROTOCOLS = frozenset({
    AH48_SITES_1BCA_1BD5_V4_HOST_INPUT_PROTOCOL,
    AH48_SITES_1BCA_1BD5_V5_AFTER_IRET_PROTOCOL,
    AH48_TITLE_LIBRARY_V6_PROTOCOL,
})
AH48_HANDLER_RETURN_PROTOCOLS = frozenset({
    AH48_HANDLER_RETURN_PROTOCOL, AH48_SITE_1B4F_RETURN_PROTOCOL,
    AH48_SITES_1BCA_1BD5_RETURN_PROTOCOL,
    AH48_SITES_1BCA_1BD5_V4_HOST_INPUT_PROTOCOL,
    AH48_SITES_1BCA_1BD5_V5_AFTER_IRET_PROTOCOL,
    AH48_TITLE_LIBRARY_V6_PROTOCOL,
})
GRACEFUL_WINDOW_CLOSE_PROTOCOLS = frozenset({
    DRIVER_LOAD_RETURN_GRACEFUL_CLOSE_PROTOCOL,
    *AH48_HANDLER_RETURN_PROTOCOLS,
})
AH48_MAX_BYTES = 4 * 1024 * 1024
AH48_MAX_EVENTS = 4096
AH48_HEADER = (
    b"# schema=project-eon.dos-ah48-observer/v2;image=TITLES.EXE;"
    b"sha256=3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6;"
    b"capture-point=DOS_21Handler-return-before-IRET;guest-memory-dump=none\n"
)
AH48_EXPECTED_AH = {0x1B26: 0x4A, 0x1B2D: 0x48, 0x1B38: 0x49, 0x1B3F: 0x48}
AH48_SITE_1B4F_HEADER = (
    b"# schema=project-eon.dos-ah48-observer/v3;image=TITLES.EXE;"
    b"sha256=3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6;"
    b"capture-point=DOS_21Handler-return-before-IRET;guest-memory-dump=none\n"
)
AH48_SITE_1B4F_EXPECTED_AH = {0x1B4F: 0x48}
AH48_SITES_1BCA_1BD5_HEADER = (
    b"# schema=project-eon.dos-ah48-observer/v4;image=TITLES.EXE;"
    b"sha256=3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6;"
    b"capture-point=DOS_21Handler-return-before-IRET;guest-memory-dump=none\n"
)
AH48_SITES_1BCA_1BD5_EXPECTED_AH = {0x1BCA: 0x48, 0x1BD5: 0x49}
AH48_IRET_HEADER = (
    b"# schema=project-eon.dos-ah48-observer/v5;image=TITLES.EXE;"
    b"sha256=3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6;"
    b"capture-point=TITLES-caller-after-DOS_21-IRET;guest-memory-dump=none\n"
)
AH48_TITLE_LIBRARY_HEADER = (
    b"# schema=project-eon.dos-ah48-observer/v6;image=TITLES.EXE;"
    b"sha256=3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6;"
    b"capture-point=DOS_21Handler-return-before-IRET;guest-memory-dump=none\n"
)
AH48_TITLE_LIBRARY_IRET_HEADER = (
    b"# schema=project-eon.dos-ah48-observer/v6;image=TITLES.EXE;"
    b"sha256=3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6;"
    b"capture-point=TITLES-caller-after-DOS_21-IRET;guest-memory-dump=none\n"
)
AH48_IRET_REGISTER_NAMES = (
    "ax", "bx", "cx", "dx", "si", "di", "bp", "cs", "ip", "ds", "es", "ss", "sp", "flags")
GAME_ROOT = "millennium-return-to-earth-2-2"
MACHINE_PROFILES = {"svga_s3", "ega"}
# A new capture must state whether it is an intentionally no-input diagnostic
# or an operator-led physical-input session.  The runner later binds that
# declaration to the recorder-created receipt; it never writes input itself.
CAPTURE_INTENTS = {"diagnostic-no-input", "physical-input"}
MIN_DURATION_SECONDS = 15
MAX_DURATION_SECONDS = 600
MAX_FOCUS_SETTLE_SECONDS = 120
MAX_OPERATOR_SHUTDOWN_SECONDS = 120
# The reviewed recorder caps host input at 256 short text records.  Keep a
# generous, fixed ceiling here so a damaged or substituted recorder cannot
# turn a receipt status check into unbounded host-side I/O.
MAX_INPUT_RECEIPT_BYTES = 64 * 1024
MAX_INPUT_RECEIPT_RECORDS = 256
MAX_RAW_OBSERVATION_BYTES = 8 * 1024 * 1024
# A defective external recorder can print a tight exception loop. Retain a
# reviewable prefix while hashing and counting the complete byte stream, rather
# than letting terminal output or an evidence cache grow without limit.
MAX_RECORDER_CONSOLE_LOG_BYTES = 1024 * 1024
# Hashing a stream indefinitely still permits a defective recorder to consume
# host CPU and pipe bandwidth for the entire operator window.  Stop the
# recorder once it crosses this deliberately generous cap, retain its bounded
# prefix, and leave a rejected-but-reviewable receipt instead of silently
# treating the partial diagnostic as a normal timeout.
MAX_RECORDER_CONSOLE_TOTAL_BYTES = 64 * 1024 * 1024
# Receipt v2 binds the retained console prefix as well as the complete stream.
# A verifier must never treat a pre-v2 receipt as if that missing integrity
# property had been observed.
# The five reviewed result shapes are external recorder observations only.
# They are a finite grammar boundary, not a DOS ABI or game-state model.
MAX_RAW_RESULT_RECORDS = 256
MAX_NORMAL_CORE_HISTORY_BYTES = 1024
MAX_NORMAL_CORE_ANOMALY_BYTES = 1024
MAX_TITLE_ENTRY_TRANSFER_BYTES = 512
# One installer transaction is a single, fixed-width ASCII observation.  This
# small cap makes a substituted external recorder unable to turn the optional
# diagnostic into an unbounded host-side input.
MAX_INT93_INSTALLATION_BYTES = 384
# Optional post-INT 21h loader observations are raw machine state only. The
# guest buffer is represented by its address, bounded length, and digest.
MAX_DRIVER_LOAD_RETURN_BYTES = 64 * 1024
MAX_DRIVER_LOAD_RETURN_RECORDS = 128
MAX_DRIVER_LOAD_BUFFER_BYTES = 4_366
DRIVER_LOAD_RETURN_SITES = (0x02D4, 0x02ED, 0x02FC, 0x030B, 0x0315, 0x031B)
RAW_RESULT_LINE = re.compile(
    r"raw-result\t([1-9][0-9]*) ([1-9][0-9]*) "
    r"(?:image=(mill\.com|titles\.exe) pc=0x(020e|0213|0129) "
    r"(?:source-int=0x(21|91) source-ax=0x([0-9a-f]{4}) ax=0x([0-9a-f]{4})|"
    r"source-call=0x0511 ax=0x([0-9a-f]{4}))|"
    r"private-vector image=titles\.exe pc=0x0127 int=0x91 vector_ip=0x[0-9a-f]{4} vector_cs=0x[0-9a-f]{4}|"
    r"private-handler-entry int=0x91 cs=0x[0-9a-f]{4} ip=0x[0-9a-f]{4}|"
    r"private-handler-return int=0x91 caller=titles\.exe pc=0x0129 ax=0x[0-9a-f]{4} flags=0x[0-9a-f]{4}|"
    r"fault=unhandled-interrupt int=0x06 cs=0x[0-9a-f]{4} ip=0x[0-9a-f]{4} "
    r"ss=0x[0-9a-f]{4} sp=0x[0-9a-f]{4}(?: return_ip=0x[0-9a-f]{4} "
    r"return_cs=0x[0-9a-f]{4} return_flags=0x[0-9a-f]{4} code=0x[0-9a-f]{8})? "
    r"ax=0x[0-9a-f]{4} bx=0x[0-9a-f]{4} cx=0x[0-9a-f]{4} dx=0x[0-9a-f]{4})\n")
V12_PREDECESSOR_FAULT_LINE = re.compile(
    r"raw-result\t([1-9][0-9]*) ([1-9][0-9]*) fault=unhandled-interrupt int=0x06 "
    r"cs=0x[0-9a-f]{4} ip=0x[0-9a-f]{4} ss=0x[0-9a-f]{4} sp=0x[0-9a-f]{4} "
    r"return_ip=0x[0-9a-f]{4} return_cs=0x[0-9a-f]{4} return_flags=0x[0-9a-f]{4} "
    r"code=0x[0-9a-f]{8} ax=0x[0-9a-f]{4} bx=0x[0-9a-f]{4} cx=0x[0-9a-f]{4} "
    r"dx=0x[0-9a-f]{4} predecessor_valid=[01] predecessor_cs=0x[0-9a-f]{4} "
    r"predecessor_ip=0x[0-9a-f]{4} predecessor_code=0x[0-9a-f]{8} "
    r"predecessor_recognised_image=[01]\n")
TITLE_INPUT_POLL_LINE = re.compile(
    r"raw-result\t([1-9][0-9]*) ([1-9][0-9]*) title-input-poll "
    r"image=titles\.exe pc=0x0d0a host_key_ordinal=([0-9]+) ah=0x06 dl=0xff\n")
NORMAL_CORE_HISTORY_LINE = re.compile(
    r"normal-core-history-v1 count=([1-9]|1[0-6]) entries="
    r"([0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{8}(?:,[0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{8}){0,15})\n")
KNOWN_V14_NORMAL_CORE_HISTORY = tuple(
    [f"0e70:{ip:04x}:00000000" for ip in range(0x18e4, 0x1901, 2)]
    + ["f000:ca60:fe380300"])
NORMAL_CORE_ANOMALY_LINE = re.compile(
    r"normal-core-anomaly-v1 target=([0-9a-f]{4}:[0-9a-f]{4}) ss=([0-9a-f]{4}) "
    r"sp=([0-9a-f]{4}) prior_count=([1-9]|1[0-6]) entries="
    r"([0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{8}(?:,[0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{8}){0,15})\n")
KNOWN_V18_IVT_PRIOR = (
    "0a8d:051f:80fa0174", "0a8d:0522:740bb950", "0a8d:052f:f7e15903", "0a8d:0531:5903c159",
    "0a8d:0532:03c159c3", "0a8d:0534:59c31e06", "0a8d:0535:c31e061e", "0a8d:076b:8bd0b403",
    "0a8d:076d:b403e8bd", "0a8d:076f:e8bdf95b", "0a8d:012f:1e565755", "0a8d:0130:56575506",
    "0a8d:0131:575506cd", "0a8d:0132:5506cd93", "0a8d:0133:06cd9307", "0a8d:0134:cd93075d")
KNOWN_V19_INT93_EVENT = b"event\t6 6 int93-vector image=titles.exe pc=0x0134 int=0x93 vector_ip=0x0000 vector_cs=0x0000\n"
TITLE_ENTRY_TRANSFER_LINE = re.compile(
    r"title-entry-transfer-v1 image=titles\.exe declared_entry_cs=0x([0-9a-f]{4}) "
    r"predecessor_valid=([01]) predecessor_cs=0x([0-9a-f]{4}) predecessor_ip=0x([0-9a-f]{4}) "
    r"predecessor_code=0x([0-9a-f]{8}) entry_cs=0x([0-9a-f]{4}) entry_ip=0x([0-9a-f]{4}) "
    r"entry_code=0x([0-9a-f]{8})\n")
KNOWN_V20_TITLE_ENTRY_TRANSFER = (
    b"title-entry-transfer-v1 image=titles.exe declared_entry_cs=0x0e70 "
    b"predecessor_valid=1 predecessor_cs=0x0000 predecessor_ip=0x0001 "
    b"predecessor_code=0xca00f00e entry_cs=0x0e70 entry_ip=0xfffe "
    b"entry_code=0x00000000\n")
# These are static, hash-addressed release offsets only.  They identify the
# two observed DOS Set Interrupt Vector instruction sites, not their runtime
# order, a private-handler ABI, or a playable path.  The external observer
# checks the complete opcode preimage and AX=2593h before it serializes a
# record; the receipt side checks the finite image/PC vocabulary below.
INT93_INSTALLATION_CANDIDATES = {
    ("titles.exe", "1163"): "c5161c01b89325cd21",
    ("2200ad.exe", "4175"): "c5161a01b89325cd21",
}
INT93_INSTALLATION_LINE = re.compile(
    r"int93-installation-v1 image=(titles\.exe|2200ad\.exe) pc=0x([0-9a-f]{4}) "
    r"vector=0x93 ds=0x([0-9a-f]{4}) dx=0x([0-9a-f]{4}) "
    r"target_preimage=0x([0-9a-f]{8}) vector_ip=0x([0-9a-f]{4}) "
    r"vector_cs=0x([0-9a-f]{4})\n")
DRIVER_LOAD_RETURN_LINE = re.compile(
    r"driver-load-return-v1 ordinal=([1-9][0-9]{0,2}) image=mill\.com cs=([0-9a-f]{4}) "
    r"pc=([0-9a-f]{4}) ax=([0-9a-f]{4}) bx=([0-9a-f]{4}) cx=([0-9a-f]{4}) "
    r"dx=([0-9a-f]{4}) si=([0-9a-f]{4}) di=([0-9a-f]{4}) ds=([0-9a-f]{4}) "
    r"es=([0-9a-f]{4}) ss=([0-9a-f]{4}) sp=([0-9a-f]{4}) flags=([0-9a-f]{4}) "
    r"buffer=(none|ds0:(?:[1-9][0-9]{0,3}):[0-9a-f]{64})\n")
DRIVER_LOAD_RETURN_OVERFLOW_LINE = re.compile(
    r"driver-load-return-overflow cap=128\n")
DRIVER_LOAD_RETURN_END_LINE = re.compile(
    r"driver-load-return-end count=(0|[1-9][0-9]{0,2}) overflow=([01])\n")
# The recorder emits SDL key data as opaque lowercase hexadecimal fields. The
# grammar authenticates the bounded external receipt shape only; it does not
# claim DOS accepted a key or assign its original-game meaning.
HOST_KEY_LINE = re.compile(
    r"host-key ([1-9][0-9]*) ticks=([0-9]+) state=(down|up) "
    r"scancode=0x([0-9a-f]+) sym=0x([0-9a-f]+) mod=0x([0-9a-f]+)\n")
# v9 binds the first post-handler caller re-entry to v8's observed endpoint.
# Its AX and FLAGS are raw machine state only; they are not a private ABI or a
# runtime result contract.
CAPTURE_RECEIPT_VERSION = "11"
TERMINATION_REASONS = {"emulator-exit", "timeout", "console-safety-cap", "known-unhandled-interrupt"}


class CaptureError(RuntimeError):
    """A local preflight failure that must not create a capture receipt."""


class RecorderConsoleStatus:
    """Small importlib-safe value object for a bounded console receipt."""

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


def is_system_tmp_path(path: Path) -> bool:
    """Reject the lexical `/tmp` route before platform-specific resolution.

    macOS resolves `/tmp` through `/private/tmp`, while Windows resolves a
    POSIX-looking `Path('/tmp/...')` on its current drive.  The capture
    contract names the route supplied by the operator, so checking only a
    resolved path silently weakened it on both hosts.
    """
    normalized = path.as_posix().replace("\\", "/")
    if normalized == "/tmp" or normalized.startswith("/tmp/"):
        return True
    parts = path.parts
    return bool(path.anchor and len(parts) > 1 and parts[1].casefold() == "tmp")


def reject_unsafe_output(source: Path, output: Path) -> Path:
    # Preserve the no-/tmp contract before Windows reclassifies a POSIX-style
    # spelling as a current-drive relative path.
    if is_system_tmp_path(output):
        raise CaptureError("output must not use /tmp; use a Project Eon cache path")
    if not output.is_absolute():
        raise CaptureError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise CaptureError("output directory must not exist")
    resolved = output.resolve(strict=False)
    if resolved == ROOT or ROOT in resolved.parents:
        raise CaptureError("output must stay outside the repository")
    if source.parent == resolved or source.parent in resolved.parents:
        raise CaptureError("output must stay outside the supplied-media directory")
    parent = resolved.parent
    if not parent.is_dir() or parent.is_symlink():
        raise CaptureError("output parent must be an existing non-symlink directory")
    return resolved


def validate_source_release(source: Path) -> tuple[str, int]:
    digest, size = sha256_file(source)
    if digest != EXPECTED_RELEASE_SHA256 or size != EXPECTED_RELEASE_SIZE:
        raise CaptureError("source release is not the exact recognised English Millennium DOS archive")
    return digest, size


def validate_recorder(path: Path,
                      expected_sha256: str | frozenset[str] | None = None) -> tuple[str, int]:
    if expected_sha256 is None:
        expected_sha256 = EXPECTED_RECORDER_SHA256
    digest, size = sha256_file(path)
    accepted_hashes = ({expected_sha256} if isinstance(expected_sha256, str)
                       else expected_sha256)
    if digest not in accepted_hashes:
        expected_display = (next(iter(accepted_hashes)) if len(accepted_hashes) == 1
                            else sorted(accepted_hashes))
        raise CaptureError(
            "recorder hash does not match the reviewed DOSBox-X build "
            f"(expected SHA-256 {expected_display}, got {digest}); select the reviewed "
            "external recorder for the requested --recorder-protocol")
    return digest, size


def require_visible_operator_input(environment: dict[str, str]) -> None:
    if environment.get("SDL_VIDEODRIVER", "").lower() == "dummy":
        raise CaptureError("headless SDL is forbidden; a physical operator must use the visible emulator window")
    if not environment.get("DISPLAY") and not environment.get("WAYLAND_DISPLAY"):
        raise CaptureError("a visible X11 or Wayland display is required for physical input capture")


def recorder_config(game_root: Path, machine_profile: str = "svga_s3", *,
                    release_sha256: str | None = None,
                    graceful_host_close: bool = False,
                    readonly_cdrom_archive: bool = False) -> str:
    """Return the fixed, evidence-reviewed configuration with one read-only game root.

    Millennium's 16-bit startup reaches a documented word copy from the last
    offset in a segment.  The pinned recorder's default ``segment limits=true``
    turns that real-mode wrapping operation into an endless #GP diagnostic
    loop.  Disable only that emulator compatibility check; this does not
    alter guest memory, the original archive, or the physical-input policy.
    The resulting configuration remains an explicit, hash-bound capture
    preimage rather than an implicit runtime fallback.
    """
    if machine_profile not in MACHINE_PROFILES:
        raise CaptureError("machine profile is not in the reviewed finite profile set")
    lines = [
        "[sdl]",
        "fullscreen=false",
        "output=surface",
        "vsync=false",
        "",
        "[dosbox]",
        f"machine={machine_profile}",
        "memsize=16",
        *(('quit warning=false',) if graceful_host_close else ()),
        "",
        "[render]",
        "aspect=false",
        "",
        "[cpu]",
        "core=normal",
        "cycles=max",
        "segment limits=false",
        "",
        "[sblaster]",
        "sbtype=none",
        "",
        # DOSBox-X's repeated INT 6 callback otherwise floods captured stderr
        # with one error line per emulated instruction. Logging is host-side
        # diagnostics; route it away and suppress only the CPU log group while
        # leaving the observer's dedicated, bounded raw sidecars untouched.
        "[log]",
        "logfile=/dev/null",
        "cpu=never",
        "",
        "[autoexec]",
        "@echo off",
        (f'mount c "{game_root.as_posix()}" -t cdrom -noioctl'
         if readonly_cdrom_archive else f'mount c "{game_root.as_posix()}"'),
        "",
    ]
    if release_sha256 is not None:
        if release_sha256 != EXPECTED_RELEASE_SHA256:
            raise CaptureError("recorder identity section requires the recognised DOS release hash")
        lines.extend((
            "[project-eon-recorder-v21]",
            f"outer_release_sha256={release_sha256}",
            f"direct_media_set_sha256={EXPECTED_DIRECT_MEDIA_SET_SHA256}",
            f"titles_exe_sha256={EXPECTED_TITLES_EXE_SHA256}",
            f"titles_exe_bytes={EXPECTED_TITLES_EXE_SIZE}",
            f"2200ad_exe_sha256={EXPECTED_2200AD_EXE_SHA256}",
            f"2200ad_exe_bytes={EXPECTED_2200AD_EXE_SIZE}",
            "",
        ))
    return "\n".join(lines)


def mount_options(mountpoint: Path) -> str:
    completed = subprocess.run(
        ["findmnt", "-T", str(mountpoint), "-no", "OPTIONS"], check=True,
        capture_output=True, text=True)
    return completed.stdout.strip()


def mount_read_only(source: Path, mountpoint: Path) -> None:
    subprocess.run(["archivemount", "-o", "ro", str(source), str(mountpoint)], check=True)
    options = mount_options(mountpoint)
    required = {"ro", "nosuid", "nodev"}
    if not required <= set(options.split(",")):
        subprocess.run(["fusermount", "-u", str(mountpoint)], check=False)
        raise CaptureError("archivemount did not report required ro,nosuid,nodev options")


def unmount(mountpoint: Path) -> None:
    subprocess.run(["fusermount", "-u", str(mountpoint)], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def write_exclusive(path: Path, content: str) -> None:
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())


def input_receipt_status(path: Path) -> str:
    """Describe a recorder-created host-input receipt without creating one.

    A missing receipt is useful negative evidence: the visible recorder ran,
    but it did not observe a host key in its SDL event loop.  It must never be
    represented as an empty, generated input timeline.  A receipt is still
    only a host-side observation, not proof that the DOS program accepted an
    input.
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


def capture_intent_status(intent: str, receipt_status: str, observed_during_capture: bool,
                          *, allow_finalized_receipt: bool = False) -> str:
    """Bind a declared operator session type to external recorder evidence.

    This is deliberately a classification of the capture procedure, not a
    claim that the original game accepted or acted on a key.  A physical-input
    session needs a non-empty recorder receipt; a diagnostic session must not
    accidentally be admitted with physical host input in it.
    """
    if intent not in CAPTURE_INTENTS:
        raise CaptureError("capture intent is not in the reviewed finite set")
    fields = dict(line.split("=", 1) for line in receipt_status.splitlines())
    state = fields.get("host_input_receipt")
    if intent == "physical-input":
        if state != "present":
            raise CaptureError("physical-input capture requires an observed non-empty host-input receipt")
        if observed_during_capture:
            evidence = "observed-during-capture"
        elif allow_finalized_receipt:
            evidence = "finalized-recorder-receipt"
        else:
            raise CaptureError("physical-input capture requires an observed non-empty host-input receipt")
        requirement = "required"
    else:
        if state not in {"absent", "empty"} or observed_during_capture:
            raise CaptureError("diagnostic-no-input capture must not retain host input")
        requirement = "forbidden"
        evidence = "none"
    return (f"capture_intent={intent}\n"
            f"capture_intent_input_requirement={requirement}\n"
            f"capture_intent_input_observation={evidence}\n")


def capture_operator_instructions(intent: str) -> tuple[str, str, str]:
    """Return the visible, intent-specific instructions for a capture run."""
    if intent == "physical-input":
        return (
            "CAPTURE PREPARED  read-only original archive; physical operator input required",
            "Focus the visible DOSBox-X window by clicking it yourself; do not use terminal or automation input. Then press and release ordinary keys only in that window (for example Return, Space, or arrows).",
            "No AUTOTYPE, debugger input, or guest-memory injection is permitted.",
        )
    if intent == "diagnostic-no-input":
        return (
            "CAPTURE PREPARED  read-only original archive; diagnostic no-input capture",
            "The DOSBox-X window remains visible for observation only. Do not click it or press any key in it.",
            "No host input, AUTOTYPE, debugger input, or guest-memory injection is permitted.",
        )
    raise CaptureError("capture intent is not in the reviewed finite set")


def experimental_driver_return_operator_instructions() -> tuple[str, str, str]:
    """The sidecar-only observer requires an orderly window close to flush."""
    return (
        "EXPERIMENTAL DIAGNOSTIC  no guest-input or gameplay evidence will be collected",
        "Leave the emulator untouched during the capture window. When prompted, close DOSBox-X with the operating-system window controls so it can finalize the bounded sidecar; do not click the guest surface or press emulator keys.",
        "No AUTOTYPE, debugger input, guest-memory injection, or guest-file writes are permitted.",
    )


def experimental_ah48_operator_instructions() -> tuple[str, str, str]:
    """Collect the bounded DOS return sidecar during visible manual input."""
    return (
        "EXPERIMENTAL DIAGNOSTIC  visible manual input required; no gameplay state is collected",
        "Focus the visible DOSBox-X window yourself and press/release ordinary keys only there. When prompted, close DOSBox-X with the operating-system window controls so it can finalize the bounded sidecar.",
        "No AUTOTYPE, debugger input, or guest-memory injection is permitted.",
    )


def require_ah48_cache_output(output: Path, environment: dict[str, str]) -> None:
    """The recorder accepts only a private direct child of the user's cache."""
    home = environment.get("HOME")
    if not home:
        raise CaptureError("AH48 observer requires HOME for its private cache receipt")
    cache = Path(home) / ".cache"
    try:
        cache_info = cache.lstat()
    except OSError as error:
        raise CaptureError(f"AH48 observer cache root is unavailable: {error}") from error
    if (stat.S_ISLNK(cache_info.st_mode) or not stat.S_ISDIR(cache_info.st_mode)
            or cache_info.st_uid != os.geteuid() or cache_info.st_mode & 0o022):
        raise CaptureError("AH48 observer requires an owned, non-group/world-writable ~/.cache")
    try:
        resolved_cache = cache.resolve(strict=True)
    except OSError as error:
        raise CaptureError(f"AH48 observer cache root is unavailable: {error}") from error
    if output.parent != resolved_cache:
        raise CaptureError("AH48 capture output must be a direct child of the private ~/.cache root")


def ah48_handler_return_status(path: Path, recorder_protocol: str) -> str:
    """Validate the hash-bound, bounded pre-IRET register sidecar as raw facts."""
    if recorder_protocol not in AH48_HANDLER_RETURN_PROTOCOLS:
        return ""
    if recorder_protocol == AH48_HANDLER_RETURN_PROTOCOL:
        expected_header = AH48_HEADER
        expected_ah = AH48_EXPECTED_AH
        required_sites: frozenset[int] = frozenset()
        boundary = "dos-handler-before-iret"
    elif recorder_protocol in {
            AH48_SITES_1BCA_1BD5_RETURN_PROTOCOL,
            AH48_SITES_1BCA_1BD5_V4_HOST_INPUT_PROTOCOL,
            AH48_SITES_1BCA_1BD5_V5_AFTER_IRET_PROTOCOL,
            AH48_TITLE_LIBRARY_V6_PROTOCOL}:
        if recorder_protocol == AH48_TITLE_LIBRARY_V6_PROTOCOL:
            expected_header = AH48_TITLE_LIBRARY_HEADER
            expected_ah = {
                0x1AF9: 0x3D, 0x1B09: 0x42, 0x1B12: 0x3E,
                0x1B64: 0x48, 0x1B74: 0x48, 0x1BCA: 0x48,
                0x1BD5: 0x49, 0x0549: 0x3D, 0x057C: 0x3F,
                0x059C: 0x3E,
            }
            required_sites = frozenset({
                0x1AF9, 0x1B09, 0x1B12, 0x1B64,
                0x1B74, 0x1BCA, 0x1BD5, 0x0549,
            })
            boundary = "dos-handler-before-iret-title-library-v6"
        else:
            expected_header = AH48_SITES_1BCA_1BD5_HEADER
            expected_ah = AH48_SITES_1BCA_1BD5_EXPECTED_AH
            required_sites = frozenset(expected_ah)
            boundary = "dos-handler-before-iret-sites-1bca-1bd5"
    elif recorder_protocol == AH48_SITE_1B4F_RETURN_PROTOCOL:
        expected_header = AH48_SITE_1B4F_HEADER
        expected_ah = AH48_SITE_1B4F_EXPECTED_AH
        required_sites = frozenset({0x1B4F})
        boundary = "dos-handler-before-iret-site-1b4f"
    else:
        return ""
    if path.is_dir():
        try:
            candidates = sorted(path.glob("dos-ah48-*.tsv"))
        except OSError as error:
            raise CaptureError(f"AH48 return sidecar directory is unavailable: {error}") from error
        if len(candidates) != 1:
            raise CaptureError("AH48 observer must produce exactly one uniquely named sidecar")
        generated = candidates[0]
        fixed = path / "dos-ah48-return.raw"
    else:
        generated = path
        fixed = path
    try:
        info = generated.lstat()
    except OSError as error:
        raise CaptureError(f"AH48 return sidecar is unavailable: {error}") from error
    if (stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode)
            or info.st_uid != os.geteuid() or stat.S_IMODE(info.st_mode) != 0o600):
        raise CaptureError("AH48 return sidecar must be a private regular file")
    if info.st_size > AH48_MAX_BYTES:
        raise CaptureError("AH48 return sidecar exceeds the 4 MiB recorder bound")
    try:
        fd = os.open(generated, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
                     | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            opened = os.fstat(stream.fileno())
            if (not stat.S_ISREG(opened.st_mode) or opened.st_uid != os.geteuid()
                    or opened.st_dev != info.st_dev or opened.st_ino != info.st_ino):
                raise CaptureError("AH48 return sidecar changed during verification")
            raw = stream.read(AH48_MAX_BYTES + 1)
    except CaptureError:
        raise
    except OSError as error:
        raise CaptureError(f"AH48 return sidecar is unavailable: {error}") from error
    if len(raw) > AH48_MAX_BYTES:
        raise CaptureError("AH48 return sidecar exceeds the 4 MiB recorder bound")
    if not raw.startswith(expected_header) or not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("AH48 return sidecar has an invalid header or line ending")
    try:
        lines = raw[len(expected_header):].decode("ascii").splitlines(keepends=True)
    except UnicodeDecodeError as error:
        raise CaptureError("AH48 return sidecar is not ASCII") from error
    if len(lines) < 2:
        raise CaptureError("AH48 return sidecar has no event and completion records")
    records: list[tuple[int, int]] = []
    caller_cs: str | None = None
    footer: tuple[str, int] | None = None
    overflow = False
    register_names = ("ax", "bx", "cx", "dx", "si", "di", "bp", "cs", "ip", "ds", "es", "ss", "sp", "flags")
    for index, line in enumerate(lines):
        if line.startswith("overflow\t"):
            if index != len(lines) - 2 or line not in {
                    f"overflow\treason=event-limit\tevents={len(records)}\n",
                    f"overflow\treason=byte-limit\tevents={len(records)}\n"}:
                raise CaptureError("AH48 return sidecar has an invalid overflow record")
            overflow = True
            continue
        if line.startswith("end\t"):
            match = re.fullmatch(r"end\tstatus=(complete|overflow)\tevents=(0|[1-9][0-9]*)\n", line)
            if index != len(lines) - 1 or not match or footer is not None:
                raise CaptureError("AH48 return sidecar completion record is invalid")
            footer = match.group(1), int(match.group(2))
            continue
        if overflow or footer is not None:
            raise CaptureError("AH48 return sidecar has records after its terminal marker")
        parts = line.rstrip("\n").split("\t")
        if len(parts) != 5 + 2 * len(register_names) or parts[0] != "event":
            raise CaptureError("AH48 return sidecar event has an invalid field count")
        fields: dict[str, str] = {}
        for part in parts[1:]:
            if part.count("=") != 1:
                raise CaptureError("AH48 return sidecar event has an invalid field")
            key, value = part.split("=", 1)
            if key in fields:
                raise CaptureError("AH48 return sidecar event has duplicate fields")
            fields[key] = value
        expected_keys = {"sequence", "site", "observed_return_ip", "dispatch_return"}
        expected_keys |= {f"{phase}_{register}" for phase in ("pre", "post") for register in register_names}
        if set(fields) != expected_keys or fields["dispatch_return"] != "before-IRET":
            raise CaptureError("AH48 return sidecar event does not match the reviewed register grammar")
        if not re.fullmatch(r"[1-9][0-9]{0,3}", fields["sequence"]):
            raise CaptureError("AH48 return sidecar sequence is invalid")
        sequence = int(fields["sequence"])
        if sequence != len(records) + 1 or sequence > AH48_MAX_EVENTS:
            raise CaptureError("AH48 return sidecar sequence exceeds its reviewed event cap")
        if not re.fullmatch(r"[0-9a-f]{4}", fields["site"]):
            raise CaptureError("AH48 return sidecar site is invalid")
        site = int(fields["site"], 16)
        if (site not in expected_ah
                or fields["observed_return_ip"] != f"{site + 2:04x}"
                or fields["pre_ip"] != fields["observed_return_ip"]
                or fields["pre_ax"][:2] != f"{expected_ah[site]:02x}"):
            raise CaptureError("AH48 return sidecar event does not match an exact reviewed INT 21h site")
        for key, value in fields.items():
            if key.startswith(("pre_", "post_")) and not re.fullmatch(r"[0-9a-f]{4}", value):
                raise CaptureError("AH48 return sidecar register is not a 16-bit hex value")
        if caller_cs is None:
            caller_cs = fields["pre_cs"]
        elif caller_cs != fields["pre_cs"]:
            raise CaptureError("AH48 return sidecar changed TITLES.EXE caller segment")
        records.append((sequence, site))
    if footer is None or footer[1] != len(records) or footer[0] != ("overflow" if overflow else "complete"):
        raise CaptureError("AH48 return sidecar footer does not match its observations")
    if overflow:
        raise CaptureError("AH48 return sidecar overflowed its bounded recorder contract")
    if not records:
        raise CaptureError("AH48 return sidecar contains no DOS return observations")
    site_counts: dict[int, int] = {}
    for _, site in records:
        site_counts[site] = site_counts.get(site, 0) + 1
    if not required_sites.issubset(site_counts):
        raise CaptureError("AH48 return sidecar is missing its exact required DOS site")
    if recorder_protocol == AH48_TITLE_LIBRARY_V6_PROTOCOL:
        sites = [site for _, site in records]
        fixed_prefix = [0x1AF9, 0x1B09, 0x1B12, 0x1B64,
                        0x1B74, 0x1BCA, 0x1BD5, 0x0549]
        if sites[:len(fixed_prefix)] != fixed_prefix:
            raise CaptureError("AH48 v6 sidecar does not follow the exact TITLES loader call order")
        tail = sites[len(fixed_prefix):]
        reads = 0
        while reads < len(tail) and tail[reads] == 0x057C:
            reads += 1
        if reads > 9 or tail[reads:] not in ([], [0x059C]):
            raise CaptureError("AH48 v6 title-library read/close sequence exceeds its exact bound")
    counts = ",".join(f"0x{site:04x}:{site_counts[site]}" for site in expected_ah if site in site_counts)
    digest = hashlib.sha256(raw).hexdigest()
    if generated != fixed:
        if fixed.exists() or fixed.is_symlink():
            raise CaptureError("AH48 normalized sidecar path already exists")
        os.rename(generated, fixed)
    return ("ah48_return=present\n"
            f"ah48_return_sha256={digest}\n"
            f"ah48_return_bytes={len(raw)}\n"
            f"ah48_return_events={len(records)}\n"
            f"ah48_return_site_counts={counts}\n"
            f"ah48_return_boundary={boundary}\n")


def ah48_after_iret_status(directory: Path, recorder_protocol: str) -> str:
    """Validate the exact TITLES caller snapshots after DOS IRET restores FLAGS."""
    if recorder_protocol not in {
            AH48_SITES_1BCA_1BD5_V5_AFTER_IRET_PROTOCOL,
            AH48_TITLE_LIBRARY_V6_PROTOCOL}:
        return ""
    fixed = directory / "dos-ah48-iret.raw"
    try:
        candidates = ([fixed] if fixed.exists() or fixed.is_symlink()
                      else sorted(directory.glob("dos-ah48-iret-*.raw")))
    except OSError as error:
        raise CaptureError(f"AH48 post-IRET sidecar directory is unavailable: {error}") from error
    if len(candidates) != 1:
        raise CaptureError("AH48 post-IRET observer must produce exactly one sidecar")
    generated = candidates[0]
    try:
        info = generated.lstat()
    except OSError as error:
        raise CaptureError(f"AH48 post-IRET sidecar is unavailable: {error}") from error
    if (stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode)
            or info.st_uid != os.geteuid() or stat.S_IMODE(info.st_mode) != 0o600):
        raise CaptureError("AH48 post-IRET sidecar must be a private regular file")
    if info.st_size > AH48_MAX_BYTES:
        raise CaptureError("AH48 post-IRET sidecar exceeds the 4 MiB recorder bound")
    try:
        fd = os.open(generated, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
                     | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            opened = os.fstat(stream.fileno())
            if (not stat.S_ISREG(opened.st_mode) or opened.st_uid != os.geteuid()
                    or opened.st_dev != info.st_dev or opened.st_ino != info.st_ino):
                raise CaptureError("AH48 post-IRET sidecar changed during verification")
            raw = stream.read(AH48_MAX_BYTES + 1)
    except CaptureError:
        raise
    except OSError as error:
        raise CaptureError(f"AH48 post-IRET sidecar is unavailable: {error}") from error
    if len(raw) > AH48_MAX_BYTES:
        raise CaptureError("AH48 post-IRET sidecar exceeds the 4 MiB recorder bound")
    expected_header = (AH48_TITLE_LIBRARY_IRET_HEADER
                       if recorder_protocol == AH48_TITLE_LIBRARY_V6_PROTOCOL
                       else AH48_IRET_HEADER)
    if not raw.startswith(expected_header) or not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("AH48 post-IRET sidecar has an invalid header or line ending")
    try:
        lines = raw[len(expected_header):].decode("ascii").splitlines(keepends=True)
    except UnicodeDecodeError as error:
        raise CaptureError("AH48 post-IRET sidecar is not ASCII") from error
    if recorder_protocol == AH48_TITLE_LIBRARY_V6_PROTOCOL:
        handler_path = directory / "dos-ah48-return.raw"
        try:
            handler_lines = handler_path.read_text(encoding="ascii").splitlines()
        except OSError as error:
            raise CaptureError("AH48 v6 handler sidecar is unavailable for cross-check") from error
        expected_sites = tuple(
            int(match.group(1), 16)
            for line in handler_lines if line.startswith("event\t")
            if (match := re.search(r"(?:^|\t)site=([0-9a-f]{4})(?:\t|$)", line)))
        if len(expected_sites) < 8 or len(expected_sites) > 18:
            raise CaptureError("AH48 v6 handler/IRET event count is outside its reviewed bound")
    else:
        expected_sites = (0x1BCA, 0x1BD5)
    if len(lines) != len(expected_sites) + 1:
        raise CaptureError("AH48 post-IRET event count differs from the handler sidecar")
    rows: dict[int, dict[str, str]] = {}
    ordered_rows: list[dict[str, str]] = []
    caller_cs: str | None = None
    for sequence, (line, site) in enumerate(zip(lines[:-1], expected_sites), start=1):
        parts = line.rstrip("\n").split("\t")
        if not parts or parts[0] != "event":
            raise CaptureError("AH48 post-IRET sidecar has an invalid event record")
        fields: dict[str, str] = {}
        for part in parts[1:]:
            if part.count("=") != 1:
                raise CaptureError("AH48 post-IRET event has an invalid field")
            key, value = part.split("=", 1)
            if key in fields:
                raise CaptureError("AH48 post-IRET event has duplicate fields")
            fields[key] = value
        expected_keys = {"sequence", "site", "return_ip", *AH48_IRET_REGISTER_NAMES}
        if set(fields) != expected_keys:
            raise CaptureError("AH48 post-IRET event has an unexpected register grammar")
        if (fields["sequence"] != str(sequence)
                or fields["site"] != f"{site:04x}"
                or fields["return_ip"] != f"{site + 2:04x}"
                or fields["ip"] != fields["return_ip"]):
            raise CaptureError("AH48 post-IRET event does not match its exact caller return")
        if any(not re.fullmatch(r"[0-9a-f]{4}", fields[name])
               for name in AH48_IRET_REGISTER_NAMES):
            raise CaptureError("AH48 post-IRET register is not a 16-bit hex value")
        if caller_cs is None:
            caller_cs = fields["cs"]
        elif caller_cs != fields["cs"]:
            raise CaptureError("AH48 post-IRET events changed TITLES.EXE caller segment")
        rows[site] = fields
        ordered_rows.append(fields)
    footer = f"end\tstatus=complete\tevents={len(expected_sites)}\n"
    if lines[-1] != footer:
        raise CaptureError("AH48 post-IRET sidecar completion footer is invalid")
    digest = hashlib.sha256(raw).hexdigest()
    if generated != fixed:
        if fixed.exists() or fixed.is_symlink():
            raise CaptureError("AH48 normalized post-IRET sidecar path already exists")
        os.rename(generated, fixed)
    fields = [
        "ah48_iret=present",
        f"ah48_iret_sha256={digest}",
        f"ah48_iret_bytes={len(raw)}",
        f"ah48_iret_events={len(expected_sites)}",
        f"ah48_iret_caller_cs={caller_cs}",
    ]
    summary_sites = ((0x1BCA, "1bca"), (0x1BD5, "1bd5"))
    if recorder_protocol == AH48_TITLE_LIBRARY_V6_PROTOCOL:
        summary_sites = tuple((site, f"{site:04x}_{index}")
                              for index, site in enumerate(expected_sites, start=1))
    for index, (site, label) in enumerate(summary_sites):
        row = (ordered_rows[index] if recorder_protocol == AH48_TITLE_LIBRARY_V6_PROTOCOL
               else rows[site])
        for register in ("ax", "bx", "es", "flags", "ip"):
            fields.append(f"ah48_iret_site_{label}_{register}={row[register]}")
    return "\n".join(fields) + "\n"


def input_delivery_file_observed(path: Path) -> bool:
    """Report a recorder-created receipt without parsing a live append stream.

    This is only an operator-facing liveness signal.  The complete receipt is
    still grammar-checked and hash-bound after DOSBox-X exits, so a partial
    write can never become capture evidence.
    """
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("live host-input receipt is not a regular non-symlink file")
    return info.st_size > 0


def parse_host_input_receipt(path: Path) -> int:
    """Validate the finite SDL host-key receipt without decoding DOS input."""
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("host-input receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("host-input receipt has a truncated final record")
    count = 0
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = HOST_KEY_LINE.fullmatch(line)
        if not match:
            raise CaptureError("host-input receipt contains an invalid recorder record")
        if int(match.group(1)) != expected:
            raise CaptureError("host-input receipt record ordinals are not contiguous")
        count = expected
        if count > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("host-input receipt exceeds the recorder record cap")
    return count


def raw_observation_status(path: Path, name: str, recorder_protocol: str = "v11") -> str:
    """Bind an optional recorder-owned raw log without reading it unbounded."""
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
    if name == "results_raw":
        return raw_result_status(path, name, recorder_protocol)
    digest, size = sha256_file(path)
    return f"{name}=present\n{name}_sha256={digest}\n{name}_bytes={size}\n"


def normal_core_history_entries(path: Path) -> tuple[str, ...]:
    """Return one canonical recorder-owned history record as opaque tuples."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        raise CaptureError("normal-core history is absent")
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("normal-core history is not a regular non-symlink file")
    if not 0 < info.st_size <= MAX_NORMAL_CORE_HISTORY_BYTES:
        raise CaptureError("normal-core history exceeds the bounded recorder contract")
    try:
        raw = path.read_bytes()
        text = raw.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("normal-core history is not ASCII recorder output") from error
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("normal-core history is not a canonical LF recorder record")
    match = NORMAL_CORE_HISTORY_LINE.fullmatch(text)
    if not match:
        raise CaptureError("normal-core history contains an invalid recorder record")
    entries = match.group(2).split(",")
    if len(entries) != int(match.group(1)):
        raise CaptureError("normal-core history count does not match entries")
    return tuple(entries)


def normal_core_history_status(path: Path, recorder_protocol: str) -> str:
    """Bind v14's one bounded normal-core sidecar without decoding game state."""
    if recorder_protocol != "v14-normal-core-history":
        return ""
    try:
        entries = normal_core_history_entries(path)
    except CaptureError as error:
        if str(error) == "normal-core history is absent":
            return "normal_core_history=absent\n"
        raise
    digest, size = sha256_file(path)
    return ("normal_core_history=present\n"
            f"normal_core_history_sha256={digest}\n"
            f"normal_core_history_bytes={size}\n"
            f"normal_core_history_entries={len(entries)}\n")


def normal_core_history_boundary_status(history_path: Path, results_path: Path,
                                        recorder_protocol: str,
                                        termination_reason: str) -> str:
    """Admit V14's observed fault boundary without assigning it game meaning."""
    if recorder_protocol != "v14-normal-core-history" or termination_reason != "known-unhandled-interrupt":
        return ""
    if not known_v11_early_stop_receipt(results_path):
        raise CaptureError("v14 history requires the exact known callback fault receipt")
    entries = normal_core_history_entries(history_path)
    if entries != KNOWN_V14_NORMAL_CORE_HISTORY:
        raise CaptureError("v14 history does not match the observed zero-context callback boundary")
    return ("normal_core_history_boundary=observed-zero-context-to-default-callback\n"
            f"normal_core_history_first={entries[0]}\n"
            f"normal_core_history_last={entries[-1]}\n")


def normal_core_anomaly_status(path: Path, results_path: Path, recorder_protocol: str,
                               termination_reason: str) -> str:
    if recorder_protocol != "v18-ivt-entry": return ""
    try: raw = path.read_bytes()
    except FileNotFoundError: return "normal_core_anomaly=absent\n"
    if not 0 < len(raw) <= MAX_NORMAL_CORE_ANOMALY_BYTES or not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("normal-core anomaly is not a bounded canonical recorder record")
    try: match = NORMAL_CORE_ANOMALY_LINE.fullmatch(raw.decode("ascii"))
    except UnicodeDecodeError as error: raise CaptureError("normal-core anomaly is not ASCII recorder output") from error
    if not match or len(match.group(5).split(",")) != int(match.group(4)):
        raise CaptureError("normal-core anomaly contains an invalid recorder record")
    entries = tuple(match.group(5).split(","))
    if termination_reason == "known-unhandled-interrupt":
        if not known_v11_early_stop_receipt(results_path) or match.group(1) != "0000:0000" \
                or match.group(2) != "0a8d" or match.group(3) != "d9e2" or entries != KNOWN_V18_IVT_PRIOR:
            raise CaptureError("v18 anomaly does not match the observed INT 93h IVT boundary")
    digest, size = sha256_file(path)
    return ("normal_core_anomaly=present\n"
            f"normal_core_anomaly_sha256={digest}\nnormal_core_anomaly_bytes={size}\n"
            f"normal_core_anomaly_target={match.group(1)}\n")


def int93_vector_status(events_path: Path, results_path: Path, recorder_protocol: str,
                        termination_reason: str) -> str:
    if recorder_protocol != "v19-int93-vector": return ""
    try:
        info = events_path.lstat()
    except FileNotFoundError as error:
        raise CaptureError("v19 event stream is absent") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("v19 event stream is not a regular non-symlink file")
    if info.st_size > MAX_RAW_OBSERVATION_BYTES:
        raise CaptureError("v19 event stream exceeds the bounded recorder contract")
    raw = events_path.read_bytes()
    if termination_reason == "known-unhandled-interrupt" and \
            (not known_v11_early_stop_receipt(results_path) or not raw.endswith(KNOWN_V19_INT93_EVENT)):
        raise CaptureError("v19 event stream does not match the observed INT 93h zero-vector boundary")
    return "int93_vector=observed-zero-ivt-target\n"


def title_entry_transfer_status(path: Path, results_path: Path, recorder_protocol: str,
                                termination_reason: str) -> str:
    """Bind one raw normal-core title-entry adjacency without assigning an ABI."""
    if recorder_protocol != "v20-title-entry-transfer": return ""
    try:
        info = path.lstat()
    except FileNotFoundError as error:
        raise CaptureError("v20 title-entry transfer is absent") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("v20 title-entry transfer is not a regular non-symlink file")
    if not 0 < info.st_size <= MAX_TITLE_ENTRY_TRANSFER_BYTES:
        raise CaptureError("v20 title-entry transfer exceeds the bounded recorder contract")
    raw = path.read_bytes()
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("v20 title-entry transfer is not a canonical LF recorder record")
    try:
        match = TITLE_ENTRY_TRANSFER_LINE.fullmatch(raw.decode("ascii"))
    except UnicodeDecodeError as error:
        raise CaptureError("v20 title-entry transfer is not ASCII recorder output") from error
    if not match or match.group(2) != "1" or match.group(1) != match.group(6):
        raise CaptureError("v20 title-entry transfer contains an invalid recorder record")
    if termination_reason == "known-unhandled-interrupt" and \
            (not known_v11_early_stop_receipt(results_path) or raw != KNOWN_V20_TITLE_ENTRY_TRANSFER):
        raise CaptureError("v20 title-entry transfer does not match the observed zero-context boundary")
    digest, size = sha256_file(path)
    return ("title_entry_transfer=present\n"
            f"title_entry_transfer_sha256={digest}\n"
            f"title_entry_transfer_bytes={size}\n"
            f"title_entry_transfer_declared_entry_cs=0x{match.group(1)}\n"
            f"title_entry_transfer_predecessor_cs=0x{match.group(3)}\n"
            f"title_entry_transfer_predecessor_ip=0x{match.group(4)}\n"
            f"title_entry_transfer_predecessor_code=0x{match.group(5)}\n"
            f"title_entry_transfer_entry_cs=0x{match.group(6)}\n"
            f"title_entry_transfer_entry_ip=0x{match.group(7)}\n"
            f"title_entry_transfer_entry_code=0x{match.group(8)}\n")


def int93_installation_status(path: Path, recorder_protocol: str) -> str:
    """Bind one optional DOS Set Interrupt Vector observation.

    A sidecar is deliberately optional: the current captured route may never
    reach either static installer site.  If it is present, only one canonical
    record is admitted.  It identifies the observed IVT bytes as the exact
    DS:DX pointer passed to DOS, but does not establish ordering, persistence,
    a handler ABI, or dispatch through ``INT 93h``.
    """
    if recorder_protocol != "v21-int93-installation":
        return ""
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "int93_installation=absent\n"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("v21 INT 93h installation sidecar is not a regular non-symlink file")
    if not 0 < info.st_size <= MAX_INT93_INSTALLATION_BYTES:
        raise CaptureError("v21 INT 93h installation sidecar exceeds the bounded recorder contract")
    raw = path.read_bytes()
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("v21 INT 93h installation sidecar is not a canonical LF recorder record")
    try:
        match = INT93_INSTALLATION_LINE.fullmatch(raw.decode("ascii"))
    except UnicodeDecodeError as error:
        raise CaptureError("v21 INT 93h installation sidecar is not ASCII recorder output") from error
    if not match:
        raise CaptureError("v21 INT 93h installation sidecar contains an invalid recorder record")
    image, pc, ds, dx, _preimage, vector_ip, vector_cs = match.groups()
    if (image, pc) not in INT93_INSTALLATION_CANDIDATES:
        raise CaptureError("v21 INT 93h installation sidecar is not a reviewed installer candidate")
    if vector_ip != dx or vector_cs != ds:
        raise CaptureError("v21 INT 93h installation sidecar does not preserve the observed DS:DX target")
    digest, size = sha256_file(path)
    return ("int93_installation=present\n"
            f"int93_installation_sha256={digest}\n"
            f"int93_installation_bytes={size}\n"
            f"int93_installation_image={image}\n"
            f"int93_installation_pc=0x{pc}\n"
            f"int93_installation_opcode_preimage=0x{INT93_INSTALLATION_CANDIDATES[(image, pc)]}\n"
            f"int93_installation_ds=0x{ds}\n"
            f"int93_installation_dx=0x{dx}\n"
            f"int93_installation_target_preimage=0x{_preimage}\n"
            f"int93_installation_vector_ip=0x{vector_ip}\n"
            f"int93_installation_vector_cs=0x{vector_cs}\n")


def driver_load_return_status(path: Path, recorder_protocol: str) -> str:
    """Validate an optional bounded MILL.COM post-INT return sidecar.

    Register values and the guest-buffer digest are preserved as opaque
    observations. This parser does not infer DOS success or file identity.
    """
    if recorder_protocol not in DRIVER_LOAD_RETURN_PROTOCOLS:
        return ""
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "driver_load_return=absent\n"
    except OSError as error:
        raise CaptureError(f"driver-load return sidecar is unavailable: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("driver-load return sidecar is not a regular non-symlink file")
    if info.st_size > MAX_DRIVER_LOAD_RETURN_BYTES:
        raise CaptureError("driver-load return sidecar exceeds the bounded recorder contract")
    if info.st_size == 0:
        return "driver_load_return=empty\n"
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
                     | getattr(os, "O_NONBLOCK", 0))
        with os.fdopen(fd, "rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise CaptureError("driver-load return sidecar changed to a non-regular file")
            raw = stream.read(MAX_DRIVER_LOAD_RETURN_BYTES + 1)
        if len(raw) > MAX_DRIVER_LOAD_RETURN_BYTES:
            raise CaptureError("driver-load return sidecar exceeds the bounded recorder contract")
        text = raw.decode("ascii")
    except CaptureError:
        raise
    except OSError as error:
        raise CaptureError(f"driver-load return sidecar is unavailable: {error}") from error
    except UnicodeDecodeError as error:
        raise CaptureError("driver-load return sidecar is not ASCII recorder output") from error
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("driver-load return sidecar is not canonical LF recorder output")
    records: list[tuple[int, int, str]] = []
    footer: tuple[int, int] | None = None
    overflow = False
    lines = text.splitlines(keepends=True)
    for expected, line in enumerate(lines, start=1):
        end_match = DRIVER_LOAD_RETURN_END_LINE.fullmatch(line)
        if end_match:
            if expected != len(lines) or footer is not None:
                raise CaptureError("driver-load return completion footer is not terminal")
            footer = int(end_match.group(1)), int(end_match.group(2))
            continue
        if DRIVER_LOAD_RETURN_OVERFLOW_LINE.fullmatch(line):
            overflow = True
            continue
        match = DRIVER_LOAD_RETURN_LINE.fullmatch(line)
        if not match:
            raise CaptureError("driver-load return sidecar contains an invalid recorder record")
        ordinal = int(match.group(1))
        site = int(match.group(3), 16)
        buffer = match.group(15)
        if ordinal != expected or ordinal > MAX_DRIVER_LOAD_RETURN_RECORDS:
            raise CaptureError("driver-load return ordinals exceed the reviewed sample cap")
        if site not in DRIVER_LOAD_RETURN_SITES:
            raise CaptureError("driver-load return uses an unreviewed instruction site")
        if buffer != "none":
            if site != 0x0315:
                raise CaptureError("driver-load buffer digest is detached from the read return site")
            _, length_text, _digest = buffer.split(":", 2)
            length = int(length_text)
            if not 1 <= length <= MAX_DRIVER_LOAD_BUFFER_BYTES:
                raise CaptureError("driver-load buffer digest exceeds the reviewed size bound")
        records.append((ordinal, site, buffer))
    if footer is None or footer[1] != 0 or overflow:
        raise CaptureError("driver-load return sidecar is incomplete or overflowed")
    if footer[0] != len(records):
        raise CaptureError("driver-load return completion count does not match its records")
    if len(records) > MAX_DRIVER_LOAD_RETURN_RECORDS:
        raise CaptureError("driver-load return ordinals exceed the reviewed sample cap")
    digest, size = hashlib.sha256(raw).hexdigest(), len(raw)
    sites: dict[int, int] = {}
    for _, site, _ in records:
        sites[site] = sites.get(site, 0) + 1
    site_counts = ",".join(f"0x{site:04x}:{sites[site]}"
                            for site in DRIVER_LOAD_RETURN_SITES if site in sites)
    buffer_records = sum(buffer != "none" for _, _, buffer in records)
    return ("driver_load_return=present\n"
            f"driver_load_return_sha256={digest}\n"
            f"driver_load_return_bytes={size}\n"
            f"driver_load_return_records={len(records)}\n"
            f"driver_load_return_site_counts={site_counts}\n"
            f"driver_load_return_buffer_digests={buffer_records}\n")


def raw_result_labels(path: Path, recorder_protocol: str = "v11") -> list[str]:
    """Parse finite recorder diagnostics into non-semantic shape labels."""
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("results_raw is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("results_raw has a truncated final record")
    labels: list[str] = []
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        title_poll = (TITLE_INPUT_POLL_LINE.fullmatch(line)
    if recorder_protocol in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry", "v19-int93-vector", "v20-title-entry-transfer", "v21-int93-installation"} and " title-input-poll " in line
                      else None)
        match = title_poll or (V12_PREDECESSOR_FAULT_LINE.fullmatch(line)
            if recorder_protocol == "v12-predecessor" and " fault=" in line
            else RAW_RESULT_LINE.fullmatch(line))
        if not match:
            raise CaptureError("results_raw contains an invalid recorder record")
        if int(match.group(1)) != expected or int(match.group(2)) != expected:
            raise CaptureError("results_raw record counters are not contiguous")
        prefix = "raw-result\t" + str(expected) + " " + str(expected) + " "
        if title_poll:
            label = "title-input-poll"
        elif line.startswith(prefix + "fault="):
            label = "fault"
        elif line.startswith(prefix + "private-vector "):
            label = "private-vector"
        elif line.startswith(prefix + "private-handler-entry "):
            label = "private-handler-entry"
        elif line.startswith(prefix + "private-handler-return "):
            label = "private-handler-return"
        else:
            label = f"{match.group(3)}:{match.group(4)}"
        labels.append(label)
        if expected > MAX_RAW_RESULT_RECORDS:
            raise CaptureError("results_raw exceeds the recorder record cap")
    return labels


def experimental_observer_raw_status() -> str:
    """Describe deliberately uncollected legacy streams without fabricating them.

    Experimental observers may own only a separate finite sidecar and not the
    historical event/results streams. Record those streams as uncollected
    rather than failing as though a pinned multi-stream recorder omitted them.
    """
    return "events_raw=not-collected\nresults_raw=not-collected\n"


def title_input_poll_ordinals(path: Path, recorder_protocol: str = "v11") -> list[int]:
    """Read v13 chronology records without assigning a DOS input result.

    Each ordinal names the count of host SDL key events observed before this
    exact original INT 21h/AH=06h call. It is not an input-delivery receipt,
    and its absence cannot be replaced by a synthesized poll or frame.
    """
    if recorder_protocol not in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry", "v19-int93-vector", "v20-title-entry-transfer", "v21-int93-installation"}:
        return []
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("results_raw is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("results_raw has a truncated final record")
    ordinals: list[int] = []
    for line in text.splitlines(keepends=True):
        match = TITLE_INPUT_POLL_LINE.fullmatch(line)
        if not match:
            continue
        ordinal = int(match.group(3))
        if ordinals and ordinal <= ordinals[-1]:
            raise CaptureError("title-input poll host-key ordinals are not strictly increasing")
        ordinals.append(ordinal)
        if len(ordinals) > 32:
            raise CaptureError("title-input poll observations exceed the recorder cap")
    return ordinals


def title_input_checkpoint_status(results_path: Path, input_receipt_path: Path,
                                  recorder_protocol: str = "v11") -> str:
    """Summarize v13's observable chronology without claiming delivery.

    A host key followed by a title poll proves neither DOS carry/AL nor a
    title action. The receipt calls it a correlation only, and rejects an
    impossible recorder ordinal beyond the independently recorded host keys.
    """
    if recorder_protocol not in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry", "v19-int93-vector", "v20-title-entry-transfer", "v21-int93-installation"}:
        return ""
    polls = title_input_poll_ordinals(results_path, recorder_protocol)
    try:
        info = input_receipt_path.lstat()
    except FileNotFoundError:
        host_keys = 0
        host_state = "absent"
    else:
        if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
            raise CaptureError("host-input receipt is not a regular non-symlink file")
        host_keys = parse_host_input_receipt(input_receipt_path) if info.st_size else 0
        host_state = "present" if info.st_size else "empty"
    if any(ordinal > host_keys for ordinal in polls):
        raise CaptureError("title-input poll names a host-key ordinal absent from the receipt")
    correlated = bool(polls and polls[-1] > 0)
    state = "host-key-and-poll" if correlated else "poll-only" if polls else (
        "host-key-only" if host_keys else "absent")
    return (f"title_input_checkpoint={state}\n"
            f"title_input_checkpoint_host_receipt={host_state}\n"
            f"title_input_checkpoint_polls={len(polls)}\n"
            f"title_input_checkpoint_last_host_key_ordinal={polls[-1] if polls else 0}\n")


def parse_raw_results(path: Path, recorder_protocol: str = "v11") -> dict[str, int]:
    """Validate only the recorder's known diagnostics-only result grammar."""
    counts: dict[str, int] = {}
    for label in raw_result_labels(path, recorder_protocol):
        counts[label] = counts.get(label, 0) + 1
    return counts


KNOWN_V10_EARLY_STOP_SEQUENCE = (
    "mill.com:020e", "mill.com:0213", "private-vector", "private-handler-entry",
    "private-handler-return", "titles.exe:0129", "titles.exe:0129", "fault",
)

# This is the complete, twice-observed recorder diagnostic that terminates the
# host process.  It is deliberately a recorder receipt, not a DOS interrupt
# contract: Eon neither consumes nor interprets these values at runtime.  A
# byte-exact gate prevents a different INT 6 or changed machine state from
# being silently classified as the known callback loop.
KNOWN_V11_EARLY_STOP_RAW = (
    b"raw-result\t1 1 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591\n"
    b"raw-result\t2 2 image=mill.com pc=0x0213 source-call=0x0511 ax=0x0000\n"
    b"raw-result\t3 3 private-vector image=titles.exe pc=0x0127 int=0x91 vector_ip=0x0000 vector_cs=0x087e\n"
    b"raw-result\t4 4 private-handler-entry int=0x91 cs=0x087e ip=0x0000\n"
    b"raw-result\t5 5 private-handler-return int=0x91 caller=titles.exe pc=0x0129 ax=0x0101 flags=0x7202\n"
    b"raw-result\t6 6 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0101\n"
    b"raw-result\t7 7 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0000\n"
    b"raw-result\t8 8 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
    b"ss=0x0a8d sp=0xc9bf return_ip=0x1900 return_cs=0x0e70 return_flags=0x7047 "
    b"code=0x00000000 ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n"
)


def known_v10_early_stop_sequence(path: Path) -> bool:
    """Check v10's observed diagnostic order without assigning it an ABI."""
    try:
        return tuple(raw_result_labels(path)) == KNOWN_V10_EARLY_STOP_SEQUENCE
    except CaptureError:
        return False


def known_v11_early_stop_receipt(path: Path) -> bool:
    """Check the complete observed callback-loop diagnostic without ABI inference."""
    try:
        return path.read_bytes() == KNOWN_V11_EARLY_STOP_RAW
    except OSError:
        return False


def raw_result_status(path: Path, name: str, recorder_protocol: str = "v11") -> str:
    counts = parse_raw_results(path, recorder_protocol)
    digest, size = sha256_file(path)
    summary = ",".join(f"{key}:{counts[key]}" for key in sorted(counts))
    status = (f"{name}=present\n{name}_sha256={digest}\n{name}_bytes={size}\n"
              f"{name}_records={sum(counts.values())}\n{name}_shapes={summary}\n")
    if recorder_protocol in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry", "v19-int93-vector", "v20-title-entry-transfer", "v21-int93-installation"}:
        polls = title_input_poll_ordinals(path, recorder_protocol)
        status += (f"{name}_title_input_polls={len(polls)}\n"
                   f"{name}_last_host_key_ordinal={polls[-1] if polls else 0}\n")
    return status


def known_unhandled_interrupt_observed(path: Path, recorder_protocol: str = "v11") -> bool:
    """Return whether the recorder completed its one known raw INT 6 record.

    This is a host-side early-stop guard for the already documented DOSBox-X
    callback loop.  It reads no guest memory and does not install a handler;
    it merely stops the external process once the recorder has durably written
    the bounded observation that cannot lead to a playable capture.
    """
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        return False
    if info.st_size == 0 or info.st_size > MAX_RAW_OBSERVATION_BYTES:
        return False
    try:
        text = path.read_bytes().decode("ascii")
    except UnicodeDecodeError:
        return False
    # A recorder append can be in progress while this helper polls.  Never
    # treat an unfinished line as an observation, and never modify that file.
    if not text.endswith("\n"):
        return False
    if recorder_protocol == "v11":
        return known_v11_early_stop_receipt(path)
    if recorder_protocol == "v12-predecessor":
        try:
            # This only stops the external callback loop after a complete
            # bounded shape. It does not admit the new values or make them a
            # private-interrupt result; two byte-identical receipts are still
            # required before any documentation or runtime use.
            return tuple(raw_result_labels(path, recorder_protocol)) == KNOWN_V10_EARLY_STOP_SEQUENCE
        except CaptureError:
            return False
    if recorder_protocol in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry", "v19-int93-vector", "v20-title-entry-transfer", "v21-int93-installation"}:
        # The legacy callback-loop receipt remains a bounded diagnostic stop
        # when no title poll was reached. Any poll-bearing run stays alive for
        # the operator's configured duration so it cannot be mistaken for a
        # completed input-to-frame capture.
        return known_v11_early_stop_receipt(path)
    return False


def should_stop_for_known_unhandled_interrupt(capture_intent: str, path: Path,
                                              recorder_protocol: str = "v11") -> bool:
    """Keep a physical-input window alive even after a known diagnostic receipt.

    The callback-loop receipt is useful for bounded no-input diagnostics, but
    it is not evidence that a visible operator had a meaningful opportunity
    to deliver input.  A physical-input run must retain its configured window
    unless the emulator itself exits, the console cap is reached, or time
    expires.  This host-side policy neither changes DOSBox-X nor injects any
    guest event.
    """
    if capture_intent not in CAPTURE_INTENTS:
        raise CaptureError("capture intent is not in the reviewed finite set")
    return capture_intent == "diagnostic-no-input" and known_unhandled_interrupt_observed(
        path, recorder_protocol)


def capture_bounded_console(stream, path: Path, over_limit: threading.Event) -> RecorderConsoleStatus:
    """Drain an emulator console while retaining only a bounded evidence prefix.

    The full transcript is hashed and counted as it is read. This preserves a
    stable identity for a pathological recorder output without keeping an
    unbounded error loop on disk or blocking the emulator on a full pipe.  It
    signals the owner as soon as the total safety cap is crossed, but continues
    draining until that owner terminates the child; stopping the reader first
    would deadlock the child on its stdout pipe.
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
    """Retain the exact capture preimage without retaining supplied media."""
    digest, size = identity
    return f"{name}_sha256={digest}\n{name}_bytes={size}\n"


def run_capture(args: argparse.Namespace) -> Path:
    source = require_absolute_regular_file(Path(args.source_release), "source release")
    recorder = require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    if args.recorder_protocol in EXPERIMENTAL_ONLY_PROTOCOLS and not args.experimental_observer:
        raise CaptureError("this DOSBox-X build is experimental-only and requires --experimental-observer")
    if (args.recorder_protocol in DRIVER_LOAD_RETURN_PROTOCOLS
            and args.capture_intent != "diagnostic-no-input"):
        raise CaptureError("the experimental driver-load observer supports diagnostic-no-input only")
    if (args.recorder_protocol in AH48_HANDLER_RETURN_PROTOCOLS
            and args.capture_intent != "physical-input"):
        raise CaptureError("the AH48 observer requires a visible physical-input capture")
    reviewed = RECORDER_PROTOCOLS.get(args.recorder_protocol)
    if reviewed is None:
        version = EXPERIMENTAL_OBSERVER_RECEIPT_VERSIONS.get(args.recorder_protocol)
        if not args.experimental_observer or version is None:
            raise CaptureError("recorder protocol is not available for an admitted capture")
        receipt_version, recorder_hash = version, None
    else:
        receipt_version, recorder_hash = reviewed
    recorder_admission = "pinned"
    if args.experimental_observer:
        if args.recorder_protocol not in EXPERIMENTAL_OBSERVER_PROTOCOLS:
            raise CaptureError("experimental observer is only available for its reviewed finite protocol")
        recorder_hash = EXPERIMENTAL_OBSERVER_PROTOCOLS[args.recorder_protocol]
        recorder_admission = "experimental-observer-not-for-recovery"
    recorder_identity = validate_recorder(recorder, recorder_hash)
    output = reject_unsafe_output(source, Path(args.output))
    if args.recorder_protocol in AH48_HANDLER_RETURN_PROTOCOLS:
        require_ah48_cache_output(output, dict(os.environ))
    if not MIN_DURATION_SECONDS <= args.duration_seconds <= MAX_DURATION_SECONDS:
        raise CaptureError(f"duration must be between {MIN_DURATION_SECONDS} and {MAX_DURATION_SECONDS} seconds")
    if not 0 <= args.focus_settle_seconds <= MAX_FOCUS_SETTLE_SECONDS:
        raise CaptureError(f"focus-settle duration must be between 0 and {MAX_FOCUS_SETTLE_SECONDS} seconds")
    environment = dict(os.environ)
    require_visible_operator_input(environment)
    source_hash, source_size = validate_source_release(source)
    output.mkdir(mode=0o700)
    mountpoint = output / "archive-ro"
    mountpoint.mkdir(mode=0o700)
    mounted = False
    try:
        mount_read_only(source, mountpoint)
        mounted = True
        game_root = mountpoint / GAME_ROOT
        if not game_root.is_dir():
            raise CaptureError("recognised archive does not expose its expected DOS game root")
        configuration = output / "recorder.conf"
        command_tail = output / "command-tail.txt"
        write_exclusive(configuration, recorder_config(
            game_root, args.machine_profile,
            release_sha256=source_hash if args.recorder_protocol == "v21-int93-installation" else None,
            graceful_host_close=args.recorder_protocol in GRACEFUL_WINDOW_CLOSE_PROTOCOLS,
            readonly_cdrom_archive=(args.recorder_protocol == AH48_TITLE_LIBRARY_V6_PROTOCOL)))
        configuration_identity = sha256_file(configuration)
        command = [str(recorder), "-conf", str(configuration), "-fastlaunch", "-c", "c:",
                   "-c", "mill.com 0"]
        write_exclusive(command_tail, " ".join(command) + "\n")
        environment.update({
            "PROJECT_EON_DOSBOX_X_RECORD": str(output / "events.raw"),
            "PROJECT_EON_DOSBOX_X_RESULT": str(output / "results.raw"),
            "PROJECT_EON_DOSBOX_X_INPUT_RECORD": str(output / "host-input-receipt.raw"),
        })
        if args.recorder_protocol in {"v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry", "v19-int93-vector"}:
            environment["PROJECT_EON_DOSBOX_X_HISTORY_RECORD"] = str(output / "normal-core-history.raw")
        if args.recorder_protocol in {"v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry", "v19-int93-vector"}:
            environment["PROJECT_EON_DOSBOX_X_ANOMALY_RECORD"] = str(output / "normal-core-anomaly.raw")
        if args.recorder_protocol == "v20-title-entry-transfer":
            environment["PROJECT_EON_DOSBOX_X_TITLE_TRANSFER_RECORD"] = str(output / "title-entry-transfer.raw")
        if args.recorder_protocol == "v21-int93-installation":
            environment["PROJECT_EON_DOSBOX_X_INT93_INSTALL_RECORD"] = str(output / "int93-installation.raw")
        if args.recorder_protocol in DRIVER_LOAD_RETURN_PROTOCOLS:
            environment["PROJECT_EON_DOS_DRIVER_LOAD_RETURN_RECORD"] = str(output / "driver-load-return.raw")
        if args.recorder_protocol in AH48_HANDLER_RETURN_PROTOCOLS:
            environment["EON_DOS_AH48_DIR"] = str(output)
        instructions = (
            experimental_driver_return_operator_instructions()
            if args.recorder_protocol in DRIVER_LOAD_RETURN_PROTOCOLS else
            experimental_ah48_operator_instructions()
            if args.recorder_protocol in AH48_HANDLER_RETURN_PROTOCOLS else
            capture_operator_instructions(args.capture_intent))
        for instruction in instructions:
            print(instruction)
        print(f"The {args.focus_settle_seconds}-second focus-settle window begins now; the {args.duration_seconds}-second capture window follows.")
        started = time.time()
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        assert process.stdout is not None
        console_path = output / "recorder-console.log"
        console_result: list[RecorderConsoleStatus] = []
        console_errors: list[BaseException] = []
        console_over_limit = threading.Event()

        def drain_console() -> None:
            try:
                console_result.append(capture_bounded_console(process.stdout, console_path,
                                                              console_over_limit))
            except BaseException as error:  # Propagate after process cleanup.
                console_errors.append(error)

        console_thread = threading.Thread(target=drain_console, name="project-eon-dos-console", daemon=True)
        console_thread.start()
        settle_deadline = time.monotonic() + args.focus_settle_seconds
        deadline: float | None = None
        operator_shutdown_deadline: float | None = None
        termination_reason = "emulator-exit"
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
            if not live_input_observed and input_delivery_file_observed(output / "host-input-receipt.raw"):
                live_input_observed = True
                print("HOST INPUT OBSERVED  receipt will be fully validated after capture.")
            if console_over_limit.is_set():
                process.kill()
                process.wait()
                exit_status = 125
                termination_reason = "console-safety-cap"
                break
            if should_stop_for_known_unhandled_interrupt(
                    args.capture_intent, output / "results.raw", args.recorder_protocol):
                process.kill()
                process.wait()
                exit_status = 126
                termination_reason = "known-unhandled-interrupt"
                break
            remaining = (settle_deadline - now if deadline is None else deadline - now)
            if remaining <= 0:
                if (args.recorder_protocol in GRACEFUL_WINDOW_CLOSE_PROTOCOLS
                        and operator_shutdown_deadline is None):
                    operator_shutdown_deadline = now + MAX_OPERATOR_SHUTDOWN_SECONDS
                    deadline = operator_shutdown_deadline
                    print("OBSERVATION WINDOW COMPLETE  close the visible DOSBox-X window to finalize its sidecar.")
                    continue
                process.kill()
                process.wait()
                exit_status = 124
                termination_reason = "timeout"
                break
            console_over_limit.wait(timeout=min(0.1, remaining))
        console_thread.join()
        if console_errors:
            raise CaptureError(f"unable to retain bounded recorder console: {console_errors[0]}")
        if len(console_result) != 1:
            raise CaptureError("bounded recorder console did not produce exactly one receipt")
        ended = time.time()
        after_hash, after_size = validate_source_release(source)
        if (source_hash, source_size) != (after_hash, after_size):
            raise CaptureError("source archive changed during capture; evidence is rejected")
        receipt_status = input_receipt_status(output / "host-input-receipt.raw")
        intent_status = capture_intent_status(args.capture_intent, receipt_status,
            live_input_observed,
            allow_finalized_receipt=args.recorder_protocol
                in BUFFERED_HOST_INPUT_RECEIPT_PROTOCOLS)
        observation_status = (experimental_observer_raw_status() if args.experimental_observer else
                              raw_observation_status(output / "events.raw", "events_raw", args.recorder_protocol)
                              + raw_observation_status(output / "results.raw", "results_raw", args.recorder_protocol))
        history_status = normal_core_history_status(output / "normal-core-history.raw",
                                                    args.recorder_protocol)
        history_boundary_status = normal_core_history_boundary_status(
            output / "normal-core-history.raw", output / "results.raw", args.recorder_protocol,
            termination_reason)
        anomaly_status = normal_core_anomaly_status(output / "normal-core-anomaly.raw",
            output / "results.raw", args.recorder_protocol, termination_reason)
        int93_status = int93_vector_status(output / "events.raw", output / "results.raw",
                                            args.recorder_protocol, termination_reason)
        title_transfer_status = title_entry_transfer_status(output / "title-entry-transfer.raw",
            output / "results.raw", args.recorder_protocol, termination_reason)
        installation_status = int93_installation_status(output / "int93-installation.raw",
                                                        args.recorder_protocol)
        driver_load_status = driver_load_return_status(
            output / "driver-load-return.raw", args.recorder_protocol)
        if (args.recorder_protocol in DRIVER_LOAD_RETURN_PROTOCOLS
                and not driver_load_status.startswith("driver_load_return=present\n")):
            raise CaptureError("experimental driver-load recorder did not produce a complete sidecar")
        ah48_return_status = ah48_handler_return_status(
            output, args.recorder_protocol)
        ah48_iret_status = ah48_after_iret_status(
            output, args.recorder_protocol)
        title_checkpoint_status = ("title_input_checkpoint=not-collected\n" if args.experimental_observer else
                                   title_input_checkpoint_status(output / "results.raw",
                                       output / "host-input-receipt.raw", args.recorder_protocol))
        write_exclusive(output / "run-status.txt",
                        f"capture_receipt_version={receipt_version}\n"
                        f"recorder_protocol={args.recorder_protocol}\n"
                        f"recorder_admission={recorder_admission}\n"
                        f"focus_settle_seconds={args.focus_settle_seconds}\n"
                        f"host_input_observed_during_capture={'true' if live_input_observed else 'false'}\n"
                        f"exit_status={exit_status}\ntermination_reason={termination_reason}\n"
                        f"start_unix={started:.6f}\nend_unix={ended:.6f}\n"
                        f"machine_profile={args.machine_profile}\n"
                        + identity_status("source_release", (after_hash, after_size))
                        + identity_status("recorder", recorder_identity)
                        + identity_status("configuration", configuration_identity)
                        + intent_status + receipt_status + observation_status + history_status + history_boundary_status
                        + anomaly_status + int93_status + title_transfer_status + installation_status
                        + driver_load_status
                        + ah48_return_status
                        + ah48_iret_status
                        + title_checkpoint_status
                        + recorder_console_status(console_result[0]))
        if console_result[0].over_limit:
            raise CaptureError(
                "recorder console exceeded the 64 MiB safety cap; evidence was retained but is not admitted")
        print("CAPTURE FINISHED  external evidence only; host-input receipt status is in run-status.txt")
        return output
    except Exception:
        # A failed preflight is not an admitted capture, but preserve every
        # recorder/configuration by-product for review. Deleting external
        # evidence after a source-integrity failure would hide the reason it
        # was rejected.
        if mounted:
            unmount(mountpoint)
            mounted = False
        raise
    finally:
        if mounted:
            unmount(mountpoint)


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-release", required=True,
                        help="Absolute recognised English Millennium DOS ZIP path")
    parser.add_argument("--recorder", required=True,
                        help="Absolute executable path to the externally reviewed DOSBox-X recorder")
    parser.add_argument("--output", required=True,
                        help="New absolute cache directory for external capture evidence")
    parser.add_argument("--duration-seconds", type=int, default=120,
                        help="Visible operator window duration (15-600; default: 120)")
    parser.add_argument("--focus-settle-seconds", type=int, default=10,
                        help="Manual visible-window focus time before capture (0-120; default: 10)")
    parser.add_argument("--capture-intent", choices=tuple(sorted(CAPTURE_INTENTS)), required=True,
                        help="Required operator declaration: physical-input or diagnostic-no-input")
    parser.add_argument("--machine-profile", choices=tuple(sorted(MACHINE_PROFILES)), default="svga_s3",
                        help="Explicit DOSBox-X video-machine profile (default: svga_s3)")
    parser.add_argument("--recorder-protocol",
                        choices=tuple(sorted(set(RECORDER_PROTOCOLS) | EXPERIMENTAL_ONLY_PROTOCOLS)),
                        default="v11",
                        help="Reviewed recorder output grammar, or an explicitly experimental protocol")
    parser.add_argument("--experimental-observer", action="store_true",
                        help=("Run only the separately reviewed, unpinned observer for a protocol test. "
                              "Its receipt is never recovery-admissible."))
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    try:
        output = run_capture(parse_arguments(sys.argv[1:] if argv is None else argv))
    except (CaptureError, OSError, subprocess.SubprocessError) as error:
        print(f"Millennium DOS capture preflight rejected: {error}", file=sys.stderr)
        return 2
    print(f"EXTERNAL CAPTURE DIRECTORY  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
