# Capture recorder restoration

Project Eon has installed normal emulators (`dosbox-x`, `dosbox`, and
`fs-uae`), but ordinary emulator output is not preservation-admissible runtime
evidence. It may be used only for visible troubleshooting. It must never be
substituted for a reviewed recorder, interpreted as a trace, or used to infer
an original ABI, input result, frame, audio result, or game state.

## Required external recorders

The recorder binaries, their source trees, patches, raw output and build
directories remain outside this repository, outside original media, and
outside packages. Restore one of these exact executables to a scoped external
cache before asking Project Eon to run an evidence capture:

| Game | Protocol | Required SHA-256 | Role |
| --- | --- | --- | --- |
| Millennium DOS | `v21-int93-installation` | `18ec0ead7d08deeca694fbbe8155d5f5e6a99562adaea22fe914a691961fe1f1` | Read-only DOSBox-X observer for the current vector-installation boundary |
| Millennium DOS | `v13-title-poll` | `07d80df74d303b519884d37dd474da071b414e98396e8ae030ad89256432521b` | Host-key to original title-poll chronology only |
| Deuteros Amiga | reviewed FS-UAE v10 | `0e0bfb1fe73a6f37dc38992b39e34e355564adc516106c399c8be86fb38232ec` | Raw PC, host-delivery and title-armed display-write observer |

The default restoration location is a new directory under
`/home/yeager/.cache/project-eon-tools/`, for example
`/home/yeager/.cache/project-eon-tools/recorders/`. Do not place a recorder in
the checkout, `~/.projecteon`, an original-media directory, `/tmp`, or a
package staging tree.

## Locate before running

Never provide a path based on a filename or version string. Locate the restored
binary by its pinned digest:

```sh
python3 tools/locate_capture_recorder.py \
  --kind millennium-dos \
  --recorder-protocol v21-int93-installation \
  --root /home/yeager/.cache/project-eon-tools

python3 tools/locate_capture_recorder.py \
  --kind deuteros-amiga \
  --root /home/yeager/.cache/project-eon-tools
```

An empty result is a hard preservation boundary. It is not permission to use
`/usr/bin/dosbox-x`, `/usr/bin/fs-uae`, AUTOTYPE, debugger input, guest-memory
injection, a screenshot, or a hand-transcribed CPU window as a replacement.

## Millennium DOS capture once restored

Use the recognised English source archive, not an extracted copy:

```sh
python3 tools/run_millennium_dos_capture.py \
  --source-release /absolute/Downloads/Millennium-Return-to-Earth_DOS_EN.zip \
  --recorder /absolute/path/reported/by/locator/dosbox-x \
  --machine-profile svga_s3 \
  --capture-intent physical-input \
  --output /home/yeager/.cache/project-eon-tools/millennium-dos-capture-YYYYMMDD-NN
```

The output directory must be new and must not already exist. The helper mounts
the archive read-only, creates no game-data files, and requires a visible
display. Enter keys only in the focused, visible emulator window during the
capture window. It has no automated input route.

Verify the completed evidence before any recovery work:

```sh
python3 tools/verify_capture_receipt.py \
  --kind millennium-dos \
  --capture /home/yeager/.cache/project-eon-tools/millennium-dos-capture-YYYYMMDD-NN
```

## Deuteros Amiga capture once restored

Use the exact runner arguments documented in
[`DEUTEROS_AMIGA_FS_UAE_RECORDER.md`](DEUTEROS_AMIGA_FS_UAE_RECORDER.md).
The same rules apply: a fresh external output directory, read-only original
media, visible operator-driven input only, then
`tools/verify_capture_receipt.py --kind deuteros-amiga` before use.

## Current recovery boundary

As of 2026-09-02, both locators returned no matching recorder under the
project cache or Downloads. Project Eon must retain that fact and continue
unblocked work, but it must not ask again for generic emulator installation:
the installed normal emulators are known and insufficient. The next required
input is either an absolute path to an already pinned recorder, or an external
reviewed source/patch/build which produces the exact documented digest.
