# Millennium DOS DOSBox-X recorder design

This is an implementation contract for an external, locally built DOSBox-X
recorder. It is not Project Eon runtime code or an emulator distribution. The
recorder itself is not an admitted trace. The recorder and every output it creates remain outside this
repository. It must never change original media, guest memory, CPU registers,
flags, vectors, input, execution order, or timing.

## Prototype status

An uncommitted, external Linux prototype has been reviewed against this
contract. Its four source edits are limited to the listed CPU, normal-core and
DOS-execution hooks plus declarations. The changed CPU and normal-core objects
compile successfully at the pinned revision. It opens its opt-in output with
exclusive creation (`O_CREAT|O_EXCL`, mode `0600`), so an existing file or
symlink is never overwritten; it rejects DS:DX strings containing separators
or a drive delimiter, and disables its image map after a duplicate entry-CS
observation.

It is **not a recorder release**. A full
serial build was completed with all compiler temporary files under the
project-scoped cache rather than `/tmp`. The corrected executable SHA-256 is
`122869702f46b5eda8f9f3ded1032c2e466dd6b0bfafaa460742ef4cd5712dc0`.
It corrects the interrupt-hook match to the actual `CD 21` at `0x020c`, while
the emitted schema remains the intentionally stable `0x0209` setup-site ID.
The first explicitly configured, write-protected run emitted two raw request
observations. Its private preimages were assembled and accepted by Project
Eon's CLI as a diagnostics-only v2 provenance record; its exact hashes and
strict runtime non-admission status are in
[MILLENNIUM_DOS_CAPTURE.md](MILLENNIUM_DOS_CAPTURE.md#first-trace-validated-capture-diagnostics-only).
Re-review the exact patch, retain a complete configuration and input timeline,
and capture the required result boundaries before extending any runtime path.

The design is locked to the locally inspected DOSBox-X source revision
`234797680781567e18c374c9e62da24de5423db0` and the Project Eon adapter
`millennium-dos-en-startup-v1`. A patch for another DOSBox-X revision must
repeat the source-anchor review below; it must not be treated as compatible by
filename or version string alone.

## Capture configuration requirements

Use only the recognised English Millennium DOS archive:

| Field | Value |
| --- | --- |
| Outer SHA-256 | `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` |
| Outer bytes | `328383` |
| Adapter | `millennium-dos-en-startup-v1` |
| CPU core | `normal` |

`core=normal` is mandatory for this first recorder. DOSBox-X's dynrec core
checks its heavy debugger at block boundaries, so it cannot establish a
pre-instruction observation for `MILL.COM:0x02cf`. A recorder must retain its
literal configuration, command tail, input timeline, raw observations, and
patched executable hash outside the repository.

Original data must be mounted only from the read-only FUSE/COW route in
[MILLENNIUM_DOS_CAPTURE.md](MILLENNIUM_DOS_CAPTURE.md). The recorder output
path must be a new regular file beneath the project-scoped cache, never a
game-media path, Eon data directory, or repository path.

## Source anchors and read-only hooks

The following source anchors are the minimal reviewed locations. The hook must
only read their exposed values and append a host-side log record after an
exact-match filter succeeds.

| DOSBox-X source anchor | Required observation | Why it is needed |
| --- | --- | --- |
| `src/cpu/cpu.cpp:CPU_Interrupt(Bitu,Bitu,uint32_t)` | Software `INT 21h` and `INT 91h`, with source `CS:(oldeip-2)`, AX, DX, ES, BX and DS before vector dispatch | Covers both DOS and game-installed private interrupts without accepting a later DOS-handler result. |
| `src/cpu/core_normal.cpp:CPU_Core_Normal_Run`, immediately before `Fetchb()` | Pre-instruction `CS:IP`, DS:DX for `MILL.COM:0x02cf` | Covers the only non-interrupt driver-load observation at its exact caller address. |
| `src/dos/dos_execute.cpp:DOS_Execute(const char*,...)`, after the new program's `csip` is known and before it starts | Full source `name` plus entry CS for a recorder-owned image map | `RunningProgram` and DOS MCB names are truncated, so they cannot establish `mill.com`, `titles.exe`, or `2200ad.exe` identity. |

`DEBUG_HeavyIsBreakpoint()` in `src/debug/debug.cpp` is useful for local
debugging, but it is not the primary recorder hook: it is not an
instruction-by-instruction guarantee under dynrec. `DOS_21Handler`,
`DOS_Execute`'s DOS-handler entry, and `DOS_OpenFile` are corroboration only;
they occur too late to prove the prescribed caller PC and cannot observe the
game's `INT 91h` vector.

## Observer algorithm

The patch must have one disabled-by-default recorder whose output is enabled
only by an explicit, fresh host output path. It keeps a recorder-owned map
from full, canonical DOS executable name to its observed entry CS. It must
reject an unknown, duplicate, conflicting, or non-8.3 mapped program identity
rather than assigning a name from a DOSBox-X title string.

At `CPU_Interrupt`, it accepts only `CPU_INT_SOFTWARE`, interrupt `0x21` or
`0x91`, and an exact mapped image/site/register shape:

```text
mill.com    setup 0x0209 / INT opcode 0x020c  int 0x21  ax=0x2591 dx=0x0000
titles.exe  0x0127  int 0x91  ax=0x0000 es=cs bx=0x1ac4
2200ad.exe  0x0124  int 0x91  ax=0x001f es=cs bx=0xd19e
titles.exe  0x0d0a  int 0x21  ah=0x06 dl=0xff
titles.exe  0x1a12  int 0x21  ax=0x4c00
mill.com    0x0337  int 0x21  ax=0x4b00 DS:DX=titles.exe|2200ad.exe
```

At the normal-core prefetch hook, it accepts only mapped
`MILL.COM:0x02cf` and a bounded, NUL-terminated DS:DX basename of
`ega640.bin` or `mcga.bin`. A bounded string read must reject unterminated,
overlong, non-ASCII, absolute, traversal, or path-containing values. It is an
observation only; it may not call a DOS file API or alter a file result.

For each accepted record, the recorder immediately appends exactly one
LF-terminated v2 event line prescribed in
[REFERENCE_TRACE_FORMAT.md](REFERENCE_TRACE_FORMAT.md#event-stream-v2-millennium-dos-adapter).
It rejects every other observed interrupt/site instead of emitting a generic
event. The recorder's sequence and tick counters are strictly increasing
integers owned by the recorder. Raw `PIC_Ticks`/`PIC_FullIndex()` observations
may be retained privately in a separate raw log, but they must not cause the
recorder to delay the guest or make event ticks non-monotonic.

No record establishes a return value, carry flag, selected driver, child
execution, private-vector behavior, input result, or game state. A failed
match is a capture failure, not a fallback to static disassembly.

## Review and admission

Before a patched build is used, review that the diff changes only the three
hooks and a recorder-local host-output implementation; it must contain no
`mem_write`, register assignment, callback/vector installation, injected
input, scheduler mutation, or guest-file write. Build it outside the Project
Eon checkout. Capture the patched executable SHA-256 and source revision in
the metadata template generated by `tools/record_reference_trace.py`.

Use the existing assembler and then the independent CLI validator. A valid
`--reference-trace` result admits diagnostic provenance only. It does not
authorize Eon to cross the DOS/private-driver/child-process boundary or to
claim Millennium gameplay parity.
