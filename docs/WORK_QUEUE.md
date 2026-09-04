# Project Eon P0 work queue

This is the ordered execution queue for the completion plan. It is a
preservation tracker, not a list of compatibility claims. A task moves only
when its acceptance evidence is committed; a missing capture is a boundary,
not permission to synthesize a result.

The queue is deliberately organized by its contribution to the first genuine,
playable vertical slice. Presentation, packaging, and broad platform work stay
behind the first proven input-to-frame-to-state loop.

| Rank | Work package | Exact current evidence | Required acceptance evidence | Status / boundary |
| --- | --- | --- | --- | --- |
| 1 | Millennium DOS: capture the launcher/title/`2200AD.EXE` handoff and private DOS ABI | English DOS release `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`; CLI-validated diagnostics-only title-init v2 profile binds the `MILL.COM:0x02cf` driver-load, setup-site `0x0209`/actual-`INT` `0x020c`, `TITLES.EXE:0x0127` request, and two raw returns at `$0129`; v6 additionally binds `svga_s3`/`ega` machine-profile selection to its exact config, while the genuine EGA diagnostic still requested `mcga.bin` and hit the console-capped `INT 6` boundary; v7 records IVT `INT 91h` endpoint `087e:0000`, v8 records a normal-core transfer to it, v9 binds the first raw caller re-entry (`AX=$0101`, `FLAGS=$7202`), v11 terminates the host recorder only after the complete twice-observed `INT 6` diagnostic receipt matches byte-for-byte, and v12 independently repeats an immediate predecessor at `f000:ca60` outside the recognised original-image map. The verified v13 no-input preflight ends at the same eight-record `INT 6` receipt (SHA-256 `8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`) with no host-key receipt or title poll; [read-only physical capture runner](MILLENNIUM_DOS_CAPTURE.md#safe-capture-procedure); [external recorder status](MILLENNIUM_DOS_DOSBOX_X_RECORDER.md#prototype-status) | Hash-bound genuine trace of each interrupt, EXEC/far-return and driver result through one navigable state | The title-init prefix and physical capture route have strict contracts. The v12 predecessor is an emulator callback boundary, and the v13 preflight has no physical receipt; neither is guest-code input, rendering, audio, EXEC, or game-state evidence. |
| 2 | Millennium DOS: admit the GX startup bridge | Same release; `2200GX.EXE` SHA-256 `093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb`; caller and record bounds in `PRESERVATION.md#millennium-dos-gx-startup-record-boundary`; strict ten-record admission builds a call-free overlay state after independently pinned trace validation; an engine-owned successor now requires the exact active English release to have independently reached title handoff and publishes only a terminal value checkpoint | A genuine hash-bound ten-record capture plus the rank-1 driver/title handoff; active admission must end at the second private-INT boundary and reject any missing/reordered/altered record without changing the prior session | Runtime ownership, byte lifetime, state, rejection, reset and revocation contracts are implemented. The current English path still stops before title handoff, so the successor remains unavailable; no genuine GX trace, frame, input or gameplay is implied |
| 3 | Deuteros Amiga: capture title initialization and display ABI | English Amiga release `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`; clean disk 1 `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`; [live debugger status](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md); [read-only physical capture runner and raw-recorder design](DEUTEROS_AMIGA_FS_UAE_RECORDER.md#media-and-execution-safeguards) | Write-protected capture of Exec/graphics return values, callbacks, bitplanes, palette, input and frame/audio checkpoints | Bootstrap-only v9 evidence is bounded. The reviewed v10 observer now waits for the actual `$1eda6` title site and then records only bounded display-register writes; no v10 operator result exists yet. A complete ordered genuine capture is still required. |
| 4 | Deuteros Amiga: define and admit title-display capture evidence | Title-stage/main-stage recovery-map entries; strict v4 24-record contract and v5 artifact contract; an engine-owned checkpoint now reopens, rehashes and revalidates all evidence at consumption time | A genuine, write-protected v5 capture satisfying every event and artifact checkpoint; then a separately reviewed presentation/ABI bridge | Immutable v4/v5 checkpoint ownership, state transition, rejection, reset and revocation are implemented. It retains no paths or bytes and grants no renderer, audio or input capability. Runtime display remains unavailable until rank 3 supplies genuine complete evidence. |
| 5 | Millennium DOS: recover first actionable state and controls | Hash-identified `TITLE.LIB`, `GX.LIB`, video-driver and title-flow profiles; the typed post-overlay continuation now covers `$d39d..$d412`, fifteen exact call returns, its explicit AL/runtime-byte branches, poll cycle and terminal dispatch-call boundaries | Replay of a real input → canonical state → frame/audio checkpoint | The deterministic caller-owned loop is coded and real-media tested, but remains a static recovery entry until ranks 1–2 prove reachability and observed call/input results; no handler is executed |
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

For every row, commit only source code, metadata, hashes, bounded offsets,
tests, and documentation. Keep raw captures, ROMs, original media, generated
pixels, and user saves outside the repository. When an item remains blocked,
record the exact missing observation in `PRESERVATION.md` and continue with the
next unblocked item.
