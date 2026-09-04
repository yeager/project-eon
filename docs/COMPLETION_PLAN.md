# Project Eon completion plan

This is the execution plan for completing Project Eon. It is intentionally a
preservation plan, not a date forecast: a phase is complete only when its exit
criteria are met with the identified original media. A green synthetic unit
test, a rendered placeholder, or a launcher card is never enough to claim
gameplay parity.

## Priority rules

1. A verified playable path through original media outranks all presentation,
   packaging, and convenience work.
2. A shared core rule or format outranks a platform-specific polish task.
3. Original-mode correctness outranks Modern improvements. Modern work may run
   in parallel only when it cannot change Original inputs, pixels, timing,
   media, or save bytes.
4. Every change that crosses an unknown ABI or a protected-media boundary must
   stop there, record the evidence, and leave the path unavailable rather than
   substitute another platform, release, asset, or behaviour.

## Phase 0 — Evidence baseline and work queue

**Current state:** in progress.

Create one release-by-release evidence inventory covering every recognised DOS,
Amiga, and Atari ST corpus member. Track exact archive/disk hashes, contained
file or raw-sector bounds, architecture, known execution entry points, real
reference-capture availability, and blockers. Keep it in the recovery map,
parity matrix, and preservation log rather than in private notes.

Exit criteria:

- Every recognised release has a truthful row in `docs/PARITY.md`.
- Each row distinguishes recognition, title/bootstrap execution, gameplay
  execution, input, rendering, audio, saves, and completion status.
- The top ten unblocked recovery tasks are ranked by their contribution to a
  playable game path and linked to exact source evidence.
- The matrix is source-parity tested against the compiled recovery map.

## Phase 1 — Millennium DOS: first end-to-end playable vertical slice

**Priority:** P0. Millennium DOS is the first target because its complete
installed data is available and its executable/resource formats are the most
directly inspectable.

Recover the handoff from `MILL.COM` through `TITLES.EXE`, `2200AD.EXE`, the
selected video/audio interfaces, and the first navigable game UI. The existing
hash-locked title flow, GX records, video-driver profiles, and positional-save
evidence are inputs; they are not a completed game session.

Work packages:

1. Capture genuine DOS execution at each currently opaque interrupt, EXEC,
   far-return, driver, and overlay boundary. Bind captures to executable and
   release hashes; record registers, memory ranges, frame pixels, audio, and
   input transitions separately.
2. Turn only captured deterministic segments into a small DOS compatibility
   layer. Keep interrupts/driver calls as typed contracts, reject unknown
   results, and retain original data as read-only byte spans.
3. Recover the first menu/game-state transition, canonical state model, and
   verified user actions. Add deterministic replay tests against reference
   captures before exposing an input in Original mode.
4. Decode the graphics/audio resources reached by that path and compare
   original-mode frames and sample sequences against capture hashes.
5. Recover save loading/writing into Eon's versioned, separate save area only
   after format and byte-level compatibility are demonstrated. Never write a
   user’s original save in place.

Exit criteria:

- CLI starts a named Millennium DOS release into a real navigable game state
  without an external emulator.
- The first complete input-to-frame-to-state path is replay-tested with genuine
  media and reference captures.
- Original mode matches all captured frame/audio/state checkpoints for that
  slice; Modern leaves the canonical state and Original output unchanged.
- Unknown actions and unproven device ABI results fail visibly and safely.

## Phase 2 — Deuteros Amiga: opening to title to first game state

**Priority:** P0, in parallel with Phase 1 only where the evidence work does
not compete for the same runtime infrastructure.

Extend the real clean ADF opening beyond the current title-stage boundary.
The current parser profiles are static provenance; only a hash-addressed FS-UAE
or hardware capture may admit the Exec, graphics, callback, input, and display
results needed to execute them.

Work packages:

1. Produce and maintain a reproducible, write-protected physical-input capture
   route using a known clean Kickstart and the two supplied clean ADFs. The
   repository now supplies `tools/run_deuteros_amiga_capture.py` for the
   reviewed external recorder; it mounts all source archives read-only and
   keeps raw output outside the repo. Commit only hashes, offsets, diagnostics,
   and a validator.
2. Record the title bridge in the existing reference-trace format: Exec and
   graphics calls/returns, callback installation and invocation, display-list
   writes, input queue observations, palette/bitplane state, and frame/audio
   checkpoints.
3. Implement a trace-gated Amiga compatibility bridge for exactly those
   captured outcomes. Every unobserved vector/call result remains a hard stop.
4. Decode the selected title and first game resources directly from the ADFs,
   derive the canonical Deuteros state transitions, and add replay tests.
5. Expand through the first actionable game screen, including its original
   input, rendering, timing, audio, and save boundary.

Exit criteria:

- The clean Amiga release reaches a captured, interactive Deuteros game state
  from CLI with no generated game data and no external emulator dependency.
- Captured title and game-state frames, palette, audio, callback order, and
  canonical state checkpoints are reproducible.
- No Exec/graphics result or input semantic is inferred from disassembly alone.

## Phase 3 — Shared deterministic game cores

**Priority:** P0 after each game has a real vertical slice.

Extract each proven game’s canonical simulation behind platform adapters. Grow
one evidence-backed subsystem at a time: calendar/tick scheduling, resources,
economy, construction/research, event resolution, UI model, AI, campaign
progression, and game-over/win conditions. Do not share rules merely because
two releases contain similarly named bytes.

Exit criteria:

- Every modeled rule has an original-code or trace provenance link and
  deterministic replay coverage.
- Long-run replays include campaign transitions, save/load, edge cases, and
  game completion checkpoints.
- The parity matrix can name remaining unsupported subsystems precisely rather
  than using a blanket “simulation incomplete” boundary.

## Phase 4 — All-platform adapters and release parity

**Priority:** P1. Add platforms only by mapping them to an already recovered
canonical rule or by proving a platform-specific divergent rule.

For Millennium, complete Amiga and Atari ST raw/bootstrap, loader, graphics,
input, audio, and save routes. For Deuteros, complete Atari ST protected-media
boot, XBIOS and its resource/runtime routes. Each edition remains explicit:
the launcher and CLI must never borrow another platform or language as a
fallback.

Exit criteria:

- Each recognised platform/release starts a playable canonical session or is
  explicitly marked unavailable with the exact preservation boundary.
- Cross-platform replay tests demonstrate the expected shared or divergent
  behaviour at documented checkpoints.
- Archive, ADF, ST, STX, and FAT/raw scanners remain bounded, immutable, and
  malformed-input resistant.

## Phase 5 — User journey, Original and Modern presentation

**Priority:** P1. Complete the already established card route only after the
target runtime paths are real: game card → verified platform/release card →
Original, Modern, or Custom → launch.

Original mode renders recovered graphics, audio, controls, aspect ratio and
timing. Modern is an opt-in renderer/input/accessibility layer on the same
canonical session. It may use lawful external high-resolution asset packs,
regenerated/upscaled sprites, scalable UI, improved input and accessibility,
but never alters Original assets, state, saves, or rules invisibly.

Exit criteria:

- CLI and menu create the same explicitly identified launch request.
- Unavailable/ambiguous media cannot be started; scanner diagnostics explain
  why without creating or modifying a data directory.
- F10 and Custom settings cover resolution, aspect ratio, renderer features,
  and accessibility without leaking input through the modal panel.
- All Eon UI and player-visible game strings are catalogued and tested in the
  20 shipped non-English catalogs. This applies equally to Original and
  Modern. Original in-game source bytes remain untouched and hash-addressed;
  only their presentation is localized.

## Phase 6 — Preservation records, diagnostics, and public documentation

**Priority:** P1 and continuous from Phase 0.

For each recovered path publish the exact media identity, source offsets,
runtime addresses, hashes, decoded formats, trace/capture recipe, uncertainty,
input controls, and verification commands. Keep the GitHub wiki and repository
documentation in English. Diagnostics must identify the release, profile,
boundary, and remediation without exposing commercial bytes.

Exit criteria:

- `PRESERVATION.md`, the wiki, recovery map, parity matrix, and CLI diagnostics
  agree and are source-parity tested where mechanically possible.
- Every public “play” instruction names supported media, controls, limitations,
  and save locations honestly.
- Reproduction recipes never require distributing original data or committing
  captures, ROMs, derived commercial pixels, or user saves.

## Phase 7 — Packaging and continuous verification

**Priority:** P2, but preserve existing CI while recovery work proceeds.

Maintain CI for Linux, macOS (arm64 and x86_64), Windows, and iPad build
outputs. Produce unsigned/non-published test artifacts only until a maintainer
explicitly asks for a release. Ship Debian, RPM, AppImage, Inno Setup, macOS
app, and sideloadable IPA packaging with no original media.

Exit criteria:

- All platform artifacts build from a clean checkout and launch the scanner
  without bundled game data.
- Linux/macOS defaults are `~/.projecteon`; Windows uses `<install-dir>/data`.
  Missing paths are not created merely by lookup.
- CMake/Ninja, CTest, Python preservation tests, `gitleaks detect`,
  `git diff --check`, and genuine-media tests pass for relevant changes.

## Phase 8 — Release-readiness audit

**Priority:** P0 once the feature matrix says both campaigns are complete.

Perform a requirement-by-requirement audit of both games, every selected
platform/release, Original and Modern contracts, controls, saves, localization,
documentation, CI artifacts, legal-media guarantees, and security scans.
Fix all P0/P1 gaps, repeat genuine-media replays, and push verified commits to
`main`. Do not create a GitHub release without explicit maintainer approval.

## Immediate ranked queue

1. Finish the evidence inventory and turn its real gaps into tracker issues.
2. Capture and validate Millennium DOS’s currently opaque handoff/device ABI.
3. Capture and validate the Deuteros Amiga title bridge from clean ADFs.
4. Implement the first trace-gated compatibility bridge for each capture.
5. Recover each game’s first actionable state and original input mapping.
6. Add frame/audio/state replay fixtures from genuine media captures.
7. Expand each canonical simulation through the next campaign subsystem.
8. Map equivalent or divergent Amiga/Atari platform paths.
9. Finish Original/Modern UX, i18n, and accessibility over real sessions.
10. Perform packaging and end-to-end readiness verification continuously, then
    repeat it before any maintainer-approved release.
