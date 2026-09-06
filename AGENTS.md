# Project Eon working agreement

## Mission

Project Eon is a preservation-first SDL3 reimplementation of **Millennium
2.2** and **Deuteros**. Build the recovered game paths toward parity, but
never claim parity or invent behaviour that evidence does not support.

## Original media

- Original game data is user supplied and read in place only. Never unpack,
  copy, install, mutate, redistribute, or commit it.
- Prefer real supplied media in the local Downloads directory for verification; never
  create synthetic replacements when an original asset or state is available.
- Keep parsers bounded and hash-addressed. Treat malformed or unproven formats
  as explicit preservation boundaries.
- Use disassembly, raw-media inspection, and reproducible tests. Record file
  offsets, runtime addresses, hashes, and uncertainty in `docs/PRESERVATION.md`.
- Never commit or push raw/generated disassembly listings, decompiler projects,
  extracted executable bytes, emulator dumps, or original-derived report
  bodies. Keep them under `/home/yeager/.cache/project-eon-tools/`, outside
  both the repository and original media. Git may contain only the tools and
  tests needed to reproduce them plus preservation metadata such as hashes,
  offsets, address ranges, line counts, uncertainty, and function maps.

## Capture recorder restoration

- Normal installed DOSBox-X, DOSBox and FS-UAE are insufficient for admitted
  capture evidence. Do not ask again for generic emulator installation and do
  not substitute them for an exact reviewed recorder.
- Follow `docs/CAPTURE_RECORDER_RESTORATION.md`: locate a recorder by pinned
  SHA-256 before running a capture, use only fresh external cache output, and
  require visible manual emulator input. An empty locator result is an
  evidence boundary, not permission for AUTOTYPE, debugger input, memory
  injection, screenshots or manually transcribed registers.

## Runtime and UI

- Support every recognised DOS, Amiga, and Atari ST release without silently
  substituting another platform or language.
- The SDL3 app must start from CLI and the card menu. Original mode uses
  recovered pixels and is the preservation contract. Modern mode may add
  explicitly labelled opt-in graphics (including regenerated or upscaled
  renderer-side assets, not merely filters), input, accessibility, and
  evidence-documented gameplay improvements, but must not mutate or replace
  original asset bytes or original save files, and must never affect Original.
  Do not commit/package commercial original pixels or unlicensed derivatives.
- Default media locations are `~/.projecteon` on Linux/macOS and
  `<install-directory>/data` on Windows. Looking up a missing default path
  must not create it.
- Keep visible launcher text translatable through `src/i18n.*` and `po/`.
  Every user-presented game string (menus, item names, messages, help, and
  labels) must use the same localization selection in both Original and
  Modern. Preserve and hash-address the original source bytes separately;
  localization is a presentation mapping and must never mutate game media.
  A newly recovered visible string is incomplete until it has a stable game
  text key and translations in every shipped PO catalog.

## Build, tests, and packaging

- Run CMake/Ninja, `ctest`, Python preservation tests, `gitleaks detect`, and
  `git diff --check` for relevant changes. Configure `EON_REAL_DATA_DIR` for
  genuine-media tests when available.
- Do not use `/tmp` for builds, tools, mounts, traces, or temporary work.
  Use a scoped path beneath `/home/yeager/.cache/project-eon-tools/` instead;
  it must remain outside the repository and user-supplied game media.
- Set `TMPDIR` to an existing directory under that cache for local compiler
  and tool invocations; an external build directory alone does not redirect
  compiler scratch files. Run native tests through CTest, which supplies
  `EON_TEST_TMPDIR`, or set that variable explicitly for direct test runs.
- CI builds Linux, macOS, and Windows; creates non-published DEB/RPM, macOS
  arm64/x86_64, and Inno Setup artifacts. Never create a release unless the
  maintainer explicitly requests one.
- Package no commercial data. Keep direct archive input and data immutability
  covered by tests.
- Run `python3 tools/verify_repository_artifacts.py` before pushing; CI must
  reject tracked original media and raw/generated reverse-engineering output.

## Git workflow

- Work directly on `main`; do not create branches. Push verified changes to
  `origin main`.
- Preserve unrelated user changes. Use `apply_patch` for source/document edits
  and avoid destructive Git operations.
