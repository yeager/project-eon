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
is not a general semantic-trace upgrade: it admits only the strict diagnostics
adapters `millennium-dos-en-startup-v1`, `deuteros-atari-st-boot-v1`,
`millennium-amiga-en-defjam-bootstrap-v1`, and
`deuteros-amiga-en-title-stage-v1`.
Their observations are checked against literal, hash-pinned source sites.
Neither replays observations nor treats a
validated result as a platform-service, private-driver, file, device, or
child-process result.

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
| `adapter` | Exactly one registered adapter described below. |

The existing DOS adapter is accepted only for the clean English Millennium DOS
outer release `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`.
It is deliberately not transferable to the Spanish DOS release, a filename
match, a modified executable, or another platform. A v1 manifest has no
`adapter` record; unknown or omitted records remain rejected in both versions.

`deuteros-atari-st-boot-v1` is accepted only for the English Deuteros Atari
ST outer archive `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653`.
Its v2 manifest adds `source_media_sha256`, exactly
`aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee`, and
`source_stage_sha256`, exactly
`2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7`.
This binds the report to the documented Replicants Disk 1 and copied
second-stage interval; it is not transferable to another Atari image,
development disk, repacked archive, or similarly named file.

`deuteros-amiga-en-title-stage-v1` is accepted only for the English Deuteros
Amiga outer archive `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`.
Its v2 manifest adds `source_media_sha256`, exactly
`6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`, for the
clean system ADF, and `source_stage_sha256`, exactly
`48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03`, for its
`ADF +0x6e000`, `0x6ca00`-byte title stage. This binds evidence only; it does
not make the title stage executable or transfer a trace into runtime.

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

## Event stream (v2 Deuteros Atari ST adapter)

`deuteros-atari-st-boot-v1` accepts only the external boot observations
required by the dynamic-trace acquisition boundary. Every record has strictly
increasing decimal `sequence` and `tick`. Hex words/longwords are lower-case,
zero-padded `0x` values. `input_frame` and `result_frame` are bounded,
even-length lower-case hexadecimal byte sequences, retaining the complete
raw XBIOS frame without declaring a service ABI or interpreting its result.

| Type | Exact declared schema |
| --- | --- |
| `trap` | `pc=0x00001edc incoming_a7=<u32> incoming_sr=<u16> selector=0x0026 callback=0x00001fa6 return_pc=<u32> return_a7=<u32> return_sr=<u16> return_d0=<u32>` |
| `callback` | `entry_pc=0x00001fa6 incoming_a7=<u32> stack_longword=<u32> outgoing_a7=0x0007b000 return_pc=<u32> return_a7=<u32> return_sr=<u16> return_d0=<u32>` |
| `state` | `ram_25f4=<u32> ram_25f4_provenance=<sha256> ram_25fc=<u32> ram_25fc_provenance=<sha256> branch_pc=<u32> state_word=<u16>` |
| `table` | `base=0x00001eac shifted_index=<u16> target_a1=<0x00001f1a\|0x00001f2e\|0x00001f50\|0x00001f52> entry_pc=<same target_a1> return_pc=<u32> return_d1=<u32> return_d2=<u32>` |
| `frame` | `site=0x00001e9c input_frame=<hex bytes> result_frame=<hex bytes>` |
| `raw-reader` | `entry_pc=0x00001e60 trap_pc=0x00001e9c call_a7=<u32> return_pc=<u32> return_a7=<u32> return_sr=<u16> return_d0=<u32>` |

The adapter validates and counts evidence only. It never calls XBIOS, installs
a callback, selects a table vector, replays a frame, supplies a result, or
uses trace data as game-media or runtime input.

## Event stream (v2 Millennium Amiga bootstrap adapter)

`millennium-amiga-en-defjam-bootstrap-v1` is a strict reserved schema for the clean English
Millennium Amiga outer release
`2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400`.
It can validate only the two byte-exact caller-side handoffs in the hash-pinned
Defjam bootstrap. The bootstrap's `ADF +0x400` source is copied to `$70000`,
so the two instruction addresses below are factual sites in that original
bootstrap; the first loaded stage remains opaque.

Each record is an LF-terminated `event<TAB>` line with strictly increasing
decimal `sequence` and `tick`. It must be exactly one of these schemas:

| Type | Exact declared schema |
| --- | --- |
| `cpu` | `image=bootstrap-loader pc=0x702e4 op=jsr-indirect a3=0x41000` |
| `cpu` | `image=bootstrap-loader pc=0x70320 op=jmp-indirect a3=0x68000 d6=0xa8d398fb` |

The first record declares the original indirect `JSR (A3)` handoff to the
first raw stage. The second declares the later terminal indirect `JMP (A3)`
handoff to the resident stage. Neither declares that a read completed, that
the JSR target ran or returned, that either transfer succeeded, a resulting
register/flag value, an AmigaOS call, or an executed resident instruction.
The exact source and boundary evidence are documented in
[`PRESERVATION.md`](PRESERVATION.md#millennium-amiga-raw-loader-evidence).

The registered adapter dispatches its event stream only for the outer hash
above and reports the CPU handoff count as diagnostics. It does not make the
opaque bootstrap executable or consume a trace as runtime input.

## Event stream (v2 Deuteros Amiga title-stage adapter)

`deuteros-amiga-en-title-stage-v1` admits raw post-observation result fields
only at already documented ABI boundary sites. `result_d0` and `result_sr` are
lower-case, zero-padded raw longword/word values; they are provenance, never
inputs to a reimplementation. Every record is LF-terminated and begins
`event<TAB>sequence tick`; sequence and tick strictly increase.

| Type | Exact declared schema |
| --- | --- |
| `exec` | `site=0x00040450 exec_base_address=0x00000004 vector=-0x0096\|-0x009c result_d0=<u32> result_sr=<u16>` |
| `open-library` | `site=0x0001ed80 name_address=0x0001ed02 exec_base_address=0x00000004 vector=-0x0228 result_d0=<u32> result_sr=<u16>` |
| `graphics` | `site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0 result_d0=<u32> result_sr=<u16>` |
| `custom-register` | `site=0x0004046c base=0x00dff000 offset=<0x0040\|0x0042\|0x009a\|0x0096> value=<0x7fff\|0xc000\|0x87ff> result_d0=<u32> result_sr=<u16>` |
| `callback` | Registration: `site=0x0001ef74 callback=0x0001f056 exec_base_address=0x00000004 vector=-0x01ce result_d0=<u32> result_sr=<u16>`; entry: `site=0x0001f056 incoming_a0=<u32> result_d0=<u32> result_sr=<u16>` |

It only verifies identity, grammar and literal caller sites, then reports
counts. It never opens `graphics.library`, calls Exec or a graphics vector,
switches privilege/state, writes custom hardware, invokes a callback, maps
`incoming_a0`, or treats an observed raw result as a service ABI, input,
timing, screen or title-stage execution result.

## Command-line boundary

Use a trace only with an explicit original-media location, game and platform:

```sh
project-eon --data /path/to/original-media --game deuteros --platform amiga \
  --reference-trace /path/to/capture/manifest.eontrace
```

Validation prints provenance only. It rejects combinations with inspection or
verification modes and rejects a missing or mismatched source release. A trace
directory is never copied, unpacked, installed or changed.
