# Millennium DOS DOSBox-X recorder design

This is an implementation contract for an external, locally built DOSBox-X
recorder. It is not Project Eon runtime code or an emulator distribution. The
recorder itself is not an admitted trace. The recorder and every output it creates remain outside this
repository. It must never change original media, guest memory, CPU registers,
flags, vectors, input, execution order, or timing.

## Prototype status

An uncommitted, external Linux prototype has been reviewed against this
contract. Its changes are limited to the listed CPU, normal-core,
DOS-execution, and SDL-event-loop hook files plus declarations. The changed objects
compile successfully at the pinned revision. It opens its opt-in output with
exclusive creation (`O_CREAT|O_EXCL`, mode `0600`), so an existing file or
symlink is never overwritten; it rejects DS:DX strings containing separators
or a drive delimiter, and disables its image map after a duplicate entry-CS
observation.

It is **not a recorder release**. A full
serial build was completed with all compiler temporary files under the
project-scoped cache rather than `/tmp`. The current reviewed external
executable SHA-256 is
`bc8796acc3748db743352beac7a77797bd4f633d1ff30f0d90b282882691d695`.
It corrects the interrupt-hook match to the actual `CD 21` at `0x020c`, while
the emitted schema remains the intentionally stable `0x0209` setup-site ID.
The first explicitly configured, write-protected run emitted two raw request
observations. A later strictly filtered run also retained the title private
vector's two observed post-return AX values and was accepted by Project Eon's
CLI as the diagnostics-only `millennium-dos-en-title-init-v2` record. Its
exact hashes and strict runtime non-admission status are in
[MILLENNIUM_DOS_CAPTURE.md](MILLENNIUM_DOS_CAPTURE.md#first-trace-validated-capture-diagnostics-only).
Re-review the exact patch, retain a complete configuration and input timeline,
and capture the required result boundaries before extending any runtime path.

On 2026-08-30, a second external-only recorder build added a bounded host-key
receipt and completed an input-free five-second preflight. It opens no receipt
file when no keyboard event reaches DOSBox-X's SDL event loop; it emitted no
such file in that run. The new executable SHA-256 was
`0ba7a23b75ed543e519e56c6ece7106b81bd1fd8efb3e1b3813b79ca44b71cca`.
The recognised user-owned outer archive was rehashed afterwards and remained
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`.
This establishes the observer's no-input behaviour only; it does not record a
physical key, title poll, guest acceptance, frame, audio, or playable state.

The currently reviewed diagnostic build is SHA-256
`7b959f7aee3d2db0513db4f14e3075f306e798e25adaeeebd96aedd81aef65da`.
In addition to the pre-existing raw result sites, its callback default handler
retains exactly one raw `unhandled-interrupt` record when an exception-vector
callback loops. The record reports the callback's machine register state and
the three words then at its guest stack top, followed by four bytes at the
derived address. It does not write a v2 event, install a vector, alter guest
state, identify a faulting original instruction, or interpret the stack words
as a caller ABI. The bounded one-record limit prevents an exception loop from
becoming an unbounded raw-result file.

The external Project Eon capture helper separately recognises that completed,
raw `INT 6` observation and may terminate the recorder with the explicit v11
reason `known-unhandled-interrupt`. This is host-process control after the
observer has written its bounded evidence; it never handles the interrupt or
changes guest execution. The resulting receipt is diagnostics-only until a
complete genuine title/input/frame capture is available.

The next reviewed external build is the v13 title-poll observer, SHA-256
`07d80df74d303b519884d37dd474da071b414e98396e8ae030ad89256432521b`.
It adds an atomic recorder-local ordinal whenever the existing SDL receipt
observer sees a physical key press or release. At the exact original
`TITLES.EXE:$0d0a` `INT 21h/AH=06h, DL=$ff` site it emits at most one raw
`title-input-poll` record for the baseline and for each new host ordinal (32
records maximum). The record contains only that prior host ordinal and the
unchanged pre-interrupt AH/DL values. It does not read the DOS return, carry,
AL, keyboard buffer, title state, video surface, or audio stream; it neither
injects a key nor calls mapper/keyboard APIs. Project Eon's v13 receipt
rejects a poll ordinal that is absent from the separate host receipt and labels
a valid ordering **host-key-and-poll**, deliberately not “delivered” or
“accepted”. One 2026-08-31 operator-visible v13 receipt has been verified, but
it stopped at the pre-existing `INT 6` boundary before a host-key receipt or
title poll; it is no-input route evidence only, not an admitted physical-input
capture.

One additional experimental CPU-only probe is private to the capture cache: it
opens a distinct `O_CREAT|O_EXCL` result file only when explicitly configured
and reads `AX` at the first post-interrupt instruction for `MILL.COM:$020e`
and `TITLES.EXE:$0129`, and at the first post-call instruction for
`MILL.COM:$0213`. It does not write candidate v2 events, mutate guest state,
or assign a return ABI. Its raw observations and their strict non-admission
status are recorded in
[MILLENNIUM_DOS_CAPTURE.md](MILLENNIUM_DOS_CAPTURE.md#title-private-vector-return-reconnaissance-not-a-v2-event).

The current build additionally emits at most one raw `private-vector` record
at the already byte-locked `TITLES.EXE:$0127` software `INT $91` request. It
reads the two words currently stored in IVT slot `$91` before DOSBox-X
dispatches that original interrupt. The record is address provenance only: it
does not install, replace, invoke, or interpret the vector, and it establishes
neither handler code, return values, driver state, title output, nor gameplay.
It then records at most one `private-handler-entry` only if the normal core
reaches that same captured `CS:IP`. This proves only the observed control-flow
edge; it neither decodes nor replaces the handler and has no ABI meaning.
At most once, a subsequent `TITLES.EXE:$0129` return records raw `AX` and
FLAGS only when that endpoint was reached first. This is an ordered caller
re-entry observation, not a handler return contract or branch interpretation.

The design is locked to the locally inspected DOSBox-X source revision
`234797680781567e18c374c9e62da24de5423db0` and the Project Eon adapters
`millennium-dos-en-startup-v1` and `millennium-dos-en-title-init-v2`. A patch for another DOSBox-X revision must
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
| `src/gui/sdlmain.cpp:GFX_Events`, immediately before `MAPPER_CheckEvent` | Host SDL key press/release, scancode, symbol, modifier mask and `GetTicks()` | Retains a separate, bounded timeline of focused host events without calling mapper or keyboard APIs. |
| `src/cpu/callback.cpp:default_handler` | First unhandled interrupt number plus raw callback/stack/register snapshot | Separates a DOSBox-X exception-vector callback loop from the hash-bound original event stream without treating its callback PC or stack as a game result. |

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

## Physical host-key receipt

Set `PROJECT_EON_DOSBOX_X_INPUT_RECORD` to a new absolute path for the
external recorder. The GUI observer opens that path with `0600`,
`O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW` and writes at most 256 LF-terminated
records only for `SDL_KEYDOWN` and `SDL_KEYUP` events dequeued by the normal
DOSBox-X event loop:

```text
host-key <ordinal> ticks=<dosbox-ticks> state=down|up scancode=0x<hex> sym=0x<hex> mod=0x<hex>
```

The observer is before `MAPPER_CheckEvent`, does not synthesize an SDL event,
and cannot call `KEYBOARD_AddKey`, AUTOTYPE, a debugger, or guest-memory APIs.
This receipt is an independent host-input timeline, not a DOS input result.
A future capture must bind its press/release pair to the recorder's
`TITLES.EXE:0x0d0a` poll and resulting frame/state evidence before Project Eon
can claim the original title accepted an action.

The v13 observer adds the bounded chronology link only: it snapshots the
number of prior SDL receipt records at that original poll and uses a relaxed
atomic counter because the GUI and normal CPU paths need not share a scheduler
thread. It never asserts a happens-before relationship with guest keyboard
delivery. A frame, palette, audio, result-register, or `EXEC` checkpoint still
requires a separately reviewed observer and two physical write-protected
captures before any trace grammar or runtime adapter may use it.

## Visible-window focus and diagnostic-boundary policy

The physical capture runner starts DOSBox-X windowed and reserves ten seconds
for the operator to click and focus that visible window before its timed
capture interval. The operator must press/release ordinary keys in that window
only. Terminal input, `xdotool`, AUTOTYPE, debugger input, playback, and
guest-memory injection remain forbidden.

The known diagnostics-only `INT 6` boundary ends only a
`diagnostic-no-input` run immediately. A `physical-input` run stays open for
its configured window after that receipt, unless DOSBox-X exits, the console
safety cap is reached, or the window expires. This gives the visible operator
the declared opportunity to produce a host-input receipt without handling the
interrupt, injecting an event, or changing guest state. The current v13 route
still reaches that emulator/callback boundary before a title poll, so keeping
the window open does not make it title-input evidence. It must be traced rather
than bypassed or deferred. `HOST INPUT OBSERVED` is a live signal that the
protected receipt file has begun; the completed receipt is still validated only
after process exit. The selected focus duration and live receipt flag are
recorded in `run-status.txt`.

## Confirmed external `INT 6` callback boundary

The repeated v12 external fault receipt records predecessor bytes
`fe 38 03 00` at `f000:ca60`, immediately before the recorder observes the
unhandled `INT 6` callback at `f000:ca64`. This is now mapped to the reviewed
DOSBox-X source revision `234797680781567e18c374c9e62da24de5423db0`, not to
original Millennium media. Its `src/cpu/callback.cpp` has SHA-256
`153a1d9ce9d75ecadad2039fee962ed49710688e72ff00cee476bdd3b02a19a7`.

At that revision, callback initialization allocates the stop, idle, then
default callbacks in that order, making default callback index `3`; it writes
the four-byte callback opcode `FE 38 <index-le16>` for each. The non-PC-98 BIOS
initialization in `src/ints/bios.cpp` places that default callback in every
vector from `0x00` through `0x5f`, including `INT 6`. Thus the repeated raw
predecessor identifies the emulator's default interrupt stub precisely. It
does not identify why execution reached vector `6`, assign the fault to the
original game, prove a processor mode, or establish the meaning of the
unmapped return context `0e70:1900`. Resolving that preceding load/callback
route still requires a separately reviewed, read-only observer.

The v14 normal-core-history recorder narrows that last statement without
changing it. Its externally hash-bound binary is SHA-256
`748c1c934a78a28baef083fc352b552644f9665bc27fc032db0fdd7463ee5c63`
(86,233,096 bytes), built from the same upstream revision. It records a fixed
16-tuple memory-only ring at the existing normal-core observer and serializes
one exclusive/no-follow host sidecar only at the exact `f000:ca64`
default-callback boundary. Two genuine, independently verified captures agree
that the 15 tuples immediately before `f000:ca60` are zero-byte fetches at
`0e70:18e4` through `0e70:1900`. That is bounded execution-history evidence,
not a mapping, provenance, ABI, title, or game-state claim.

The v20 transfer observer is armed only by the existing original
`TITLES.EXE:$0134` `INT $93` observation after its IVT target has been read.
It retains exactly one predecessor/current normal-core pair when execution
later first enters `CS=$0e70`. Its sidecar has fixed lowercase-hex fields for
the declared context, a valid predecessor `CS:IP` and four-byte preimage, and
the first `0e70` `CS:IP` and four-byte preimage. It uses an exclusive,
no-follow, mode-`0600` host output path and is disarmed before sidecar I/O.
The observer is neither a DOS load hook nor a claim that the adjacency is a
guest `RETF`, callback, handler, or valid code path.

The v21 installer observer is separate and optional. It checks the full
original opcode preimage and `AX=$2593` at either `TITLES.EXE:$1163` or
`2200AD.EXE:$4175`, then observes the `DS:DX` target and resulting IVT `$93`
entry after the original DOS call. It writes at most one exclusive/no-follow,
mode-`0600` `int93-installation.raw` record through
`PROJECT_EON_DOSBOX_X_INT93_INSTALL_RECORD`. It does not install or modify an
IVT entry, guest memory, registers, input, scheduling, callbacks, or files.
Its V21 binary is SHA-256
`18ec0ead7d08deeca694fbbe8155d5f5e6a99562adaea22fe914a691961fe1f1`
(86,312,600 bytes). A missing sidecar means neither reviewed site was observed;
it is not a negative proof of installation elsewhere.

## Review and admission

Before a patched build is used, review that the diff changes only the four
hooks and recorder-local host-output implementations; it must contain no
`mem_write`, register assignment, callback/vector installation, injected
input, scheduler mutation, or guest-file write. Build it outside the Project
Eon checkout. Capture the patched executable SHA-256 and source revision in
the metadata template generated by `tools/record_reference_trace.py`.

Use the existing assembler and then the independent CLI validator. A valid
`--reference-trace` result admits diagnostic provenance only. It does not
authorize Eon to cross the DOS/private-driver/child-process boundary or to
claim Millennium gameplay parity.
