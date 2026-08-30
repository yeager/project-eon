# Deuteros Amiga title-display capture status

This record is deliberately a status boundary, not an admitted reference
trace. It records a direct debugger observation made on 2026-08-30 from the
recognised English Amiga release, without storing source bytes, a frame, an
audio stream, a save state, or a generated image in the repository.

## Source and route

| Item | Value |
| --- | --- |
| Outer release SHA-256 | `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` |
| System ADF SHA-256 | `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38` |
| Data ADF SHA-256 | `99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a` |
| Kickstart 1.3 SHA-256 | `ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53` |
| Emulator | FS-UAE 3.2.35 |
| Local capture-config SHA-256 | `c2cc8f266b6c8a72b202705d99da78fb7af160d6781897e679fb7a6ee30282fd` |
| Media route | `archivemount` FUSE, mounted read-only; both floppy drives write-protected |

The configuration and every transient debugger dump remain outside the
repository under the project-scoped cache. The three temporary buffers used
solely to calculate the hashes below were removed immediately afterwards.

## Direct title-stage observations

The built-in UAE debugger stopped at the title-stage display-initialisation
site `0x0001eda6`. At that stop, executing the first load produced
`0x0000ab00`; the following two stores wrote that value to
`0x0001f168` and `0x0001f164`. This is a live observation of that one
execution, rather than a claim about every display update.

At the later bitplane-clear site `0x0001f182`, the live source pointer was
`0x0000ab00`. A contemporaneous custom-register sample exposed the following
state:

| Observation | Value |
| --- | --- |
| `COP1LC` | `0x00000420` |
| Plane 0 pointer | `0x0000b5f0` |
| Plane 1 pointer | `0x0000d530` |
| Plane 2 pointer | `0x0000f470` |
| Plane 3 pointer | `0x000113b0` |
| `BPLCON0` | `0x4200` |
| `BPL1MOD` / `BPL2MOD` | `0x0000` / `0x0000` |
| `DDFSTRT` / `DDFSTOP` | `0x0038` / `0x00d0` |

The four addresses are separated by `0x1f40` (8,000) bytes. Combined with
the observed 320×200, 40-byte-row, zero-modulo layout and the title stage's
hash-locked `0x1f40` longword clear loop, this is now an explicit v4 capture
admission boundary—not a claim that later display updates share it.

The following hashes were calculated from the observed RAM ranges at that
same paused sample. They identify the bytes without publishing them:

| Range | Bytes | SHA-256 |
| --- | ---: | --- |
| Copper list `[0x00000420, 0x00000478)` | 88 | `cf827847c13dbeafeea72c86f2c4fb90a6d717bf548f0914b2f203abb94293f6` |
| RGB4 palette destination `[0x00012ecc, 0x00012ef4)` | 40 | `5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55` |
| Four-plane contiguous range `[0x0000b5f0, 0x000132f0)` | 32,000 | `fad588ff5f6e0ec471cb4889987dab4a40c11d7da6e532564d48475149c68490` |

## Why this is not a v4 trace

The `deuteros-amiga-en-title-display-v4` contract requires an ordered v3
title-bridge prefix followed by canonical display, input, frame, and audio
checkpoints. This run only establishes the above debugger samples. In
particular, it did **not** produce:

- the ordered v3 callback/Exec prefix;
- an independently recorded game-input timeline (debugger keystrokes are not
  game input);
- a defined raw-to-RGBA conversion and a canonical RGBA frame checkpoint;
- a host PCM capture with sample rate, channel count, and frame count;
- a capture manifest with start/end times and command/input fingerprints.

The local route intentionally used `uae_sound_output=interrupts`, so FS-UAE
did not expose a host PCM stream for a falsely precise audio hash. The UAE
debugger can inspect RAM and custom registers, but it is not a complete v4
recorder. Consequently no `events.trace`, evidence manifest, runtime bridge,
or gameplay claim was created from these values.

## Next admissible capture

Use an external recorder which timestamps the ordered v3 sites and writes a
separate input timeline, captures the exact Copper/bitplane state at a
specified frame boundary, converts it with a documented RGB4 rule, and
captures PCM after mixing. Bind those files to the source identities above
with `tools/record_reference_trace.py`; only then may the v4 validator be
asked to admit the trace.
