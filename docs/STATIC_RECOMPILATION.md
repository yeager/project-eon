# Static recompilation strategy

## Scope

Project Eon may statically translate individually admitted original code
regions into host C++ and execute those translations natively.  This is a
recovery technique, not a compatibility claim: a translated region is
available only for the exact release, code span and run-time preconditions
bound by its parser profile.

Static recompilation is distinct from both an interpreter and a full-machine
emulator.  It does not fetch and execute arbitrary guest instructions at run
time.  It lowers a reviewed, finite instruction region ahead of time and
connects its explicit external dependencies to narrow host contracts.

The method does not change Project Eon's preservation rules.  Commercial
bytes, generated translations, decompiler databases and disassembly listings
remain outside the repository and outside supplied media.  Repository source
contains only handwritten lowering code, tests, source hashes, addresses,
function maps and uncertainty records.

## Translation unit contract

Every translation unit must have all of the following before it is connected
to a launch path:

1. An exact release and source-span SHA-256, runtime address range and
   instruction boundary map.
2. A closed entry-point set.  Computed calls, returns, table dispatches and
   self-modification are either recovered with bounded evidence or terminate
   the unit at an explicit boundary.
3. A typed register/flag state only for values that cross an instruction or
   call boundary.  Integer widths, signedness, byte order and condition-code
   effects are encoded literally; host integer overflow and host structure
   layout are never used as substitutes for guest semantics.
4. A memory map whose each readable or writable range has an owner.  Reads of
   original media stay read-only.  Writes are made only through Eon's private
   runtime-memory transactions.
5. A named host contract for every OS, ROM, custom-chip, timer, input or
   dynamically supplied value.  A contract may return a source-backed result,
   accept a separately admitted observation, or stop.  It must never silently
   choose a plausible value.
6. Differential tests over source-backed fixtures and edge cases.  Each test
   records the input state, register/flag result and ordered private-memory
   writes.  A mismatch is a recovery defect, not a host-specific variation.

## Host shape

The recommended generated shape is one native C++ function per recovered
entry point with a small explicit context:

```text
translated_block(context, owned_memory, host_contracts) -> next boundary
```

`context` contains only the required guest-visible registers, flags and
program counter. `owned_memory` provides endian-aware, range-checked reads and
writes. `host_contracts` is a collection of narrow interfaces such as
`open_library`, `graphics_vector`, `dos_process_prefix` or `video_status`.
Calls inside the closed unit are direct host calls; a jump-table target is a
checked switch over the proven target set.  The result names either another
translated entry point or a stop reason carrying the still-unproved original
address.

This keeps the native program debuggable: source-address comments and an
address-to-host-function map are generated into the external analysis cache,
while the checked-in implementation retains only the hand-reviewed logic and
metadata necessary to reproduce it.

## Platform boundaries

For the Amiga code, a graphics or Exec vector is not a generic emulated
library call.  It is a versioned host contract with a source-addressed caller,
argument layout, return registers/flags and an evidence status.  The contract
cannot advance a path when its result is unknown.  Raster timing, CIA state,
process-prefix data and other machine state use the same rule.

For DOS code, the DOS process prefix, interrupts and video ports are likewise
separate contracts.  A known executable image at `CS:0100` is not evidence for
the process-prefix range at `CS:0000..03ff`; the latter must be recovered or
remain a stop boundary.

For Atari ST code, GEMDOS/XBIOS/Line-A and STX loader effects must be declared
as contracts.  A translated PRG routine must not treat an undocumented trap
result as a fixed host value.

## Adoption order

1. Continue using existing bounded, manually recompiled sessions as the
   reference style.  They already preserve exact source identities and stop at
   unknown values.
2. Build an offline-only 68000 lowering experiment in the external analysis
   cache.  Start with a short, straight-line, hash-locked routine that has no
   OS, ROM or custom-chip calls.  The experiment may emit C++ and a block test
   vector, but neither output is committed.
3. Promote only the reviewed lowering rules into a small checked-in translator
   library.  Start with arithmetic, moves, condition flags, direct branches
   and direct calls; reject every other instruction class.
4. Add a release-bound translation manifest which maps each translated block
   to source span, entry addresses, contracts and differential fixtures.
5. Expand one block at a time.  A block that first reaches an external
   dependency returns a named boundary, preserving the current evidence model.

The first useful Deuteros pilot is a self-contained 68000 helper, not the
current `$403f4` service batch: that batch immediately reaches graphics-vector
contracts whose return values are still unobserved.  Translating it now would
only move the existing boundary.  Likewise, the Millennium DOS title loop may
not cross its process-prefix read until that input has independent evidence.

## Current native frontiers

The live routes were rechecked against the admitted direct media on
2026-10-28.  They are useful translation tests, but neither one is a licence
to select a convenient host value:

| Route | Reached native boundary | What the source proves | What remains unknown |
| --- | --- | --- | --- |
| Deuteros Amiga title | `$fcca42` after the MakeVPort symbolic timing and mode prefixes | `$fcc9a2..$fcca42` writes only two source-defined Copper records after the unresolved timing word. The translator retains candidates `$0200`/`$0302`, leaves that target word unmaterialized, and derives the later record's third word directly from owned ViewPort data. | The next branch depends on the source's earlier View-mode-derived A5 scratch state. The supplied media does not establish that dynamic preimage, nor monitor standard, beam origin or phase. |
| Millennium DOS title | `$1917` after the descriptor-loop selector prefix | The hash-locked selector derives one ten-bit process-prefix offset from already-owned words at `CS:$1181` and `CS:$18f7`; diagnostics name that one byte rather than a whole prefix. | The admitted title image begins at offset `$0100`; it does not establish the selected preceding DOS process-prefix byte or the caller's environment. |

An explicit Modern-mode experiment may expose a labelled timing or process
profile for investigation, but it is not an Original-mode translation and may
not be used to claim a recovered title frame or game transition.  The
Original path continues to return the named boundary until evidence admits one
of the alternatives.

## Completion criteria

A translated path is eligible to contribute to playability only when it is
reachable from the supported launcher, accepts real SDL input through a
source-backed mapping, produces a source-backed visible state change, and its
external contracts are all admitted for that release.  Native instruction
coverage, a title screen, or a successful static parse alone is not gameplay
parity.
