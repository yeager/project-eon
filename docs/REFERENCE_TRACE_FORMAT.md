# Reference trace format v1

Project Eon reference traces are external preservation evidence. They are not
game data, emulator snapshots, replay scripts, or a license to invent runtime
results. No trace, ROM, disk image, executable bytes, screenshots, audio, or
synthetic trace fixture belongs in this repository.

The initial implementation is validation and provenance reporting only. It
does not emulate a platform service, modify supplied media, create state, or
advance a game session. A game-specific adapter may consume an event only
after its caller, ABI and result are separately documented in
[`PRESERVATION.md`](PRESERVATION.md).

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

## Event stream

Each event record is exactly `event<TAB>sequence tick type`, with decimal,
strictly increasing `sequence` and `tick` fields. The accepted event types are
`cpu`, `interrupt`, `file`, `memory`, `frame` and `audio`; the v1 generic
validator rejects any other type. It records no event payload semantics beyond
this ordering and identity check. A later adapter must define and validate raw,
type-specific evidence fields before consuming any event. Events are
intentionally not an execution request until that path-specific preservation
adapter exists.

## Command-line boundary

Use a trace only with an explicit original-media location, game and platform:

```sh
project-eon --data /path/to/original-media --game deuteros --platform amiga \
  --reference-trace /path/to/capture/manifest.eontrace
```

Validation prints provenance only. It rejects combinations with inspection or
verification modes and rejects a missing or mismatched source release. A trace
directory is never copied, unpacked, installed or changed.
