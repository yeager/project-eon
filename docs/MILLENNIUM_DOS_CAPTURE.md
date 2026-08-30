# Millennium DOS startup capture recipe

This document records the current acquisition boundary for the first
Millennium 2.2 DOS runtime evidence.  It is intentionally a capture recipe,
not an emulator setup guide and not a claim that the game is playable.

One external capture has been assembled and independently CLI-validated under
this recipe. That validation proves only the declared release identity,
provenance hashes, ordering, and narrow event grammar. In particular, no event
in this repository represents an emulator observation, and validation does
not prove a DOS/private-driver result, title transition, input result, frame,
audio checkpoint, or playable game state.

## First trace-validated capture (diagnostics only)

On 2026-08-30, the reviewed external recorder was built from the pinned
DOSBox-X revision and run against the English archive through an
`archivemount -o ro` FUSE mount. The mount was verified as
`ro,nosuid,nodev`, and the archive hash was identical before and after the
run. The first recorder build emitted one raw driver-load request before a
20-second headless smoke run was stopped. After correcting its interrupt hook
to match the actual `INT` opcode at `0x020c` while retaining the declared
`0x0209` setup-site identifier, the rebuilt recorder emitted this two-event
sequence:

```text
event 1 1 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin
event 2 2 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000
```

The raw event files remain outside this repository. The corrected two-event
file has SHA-256
`dc4a67ce61ed6bcd32767c2bf354444f525176ba81308cb9374d7718d1b7aa9a`; its
recorder executable has SHA-256
`122869702f46b5eda8f9f3ded1032c2e466dd6b0bfafaa460742ef4cd5712dc0`.
The explicitly configured 2026-08-30 capture was assembled with the source
archive hash checked before and after the run, and CLI validation accepted its
two ordered events through `millennium-dos-en-startup-v1`. The private
assembled manifest SHA-256 is
`79b77f399ebc0dd14494e9a6c0f0c7d55c13bbd864bba008f0f7f29cd7c885fa`.

| Private capture field | SHA-256 / value |
| --- | --- |
| Capture UTC | `2026-08-30T03:49:37Z` to `2026-08-30T03:50:00Z` |
| Explicit recorder configuration | `ba56ee4f77e3e982e5ddb36680190ec17b109e496bad8452cc0a0a4c895d3291` |
| Literal command tail | `ff9c433831db4975cefa7ee23b646cee2eba1ad676aad4f8df2a48ce36b72ca8` |
| Empty guest-input timeline | `f8a02c12d4fed2b0f8374ac5137e6565d1d9ca520cfe49d9e194376e7d7ad704` |
| Recorder source-review anchor | `234797680781567e18c374c9e62da24de5423db0` |
| Recorder-reported version | `DOSBox-X 2026.08.02 source e522642` |

The FUSE mount was `ro,nosuid,nodev`, and it was unmounted after the run. The
newly assembled directory contains private copies of the exact configuration,
command-tail, input-timeline, metadata, event stream, manifest, and receipt;
it contains no original release bytes. The CLI's `REFERENCE TRACE VERIFIED`
report is diagnostic provenance only and does not authorize Eon to replay the
capture or cross any result boundary.

The preliminary one-event file remains retained with SHA-256
`402b411a0a6958e2fd425bdc6dcdf4cedb1cee3a9fac9437745d6fa7b63e1c76` and
its executable has SHA-256
`5c1132e7a78b36703aa24347e08ba5d8c48fd4fb385a2412f9ce1818d895af09`.
An additional alternate video-mode run emitted the bounded `ega640.bin`
driver-load request (event-file SHA-256
`1a7c4cee85ba64a159d167948ca0162fc1425ba72f4738bae9dcb7d57f21c874`).
The retained DOSBox-X log for the corrected observation is
`6bf278b48d4d7fd05211afce73433775c21fc9995dd998bf6e9d90c749eb84ef`.
Every run was intentionally interrupted before a complete request sequence,
return value, input result, title state, frame, audio checkpoint, or game
state was observed. It is therefore a trace-validated diagnostic provenance
record, not a runtime input or a gameplay/parity claim.

### First raw result reconnaissance (not a v2 event)

The same exact archive and explicit read-only route were used for a separate,
private result probe on 2026-08-30. Static 8086 bytes establish that the
`INT 21h` opcode at `$020c` returns to `$020e`; the probe reads `AX` only when
the normal core reaches that instruction, before the original `PUSH CS`.
Its raw output SHA-256 is
`d09a4c840a4d9f1877b033a6e67f9a96b9ca7201ca5fddf932ddcaa2ed60c391`:

```text
raw-result 1 1 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591
```

The probe executable hash is
`cf0f3d67d2f9ca8857f9daf749136b19ed625918764ba23fd02b284c65423f78`.
The next caller-connected point is the return from original `CALL $0511` at
`$0210`, observed before `AND AL,AL` at `$0213`. No `$0213` record appeared
before the host ended this input-free run. This does **not** establish that
the call cannot return, nor any function meaning, result flag, branch,
driver outcome, title, input, frame, audio, or game state. The raw result has
no v2 grammar entry, is not assembled, and is not a runtime input.

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
| `mill.com` | setup site `0x0209`; `INT` opcode `0x020c` | `INT 21h`, `AX=0x2591`, `DX=0x0000` |
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
The assembler must receive those exact configuration, command-tail and
input-timeline files as well as their hashes described in
[`REFERENCE_TRACE_ASSEMBLY.md`](REFERENCE_TRACE_ASSEMBLY.md). It checks and
retains their evidence copies in the private capture directory. Commit neither
the archive, mount, saved state, screenshot, audio, raw recorder output, or
trace pair.

The exact reviewed DOSBox-X hook locations, normal-core restriction, program
identity map, and non-mutation requirements are in
[`MILLENNIUM_DOS_DOSBOX_X_RECORDER.md`](MILLENNIUM_DOS_DOSBOX_X_RECORDER.md).
That design is an external recorder implementation contract, not an admitted
trace or a runtime integration.

## Acceptance boundary

The trace-validated v2 capture improves diagnostic evidence for two of the
seven request sites only. It does not satisfy work-queue rank 1, whose acceptance
requires observed interrupt/EXEC/far-return and driver results through a real
navigable state. The next engineering task after this validation is to
define one bounded result-return capture contract, with its caller site,
register/flag widths, input boundary and output checkpoint proved before any
runtime implementation is extended.
