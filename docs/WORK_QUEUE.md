# Project Eon P0 work queue

## Current DOS descriptor continuation

Mode one's palette source is now corrected to TITLE.LIB `[$25f9,$28f9)`.
The original LDS restores library-base:$0006; the former `$4865` overread
was a relocated-pointer error. The complete 768-byte copy now reaches its
real BIOS request at `$0fd8`, AX=$1012/CX=$00ff. A separate typed BIOS result
permits return to the shared setup. Hardware-dependent graphics selection
still needs captured evidence. The active runtime now owns a hash-locked
EGA640/MCGA loader session behind typed selector and DOS-result observations,
and unit tests cover the explicit open/seek/allocate/rewind/read/close path
through the `$020c` SetVect result. Those tests use synthetic observations;
they do not prove the original selector, IVT contents, or INT $91 dispatch.
An optional typed INT $91 IVT tuple may now follow that raw DOS result. It
preserves the observed interrupt, offset, and segment verbatim, including a
mismatch with the loaded image address. That comparison is correlation only;
it does not establish successful installation, handler dispatch, or service
execution, and it does not select a private service. No private service can
yet be selected from the driver profiles. Title mode alone does not establish
which driver is installed.
The hash-bound English function-$00 profiles validate exact instruction spans
through their match-continuation opcodes. The standalone session accepts the
conditional cache query result, set-mode result, and verify-query result as
typed external inputs; it stops at the successful continuation or mismatch
RET opcode after applying the proven local zero-AX operation. Focused real-
media tests cover both original leaves and rejected/reordered results. No BIOS
behavior, caller return, IVT contents, or private-handler dispatch is
established. The bounded successful postlude below records only hash-bound
local instruction effects and typed stop points.
The separate success-postlude step models EGA's local `$0192` write and
register clamp. Its next prefix records an explicit pending `PUSH SI` stack
write, then stops before one-count `POP AX` `$022e` or VGA `OUT` `$0207`. The
one-count path advances through its paired POP and, with explicit DS input,
records the pending `$008a` store and stops at RET `$0234`; continue only with
an exact caller/return observation. The multi-count EGA path now accepts
explicit DS and records each bounded descriptor write and `$1f40`-byte plane
clear through the balancing POP at `$022e`. The shared local-pop/store path
then restores SP and records the `$008a` write through RET `$0234`; its RET
destination is accepted only as a typed stack word at that exact `SS:SP`.
The session records the near-RET IP/SP effect but does not execute the target.
The loop outcome retains the preceding VGA `OUT` as a port-write intent,
without modeling device effects.
The zero-count input decrements to `$ffff` in the original and is rejected by
this bounded step rather than expanding into an unbounded clear loop.
MCGA's branch step stops at the `$023d` single-count target or before INT
`$92` at `$021f`. No graphics or private-service behavior is inferred.
The standalone English function `$1f` session now requires the explicit
runtime DS-local byte and stops at its hash-bound RET instruction for EGA640
and MCGA. It does not use either file's initialized zero or establish caller
reachability.
Function `$13` also has a standalone poll-loop session; continue it only with
ordered raw reads from VGA status port `$03da`. A completed poll does not prove
an MCGA interrupt postlude or select an installed driver.
The standalone INT `$91` function-`$13` session now hash-binds the EGA/MCGA
dispatcher prefix and table route, carries the typed `$0129` interrupt frame,
and models MCGA's `$01e4` clear plus explicit `$01e5` conditional byte. It
continues the nonzero `$01e5` path through the hash-bound local callback
prefix at `$0d22`. Its typed byte and word reads select only the proven branch
destinations `$0c94`, `$0d10`, or fallthrough `$0d35`. From `$0c94`, a typed
nonzero `$0c92` counter is decremented and the local `$01e4/$01e5` flags are
cleared before stopping at RET `$0d10`; a zero counter now records the exact
eight register pushes from `$0ca2` through `$0ca9`, accepts a typed
`CS:$0c8c` far pointer at `$0caa`, pushes its offset, and stops before the
descriptor-word read at `$0cb0`. The `$0d35` branch records `$01e4=1`, accepts
explicit register values and `SS:SP`, and records the nine ordered stack
pushes. It accepts a fresh CS:`$0d18` word at `$0d44`, a far pointer from
CS:`$0d1a` at `$0d49`, then DS:`SI+8` at `$0d4e`, stopping at `$0d54` or
`$0dad`. From `$0d54`, it reads the video pointer at CS:`$0d1e` and the byte at
DS:SI, selecting `$0dcd` for 1, `$0dcb` for 2/3, or `$0dad` for 0/4..255.
The bounded `$0dad` model applies one authenticated decrement/`SI += $0c`/`LOOP`
iteration and accepts a fresh typed word at `$0d4e`; it stops at `$0db5` when
CX reaches zero. The `$0dcb` route records its jump to `$0e46`. Its
hash-bound local setup now accepts typed descriptor bytes/word at `$0e49`,
`$0e55`, and `$0e62`, computes the row count and 16-bit source/destination
registers. The hash-bound pixel loop accepts its ordered source/destination
reads, conditionally applies the threshold reduction, records memory writes,
and returns to `$0e71` between pixels or reaches the final-pixel epilogue at
`$0e8f`. Explicit ordered SS:SP reads
restore SI/DS, then the descriptor count is decremented; its zero branch clears
the descriptor byte, and both branches reach the jump boundary `$0d6a`. These
stack and memory values are scenario inputs, not captured runtime observations.
The palette-descriptor prefix from `$0d6a` now updates the index/count fields
and records the first VGA `OUT DX,AL` effect at `$0d82`. Explicit status reads
from `$03da` are accepted at `$0d94` until bit 0 is set. The palette pointer at
CS:`$0d1e` is then observed, and all three byte/write pairs to `$03c9` are
modeled per bounded step. The `$0da1` LOOP updates CX and either begins another
triple or reaches `$0da3`; the hash-bound descriptor epilogue is modeled
through `$0db5` with explicit SI/DS/CX stack words and a branch to `$0d4e` or
`$0db5`. Nine final register pops and both CS flag clears are modeled through
the RET at `$0dca` to the hash-bound `$0021` CALL return at `$0024`, then reaches
the callback-path IRET boundary. This proves only the static route; runtime
reachability remains unobserved. Other VGA port I/O remains outside the model.
The `$0d10` RET also returns to the hash-bound `$0024` CALL continuation;
the zero-counter branch stops before its `$0cb0` descriptor read. The zero `$01e5` path still
stops at IRET. These scenario
inputs do not authenticate the installed vector or prove title execution
selected this handler.
Successful original MCGA/EGA640 initialization returns title mode 1/4,
respectively. The existing global-mode-2 descriptor regression uses explicit
arithmetic observations; it is not evidence that either supplied driver
reaches that path. Mode one's repeated palette now uses the current library
bytes, selects `$1c07->$1ac6`, and enters its own bounded descriptor
postprocessing. All 37 records translate 368 decoded bytes in place before
the shared `$1967` return. This execution accepts the actual library allocation
instead of requiring the test fixture's `$3000` segment. Earlier BIOS/private
results remain explicit inputs; graphics-loader ownership and EGA mode-four
postprocessing remain priorities before claiming a complete driver path.

The owned mode-two path now addresses the complete 37-record loop using the
actual relocated directory at TITLE.LIB+$4813. The first record is
$3294:$0001; the former $5050 mapping and lookup alias are rejected.
All header, payload and lookup reads remain within the verified leaf.
Each finite quantum preserves exact segmented addresses and commits session
plus memory together. After the loop count reaches zero, the owned caller
continues through RET $1967 to $1c20, reads runtime word CS:$1896, shifts it
right once, and enters $1931. The child executable image already supplies
this word; the continuation reads current native memory rather than assuming
its initial value. It stops at private INT $91 at $0127, requested by
$1937 with AX=$0013. Recover this service and its caller-specific return
before advancing into the $1917 patch helper or title input.

The EGA640 mode-four descriptor continuation reads the exact `$14f0`, `$14fc`,
and `$1500` header operands, then emulates the hash-bound `$15c5..$163a`
planar loop using initialized decoded bytes and the exact record table. A
real-media regression now drives the body for all 37 records, checks stride,
clear, planar writes and resulting emulated memory, and uses only the admitted
child image to seed native memory. Its startup INT 91h, DOS and BIOS returns
are synthetic fixture inputs, not captured observations; captured runtime and
decoded-pixel parity remain unverified. Keep mode four separate from MCGA
mode-one translation and the synthetic mode-two regression; keep mode three
distinct and leave private function `$0013` unexecuted until its result is
evidenced.

The title's hash-bound AX=`$0006` caller now has standalone EGA640/MCGA
entry-side clipping models. They accept only ordered descriptor-word reads,
preserve each driver's distinct signed/unsigned 16-bit branch behavior, and
stop before source pointers, helper calls, VGA I/O, or pixel writes. This does
not select the installed driver or establish a captured `$0006` dispatch; real
call reachability, descriptor provenance, rendering, and frame comparison
remain open.

MCGA static follow-on spans now have a standalone observation model for
`$071d..$072d`: read the descriptor byte at `ES:BX+$12`, derive `DI=$b0+byte`,
read `DS:[DI]`, and record its store at `DS:$07b9`. DS and CS remain distinct
types because later code reads `CS:$07b9`. This model is independent of the
clip-prefix model and proves no runtime reachability. A second standalone
model now covers `$072d..$073a`, including the recorded DS stack write, ordered
descriptor/source far-pointer reads, source header word, DS changes and stack
pop. It stops before `$073a`. Neither model is chained to the clip prefix.
`$073a..$076f` now has a standalone typed register/stack observation model,
starting from an explicit 16-bit register snapshot and ordered descriptor
reads, with no assumed reachability from either earlier span. It stops before
opaque helper `$0666` and assigns no helper/blit meaning. Its real-media native
test checks signed multiply, 16-bit register arithmetic, all five `SS:SP`
writes, and the no-wrap bound BX <= `$ffee`. The MCGA loops
`[$078b,$079d)` and `[$07a1,$07b5)` now have a hash-bound register/address
observation session. It records the word/byte REP phases, DF-directed SI/DI
stepping, per-row BX/DX strides, and BP/JNZ iteration. DF, segments, and
registers remain explicit inputs. A separate bounded executor now reads only
initialized bytes from `NativeRuntimeMemory`, takes the real-mode A20 mapping
explicitly, preserves ordered MOVSW/MOVSB reads for overlapping copies, and
commits unique final destination bytes in one atomic batch. Its real-media
test covers both DF directions, overlap, an A20 alias, and rejection without
partial mutation when source bytes are absent. It accepts the completed
hash-bound caller result directly, validates the returned loop against the
driver bytes, and refuses outcomes without the `$88` branch and matching RET.
This is still not wired into release runtime: the DOS guest does not feed it
live registers or memory, and neither runtime reachability nor pixels are
established. A new complete linear candidate scan of the exact English game
image locates three AX=6 near calls to the private wrapper at `$0124` (`$5d7c`,
`$6b0c`, and `$ad15`); the third call uses 16-bit IP wrap. See the caller
facts and report hash in `PRESERVATION.md`. This is static caller evidence, not observed
reachability or an installed-driver proof. Next, capture the caller registers,
descriptor bytes, private INT `$91` return and loaded driver identity at one
of those sites; then connect only that call path to guest-owned state, resolve
A20 mapping from the running DOS environment, and compare visible output
against a capture.

An integrated MCGA caller observation now connects the setup and helper
outcomes across `CALL $076f`, explicit caller stack reads, `CS:$07b9` and the
two `$88` copy branches. It feeds the selected state into the corresponding
hash-bound copy-loop address model and records the outer RET word at `$079c`
or `$07b4`; a helper carry path instead models `$079d` cleanup and `$07a0`
RET. A non-`$88` CS byte now exports the recovered `$07b5` register snapshot
to a bounded byte-effect model for `[$07b5,$07c7)`: ordered `DS:SI` reads,
`ES:DI` reads/OR writes, DF-aware SI, forward DI, row strides and SS:SP
push/pop observations. The snapshot and branch remain caller-supplied facts;
no game reachability or pixel interpretation is established.

The following paragraphs retain earlier recovery history. Their intermediate
stops and historical source assumptions are superseded by the current
[title initialization contract](MILLENNIUM_DOS_TITLE_INITIALIZATION.md).

The Millennium DOS non-mode-1 title path now observes both external vector
pairs and atomically installs the exact timer and video hooks. Its verified
caller continuation enters the repeated mode call `$1c02->$1ada`. Mode one
has a distinct BIOS boundary at `$0fd8` after its corrected palette copy.
The repeated `$1ada` call now consumes a fresh typed
INT `$91` result and sixteen fresh typed BIOS palette results, returns through
the caller setup, and stops before `$1c0e->$135e`. The next DOS evidence job
is recovering that callee without inventing setup or driver behaviour.
The `$135e` callee is now native and atomically binds its selected allocation
pointer into title state. The `$1c11->$0ff3` request now reaches typed private
INT `$91` function `$0019` at `$0127`. Continue only with its raw result;
its setup ABI remains unproven. The raw result is now retained separately and
the caller stops before `$1c17->$1725`; recover that callee next.
The `$1725->$1390` route is now native through its exact pointer setup and
stops at the typed two-word far read `$13aa`. Supply only the genuine words
from the reported relocated source before continuing.
The genuine `$0006/$0000` words are now provenance-checked and the normalized
pointer is committed. Continue with the typed record word at `$13cd`, source
`$3000:$001e`; do not infer loaded record contents.
An attempted automatic continuation from the admitted `TITLE.LIB` allocation
was rejected during focused native testing: later record-byte values cannot yet
be established solely from that allocation in this caller route. They remain
typed external observations until a hash-bound provenance mapping proves their
source; do not use generic DOS physical aliasing as a substitute.
The first record word `$0140` is now admitted through the dedicated
single-word facade. Continue at `$13d0` with genuine word `$00c8` from
`TITLE.LIB+$001c`.
That `$00c8` word and its exact multiplication effects are now native. Continue
at `$13e2` with the external `$3000:$001a` word.
That genuine zero word and adjusted-product write are native. Continue at the
typed byte boundary `$13e9`, source `$3000:$0007`.
The genuine `$23` byte is now admitted through all production facades and its
incremented `$24` is committed. Continue at `$13f2`, source `$3000:$000a`.
The genuine zero byte and complete local return/request build are now native.
The raw function-`$0006` result resumes into function `$001a`; its raw AX,
FLAGS, and ten-byte `CS:$0fdf` record are now admitted atomically. The first
`$1941` title-loop iteration advances both output pointers and stops at the
typed two-word `$13aa` read from relocated `TITLE.LIB+$000f`. The genuine
`$0503/$1f02` pair is now provenance-checked and normalized to
`$5050:$0003`. The raw runtime words at `$5050:$001b` and `$5050:$0019`
are now admitted through all single-word facades. Their exact unsigned
product/store sequence is native.
The third typed runtime word from `$5050:$0017`, the byte from `$5050:$0004`,
and their exact arithmetic/store sequences are native. The typed byte at
`$13f2`, source `$5050:$0007`, and both deterministic branch continuations
are also native. The one/two branch owns its first payload byte and exact
prefix through `$1427`; the complete hash-bound `$1437..$1487` escape, run,
lookup, high/low-half, and extended mode-two output loop is now native with
atomic writes. The `$1488` post-record mode dispatch is native and stops at
typed first-header bytes `$14a9`, `$14f0`, or `$1647`; continue from those
genuine ordered runtime observations. Mode two now owns its genuine `$1647`,
`$1653`, and `$1657` inputs and exact setup through typed source byte `$16b3`;
continue there without assigning lookup or pixel semantics.
The full typed `$16b3..$16e8` nested byte-pair loop is native, including
lookup boundaries, atomic destination writes, repeated row/column edges, and
return. It can now consume bytes directly from owned native memory, including
physical-equivalent DOS segment aliases, under a finite transactional cap.
Its `$16e8` return is caller-connected through exact `$1740..$1763`, genuine
embedded `$170c/$170e` table words, raw callee-word copies, and the complete
private function-six boundary at `$1764->$0122`. Continue from that typed
private result; do not infer pixel or palette meaning. The later function-six
return is now state-distinct, advances the title loop's output pointers to
`$02e0`, and stops at the next descriptor pair `$13aa`, source `$3481:$001b`.
That one pair is now automatically sourced from hash-admitted
`TITLE.LIB+$482b` after an exact segment-to-physical proof and normalizes to
`$32a1:$0006`. Continue only with the typed runtime word at `$32a1:$001e`;
do not generalize this one proven alias into generic DOS memory semantics.
The same leaf-relative proof now admits that record's complete fixed header
atomically. It additionally admits the exact first payload byte at
`TITLE.LIB+$2a32` and executes `$1419..$1427`, including the write to
`$4000:$02e0` and the non-zero loop edge. The exact next stream byte `$10`
and its two admitted lookup bytes now execute both decoder nibbles, producing
`$00/$01` at `$4000:$02e1..$02e2`. A bounded transactional stream driver then
admits only the canonical relocated second-descriptor byte at `$32a1:$0024`
(`TITLE.LIB+$2a34 == $1e`); it rejects physical-equivalent aliases and
commits no partial transition on a contradictory leaf. That byte and its
canonical little-endian word `$201e` now execute `$144a..$146b`, writing the
three repeated bytes at `$4000:$02e3..$02e5` and returning to `$1428`, source
`$32a1:$0025`. Its exact high-nibble continuation and lookup then return to
the ordinary stream boundary `$32a1:$0026`. The same 16-observation bounded,
transactional native continuation now validates the exact 6-byte
`TITLE.LIB+$2a34..+$2a39` prefix (SHA-256
`f5e49eddff72cad076c01cc7d4b884404224ea78a24ebe659e320951bca14b29`),
executes its two low/high lookup pairs and the selected mode-two run, and
returns to `$1428`, source `$32a1:$002b`. Do not infer codec semantics or
generalize this relocation into DOS-memory aliasing.
The other-value branch owns the second descriptor and first two raw words plus
their product and subtraction; continue at `$13e9`, source `$3c80:$0001`. Do not assign
graphics or codec semantics to these fields.

The Millennium Atari config loop now has all three D0-indexed iterations, D7
termination, and its deterministic epilogue native through RTS. The saved-
register `MOVEM.L (A7)+` at
`$2b562`; its external frame must be typed before caller execution continues.
The typed frame now restores all 15 registers and returns through `$2aac8`.
The caller-connected `$2aa68` prefix is native through XBIOS selector `$26`;
the next exact boundary is `TRAP #14` at `$2aa72`.
Selector `$26` now has a typed return; cleanup, RTS, and caller D7 setup are
native. The absolute `$2aa0c->$2a5aa` call and its argument pushes are native.
The next exact boundary is GEMDOS selector `$3d` `TRAP #1` at `$2a5b4`;
continue only with a generation-owned typed Fopen result.
The typed raw result is now admitted and stored exactly. Nonnegative results
load the exact literal arguments and stop before `JSR $2a5c2` at `$2aa28`.
Negative results reach the verified self-loop at `$2a632`. The positive callee
is the next executable evidence job.
The positive callee's argument setup is now native through GEMDOS selector
`$3f`; the next boundary is `TRAP #1` at `$2a5d0`. Its return and any bytes
written to `$7d42` require an explicit typed observation.
The raw Fread result is now typed; because original code does not branch on
it, no buffer observation is needed to reach Fclose. Execution now stops at
selector `$3e` `TRAP #1` `$2a5e6`; its return is the next boundary.
The Fclose return and the only four Fread bytes consumed before the next call
are now typed and native. The next exact boundary is `JSR $2b2be` at `$2aaec`.
The correctly mapped `$2b2be` setup is now native through its atomic D6/D7
stores. The next boundary is `MOVE.B (A4)+,D0` at `$2b2de`, source `$2c250`.
That first source byte and all four exact dispatch outcomes are now native.
The production facade now binds the prefix and token observations to the
resident hash-admitted `MILL22A.INF` bytes; synthetic token streams remain
confined to focused state-machine tests and cannot enter runtime memory.
The normal path now owns the typed pair at `$2c251..$2c252` and executes the
hash-bound D6/D7/D5 run, row and plane continuation at `$2b2f2..$2b321`.
It stops only when the next pair/token needs source bytes or at the routine
RTS. The repeated-byte, swapped-pair, and extended 14-bit run paths at
`$2b338`, `$2b376`, and `$2b3b8` are now native with typed payload bytes and
atomic destination effects. The next large Atari job begins with the caller
continuation after `$2b2be` returns. That caller and the `$2b448` clear/copy
prefix are now native through `$2b486`, with the 96-byte original source and
all 32 longword writes hash-bound and atomic. The 16-by-3 palette arithmetic
loop is now native with typed existing destination words and atomic byte/word
effects through `TRAP #14` `$2b4ac`. Continue with a typed XBIOS selector-6
return and the largest deterministic portion of the following timing loop.
That raw return, stack cleanup, 20,000-iteration D0 delay and first D7 `DBF`
edge are now native through the corrected recurrence boundary at `$2b46e`.
All six recurrent palette passes, their typed selector-6 returns, delay loops,
D7 transitions, terminal selector-6 return, and local RTS `$2b4c6` are now
native. Continue with a typed RTS destination and the largest deterministic
caller continuation. The exact `$2ab04` return is now typed; its caller loads
the corrected D7 pointer `$2a634`, enters the reused `$2aa0c` helper, and stops
at the second configuration file's GEMDOS selector-`$3d` boundary. Continue
with that typed Fopen result and its bounded success/failure caller path. Both
are now native: negative returns reach `$2a632`, while nonnegative returns
atomically retain the handle and reach the existing `$2a5c2` Fread boundary.
Continue with the second-config Fread/Fclose results and prove its caller
return separately from the first configuration path. Typed Fread and Fclose
results now reach helper RTS `$2a5ec`; a typed `$2ab10` return owns the exact
caller through XBIOS selector `$26` at `$2ab24`. Continue with that typed
firmware result and the deterministic cleanup/RTS continuation. The raw result,
cleanup, RTS `$2ab28`, typed return `$77042`, and staged-PRG caller are now
native through GEMDOS selector `$3d` at `$77056`. Continue with that typed
service result and its exact success/failure branches. Both branches are now
native: failure stops at `$77060`, while success reaches GEMDOS selector `$3f`
at `$77074` with count `$20000` and buffer `$11e00`. Continue from the typed
Fread result. Typed Fread and Fclose results now reach the two atomic constant
writes and local RTS `$770ba`. Its typed even 24-bit stack destination is now
the terminal preservation boundary: the staged target was entered by `JMP`,
so there is no statically encoded caller continuation to recover. Do not infer
filesystem, firmware, or wall-clock effects from static bytes.

This is the ordered execution queue for the completion plan. It is a
preservation tracker, not a list of compatibility claims. A task moves only
when its acceptance evidence is committed; a missing capture is a boundary,
not permission to synthesize a result.

The queue is deliberately organized by its contribution to the first genuine,
playable vertical slice. Presentation, packaging, and broad platform work stay
behind the first proven input-to-frame-to-state loop.

## 2026-09-04 native execution checkpoint

### Later native batch

The English Millennium DOS title path now continues past its BIOS palette
loops through the exact DOS memory allocation sequence and the complete
`title.lib` loader helper. It opens the hash-verified original leaf, admits
nine bounded reads into observed allocated segments, closes it, applies the
recovered header relocation fields, and stops at `$0f6b` for mode 1 or
`$0f6a` for other modes. Raw DOS returns and failure paths remain explicit.

The clean Deuteros Amiga title path now executes both caller-connected
`$41bb4` paired dispatches from genuine ADF bytes. The second `$004e` route
uses the proven fixed `$4128e` descriptor, consumes its complete 229-byte
payload, observes only the 64 genuinely preexisting final row/plane words,
and atomically completes all 320 merge writes through `$4051e`. Decoded sparse
memory is not yet a renderer or parity claim.

The Millennium Atari Equinox path now crosses XBIOS selectors 2, 3, 4, Line-A,
selector `$15`, and selector 6 through typed observations and deterministic
local continuations. It stops at `JSR $2b55a`. A previously considered byte
sequence maps to `$2b57c` under the exact admitted `$2a500` load, so Eon now
rejects that 22-byte-shifted candidate instead of executing it.

Player-visible game-text presentation now has a declarative, source-parity
tested map with complete leaf SHA-256, offset, and length. It covers ten
English DOS launcher strings plus all 41 celestial labels from both the exact
English and Spanish static-data leaves: 92 source-bound definitions and 51
catalog messages. Every message is present in all shipped PO catalogs for both
presentation modes. This is infrastructure and current-string coverage, not a
claim that unrecovered game text has already been extracted.

The mechanical disassembly inventory is complete for all eight declared
releases: 19 code images, 21 admitted ranges, and 764,867 source bytes are
accounted for. This is byte coverage, not semantic recovery or parity.

The active English Millennium DOS path now owns the selected sound-driver
load in a bounded native paragraph arena, admits the exact `TITLES.EXE` child,
executes its entry and initialization prefixes, consumes one explicitly typed
private-`INT 91h` function-0 result, and reaches the second function-4 request.
It still requires an observed result before executing either selected title
callee beyond that request; no DOS PSP, parent `EXEC` return, display result,
or game-state transition is inferred.

The normal SDL and CLI startup route no longer depends on a test-only injected
child code segment after the original sound selection. Choices `0`, `1`, and
`2` now bind the exact supplied `SIBM.DRV`, `SSBL.DRV`, and `SCVX.DRV` leaves
by hash and enter their shared bounded file-loader prefix. Choice `0` was
previously a dead end despite being a literal original option. Each choice
creates a labelled Eon compatibility process at `$e000`, then lets the
existing deterministic loader consume only the selected hash-admitted leaf.
That segment is engine-owned state, never an original allocation claim; the
first original-dependent parent-stack result and every later driver/device
ABI boundary remain blocked pending evidence. This does not claim SIBM
initialization or audio playback.

The clean Deuteros Amiga path now accumulates the zero/zero route and all three
remaining deterministic non-negative `$1fbe6` planar routes into a sparse
320x200 four-plane Original surface. SDL presents only pixels whose four source
plane bytes are admitted; every other pixel remains invalid and transparent.
This is not a complete title frame and supplies no title-input semantics.

The Millennium Atari Equinox path now materializes the complete exact
`MILENIUM.TOS` TEXT+DATA+BSS image at an Eon-owned address, applies all 227
relocations, reads the exact `MILL22A.inf` payload through a narrow read-only
GEMDOS compatibility service, and follows its native JSR/JMP chain to
`$2aa88`. It stops before `MOVE SR,D0`; the original status/privilege value and
the resulting branch remain explicit observations.

The corrected Millennium Amiga first stage is native from `$41000` through
its register-save/vector setup, two typed frame-complete ILLEGAL exception
entries. Vector-9 tracing decrypts and executes the exact ADDX plus ten
unconditional branch steps through `$411d8`, then the deterministic LEA,
MOVEQ, table-word and ADD register prefix. A typed group-0 frame admits the
24-bit bus-error route, and a typed custom-chip/ExecBase observation advances
the deterministic setup. The typed graphics.library returns at `$41780`,
`$417b4`, `$417c6`, and `$417d2` now commit the bounded bitmap/pointer setup;
the exact custom-chip effects at `$42546` and `$4254c` are retained as typed
records. Execution stops before `$42552 -> $41d4a`, whose device-call result
and visible display remain unproven.

The English DOS native-process adapter now connects only after a typed
`2200AD.EXE` child-entry observation at the title-to-game boundary. Its
first and second `$0129` returns require explicit, strictly ordered AX
observations and are visible through copy-only checkpoints. The API does not
provide those values or establish original-process reachability; rank 1 still
requires a genuine title/EXEC capture.

| Rank | Work package | Exact current evidence | Required acceptance evidence | Status / boundary |
| --- | --- | --- | --- | --- |
| 1 | Millennium DOS: capture the launcher/title/`2200AD.EXE` handoff and private DOS ABI | English DOS release `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`; CLI-validated diagnostics-only title-init v2 profile binds the `MILL.COM:0x02cf` driver-load, setup-site `0x0209`/actual-`INT` `0x020c`, `TITLES.EXE:0x0127` request, and two raw returns at `$0129`; v6 additionally binds `svga_s3`/`ega` machine-profile selection to its exact config, while the genuine EGA diagnostic still requested `mcga.bin` and hit the console-capped `INT 6` boundary; v7 records IVT `INT 91h` endpoint `087e:0000`, v8 records a normal-core transfer to it, v9 binds the first raw caller re-entry (`AX=$0101`, `FLAGS=$7202`), v11 terminates the host recorder only after the complete twice-observed `INT 6` diagnostic receipt matches byte-for-byte, and v12 independently repeats an immediate predecessor at `f000:ca60` outside the recognised original-image map. The verified v13 no-input preflight ends at the same eight-record `INT 6` receipt (SHA-256 `8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`) with no host-key receipt or title poll; [read-only physical capture runner](MILLENNIUM_DOS_CAPTURE.md#safe-capture-procedure); [external recorder status](MILLENNIUM_DOS_DOSBOX_X_RECORDER.md#prototype-status) | Hash-bound genuine trace of each interrupt, EXEC/far-return and driver result through one navigable state | The title-init prefix and physical capture route have strict contracts. The v12 predecessor is an emulator callback boundary, and the v13 preflight has no physical receipt; neither is guest-code input, rendering, audio, EXEC, or game-state evidence. |
| 2 | Millennium DOS: admit the GX startup bridge | Same release; `2200GX.EXE` SHA-256 `093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb`; caller and record bounds in `PRESERVATION.md#millennium-dos-gx-startup-record-boundary`; strict ten-record admission builds a call-free overlay state after independently pinned trace validation; an engine-owned successor now requires the exact active English release to have independently reached title handoff and publishes only a terminal value checkpoint | A genuine hash-bound ten-record capture plus the rank-1 driver/title handoff; active admission must end at the second private-INT boundary and reject any missing/reordered/altered record without changing the prior session | Runtime ownership, byte lifetime, state, rejection, reset and revocation contracts are implemented. The current English path still stops before title handoff, so the successor remains unavailable; no genuine GX trace, frame, input or gameplay is implied |
| 3 | Deuteros Amiga: capture title initialization and display ABI | English Amiga release `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`; clean disk 1 `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`; hash-bound `$389e2` caller establishes start disk offset `$14000`, buffer `$26cc0`, and `$5800` total; the `$208c0` loop plus `$20902` vector helper gate four ordered `-$1c8` calls, each followed by `io_Error` at IORequest+$1f; both selector copy spans are checked inside that completed read buffer; [capture status and latest receipt](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md) | Capture bitplane/palette/audio checkpoints and compare them with native Original output; verify a subsequent guest action changes state | Focused direct-media CTest passes against the supplied archive. The gate admits request order/status and copy-address containment only; active disk identity and returned payload remain unobserved. No captured bitplanes, frame hash, audio, selector meaning, or validated actionable state exists. |
| 4 | Deuteros Amiga: define and admit title-display capture evidence | Title-stage/main-stage recovery-map entries; strict v4 24-record contract and v5 artifact contract; an engine-owned checkpoint now reopens, rehashes and revalidates all evidence at consumption time; separately, the native runtime publishes bounded Original frames from recovered title/main-stage code | A genuine, write-protected v5 capture satisfying every event and artifact checkpoint; then compare captured display and ABI results against the native runtime | The v16 capture validates a bounded register-write receipt only; it does not contain the complete v5 display artifact. Immutable v4/v5 checkpoint ownership, state transition, rejection, reset and revocation are implemented. Full capture-backed display validation and parity remain open. |
| 5 | Millennium DOS: recover first actionable state and controls | Hash-identified `TITLE.LIB`, `GX.LIB`, video-driver and title-flow profiles; the typed post-overlay continuation covers `$d39d..$d412`, fifteen exact call returns, explicit AL/runtime-byte branches, poll cycle and terminal dispatch-call boundaries; explicit `$d40a → $76f1` observations can create owned typed index `5` → `$7415`, index `6` → `$7521`, and index `9` → `$7384` sessions | Replay of a real input → canonical state → frame/audio checkpoint | F6/F7/F10 runtime ownership, typed forwarding, copy-only checkpoints and revocation are coded and real-media tested at independent recovery boundaries. Positive active dispatch remains unavailable until genuine predecessor evidence exists. No dispatch resolution, call return, input, handler meaning or frame is invented |
| 6 | Deuteros Amiga: recover first actionable state and controls | Clean ADF loader, bundle, VM, opening-frame and title-stage profiles; bounded native main-stage execution publishes recurring Original frames at the recovered visibility boundary; the installed `$224cc` vector target has a hash-bound 30-byte prologue at ADF `+$7ccc`, and its statically reached `$22816..$229e7` span is hash-bound at ADF `+$8016`; active-runtime worker entry validates generation, PC, owned nested-BSR stack return, vector, and resident code before applying translated writes to owned software memory; v19 visible capture records held-Fire poll outcome at `$21866` (Z=1, BNE falls through), the title-stage `$218cc` path, display setup, and two `$1fbe6` selector reads with `$1f98c/$1f98e=$00`; a host-delivered `KEY_1` is followed by a selector-code sample at A0=`$1eed5`; a hash-bound native continuation now executes `$21870..$21892` with typed lazy reads of `$2171e/$21720/$21696`, recording exact branch PCs and SR | Replay of a real input → canonical state → bitplane/palette/audio checkpoint | The capture links held Fire to the poll condition and delivers `KEY_1`; static fallthrough predicts the `$21720` latch store but no guest-memory write is recorded. The selector was visible and title display writes were recorded, but the screen became black after the key. A0=`$1eed5` lies in a zero-filled source span selected by the hash-bound main-stage load profile, so it does not identify a prompt string; no visible disk prompt, main-stage handoff, accepted game control, or canonical state change is proven. The added static continuation does not prove it is reached by the runtime. Custom-register outputs remain intents and do not establish device/audio effects or cadence. Capture-backed state equivalence, interrupt cadence and audio timing remain open. |
| 7 | Millennium DOS: establish video/audio device contracts for that slice | `EGA640.BIN`, `MCGA.BIN`, `SSBL.DRV`, `SCVX.DRV`, and VOC source-byte profiles | Captured calls plus pixel/sample hash comparison; no guessed hardware behavior | Depends on rank 1 |
| 8 | Canonical game-core subsystems for the two proven slices | Only rule paths that have a caller-connected code proof or trace | Deterministic long-run replays, including state transitions and edge cases | Cannot begin from strings, assets, or inferred genre mechanics |
| 9 | Remaining Amiga/Atari ST execution adapters | Millennium Amiga `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400`, Millennium Atari `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01`, Deuteros Atari `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653`; the latter has a hash-gated static bootstrap checkpoint and a non-admitted Hatari `Floprd` shape cross-check | Per-release recorder-backed bootstrap/device traces and explicit shared/divergent replay checks | Deferred until a playable vertical slice establishes the right core boundary; ordinary emulator output remains diagnostics-only |
| 10 | Final UX, localization, packages, and release audit | Existing card route, i18n catalogues, CI package recipes, and preservation contracts | Real-session menu/CLI equivalence, 20-language checks, clean-package scans and end-to-end replay | Continuous maintenance only; never substitutes runtime recovery or authorizes a release |

### 2026-10-08 Millennium DOS title-entry scenario follow-up

The context-sensitive analyzer now follows the two recorded `TITLES.EXE`
`INT 91h` AX values (`$0101`, `$0000`) as an explicit static scenario. It
reaches the candidate BIOS `INT 10h` at `$046d` after the `$044c` initializer;
the call sets AH/AL to `$10`, while the other request registers and BIOS
effects remain unmodeled. `$0134`/`$125c` are not reached under this scenario.
The report is external at
`/home/trv2/.cache/project-eon-tools/millennium-title-context-reachability-20261008-v10.tsv`
(SHA-256 `6e9afca1fead092f539def88698f9d2a9d47f2e1695cb74c4d0dd45bf0cad252`).
This narrows the next capture to BIOS input/output at `$046d`; it does not
establish that this caller route ran or that the title/game is playable.

A separate hypothetical BIOS-return scenario now follows only `$046d` to
`$046f`, clearing tracked register/flag facts and leaving BIOS effects opaque.
It reaches `$1ad6` after the candidate `$044c` return, then stops because the
DS-relative write at `$0107` cannot be proven disjoint from the live return
word at `$1bb0`. The updated external report is
`/home/trv2/.cache/project-eon-tools/millennium-title-context-reachability-20261008-v13.tsv`
(SHA-256 `c9ecba7a83791125ad7237ef3d7ec639af5d1ff776366dbc29db696a240710f9`).
The scenario does not establish a BIOS return or runtime execution. The next
useful DOS observation is the exact DS/stack relation at `$1ad6`, along with
the BIOS return at `$046d`; no memory write is modeled from this analysis.

The analyzer now distinguishes a source `PUSH` from a register write and can
optionally preserve the known DS value across the exact wrapper pop at
`$012d`, subject to an explicit saved-stack assumption. Its DS-absolute alias
proof now uses that fact only after proving the access disjoint from active
return words. With additional explicit BIOS-return/DS and DOS-return scenarios,
the path passes `$1ad6` without applying its store and reaches the next DOS
`INT 21h` at `$1b2d`, with AH=`$48` and BX=`$fa00`. The allocator call remains
opaque; no return value, memory-manager effect, runtime route or playable state
is established. The report is
`/home/trv2/.cache/project-eon-tools/millennium-title-context-reachability-20261008-v16.tsv`
(SHA-256 `1f4750d18d5df57e8bc9161808345ebad139fd0462f55ae88fa76f286626c95a`).
The next DOS observation should bind the `$1b2d` return and the resulting
allocation segment before following any memory writes.

The English DOS startup continuation now records a typed BIOS return at the
hash-checked palette `INT 10h` site `$0476` only when the raw return CS:IP is the
expected child segment:`$0478`. Its receipt preserves observed AX/FLAGS and
sequence ordering without applying BIOS/palette effects or continuing the
loop. Focused direct-media coverage exercises the accepted boundary and rejects
wrong INT site, interrupt number, return PC/segment, stale order, and duplicates;
this does not replace rank 1's genuine capture requirement.

The same direct-media `TITLES.EXE` inspection has narrowed the next DOS capture
after `$1b2d` to the caller's ordered allocation sequence: `$fa00` paragraphs,
free that returned segment, `$1000` paragraphs, open/seek-end/close of the
CS-relative NUL-terminated `title.lib` at `$0e4e` to compute its rounded size,
allocate that size, then one paragraph. The exact ten-byte name at file offset
`$0d4e` hashes to
`62bfc3e4275f23097edf305a3e1144d3eac79b4a4c75cc35cfbb3eb0b9255aed`. The
file-size helper `$1af6..$1b1e` hashes to
`4fd3a9694c9ea36d7baf33607ed0b70ac764bb1f27bb6b686c3401bce5ef6b3d`; the
caller span `$1b28..$1b7f` hashes to
`b014a155ccc31c33a5f74e530634a09abfb00f5ec2de3686307dbf4d50ffc406`.
This is a capture target, not a runtime result: no allocator/file return,
title-library result, driver installation, or playable state is admitted yet. Preserve
the precise DOS return order and carry outcomes before extending native runtime
execution.

### 2026-09-03 native media-admission update

The English Millennium DOS native session now admits a read-only catalogue of
the fourteen executable-named original VOC leaves after exact media
verification and decoding. It retains filename/hash/sample-rate/sample-count
facts only and discards PCM bytes. This closes a resource-admission task, not
an audio-playback task: event-to-index mapping, driver ABI and timing remain
outside the recovered engine boundary.

### 2026-09-03 split-container Deuteros admission

A read-only scan of the user-supplied `~/.projecteon` collection found real
single-disk ZIP containers whose inner disk hashes match existing Deuteros
parser leaves but whose outer ZIP hashes are not the one combined-release
archive currently represented by `ReleaseArchive`. In particular, the Atari
ST pair contains the existing first-stage leaf
`aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee`
(Replicants disk 1) and the existing killer-boot leaf
`5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193`
(clean disk 2). The clean Amiga disk 1 likewise contains the existing
`6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`
leaf.

The scanner now admits two declarative, ordered split-container sets. Each
required ZIP occurs exactly once; its outer hash/size and its specific inner
disk hash/size are independently verified before an ordered-set digest binds
the logical release identity. The pair is rejected when incomplete,
ambiguous, altered, or mixed, and runtime admission reopens every selected
container instead of trusting scan-time state. No disk is copied, unpacked,
or substituted.

The admitted Atari ST set is a bootstrap-only native session: it proves static
loader facts but not XBIOS/raw-read results, title, input, frame, audio or
gameplay. The admitted clean Amiga pair supplies both hash-addressed ADFs to
the existing native recovered-opening session, which now reaches `READY` from
the user collection. That is not parity: title-display ABI, player controls,
and game state remain governed by ranks 3–6.

### 2026-08-31 capture-route update

The rank-1 v13 Millennium DOS operator-visible no-input capture is independently
receipt-verified. It ends at the same eight-record `INT 6` diagnostic receipt
(SHA-256 `8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`)
with no host-key receipt and no title-input poll. The rank-3 Deuteros Amiga v9
15-second realtime no-input preflight is likewise receipt-verified: its 256
raw-PC records have SHA-256
`fd52c57cb44a402fc7b9ddbeea0e8d1867dd09e8851f586ef515d6aba8698c39` and
zero host-delivery links. These are capture-route/no-input facts only. They
do not establish guest input, title execution, display, audio, ABI results,
or gameplay, and the next operator-led capture remains the required P0
evidence.

### 2026-09-01 Millennium execution-history update

Two independently verified `v14-normal-core-history` captures of the same
write-protected English DOS archive retain an identical 16-entry normal-core
sidecar (SHA-256
`248969bc16cfd773f64140ff3e314f6cd465ad7514de0868d24803b399bf4dbb`). It
records zero-byte fetches at `0e70:18e4` through `0e70:1900`, followed by the
already known DOSBox-X default-callback opcode at `f000:ca60`. This isolates
the current P0 boundary to the unproven transfer into the `0e70` context. Two
later V20 transfer-observer receipts independently retain the same one-step
normal-core adjacency, `0000:0001 ca00f00e` to `0e70:fffe 00000000` (sidecar
SHA-256 `b4434953ad218801db9b3966d9d2be226b0261c7d4a87316c58feb8599472236`).
The first independently verified V21 capture reached the existing eight-record
`INT 6` boundary in 0.61 seconds and retained `int93_installation=absent`.
Thus neither reviewed original installer site executes on that route before the
known stop. The observer remains prepared to retain one verified original
`INT 21h/AH=25h` vector-$93 transaction at either site if a later capture
reaches it. Absence is explicit rather than synthesized and demonstrates no
installation, handler, or dispatch.
On 2026-09-01, the tightened V22 `diagnostic-no-input` runner independently
reproduced that same stop: its eight raw results retain SHA-256
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`, with
`host_input_receipt=absent`, `host_input_observed_during_capture=false`, and
`int93_installation=absent`. The V22 receipt is a procedure check as well: it
proves the runner rejected host input for the declared no-input diagnostic. It
does not prove guest polling, key acceptance, a private ABI result, rendering,
audio, title execution, or gameplay.
It does not authorize a callback bypass, guest-memory repair, inferred mapping,
or a gameplay claim: the exact original vector installation/dispatch path and
a navigable trace remain the rank-1 missing evidence.

On 2026-09-01, a second independently receipt-verified Deuteros Amiga v9
realtime run lasted 120 seconds. Its 384 raw-PC records (SHA-256
`d8732ec5aab06123147688b19b8bc750b0ee6ca1f9a03cdc68a5787271a1e5b9`)
remain capped at three known bootstrap sites and retain zero host-delivery
links. It is additional no-input reachability evidence only, not a guest
input, title, display, audio, ABI, or gameplay claim.

### 2026-09-02 recorder restoration state

The external Millennium DOS v21 observer reconstruction is now
`OBSERVER_FIX_REQUIRED`. Its v3 delta was independently reviewed for
post-`RealSetVec` observation, bounded host output and absence of
guest/input/scheduler changes, but an explicit read-only experimental run
proved the build contains only that delta on vanilla DOSBox-X. It lacks the
older normal-core/default-callback recorder hooks and reaches the known
unhandled-`INT 6` console loop before it produces the legacy receipt streams
or a v21 installer record. The next work item is therefore to recover and
review the complete base-recorder patch provenance, then integrate v3 on top
of it; pinning, locator admission, capture recovery and hash substitution are
all forbidden until then. See
[`CAPTURE_RECORDER_RESTORATION.md`](CAPTURE_RECORDER_RESTORATION.md) for exact
candidate provenance and the retained negative observation.

## Operating rule

The Deuteros title chain now advances from its dedicated fail-closed
OpenLibrary return boundary through the proven nonzero local call chain and
stops at `$1eda6`. A typed genuine observation can now supply the external
display-base value read from `$12ff4`; the engine advances the local
palette/clear plan and stops at `$40498`. The next increment must preserve the
custom-chip write boundary—do not substitute host hardware or claim display
output from the bounded plan. Four exact custom-chip observations now advance
the local callback-registration plan to `$1f04a`; the next increment requires
an explicit return observation for its Exec vector `-$1ce` and must not infer
the service or callback semantics. That typed return is now admitted and the
local RTS reaches `$404b6`; the next continuation must resolve or explicitly
observe the `$206d4` boundary rather than assigning it invented behavior.
The `$206d4` prefix and its explicit `-$126` return now advance the first local
descriptor plan to `$20708`. Continue only with an exact `-$162` observation;
do not infer either Exec service or manufacture its result.
The exact `-$162` return and its local pointer/link setup now reach `$2072e`.
Continue with a typed `-$1bc` return only; its service and branch result remain
unresolved.
The `-$1bc` return is now an explicit branch boundary: nonzero stops at the
original loop, while zero reaches `$20776` with the earlier observed D0 value.
Continue only through an exact second `-$162` return at `$2077a`.
That return and its local second-pointer setup now reach `$2079c`; the next
required boundary is the exact `-$1bc` return from `$207a0`.
That final return now completes the hash-proven `$206d4` routine and reaches
`$404bc`. Continue by recovering `$206be`; do not infer its returned D0 or
pointer effect.
The fully local `$206be` controller transfer and `$403e6` literal pointer seed
now advance to `$404ce`. The next boundary is the `$403f4` service batch; each
opaque callee must return through explicit evidence before later setup runs.
The first batch graphics return and `$20510` literal prefix now reach the
runtime read at `$2052a`. Continue with an exact typed `$20276` observation;
do not infer the word or claim the graphics vector copied pixels.
The explicit `$20276` word now completes `$20510` and reaches `$1f37a`.
Recover or explicitly bridge nested target `$20094` before advancing the
remaining `$403f4` batch; do not infer its graphics/service behavior.
The first `$20094` graphics return now advances to `$200b0`. Continue only
with the exact `-$198` return using the same observed library base; do not
invent its D0 byte or descriptor effect.
The formerly listed `$200f4` graphics boundary is crossed through its exact
same-library `-$1a4` return, and the caller-connected chain now includes the
tail, command path, and both `$41bb4` merges. The first tail chain now commits
only hash-proven local effects to private native memory: the `$20276->$2027c`
word, `$20094`'s status/pointer/descriptor stores, the four observed tail
words and `$ffff` literals, both bounded selection results, and `$404da`'s
two observed table longwords plus `$204c8`'s descriptor stores. Graphics and
Exec vectors remain typed external boundaries; none are emulated. From authoritative boundary
`$4051e`, the next deterministic service prefix commits seven exact effects.
Its typed `$20e6a -> $1fb9a` return now reloads the owned selector, adds
`$00a0`; its typed `$20e7a -> $1ff08` return then selects immutable table
longword `$127a3980` and commits it to `$1378e`. Its typed
`$20e96 -> $22bca` return now enters `$20ba8`; ordered observations of
`$13008/$202bc` resolve the first loop branch. The clear-carry route stops
before `$20bd6 -> $41a68` (D0 `$0048`, D1 `$0010`); carry/zero skips at
`$20bea`. Exact typed `$41a68` returns and local skip routes now complete all
eight bounded iterations and return through `$20bf0`. The typed
`$20bf4 -> $1f9b8` return and exact three-read pointer chain now select and
atomically write `$00b0/$00bd` to `$417a2`. Selector `$005c` follows the
direct `$41c32` route, and both distinct `$74576/$76e24` streams are now
hash-bound, fully decoded, and atomically written through the typed `$1f168`
destination. Exact typed `$20c4c->$41ad2` returns now complete the bounded
12-entry descriptor-bit loop and atomically store `$00bd` at `$416b4`.
The mutable `$20a10` byte now has an exact typed observation; its low-byte
addition atomically adjusts `$416b4`, and D0 becomes `$004b`. Continue at
`$20c7a->$41bb4` by proving the adjusted descriptor route selected by that
observed byte; do not reuse the unadjusted `$00bd` stream implicitly.
The genuine `$03` observation now selects a separately hash-bound `$00c0`
descriptor and completes its 68-by-168 decode as 22,848 atomic byte writes
after one typed destination-pointer read. Caller `$20c80` now owns the typed
`$19d1e` pointer and exact zero branch. The qualifying nonzero object gate
now owns typed `$ee`/`$f0` bytes and immutable table loads through the first
`$20ca8->$41ad2` call. Both helper returns and the second table pair are now
owned through local RTS `$20cb8` without assigning helper effects. The typed
stack frame selects only caller `$40530`; its repeated `$20ba8` local-service
call now consumes two ordered runtime words, all eight typed `$41a68`
returns, exact counter effects and DBF iterations, then returns through
`$20bf0` to known caller address `$40536`. The caller now loads literal A0
`$20cfe`, admits its typed return, reads typed long `$12fe4`, shifts it right
three bits, and atomically stores the low word at `$1f42a`. A typed return from
`$4054c->$37180` now admits the exact longword copy `$1378e->$1c26c` and a
typed `$4040e` mode selects `$40566->$36a8c` or `$4056e->$1fb9a`. Continue
from that selected external call; do not
treat the sparse decoded memory as a renderer surface.
Both selected returns and their join at `$40574` are exposed through every
runtime facade with replay and revocation checks. The following `$222c0` and
`$23e4e` calls now require typed returns, after which the exact word-change and
60,000-count gate is recovered atomically. The optional `$405b6->$4069a`
return is typed, clears `$40410` atomically, and joins the not-due route at
`$405c6`. A nonzero typed `$1bf36` byte now admits `$405d0->$1f9a4` returning
after its exact inline byte stream at `$405de`, followed by typed word `$22a0`
and `$405e4->$1fe88` return. Three more typed service returns and their exact
conditional word reads recover the caller through `$4062c`: rejected
conditions join at `$40638`, while the selected tail stops at external jump
`$37f56`. At `$40638`, the first `$1f238` return is typed. A non-`$43` low
byte loops to `$40574`; `$43` atomically XORs word `$1bf36` with `$0101`,
selects exact colour word `$00f0` or `$0f00`. The repeated `$40662->$1f238`
returns are now observation-driven: each iteration atomically writes the
selected word to `$dff180`, non-`$43` repeats at `$40656`, and `$43` exits
through `$40670` to the recovered `$40574` loop.
The alternate selected gate now types returns from `$3880a` and `$204fa`,
then atomically copies exactly `$9392` hash-bound original-stage bytes from
`$13006` to `$66000`, overlaying any earlier admitted source mutations from
the sparse runtime ledger. No caller-supplied replacement bytes are accepted.
Local `$37f7a->$37f9a` is now caller-connected through its initial service,
the five-call equal or seven-call unequal Exec route, typed comparison and
the exact profile-two bootstrap writes. Its `$12800` jump now follows the
opcode-validated `$12a4e` dispatcher, `$12b44->$12b1c` profile route and
atomically reloads all `$4200` genuine main-stage bytes at `$20000`, stopping
at `$21734`. Continue with a re-entry session that owns the exact incoming A1
controller and typed D0 stores before the first main-stage service boundary;
do not assign a gameplay meaning to profile two. That entry prefix is now
atomic; typed Exec `-$96` and `-$9c` returns now advance through literal
`$7fff0`, followed by ordered typed returns from `$20068` and `$2013a`. The
recovered pointer copies reach `$20510/$20c20`. Ordered `$22a5a/$22bea/$22bea`
returns, four literal custom-register writes, and both typed pointer chains now
reach `$2197a/$2197e`. The `$217d8->$20994` edge now follows its exact
hash-bound local prefix, clears A1, reloads the Exec base from `$0004`, and
types the `$2099e` vector `-$126` return, atomically applies its five exact
big-endian writes, and stops at the `$209ca` vector `-$162` boundary (return
`$209ce`). Its return is now typed; two exact longword writes commit atomically
before the next boundary at `$209f0`, vector `-$1bc`, return `$209f4`.
That return now preserves both exact branches: nonzero terminates in the
`$209fa` spin, while zero atomically writes `$2094c/$2093a`, returns to
`$217de`, loads D1 with `$20000`, and reaches a typed `$217e4` stateful-bit
observation on `$bfe001`. That observation now atomically sets raw bit 1,
validates the resident `$21704` source word, mirrors it through local `$21926`
at `$21704/$21706`, then accepts the typed return from `$22a5a`. That return
atomically clears words `$21720` and `$2171e` and sets `$210f2` to one. The
hash-bound `$21276` native-body candidate requires an owned `$32a24` record;
the caller's typed re-entry D0 supplies that selector through `$21704` and
`$21926`. Selector zero now drives the exact `$21708` table entry, bounded ADF
probe/body transfer, and observed released `$2196e` hardware retry into owned
`$2ad24/$32a24` memory. The earlier typed OpenLibrary return now owns `$12fec`
only through its exact post-return store, so `$21276` can consume both owned
sources. Ordered typed returns at `$21310` and `$2132a` now continue to the
local `$2132e->$20888` call and its four native stores. Continue from the
typed `$208b0` ExecBase read and `-$a8` return, then the owned `$2126a`
conditional: `$2133c->$22330` when nonzero or local `$21342` when zero. The
nonzero branch now executes the complete native optional-resource initializer,
bounded maximum scan and 15-record relocation transaction before joining the
same `$21342` record loop.
The zero branch now executes the complete native record construction and
first processing pass, typed buffer selection, full buffer clear, and initial
no-draw record walk and owned view selection through `$216ee/-$de`. Continue
with the now-typed view return and resumable hardware wait into the next
processing pass. Its owned `$214aa` local interpreter now executes sound
descriptor staging and the original random routine; the real initial pass
reaches the second record's palette boundary `$214ee`. The native prefix,
ordered library-return admission and saved-position scheduler resumption now
continue through the second buffer/view pass to the outer `$21822` gate.
The ordered outer port samples and native latch/control logic now re-enter
the scheduler and select the first sprite. Native opaque rendering now writes
its exact bitplanes and covers all 142 original resource bitmaps in tests.
The masked `$20cc6/$20fb2` cache/merge path now renders the following real
buffer and is cross-checked on all 142 original bitmaps. Native saved-scanline
save/restore now covers those same bitmaps, including bottom-clipped restores
with the original source-plane stride. Alternate-resource selector `$fe` now
executes the bounded `$20580` command classes directly against owned native
memory inside the atomic frame transaction, including its original global
video cursors, selector tables, embedded glyph rows, and four bitplane writes.
The distinct `$218cc`, `$21892` and `$21982` cleanup/transition
prefixes now execute native selector decisions and software sound reset;
the nonzero-resource fade now performs its 16-color RGB4 steps, ordered
dual palette calls and owned 32-step counter. Its two final native buffer
clears, exact fade return, `$224a2` cleanup writes and software sound reset
are implemented. Ordered asynchronous counter observations now continue both
wait routes to `$208ba`, retaining the original snapshot at `$12fe4` where
required. The `$208ba` service now continues through its typed Exec return,
then either the raw restart wait to `$217f6` or the `$20a74` request writes,
typed return and native selector increment. The reached `$21926` resource
loader and `$21a4c` special transition are implemented. The complete
`$219f8` boot-disk check is hash-gated and has a bounded native
success/mismatch/message/input state machine. It is now connected through the
production launcher, revocation-aware host, native-session state gate, and
coordinator with explicit system/data selection and atomic memory/session
publication. Direct coordinator/facade tests reject unknown media, wrong
boundaries, out-of-order palette/input callbacks, and source revocation
without changing the native-memory checksum. Recurring resource
0/1 loads now verify the owned table, transfer original bytes in native
memory and follow the retained primary return into fresh loop preparation;
branch-entered unknown returns remain at `$21978`.
The state-two `$21a4c` transition now conditionally clears its fixed buffer,
admits both ordered Exec returns and publishes handoff metadata before
reaching `$12800`. The `$12800` re-entry now passes five typed Exec
returns and native request setup, reaching the original error spin or the
`$13000`/`$12932` caller boundary. Continue those routines and profile dispatch.
The configured bootstrap route now executes `$12932` request initialization,
its ordered Exec return and the caller's `$12a92` return, then dispatches via
the owned profile table at `$12a36`. Profiles 0–4 now execute their fixed
native load bodies and prepare the `$12ad2` read request;
profile 5 selects its distinct `$12b46` body.
The exact profile-five source block is now independently parser-gated across
all 204 bytes at `$12b46..$12c11`, with its four service sites, two literal
read ranges, mutable `$66000->$13006` copy, and common `$12ada` tail recorded
as preservation metadata. The native runtime now executes its helper and
four private phases, performs both hash-locked media transfers and the
owned-memory copy, then joins the three-return common cleanup and reaches the
real `$13000` program entry with profile 5 retained. Failed or reordered
returns roll back their pending effects.
The restored `$13000` JMP is now a distinct typed program-entry boundary for
both the initial profile-one title load and the later profile-five reload, not
the bootstrap graphics routine that previously occupied that address. Its
explicit local advance validates owned runtime bytes/checksum, applies only
the selected profile's controller/mode stores, resets stale title receipts,
and rejoins shared title startup at `$40456`. The initial handoff installs the
exact admitted title span into native memory atomically before publishing the
boundary. The SDL runtime advances either deterministic profile before its
next presentation query so production cannot remain stranded at the JMP.
The successful `$12ad2` path now transfers the exact parsed main/title range
into private native memory, clears the request length and issues command 9,
then admits the separate `$12aee`, `$12afc` and `$12b0a` returns. Final RTS
uses the retained loaded destination rather than the final service's D0.
The `$20000` entry now reaches native setup at `$21734` below, and the `$13000`
title reload rejoins shared title startup. Failed or partial device effects
and profile-five execution remain distinct unfinished paths.
The loaded `$20000` jump now enters `$21734` natively, stores its actual
controller/profile, establishes stack metadata and crosses the two ordered
Exec services. Native `$20068` now handles the graphics-library return and
its zero-result spin; success continues through `$2013a/$2008e` to the
typed graphics return and both pointer stores at `$21768/$21772`. The reached
`$2177c -> $22a5a` call and two `$22bea` audio calls are implemented. The loaded
title at `$13000` remains a separate program-entry boundary.
Native re-entry now performs that sound reset and both `$22bea` consumers,
retaining the four ordered DMA write intents and reaching `$2178e`.
Both full four-channel register receipts now cross the coordinator atomically
as metadata with owned-range hashes and a generation. The only recovered
production pair is sound zero with zero volume, so its fail-closed host render
is empty; sound-table entries 1/2 are not autoplayed.
The consumer implements all four descriptor channels, delayed silence,
countdown, period changes, tail selection and ROM-dependent random modes.
The `$2178e` custom-register writes and owned graphics pointer chains are
implemented below. Recover an explicit later audible consumer edge before
extending playback; raw register writes do not prove playback, and `$224cc`
cadence remains unknown.
Random audio modes require their original owned ROM byte at `$ff0000+index`.
The `$2178e` native prefix now writes the four raw custom-register values,
follows both owned graphics pointer chains and enters `$20994`. Three ordered
Exec returns build the task/port/device request, then reach either the original
error spin or the typed `$217e4` CIA observation. The CIA operation and
`$217f2 -> $21926` return to `$217f6` are implemented below. Graphics roots
must be produced by the earlier graphics setup; controlled unit fixtures are
not a runtime fallback.
The typed `$217e4` CIA sample now drives native byte BSET and the owned
`$21704` selector read; the bounded interpreter path also checks the resulting
MOVE.W condition codes and X/upper-SR preservation before its call boundary.
Its retained `$217f6` return is recognized by the existing resource-0/1
transfer transaction, which resets loop-local receipts and reaches fresh
`$21276` preparation. Unproven selectors and unknown return addresses remain
separate boundaries; no alternate media is substituted.
The unconfigured bootstrap `$12a76 -> $13000` route now crosses library
opening, both memory-query branches and the two initial graphics calls,
reaching `$1306c`. The first query requires the returned Z flag explicitly;
it is not inferred from D0. Continue native view/viewport structure setup
from `$1306c`, followed by the remaining graphics allocation/assembly calls.
Do not confuse this bootstrap routine with the separately loaded title whose
destination is also `$13000`.
Both native view/viewport layout paths now continue through their ordered
bitmap, raster-port, viewport-build and merge returns. The first view alone
has the original load call; the second returns to `$12a7a -> $1330e`.
The caller's structures, shared 20-word palette and four plane pointers are
owned writes. The `$1330e` routine now performs both request-service calls
and transactionally transfers the hash-locked `$b000/$1800` original-media
span to `$1fe00`. Its ByteKiller routine is now translated natively with exact
input/output/checksum bounds; the resulting palette and 320x200x4 image are
written to owned memory, deinterleaved into the four original planes, and
continued through the typed `LoadRGB4` return to `$12a7e`. Continue the now
reached configured bootstrap/profile dispatch. The accepted palette return
now atomically publishes the hash-locked 320x200 Original frame through the
coordinator, host and SDL renderer, superseding the stale opening preview.
Library-generated list pointers are still
not inferred from return values, and the final `$6e0` decoded bytes remain
unlabelled until a consumer proves their role.
The unconfigured `$13000` graphics initialization remains separate. Opening
acquisition now retains the genuine 24-byte bootstrap profile table in owned
memory from its boot-track source. Tests consume that production checkpoint,
without privately injecting table bytes. Remaining mutable bootstrap memory
must still be produced by its reached native paths, not reseeded from disk.
The restart route
at `$217f6` now executes sound/flag reset and re-enters `$21276` using fresh
per-loop continuation state and effect identifiers. Continue verifying its
new ordered service returns without reusing previous observations.
The owned recurring main-stage framebuffer and accepted RGB4 palette now pass
through the coordinator, revocation-aware host and SDL renderer. A completed
buffer remains pending until the original LoadView/bit-5 visibility boundary;
tests cover four successive publications and genuine planar hashes. The
reached outer control/input routes now have typed port, counter, and service
boundaries; do not create those observations in the host. The initial
`$21782/$21788` `$22bea` calls are already connected to the native descriptor
consumer. Opcode `$0b` still stages records outside that initialization path,
but the asynchronous `$224cc` caller cadence and channel state at each
consumer invocation remain unproven. Continue only with a caller-connected
trace of `$224cc`, its ordered invocation timing, the relevant `$22a20`,
`$22a16`, `$22a30` and AUDx state, and the DMA/register observations that
select the consumer branches. Do not run the consumer once per command or
frame, or assign playback-loop timing from descriptor writes.
The caller-connected ordinary, masked, saved-scanline, restored and `$fe`
sprite routes are implemented; do not invent another renderer for the
remaining asynchronous `$224cc` audio/control interrupt path. Do not reuse
unrelated observed bases or assign graphics, audio, scheduler, input, or
resource semantics.

The runtime now has a bounded main-stage deterministic driver. Each SDL pass
runs already-proven local transactions until the next typed external
observation, then stops idempotently. It never creates an Exec, graphics,
custom-register, counter, input, or media observation. A nonzero step cap
guards accidental local cycles, and every constituent transition retains its
copy/validate/commit rollback contract.
The SDL path now uses the encompassing deterministic session driver rather
than special-casing `$13000` and invoking the main-stage pump separately. It
reports an exact typed stop reason and boundary address across title load,
profile reload/re-entry, title-stage local work, and main-stage local work.

For every row, commit only source code, metadata, hashes, bounded offsets,
tests, and documentation. Keep raw captures, ROMs, original media, generated
pixels, and user saves outside the repository. When an item remains blocked,
record the exact missing observation in `PRESERVATION.md` and continue with the
next unblocked item.

### 2026-10-03 Millennium Atari Fcreate boundary

The typed `MILL22B.INF` Fopen return checks the dynamic RTS destination. Only
if that observed address is `$11fe6` does it follow the hash-identified local
prefix through GEMDOS selector `$3c` at `$11ff6`; otherwise it stops after the
typed Fopen result. The 18-byte continuation hash is
`352b6ca9a375e016e129667085dbd6fcd89d1e2eece201d75d97972c60b94cd4`; the
prepared `2200AD.PRG` argument frame is not a disk write, and no Fcreate
result is synthesized. A typed raw result can now cross the hash-bound
`$11ff8..$12003` suffix and store its D0.W value before returning to the
stack-derived caller address. A recorder-backed trace has not established
that `$11fe6` is the runtime destination. The supplied Equinox roots also
lack `2200AD.PRG`, so its Fcreate result, program bytes, and later gameplay
remain unresolved. Next, obtain the real returned PC/service result from an
admitted trace or locate hash-identified supplied media, then continue on
that path.

### 2026-10-05 Deuteros v16 no-input boundary

The bounded v16 observer adds only `$218cc` to the v15 raw-PC site set.
Its visible, realtime, 180-second no-input capture passes schema-24 receipt
verification, contains 896 raw-PC records, no host-input receipt, no title-
display receipt, and no `$218cc` sample. The PC observer and receipt grammar
are ready, but this run does not advance the title or gameplay state. Next,
obtain a visible physical-input capture linking the relevant guest route to
`$218cc` and display/state checkpoints before advancing native execution.

### 2026-10-05 Deuteros v17 physical-input attempt

The pinned v17 recorder was launched visibly on trv2 with a 30-second focus
settle and a 300-second realtime physical-input window. FS-UAE produced raw-PC
output, but no non-empty host-input receipt. The runner therefore rejected the
capture with `physical-input capture requires an observed non-empty host-input
receipt` before writing `run-status.txt`; this attempt is not admitted evidence
and establishes no game response or selector-cell value. Repeat only with
visible manual input in the VNC window, then require a complete receipt before
using any resulting observation.

The follow-up codex09 run used the same pinned v17 binary with a 120-second
focus settle and a 600-second realtime physical-input window. It reached the
active window and ran FS-UAE visibly, but again ended without a host-input
receipt and was rejected before `run-status.txt` was written. The only changed
condition was the longer visible window; no new game-state evidence was
produced.

### 2026-10-05 Millennium Atari media recheck

A metadata-only search across local Downloads, local `~/.projecteon`, trv2
Downloads and trv2 `~/.projecteon`, including archive member listings, found
no `2200AD.PRG`. The supplied alternate Equinox root contains `TDS.TOS` with
the exact active Equinox `MILENIUM.TOS` SHA-256
`4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686`; neither
root supplies `2200AD.PRG`. Do not substitute or synthesize the missing child
program; its Fcreate result, bytes and later gameplay remain unresolved.

### 2026-10-05 Millennium Atari bytecode boundary correction

Exact-media execution now follows only the first selector-3 macro command at
`$1decf`: bytes `16 01 06` store `$00081e01` at `$1ff66`, and execution stops
at the next token `$10` at `$1ded2` (dispatcher return `$20026`). The native
runtime now follows the full hash-bound 44-byte macro through its NUL, runs
the `$10`/`$11` handlers and positive-mode text raster from the actual
selector-4 word and typed `$5f00` screen source, then stops at selector-7
trap `$11ebc`. Next, obtain the external Crawcin return/branch observation and
continue the already hash-identified caller path. The test's patterned buffer
proves only mechanics. The Equinox disk-member test passes
when the original image is streamed directly from its user-supplied ZIP; no
media is extracted. The missing `2200AD.PRG` boundary still prevents claims of
Atari gameplay.


### 2026-10-05 Millennium DOS child image availability

A metadata-only Downloads/archive recheck confirms the English DOS package
contains `millennium-return-to-earth-2-2/2200AD.EXE`, 54,391 bytes, SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`. The
member streamed from the user-supplied archive matches the existing
`~/.projecteon` file byte for byte; no extraction or copy was performed. This
removes child-image absence as a static DOS blocker. Continue only from the
existing typed parent EXEC/process-entry path: dynamic private-driver returns
remain unobserved, so the executable's presence is not evidence of gameplay.
This DOS `.EXE` is not the missing Atari ST `2200AD.PRG`.

### 2026-10-05 Linux real-media CTest coverage

A Debug/Ninja build on trv2 compiled the current source snapshot. In direct-
media mode, with `EON_REAL_DATA_DIR` unset and `EON_DIRECT_DATA_DIR` pointing
to the existing `~/.projecteon`, CTest passed 21/21 tests, including split-container admission,
Deuteros Amiga worker scenarios, Millennium DOS video-driver profiles, DOS
title/runtime and MCGA function-six paths, and the DOS title EXEC entry. The
separate full-corpus mode cannot pass its scanner count because the available
recognized archive set is incomplete; its real platform-start test also reports the canonical
Millennium Amiga release (`2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400`)
and Atari ST release (`ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01`)
missing or substituted. The direct Millennium DOS directory and its title
EXEC path pass; no English archive or observed runtime from another platform
was substituted for the absent releases.

### Deuteros Amiga title selector dispatch

The title-stage session now consumes the captured selector passage followed
by exact ordered raw reads of `$1f98c` and `$1f98e`, validates the original
branch PCs, and reports the next local PC for each of the five static routes.
Native direct-media coverage tests all five mechanics and rejects missing or
misplaced secondary reads. Fixture values are synthetic test inputs, not
captured game state. The runtime facade now checks these bytes against any
already-owned memory and atomically admits them into guest memory, returning the
computed next PC. A verified physical-input capture now contains 31 input-linked
reads at `$1fbe6`, but it lacks the ordered `$1fe84 → $1fea8 → $1fe88` and
`$1fe92 → $1fea8 → $1fe96` helper-return passage required before those reads
can be admitted. A later visible physical-input run on 2026-10-06 passed the
language selector and Disk 2 prompt and displayed the main game interface.
Its verified schema-30 receipt linked three selector samples at `$1fbe6` to
input ordinals 22, 24 and 28; both selector cells remained zero. The late
post-input sidecar recorded three executions at `$1fc22` and none at `$1fc9c`.
This establishes zero-route reachability after physical input, not the
pattern/mask reads, destination writes, displayed playfield contents or
gameplay behavior. The first bounded capture was removed after receipt
verification to reclaim cache space; its full sidecars are no longer
available. A repeated 2026-10-06 run is now preserved under
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-v20-selector-preserved01/`;
its schema-30 receipt verifies and its sidecars retain the input chronology.
The repeated run again captured only three `$1fc22` hits, at input ordinals
18, 20 and 24, with no `$1fc9c` hit. Next, extend the bounded external
observer to record the zero-route's ordered chip-RAM operands and bitplane
writes, then capture the required helper returns and selector passage. Do not
enable native route writes until those exact ordered effects are observed.
Continue only along admitted paths while retaining the external service stop
at `$3fbf8`.
Millennium DOS still lacks an admitted private-INT driver return and live
function-six caller. Neither game's playability target is complete.

### 2026-10-07 Millennium DOS LOADNGO identity diagnostic

A fresh external no-input diagnostic on trv2 compiled the experimental
DOSBox-X observer and recorded complete `LOADNGO` identity for the first two
COM members. `MILL.COM` entered at `0812:0100`, and `TITLES.EXE` entered at
`0a8d:0100`; both full COM hashes match the separately hash-identified direct
media. Twelve ordered private-INT driver returns use the observed MILL.COM
segment `CS=0812`. Captures 08–10 subsequently showed that the saved `IP` was
truncated: the actual core fetch at the INT-6 stop is `CS=0e70:EIP=0x000a1900`,
linear `$b0000`, not a `0e70:1900` flat-file offset. Capture 10 traced the
first high EIP from `$fffe` to `$10000` in a 16-bit real-mode code segment,
then execution advanced beyond the recorded `$ffff` CS limit into the VGA
aperture. Capture 13 also records the preceding path: `TITLES.EXE+0x34`
executes `INT 93h` with a zero IVT target; bytes in the DOSBox-X INT-0 callback
at physical zero execute as `PUSHA; RETF $f000`, using preexisting `DI:SI` to
resume at `0e70:fffe`. This establishes the current emulator route and missing
INT-93 target, not the original handler or intended behavior. Do not synthesize
the vector or treat DOSBox's callback bytes as game logic. Raw sidecars, build,
and helper remain external under the trv2 cache; hashes, sizes, and bounded
metadata are in `PRESERVATION.md`. A hash-gated full linear disassembly of
`MILL.COM`, `TITLES.EXE`, `2200AD.EXE`, and `2200GX.EXE` now finds the GX
`INT 93h` wrapper at member offset `$51` in addition to the title and game
wrappers. Both supplied text drivers have a hash-verified, identical
31-byte INT-93 dispatcher and ten-entry AH table at `$27`; the dispatcher
uses absolute near offsets in the active CS and has no AH bounds check.
Their AH0/AH6 shared-RET routes and AH3/4/5/7/8/9 handler effects now have a
separate hash-bound typed profile/session and real-media test. The session requires explicit driver-load
and vector observations and is not connected to runtime, because the captured
TITLES route has not established those observations. The bounded cursor and
color-state effects are modeled. AH1/AH2 draw into VGA memory or EGA planes,
whose visible results are not established here. Exact member, dispatcher,
table, and handler hashes are in
`PRESERVATION.md`. The linear listing also
contains a candidate title-loader fragment:
it reads a mode value from INT 91h, selects a text-handler file, and reads it
through the far pointer at `CS:$011c`. An independent worklist from the
declared title entry does not reach the fragment's `$125c` Set-Vector site, so
this listing does not establish that the loader runs or installs a handler.
The supplied `VGATXT.BIN` (1,024 bytes; SHA-256
`c31cb760d5f62a21b3baf9c09a6be413514780bd88eeac0273620e81b5d69318`) has an
interrupt-shaped dispatch prefix and IRET, but no runtime handler contract is
proven. The diagnostic capture does not show whether the candidate loader ran.
A bounded
candidate graph from the title entry decoded 1,346 instructions but reported
the `$125c` Set-Vector site as unreached; its code/data and indirect-transfer
limitations are recorded in `PRESERVATION.md`. Next correlate the INT 91h
mode result, chosen file, read completion, Set-Vector result, and first INT
93h call in a reviewed interactive trace before implementing behavior. This
remains diagnostics-only and cannot satisfy either game's playability target.

### 2026-10-06 Deuteros v21 zero-route capture

The v21 visible capture now verifies one complete 147-record zero/zero planar
invocation from `$1fc22` through `$1fc9a`. Its receipt is retained at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-v21-physical04/`;
the zero-route sidecar SHA-256 is
`1290bcb23017c86d4f115b281a4cb0a8315efb42c9757803a09635e3379891ff` (54,723
bytes). The parser binds all rows to delivered host input and validates the
32 `$1fc74` intended byte writes against the `$1fc76` readbacks. The VNC view
reached the interface labelled “THE SUN”. Independent source and build review
against pinned FS-UAE v3.2.35 is complete: a rebuild from the exact observer
patch matches all 26 loaded ELF sections of the pinned binary byte-for-byte.
The normal Deuteros receipt verifier accepts this receipt without an
experimental-observer flag; that flag is not a Deuteros admission gate. The
recorder identity is now reviewed and pinned, while the capture remains
diagnostic and does not prove an accepted game action or playable state.

The physical04 run fixes two omissions found during live validation: the
runner now passes `PROJECT_EON_FS_UAE_ZERO_ROUTE_RECORD` to v21, and the
schema-31 grammar accepts selector-cell samples at `$1fc28` as well as the
start/entry sites. Focused parser and receipt tests pass. Review confirms the
existing owned runtime API already has a bounded, atomic zero/zero planar
write path via `observe_command_planar_write`; it derives only the 32
hash-bound route effects and the pointer update, and publishes a surface only
after memory admission. The separate generic variant API remains gated from
zero/zero without its own typed evidence. Reinspection of physical04 shows
input ordinal 26/frame 5031 traversing `$1fe7a`, both ordered `$1fea8` calls,
their `$00310000`/`$00300000` returns, `$1fbe6`, and `$1fc22`; this satisfies
the existing captured-selector-passage checkpoint but does not explain the
opaque input action or establish a screen/state change. The title-display
recorder had exhausted its 2,048-write budget by input ordinal 20. Next,
reserve an additional bounded display-register sidecar for later input-linked
writes, then use a visible capture to bind action and display/state change.
Keep selector and planar runtime checkpoints tied to their existing owned,
hash-bound APIs; do not claim general playability from these local passages.

Historical note: the Deuteros v21 external FS-UAE observer was initially
unexercised. It was subsequently run with visible manual input; see the
2026-10-06 capture section above for the verified diagnostic result and its
gameplay evidence boundary.

### 2026-10-06 local validation and remaining playability evidence

The current shared source snapshot configures and builds with CMake/Ninja on
macOS when the installed Xcode 26.5 SDK is selected explicitly. CTest passes
22/22 in direct-media mode against the existing `~/.projecteon` collection;
the full Python preservation suite passes 455 tests (6 skipped), and the
repository artifact verifier, Gitleaks, and `git diff --check` pass. Both the
Millennium DOS and Deuteros Amiga CLI launch checks report `READY`; these are
media/configuration checks, not playability evidence. No runtime fix is
justified by the current traces: Millennium still needs the first real INT
`$93` vector change and subsequent call/return context, while Deuteros needs
captured post-selector display state and ordered helper returns. Continue with
visible, physical-input captures and do not synthesize missing guest state.

On 2026-10-06 an external experimental DOSBox-X observer was built against
the pinned upstream commit. A clean x86-64 Linux build of the same patch was
completed on trv2 on 2026-10-07; its exact binary identity is recorded in
`MILLENNIUM_DOS_DOSBOX_X_RECORDER.md`, and the local runner/verifier now bind
the macOS and Linux binaries to separate hash/size identities under the same
experimental protocol. Its bounded sidecar can record the six exact
`MILL.COM` post-INT `$21` return sites and a digest of the read buffer at
`$0315`. The prototype has no reviewed v21 event/result/input hooks, and no
game run or capture has been performed. Next, use its explicitly experimental
runner mode for a visible, operator-closed diagnostic to distinguish
file-service returns from the later `INT 6` path. Compare any observed read
digest with the hash-identified original driver without retaining guest
bytes. This does not replace either game's capture-backed playable-state
requirement.

The later origin-observer candidate was rebuilt on trv2 with the exact
experimental identity recorded in `MILLENNIUM_DOS_DOSBOX_X_RECORDER.md` and
run visibly against the unchanged English archive
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`. The
window remained black with DOSBox-X title `TITLES`. Closing it spawned a
`zenity` confirmation helper that inherited the recorder console pipe; the
180-second runner timeout killed DOSBox-X but then waited indefinitely for
that helper to close the pipe. The retained external run
`/home/trv2/.cache/project-eon-tools/dos-origin-experiment-runs/origin-20261007-rebuilt02/`
has `failure_reason=timeout`, no success marker, and no raw receipt. The
runner now starts DOSBox-X in its own process group and kills the complete
group at timeout or console safety cap, so inherited UI helpers cannot strand
cleanup again. This corrects the failure mode, not the emulator's black
screen; no runtime or playability claim follows from the attempt.

Direct, non-materializing inspection of the hash-matched member found its
first word is `$1f0e`, not either DOS MZ signature. DOSBox-X's pinned loader
source classifies an unrecognised header as a COM image and chooses PSP:`$0100`
as its initial CS:IP. This is a source-path prediction, not an observation of
this run's EXEC parameters or a guest-image mapping. The observer's first
recorded fetch for CS `$0e70` was `$fffe`, not `$0100`; the origin observer
also does not retain the time or full instruction history between the DOS
loader handoff and that fetch. The next recorder increment should capture the
hash-validated DOS `TITLES.EXE` identity and exact `LOADNGO` CS:IP together
with the first `CS=$0e70` normal-core fetch, keeping their chronology explicit.

Source inspection identified DOSBox-X's default `[dosbox] quit warning=auto`
as the cause of the hidden zenity prompt. The experimental-only origin runner
now sets `quit warning=false` in its hash-bound generated configuration; the
general Millennium capture profile is unchanged. The manually closed retry
completed and its metadata/raw-record digest passed the updated verifier. It
records a last fetch at `0000:0001` (opcode `$ca`) before the first observed
fetch at `0e70:fffe` (opcode `$00`). Both addresses are outside the declared
flat candidate interval for the 7,022-byte `TITLES.EXE`, so those bytes are
not attributed to original code. This remains only last-fetch context: it does
not identify a DOS EXEC image, prove a causal transfer, or explain the black
screen. The next DOS boundary is an exact observation of the DOS EXEC load
image and entry context before that out-of-range fetch; no model may consume
the `$00` as an original instruction.

The 2026-10-06 guided v18 run again stayed on the animated Deuteros logo.
Its verified v28 receipt has 18 delivered host-input events but only 1,024
raw-PC samples, capped at eight sites; no selector-dispatch or title-display
artifact exists. The new operator prompt did not close the title-to-selector
evidence gap. Continue from a visibly reached language selector and capture
the ordered helper returns, dispatch read, and selector-cell values together;
do not add guessed runtime behavior.

### 2026-10-06 Deuteros v19 late-input recorder

The v19 FS-UAE executable is hash-pinned at 62,020,024 bytes. Its source patch
and archive-member comparison are documented in
`DEUTEROS_AMIGA_FS_UAE_RECORDER.md`. Receipt schema 29 and focused parser tests
cover distinct late-input samples and the joined selector-cell sidecar. The
visible v19 captures pass receipt verification. Early runs delivered Space,
Return, and short Fire clicks but remained on the animated logo. A held Fire
drag then produced Z=1 at `$21866` while action 33 was held; static bytes fall
through to a store of 1 at `$21720`, after which the language selector became
visible. The selector sample read cells `$1f98c=$00` and `$1f98e=$00`.

A follow-up run visibly reached the selector and delivered key `1` at input
ordinals 29/30. The following selector-dispatch sample (ordinal 30) has D0 low
byte `$20` and A0 `$1eed5`. Both selector cells were still `$00`. A source-span
check maps A0 to a zero-filled region, so the earlier prompt label is
withdrawn. The display later appeared black;
the capture does not prove that the prompt was drawn, that English was accepted,
or that the game reached a playable state. Its raw-PC, late-PC, dispatch and
display data remain external under
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-vnc-v19-language-confirm01/`.

The next Amiga step is to identify the bounded code and display writes between
that second dispatch and a visible disk prompt, then capture the ordered helper
returns and selector reads needed by native route admission. Do not synthesize
the prompt or advance a disk swap without visible, input-linked evidence.

Hash-locked disassembly of Disk 1 ADF offset `$7ac22` / runtime `$1fc22`
(256 bytes, SHA-256
`9f1ecf6524f3e88e4124cde39fed5a01fb9008a44871ea7907aa78c029f0ecd3`)
confirms that the zero/zero route subtracts `$20` from D0's low byte, indexes
an eight-byte pattern at `$1f99c`, and performs original four-plane merges
through `$1f974`. The v19 ordinal-30 dispatch sample has D0 low `$20`, A0
`$1eed5`, and both cells zero. This predicts the route only if the sampled
cells persist to the later tests; no `$1fc22` execution or plane write is
captured. Next recorder build should probe `$1fc22/$1fc9c` and retain the
ordered selector-helper/dispatch passage. Do not treat `$1eed5` as proof that
the prompt was displayed.

FS-UAE v23 and receipt schema 33 now add a bounded observer for the exact
main-stage store at `$21868` to `$21720`; the receipt verifies the fixed
instruction, 0→1 byte transition, and matching input frame. A visible
operator-driven attempt on trv2 ended with no host-input receipt and was
rejected, so it yields no new game evidence. The next Amiga step still needs a
human-driven capture beyond the selector. The v23 build and incomplete output
remain outside the repository and supplied media.

Millennium DOS still lacks an admitted private-INT driver return and live
function-six caller. Neither game's playability target is complete.

### 2026-10-07 Millennium DOS zero-context disassembly

The `TITLES.EXE` context around the V14 zero-byte history was re-disassembled
from the exact direct-media member, without extraction. Under the existing
flat COM-style `IP - 0x100` candidate mapping, the captured `$18e4..$1900`
range maps to file `+$17e4..+$1800`. Those title bytes are nonzero; `$18e4`
lands inside the instruction linearly decoded from `$18e3`, and `$1900` lands
on the final displacement byte of the instruction beginning at `$18fd`. The
same 30-byte file span in `2200AD.EXE` is zero-filled. Captures 08–10 later
proved that the exception stack's 16-bit IP did not identify the effective
fetch address, so this apparent match is retracted and does not support
mapping the observed fetch to `2200AD.EXE`.
The complete linear report and exact span hashes are recorded in
`PRESERVATION.md`; raw disassembly remains outside the repository. Captures
08–13 supersede the flat-offset candidate mapping: the effective fetch comes
from the null-INT-93 route described there, and no `2200AD.EXE` identity is
established for it. Next DOS work must locate the original INT-93 handler or
installer using a reviewed read-only observer. Under the retracted
`2200AD.EXE` candidate, the 15 zero-opcode pairs would decode as
`ADD byte ptr [BX+SI],AL`; this must not be treated as evidence that this image
was executing. Any further observer must retain bounded register,
effective-address, and pre/post-write evidence alongside full EXEC
image identity; PC samples alone cannot explain the path. The exact source
zero run is `2200AD.EXE+$124b..+$26c3` (5,241 bytes), with nonzero bytes
resuming at `+$26c4`; this is file structure, not runtime mapping evidence.
The reproducible direct-transfer scan tool finds no instruction-aligned
relative CALL/JMP target among the observed IPs; tests reject byte matches
that begin inside another instruction. This still leaves indirect transfers
and computed returns open.
The tool and focused unit tests are now in `tools/scan_dos_transfer_targets.py`
and `tests/test_dos_transfer_scan.py`; they revalidate the direct-media set
and selected file hashes and write mode-0600 metadata only under the external
cache. The reproduced empty result is hash-recorded in `PRESERVATION.md`. The
full Python suite passes 463 tests (6 skipped), repository artifact policy
passes, Gitleaks reports no leaks, and `git diff --check` passes. These static
checks refine the DOS fault hypothesis but do not advance either game's
verified playable state.
Do not infer executable identity or runtime behavior from zero-byte
compatibility, stack contents, or a candidate offset mapping.
Deuteros Amiga still needs the ordered selector/helper passage and a captured
state-changing control leading beyond the observed “THE SUN” interface. Both
games remain short of verified playability.

The follow-up `tools/analyze_dos_reachability.py` starts at `$0100` and `$d2b0`;
all six English DOS candidate addresses are reachable only in an
over-approximation with 24–34 assumed near-call returns and two assumed
`INT $91` returns. Neither entry has a direct-control-only route to those
sites. Hashes, route classes, and decoder limits are recorded in
`PRESERVATION.md`; the caller models remain unconnected to runtime.

The external CMake/Ninja build completed and CTest passed 22/22. The full
Python preservation suite passed 470 tests (6 skipped), repository artifact
policy and Gitleaks passed, and `git diff --check` is clean. Neither title has
yet produced verified gameplay evidence.

### 2026-10-07 Deuteros later-input display observation

Rechecking the v21 schema-31 capture found that input ordinal 26 reaches the
hash-profiled selector passage and `$1fc22`, with selector cells still zero.
The established title-display sidecar ended at ordinal 20 after using its
2,048-write budget, so it could not determine whether that later passage
changed display registers. FS-UAE v22 is now compiled and hash-pinned in the
external cache, adding an independently bounded display sidecar for writes
after ordinal 20. Schema 32 verifies its exact binary identity, register
allowlist, per-register and total caps, frame/ordinal links, and file hash.
Focused tests cover these constraints and a full synthetic schema-32 receipt;
the full Python suite passes 472 tests (6 skipped). The prior v21 physical
capture still verifies normally. A visible v22 no-input diagnostic at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261007-v22-physical02/`
produced 1,024 raw-PC samples, all with input ordinal zero, then failed
preflight because no physical-input receipt or completed sidecars existed.
It supplies no new evidence of a game action or visible state change. Next,
run a visible physical-input capture with v22 and inspect later-input display
writes together with the selector route; do not infer gameplay from
display-register writes alone.

A new v22 no-input diagnostic now passes receipt verification after using
`LIBGL_ALWAYS_SOFTWARE=1` on trv2. It loads the recognised media and records
only the bounded no-input polling loops; it supplies no title or gameplay
evidence. The earlier locator miss came from trv2's stale helper copy; the
current pinned-hash locator finds the exact v22 binary. A 1,024-byte
hash-bound continuation disassembly also confirms the character-blit caller
and the zero-route helper. Source mapping retracts the earlier interpretation
of A0 `$1eed5` as static prompt text. The command parser reads its stream via
A4 at `$1fa0a`; static code sets table pointer `$1f97c=$1c482` and later
dispatches entry `1`, but no capture establishes that initialization or call.
The next visible trace should bind A4 and its bytes to glyph calls and display
writes. Continue with visible manual input only; never replace it with
scripted guest input.

### 2026-10-07 Deuteros Exec interrupt-server boundary

The hash-bound post-Exec call at `$204f4` uses Exec LVO `-$a8`, which the
Exec Autodocs identify as `AddIntServer(D0, A1)`. The call registers a handler
on a priority-ordered Paula interrupt chain; the handler then runs when its
interrupt is dispatched. This rules out treating the call as a no-op typed
return, but does not identify its actual D0 interrupt number, A1 structure,
registered code, or invocation cadence. Model this OS list and interrupt path
only after those operands and handler effects are tied to the admitted guest
state. No new gameplay behavior is admitted by the LVO identification.

### 2026-10-08 Deuteros VNC follow-up

The v23 physical-input capture initially ran on Xvnc `:6` without its required
`XAUTHORITY`; the emulator window was therefore not visible and the runner
rejected the no-input result. The capture helper now probes an available
`xdpyinfo` before mounting media, with focused tests for authenticated,
inaccessible, and headless displays. A retry with the correct auth file shows
the FS-UAE window in VNC, but the guest image remains black and no physical
input is delivered. The recorder reaches its sampled `$21866` poll with
input ordinal zero. FS-UAE's retained log confirms a Mesa llvmpipe OpenGL 4.5
context and a started emulation thread, so missing X11 authentication and a
failed GL context are no longer the leading display hypotheses. Preserve the
current evidence boundary: the 1,024-record raw-PC budget is full with 128
samples at each of eight sites, all in the input-zero phase; none are at the
allowlisted title-display or selector sites. Trace the startup/entry and
guest-side display setup before the repeating poll, then capture manual input
and a subsequent state/frame change. This finite sample set is not a global
reachability proof. No game-control meaning or playability is inferred from
reaching the poll.

The hash-bound secondary-input continuation now commits the sampled CIAA
`$bfe001` byte and `$21720` latch only after its bounded branch succeeds, so
failed paths cannot leak partial input state. Native assertions cover the
committed port value and the real-media selector routes; the focused real-media
CTest passes. The 23 tests independent of the two missing-platform corpora
also pass on trv2. This validates the bounded continuation only; a visible
input-driven state change and a playable Deuteros session remain unverified.

A separate normal FS-UAE visual troubleshooting session is now running on
trv2 VNC `:6` from `/home/trv2/.cache/project-eon-tools/vnc-troubleshoot-20261008-01/`.
The original disk archives and Kickstart are mounted read-only and hash-match
the recognised English release; the visible guest reaches the Deuteros intro
logo. No operator input was sent, and this normal-emulator display is not
capture evidence. Leave the session available for visible manual input; do
not infer a title menu or playable state from the logo.

The normal FS-UAE log's `Illegal instruction: 4e7b at 00FC0564 -> 00FC0582`
message was checked against the exact mounted Kickstart 1.3 bytes. It is
consistent with the ROM's 68000 CPU-detection probe, which temporarily installs
an exception continuation, tries a 68010-only opcode, then restores the old
vectors and registers. This log line alone is not evidence of a crash: the
separate normal-session VNC snapshot shows the intro logo. The black image was
observed in separate v23 capture attempts and remains unexplained. Keep tracing
the guest startup before its repeating input poll, and continue to require a
visible input-linked state change before claiming Deuteros playability.

### 2026-10-08 Millennium DOS loader and INT 93h cross-reference

The exact English `TITLES.EXE` now has an external full linear candidate
listing and bounded instruction-start transfer scans. A file-selection/read
fragment at `$104a..$1082` reads a far pointer from `CS:$011c`; the candidate
installer at `$1247..$1265` uses DOS `GetVect`/`SetVect` for INT `$93` around
that pointer. The source bytes at `+$001c..+$001f` are zero. The listing records
`$1061 -> $1066` and `$1082 -> $1247`, but no edge proves entry into `$104a`
from the modeled `$1b80` startup. No handler identity or runtime pointer write
is established. See the matching hash-addressed evidence in
`PRESERVATION.md#2026-10-08-millennium-dos-int-93h-installer-disassembly-cross-check`.

The call-context scenario that assumes the recorded INT `$91` returns plus
named BIOS/DOS returns reaches the DOS memory request at `$1b2d` with
`AH=$48`, `BX=$fa00`; its carry, returned registers and memory effects are
unknown. Next DOS work must capture the exact EXEC child and the actual
allocation result, then trace who writes `CS:$011c` and which indirect/far
transfer reaches the candidate loader. Do not synthesize the DOS return,
handler vector, or child state.

The exact `MILL.COM` linear candidate also identifies DOS EXEC function
`$4b00` at `$0336`, called from `$0240` and `$024c` with distinct runtime
filename pointers. The next recorder should bind each site to the actual
`DS:DX`, parameter block and hash-identified child image. This links the
existing `LOADNGO` identity work to a specific runtime boundary only when
those values are observed; see the parent EXEC record in `PRESERVATION.md`.

The v23 Deuteros capture helper also had a schema-33 bookkeeping defect: it
activated the zero-route observer but omitted its status fields from the final
receipt. A pinned-recorder hash allowlist now drives both activation and
serialization. The runner/verifier suites pass 86 tests, and a visible
software-OpenGL no-input diagnostic passes receipt verification on VNC `:6`.
Its raw-PC trace is byte-identical to prior runs and contains only pre-input
poll sites; it does not demonstrate display/game-state progress. Keep requiring
visible manual input and a later verified state/frame change before claiming
Deuteros playability.
