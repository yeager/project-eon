# Millennium DOS startup capture recipe

This document records the current acquisition boundary for the first
Millennium 2.2 DOS runtime evidence.  It is intentionally a capture recipe,
not an emulator setup guide and not a claim that the game is playable.

No Project Eon reference trace has been recorded or admitted by following
this recipe yet.  In particular, no event in this repository represents an
emulator observation.

## Audited local route

The only source release eligible for the current English DOS adapter is the
user-owned outer archive with this identity:

| Field | Value |
| --- | --- |
| Game/platform/language | `millennium` / `dos` / `en` |
| Outer archive size | `328383` bytes |
| Outer archive SHA-256 | `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` |
| Required adapter | `millennium-dos-en-startup-v1` |

On the capture host examined on 2026-08-30, the installed `/usr/bin/dosbox`
was DOSBox `0.74-3-5build3` for arm64, with binary SHA-256
`12ddd009fcb0372b492d90632cf8394ef5adbe04fbacb733dd622d872d38e1a8`.
Its documented command line provides normal configuration, directory/disk
mounting and media captures; it does not provide a documented machine-readable
CPU, interrupt, EXEC, or register-return recorder.  Its capture directory is
therefore not evidence for the reference-trace adapter.

The release is a ZIP archive containing a directory tree.  DOSBox 0.74's
`MOUNT` command accepts a host directory, not that archive.  A previously
created writable extracted duplicate in the local tools cache is *not* an
eligible media route: it violates Eon's no-unpack/no-copy rule.  Do not mount,
repair, re-chmod, or use that copy for a capture.

`archivemount` is available on this host and explicitly supports `-o ro`.
It can expose the owner-supplied archive through a read-only virtual directory
without extracting or modifying it.  That makes it suitable only as the lower,
original-data layer.  A fuller run may write a save or other guest state, so a
separate disposable copy-on-write layer must be provided before the game is
allowed to write.  Do not point a writable DOSBox mount at the archive, its
read-only VFS, the real-data directory, or any extracted cache copy.

## Required recorder, not just DOSBox 0.74

Use an emulator build with a debugger/instrumentation hook at instruction
execution and DOS dispatch.  A debugger can help establish the first sites,
but a screenshot, a hand-transcribed register window, or a generic CPU dump
is not itself an admissible event stream.  The recorder must write the exact
LF-only event records at the moment it observes them and retain its unmodified
raw log separately.

The locally cached DOSBox-X source/build is a possible investigation tool: it
contains instruction and interrupt breakpoints and optional heavy CPU logging.
That is not an endorsement of its emulation behaviour, and its generic heavy
log is not Eon's event format.  If it is used, retain its exact executable
SHA-256, source revision, configuration, command tail, and raw log alongside
the capture.  A small, reviewed recorder hook is still required to emit only
the schema below; it must observe and serialize values, never alter registers,
interrupt vectors, memory, file results, input, or guest timing.

The first hook must identify the loaded image by its original DOS name and
observe only these byte-locked request sites:

| Image | PC | Observation |
| --- | --- | --- |
| `mill.com` | `0x0209` | `INT 21h`, `AX=0x2591`, `DX=0x0000` |
| `titles.exe` | `0x0127` | `INT 91h`, `AX=0x0000`, `ES=CS`, `BX=0x1ac4` |
| `2200ad.exe` | `0x0124` | `INT 91h`, `AX=0x001f`, `ES=CS`, `BX=0xd19e` |
| `titles.exe` | `0x0d0a` | title input poll: `INT 21h`, `AH=0x06`, `DL=0xff` |
| `titles.exe` | `0x1a12` | `INT 21h`, `AX=0x4c00` |
| `mill.com` | `0x02cf` | driver-load request for `ega640.bin` or `mcga.bin` |
| `mill.com` | `0x0337` | `INT 21h` EXEC request for `titles.exe` or `2200ad.exe` |

These are request observations only.  They do not establish a DOS return,
private `INT 91h` result, driver behaviour, selected display mode, child
execution, input outcome, or navigable state.  Capturing any such result
requires a new, separately reviewed schema and evidence boundary before Eon
can consume it.  Do not add guessed return values to the current adapter.

## Safe capture procedure

All capture material remains outside the repository and outside the
user-supplied media tree.  Use a new, scoped path such as
`/home/yeager/.cache/project-eon-tools/millennium-dos-capture-<UTC>`; never
use `/tmp`.

1. Hash the owner-supplied outer archive before and after the run.  Both
   values must equal the table above.  List the ZIP directory with a read-only
   tool only; do not extract it.
2. Create a new empty capture directory and a separate VFS mountpoint beneath
   that scoped cache path.  Mount the archive with `archivemount -o ro` (and,
   where supported, select only the archive's game-root subtree).  Record the
   exact `archivemount` executable/version and mount command in the retained
   command-tail preimage.  Verify the mount is read-only before starting an
   emulator.
3. If the observed path reaches a guest write, stop the run unless a separate
   reviewed copy-on-write union presents the read-only VFS as its lower layer
   and a newly empty disposable upper layer as its only writable target.  Hash
   and retain the union tool/configuration.  The upper layer is capture
   by-product, never original media and never an Eon input.  DOSBox 0.74 alone
   does not provide this merged read-only-plus-COW game directory.
4. Configure the instrumented emulator with explicit CPU, memory, machine,
   video, sound, timing, mount, and `autoexec` settings.  Mount only the
   verified VFS or reviewed COW view as the DOS game drive.  Retain the exact
   configuration text; do not use an implicit per-user default configuration.
5. Start `MILL.COM` from that mounted original directory.  The hook writes an
   unmodified raw recorder log and, only for the seven accepted site shapes,
   an LF-only candidate event file.  Stop if loaded image identity, registers,
   operand bytes, or event ordering disagree with the table.  Do not fill a
   missing event with a static-disassembly conclusion.
6. Retain an input-timeline preimage, including an explicit empty timeline if
   no keys were supplied.  Record UTC start/end times before normalizing
   nothing.  Hash the emulator executable, config, literal command tail, and
   input timeline with SHA-256.
7. Assemble the external evidence using
   [`record_reference_trace.py`](../tools/record_reference_trace.py) and the
   `millennium-dos-en-startup-v1` adapter.  Then validate it against the same
   owner-supplied archive using the CLI.  The CLI report is provenance-only;
   it must not be used to advance the game session.

The candidate event records must exactly follow the registered schema in
[`REFERENCE_TRACE_FORMAT.md`](REFERENCE_TRACE_FORMAT.md#event-stream-v2-millennium-dos-adapter).
The assembler manifest must include the exact configuration, command-tail and
input-timeline hashes described in
[`REFERENCE_TRACE_ASSEMBLY.md`](REFERENCE_TRACE_ASSEMBLY.md).  Retain the
preimages and raw recorder output privately; commit neither those files nor
the archive, mount, saved state, screenshot, audio, or trace pair.

## Acceptance boundary

An admitted v2 capture would improve the diagnostic evidence for the seven
request sites only.  It does not satisfy work-queue rank 1, whose acceptance
requires observed interrupt/EXEC/far-return and driver results through a real
navigable state.  The next engineering task after such an admission is to
define one bounded result-return capture contract, with its caller site,
register/flag widths, input boundary and output checkpoint proved before any
runtime implementation is extended.
