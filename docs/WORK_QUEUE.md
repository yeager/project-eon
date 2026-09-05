# Project Eon P0 work queue

The Millennium DOS non-mode-1 title path now observes both external vector
pairs and atomically installs the exact timer and video hooks. Its verified
caller continuation enters the repeated mode call `$1c02->$1ada`. Mode one
remains independently stopped at `$0fc6` because that requested source exceeds
verified `TITLE.LIB`. The repeated `$1ada` call now consumes a fresh typed
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
Continue by caller-connecting the resulting `$16e8` return toward the first
complete admitted destination state; do not infer pixel or palette meaning.
The other-value branch owns the second descriptor and first two raw words plus
their product and subtraction; continue at `$13e9`, source `$3c80:$0001`. Do not assign
graphics or codec semantics to these fields.

The Millennium Atari config loop now owns the first taken DBF edge and its
iteration-one setup through `$2b5de` (hash
`9efa7511411f3ca6698746d8bac484420a14e67e35467be2909f3647b0612034`).
Next work must verify the iteration-one indexed word and preserve D7 loop
termination plus the saved MOVEM/BSR return chain.
The three DBF iterations and deterministic epilogue are now native through
RTS. The next exact boundary is the saved-register `MOVEM.L (A7)+` at
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
releases: 19 code images, 21 admitted ranges, and 1,124,867 source bytes are
accounted for. This is byte coverage, not semantic recovery or parity.

The active English Millennium DOS path now owns the selected sound-driver
load in a bounded native paragraph arena, admits the exact `TITLES.EXE` child,
executes its entry and initialization prefixes, consumes one explicitly typed
private-`INT 91h` function-0 result, and reaches the second function-4 request.
It still requires an observed result before executing either selected title
callee beyond that request; no DOS PSP, parent `EXEC` return, display result,
or game-state transition is inferred.

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

| Rank | Work package | Exact current evidence | Required acceptance evidence | Status / boundary |
| --- | --- | --- | --- | --- |
| 1 | Millennium DOS: capture the launcher/title/`2200AD.EXE` handoff and private DOS ABI | English DOS release `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`; CLI-validated diagnostics-only title-init v2 profile binds the `MILL.COM:0x02cf` driver-load, setup-site `0x0209`/actual-`INT` `0x020c`, `TITLES.EXE:0x0127` request, and two raw returns at `$0129`; v6 additionally binds `svga_s3`/`ega` machine-profile selection to its exact config, while the genuine EGA diagnostic still requested `mcga.bin` and hit the console-capped `INT 6` boundary; v7 records IVT `INT 91h` endpoint `087e:0000`, v8 records a normal-core transfer to it, v9 binds the first raw caller re-entry (`AX=$0101`, `FLAGS=$7202`), v11 terminates the host recorder only after the complete twice-observed `INT 6` diagnostic receipt matches byte-for-byte, and v12 independently repeats an immediate predecessor at `f000:ca60` outside the recognised original-image map. The verified v13 no-input preflight ends at the same eight-record `INT 6` receipt (SHA-256 `8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`) with no host-key receipt or title poll; [read-only physical capture runner](MILLENNIUM_DOS_CAPTURE.md#safe-capture-procedure); [external recorder status](MILLENNIUM_DOS_DOSBOX_X_RECORDER.md#prototype-status) | Hash-bound genuine trace of each interrupt, EXEC/far-return and driver result through one navigable state | The title-init prefix and physical capture route have strict contracts. The v12 predecessor is an emulator callback boundary, and the v13 preflight has no physical receipt; neither is guest-code input, rendering, audio, EXEC, or game-state evidence. |
| 2 | Millennium DOS: admit the GX startup bridge | Same release; `2200GX.EXE` SHA-256 `093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb`; caller and record bounds in `PRESERVATION.md#millennium-dos-gx-startup-record-boundary`; strict ten-record admission builds a call-free overlay state after independently pinned trace validation; an engine-owned successor now requires the exact active English release to have independently reached title handoff and publishes only a terminal value checkpoint | A genuine hash-bound ten-record capture plus the rank-1 driver/title handoff; active admission must end at the second private-INT boundary and reject any missing/reordered/altered record without changing the prior session | Runtime ownership, byte lifetime, state, rejection, reset and revocation contracts are implemented. The current English path still stops before title handoff, so the successor remains unavailable; no genuine GX trace, frame, input or gameplay is implied |
| 3 | Deuteros Amiga: capture title initialization and display ABI | English Amiga release `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`; clean disk 1 `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`; [live debugger status](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md); [read-only physical capture runner and raw-recorder design](DEUTEROS_AMIGA_FS_UAE_RECORDER.md#media-and-execution-safeguards) | Write-protected capture of Exec/graphics return values, callbacks, bitplanes, palette, input and frame/audio checkpoints | Bootstrap-only v9 evidence is bounded. The reviewed v10 observer now waits for the actual `$1eda6` title site and then records only bounded display-register writes; no v10 operator result exists yet. A complete ordered genuine capture is still required. |
| 4 | Deuteros Amiga: define and admit title-display capture evidence | Title-stage/main-stage recovery-map entries; strict v4 24-record contract and v5 artifact contract; an engine-owned checkpoint now reopens, rehashes and revalidates all evidence at consumption time | A genuine, write-protected v5 capture satisfying every event and artifact checkpoint; then a separately reviewed presentation/ABI bridge | Immutable v4/v5 checkpoint ownership, state transition, rejection, reset and revocation are implemented. It retains no paths or bytes and grants no renderer, audio or input capability. Runtime display remains unavailable until rank 3 supplies genuine complete evidence. |
| 5 | Millennium DOS: recover first actionable state and controls | Hash-identified `TITLE.LIB`, `GX.LIB`, video-driver and title-flow profiles; the typed post-overlay continuation covers `$d39d..$d412`, fifteen exact call returns, explicit AL/runtime-byte branches, poll cycle and terminal dispatch-call boundaries; explicit `$d40a → $76f1` observations can create owned typed index `5` → `$7415`, index `6` → `$7521`, and index `9` → `$7384` sessions | Replay of a real input → canonical state → frame/audio checkpoint | F6/F7/F10 runtime ownership, typed forwarding, copy-only checkpoints and revocation are coded and real-media tested at independent recovery boundaries. Positive active dispatch remains unavailable until genuine predecessor evidence exists. No dispatch resolution, call return, input, handler meaning or frame is invented |
| 6 | Deuteros Amiga: recover first actionable state and controls | Clean ADF loader, bundle, VM, opening-frame and title-stage profiles | Replay of a real input → canonical state → bitplane/palette/audio checkpoint | Depends on ranks 3–4 |
| 7 | Millennium DOS: establish video/audio device contracts for that slice | `EGA640.BIN`, `MCGA.BIN`, `SSBL.DRV`, `SCVX.DRV`, and VOC source-byte profiles | Captured calls plus pixel/sample hash comparison; no guessed hardware behavior | Depends on rank 1 |
| 8 | Canonical game-core subsystems for the two proven slices | Only rule paths that have a caller-connected code proof or trace | Deterministic long-run replays, including state transitions and edge cases | Cannot begin from strings, assets, or inferred genre mechanics |
| 9 | Remaining Amiga/Atari ST execution adapters | Millennium Amiga `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400`, Millennium Atari `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01`, Deuteros Atari `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653`; the latter has a hash-gated static bootstrap checkpoint and a non-admitted Hatari `Floprd` shape cross-check | Per-release recorder-backed bootstrap/device traces and explicit shared/divergent replay checks | Deferred until a playable vertical slice establishes the right core boundary; ordinary emulator output remains diagnostics-only |
| 10 | Final UX, localization, packages, and release audit | Existing card route, i18n catalogues, CI package recipes, and preservation contracts | Real-session menu/CLI equivalence, 20-language checks, clean-package scans and end-to-end replay | Continuous maintenance only; never substitutes runtime recovery or authorizes a release |

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
tail, command path, and both `$41bb4` merges. From authoritative boundary
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
Continue at local `$37f7a->$37f9a`; its nested system calls remain
separate typed boundaries.

For every row, commit only source code, metadata, hashes, bounded offsets,
tests, and documentation. Keep raw captures, ROMs, original media, generated
pixels, and user saves outside the repository. When an item remains blocked,
record the exact missing observation in `PRESERVATION.md` and continue with the
next unblocked item.
