# Reference trace format

Project Eon reference traces are external preservation evidence. They are not
game data, emulator snapshots, replay scripts, or a license to invent runtime
results. No trace, ROM, disk image, executable bytes, screenshots, audio, or
synthetic trace fixture belongs in this repository.

The initial implementation is validation and provenance reporting only. It
does not emulate a platform service, modify supplied media, create state, or
advance a game session. A game-specific adapter may consume an event only
after its caller, ABI and result are separately documented in
[`PRESERVATION.md`](PRESERVATION.md).

Format **v1** remains the generic identity-and-ordering format. Format **v2**
is not a general semantic-trace upgrade: it currently admits exactly one
strict diagnostics adapter, `millennium-dos-en-startup-v1`. It validates a
small set of declared observations against literal, hash-pinned source sites.
It neither replays the observations nor treats a validated result as a DOS,
private-driver, file, or child-process result.

## Pair and encoding

A capture directory contains a manifest selected with `--reference-trace` and
the event file named by its `event_file` record. Both are regular external
files. They use UTF-8 with LF line endings, ASCII keys and values, and one
record per line:

```text
key<TAB>value
```

Blank lines, comments, duplicate keys, unknown keys, path separators in
`event_file`, non-canonical hashes, oversized files and malformed records are
rejected. The manifest limit is 16 MiB and the event stream limit is 256 MiB.
The event filename is a basename so validation never opens a path outside the
manifest directory.

## Required manifest records

| Key | Rule |
| --- | --- |
| `format` | Exactly `project-eon-reference-trace-v1`. |
| `event_file` | Basename of the adjacent event stream. |
| `event_size` | Decimal byte count of that stream. |
| `event_sha256` | Lower-case SHA-256 of that stream. |
| `game`, `platform`, `language` | Exact Project Eon identifiers for the traced release. |
| `source_release_sha256`, `source_release_size` | Hash and decimal byte count of the original outer archive. |
| `capture_start_utc`, `capture_end_utc` | UTC capture timestamps retained as provenance. |
| `emulator_name`, `emulator_version`, `emulator_sha256` | Capture environment identity. |
| `config_sha256`, `command_tail_sha256`, `input_timeline_sha256` | Hashes of the recorded configuration, invocation and input evidence. |

Project Eon accepts the pair only when `--data` scans an already recognised
user-supplied release whose game, platform, language, outer SHA-256 and byte
size all match the manifest. Matching a filename, a similar regional release,
or another platform is insufficient.

### v2 adapter manifest addition

A v2 manifest has the same required records plus this one exact record:

| Key | Rule |
| --- | --- |
| `format` | Exactly `project-eon-reference-trace-v2`. |
| `adapter` | Exactly `millennium-dos-en-startup-v1`. |

This adapter is accepted only for the clean English Millennium DOS outer
release `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`.
It is deliberately not transferable to the Spanish DOS release, a filename
match, a modified executable, or another platform. A v1 manifest has no
`adapter` record; unknown or omitted records remain rejected in both versions.

## Event stream (v1)

Each event record is exactly `event<TAB>sequence tick type`, with decimal,
strictly increasing `sequence` and `tick` fields. The accepted event types are
`cpu`, `interrupt`, `file`, `memory`, `frame` and `audio`; the v1 generic
validator rejects any other type. It records no event payload semantics beyond
this ordering and identity check. A later adapter must define and validate raw,
type-specific evidence fields before consuming any event. Events are
intentionally not an execution request until that path-specific preservation
adapter exists.

## Event stream (v2 Millennium DOS adapter)

Each record remains one LF-terminated `event<TAB>` line with strictly
increasing decimal `sequence` and `tick`. The remainder is a type followed by
single-space-separated `key=value` fields. Every field is required exactly as
shown below; additional, omitted, duplicate, reordered-result, or
nearby-address records are rejected. The entries describe a recorder's
observation at a verified original site, not an instruction for Eon to perform
the operation.

| Type | Exact declared schema |
| --- | --- |
| `interrupt` | `image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000` |
| `interrupt` | `image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4` |
| `interrupt` | `image=titles.exe pc=0x0d0a int=0x21 ah=0x06 dl=0xff` |
| `interrupt` | `image=titles.exe pc=0x1a12 int=0x21 ax=0x4c00` |
| `file` | `image=mill.com pc=0x02cf op=driver-load path=ega640.bin` or `path=mcga.bin` |
| `exec` | `image=mill.com pc=0x0337 int=0x21 ax=0x4b00 path=titles.exe` or `path=2200ad.exe` |

For example, this is a declaration of the already documented raw vector
request, **not** evidence that the vector was installed:

```text
event	1 10 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000
```

The adapter does not infer carry flags, register returns, selected video
driver, file-open/load completion, DOS behaviour, private `INT 91h` dispatch,
or an executed child. It reports only counts by type after validating the
external trace's hash and source-release provenance.

## Command-line boundary

Use a trace only with an explicit original-media location, game and platform:

```sh
project-eon --data /path/to/original-media --game deuteros --platform amiga \
  --reference-trace /path/to/capture/manifest.eontrace
```

Validation prints provenance only. It rejects combinations with inspection or
verification modes and rejects a missing or mismatched source release. A trace
directory is never copied, unpacked, installed or changed.
