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
| 1 | Millennium DOS: capture the launcher/title/`2200AD.EXE` handoff and private DOS ABI | English DOS release `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`; CLI-validated diagnostics-only title-init v2 profile binds the `MILL.COM:0x02cf` driver-load, setup-site `0x0209`/actual-`INT` `0x020c`, `TITLES.EXE:0x0127` request, and two raw returns at `$0129`; v6 additionally binds `svga_s3`/`ega` machine-profile selection to its exact config, while the genuine EGA diagnostic still requested `mcga.bin` and hit the console-capped `INT 6` boundary; v7 records IVT `INT 91h` endpoint `087e:0000`, v8 records a normal-core transfer to it, and v9 binds the first raw caller re-entry (`AX=$0101`, `FLAGS=$7202`); [read-only physical capture runner](MILLENNIUM_DOS_CAPTURE.md#safe-capture-procedure); [external recorder status](MILLENNIUM_DOS_DOSBOX_X_RECORDER.md#prototype-status) | Hash-bound genuine trace of each interrupt, EXEC/far-return and driver result through one navigable state | The title-init prefix and physical capture route have strict contracts, but no private ABI, title rendering, input, audio, EXEC, or game state is yet admitted |
| 2 | Millennium DOS: admit the GX startup bridge | Same release; `2200GX.EXE` SHA-256 `093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb`; caller and record bounds in `PRESERVATION.md#millennium-dos-gx-startup-record-boundary`; strict ten-record admission builds only a disposable, call-free overlay state after independently pinned trace validation | A genuine hash-bound ten-record capture; then the admission helper must end at the second private-INT boundary and reject any missing/reordered/altered record | Code and rejection tests are complete; no genuine GX trace, title handoff, frame, or gameplay is implied |
| 3 | Deuteros Amiga: capture title initialization and display ABI | English Amiga release `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`; clean disk 1 `6ea0cc68d3af37203a885032eddf7c28e8396abb59d8c9cd3792f1308bdec38`; [live debugger status](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md); [read-only physical capture runner and raw-recorder design](DEUTEROS_AMIGA_FS_UAE_RECORDER.md#media-and-execution-safeguards) | Write-protected capture of Exec/graphics return values, callbacks, bitplanes, palette, input and frame/audio checkpoints | Partial live display evidence is retained. Both a 120-second realtime and a separately labelled 30-second warp diagnostic stop at the same bounded bootstrap sites; warp is receipt-bound reachability evidence only. A complete ordered genuine capture is still required. |
| 4 | Deuteros Amiga: define and admit title-display capture evidence | Title-stage/main-stage recovery-map entries; `deuteros-amiga-en-title-display-v4` strict 24-record contract; CLI-visible counts for display layout, bitplanes, palette, input, frame and audio | A genuine, write-protected v4 capture satisfying every recorded checkpoint; then a trace-gated bridge | Schema, recovery-map binding and diagnostics are complete. Runtime display remains blocked until rank 3 produces a genuine complete capture; the shipped contract never replays its placeholder hashes. |
| 5 | Millennium DOS: recover first actionable state and controls | Hash-identified `TITLE.LIB`, `GX.LIB`, video-driver and title-flow profiles | Replay of a real input → canonical state → frame/audio checkpoint | Depends on ranks 1–2; unknown input remains unavailable |
| 6 | Deuteros Amiga: recover first actionable state and controls | Clean ADF loader, bundle, VM, opening-frame and title-stage profiles | Replay of a real input → canonical state → bitplane/palette/audio checkpoint | Depends on ranks 3–4 |
| 7 | Millennium DOS: establish video/audio device contracts for that slice | `EGA640.BIN`, `MCGA.BIN`, `SSBL.DRV`, `SCVX.DRV`, and VOC source-byte profiles | Captured calls plus pixel/sample hash comparison; no guessed hardware behavior | Depends on rank 1 |
| 8 | Canonical game-core subsystems for the two proven slices | Only rule paths that have a caller-connected code proof or trace | Deterministic long-run replays, including state transitions and edge cases | Cannot begin from strings, assets, or inferred genre mechanics |
| 9 | Remaining Amiga/Atari ST execution adapters | Millennium Amiga `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400`, Millennium Atari `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01`, Deuteros Atari `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653` | Per-release bootstrap/device traces and explicit shared/divergent replay checks | Deferred until a playable vertical slice establishes the right core boundary |
| 10 | Final UX, localization, packages, and release audit | Existing card route, i18n catalogues, CI package recipes, and preservation contracts | Real-session menu/CLI equivalence, 20-language checks, clean-package scans and end-to-end replay | Continuous maintenance only; never substitutes runtime recovery or authorizes a release |

## Operating rule

For every row, commit only source code, metadata, hashes, bounded offsets,
tests, and documentation. Keep raw captures, ROMs, original media, generated
pixels, and user saves outside the repository. When an item remains blocked,
record the exact missing observation in `PRESERVATION.md` and continue with the
next unblocked item.
