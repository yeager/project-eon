# Reference trace format

Project Eon reference traces are external preservation evidence. They are not
game data, emulator snapshots, replay scripts, or a license to invent runtime
results. No trace, ROM, disk image, executable bytes, screenshots, audio, or
synthetic trace fixture belongs in this repository.

For a safe, non-emulating way to assemble recorder output into an external
manifest/event pair, see [REFERENCE_TRACE_ASSEMBLY.md](REFERENCE_TRACE_ASSEMBLY.md).
Assembly records source identity before and after the operation; it does not
admit, replay, or validate a trace's gameplay meaning.

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

After verification the CLI prints the three opaque capture fingerprints as
`config=`, `command-tail=`, and `input-timeline=` alongside the source and
event identities. This lets an independent reviewer compare two capture
environments without Project Eon opening those potentially private files or
turning their contents into runtime input. The recorder must retain the
preimages separately when disclosure is appropriate; a SHA-256 declaration
alone proves identity only when it can be compared to such retained evidence.

Capture timestamps are validated as real Gregorian UTC instants (including
leap years), not merely strings shaped like timestamps. They establish a
capture boundary and ordering only; they do not establish emulation timing.

### Versioned adapter registry and source identity

A v2 manifest has all v1 records, changes `format` to exactly
`project-eon-reference-trace-v2`, and adds exactly one `adapter` record.  It
does **not** have a common "v1 plus one field" shape: the two adapters that
observe a disk subrange additionally require both `source_media_sha256` and
`source_stage_sha256`.  Unknown, omitted, or surplus records are rejected, so
capture tooling must choose the row below before it writes a manifest.

| Adapter | Required original release (`game`/`platform`/`language`, bytes, SHA-256) | Additional required manifest fields | Bounded observation scope |
| --- | --- | --- | --- |
| `millennium-dos-en-startup-v1` | `millennium`/`dos`/`en`, 328383, `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` | None | Clean English DOS startup sites only. |
| `deuteros-atari-st-boot-v1` | `deuteros`/`atari-st`/`en`, 3021682, `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653` | `source_media_sha256=aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee`; `source_stage_sha256=2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7` | Replicants Disk 1 and its copied second-stage interval. |
| `millennium-amiga-en-defjam-bootstrap-v1` | `millennium`/`amiga`/`en`, 2558009, `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400` | None | Two caller-side Defjam bootstrap handoffs. |
| `deuteros-amiga-en-title-stage-v1` | `deuteros`/`amiga`/`en`, 4066771, `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` | `source_media_sha256=6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`; `source_stage_sha256=48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03` | Clean system ADF and `ADF +0x6e000`, 0x6ca00-byte title stage. |
| `deuteros-amiga-en-main-copy-loop-v3` | `deuteros`/`amiga`/`en`, 4066771, `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` | `source_media_sha256=6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`; `source_stage_sha256=a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6` | Clean system ADF and `ADF +0x5800`, 0x4200-byte main stage. |
| `deuteros-amiga-en-title-display-v4` | `deuteros`/`amiga`/`en`, 4066771, `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` | `source_media_sha256=6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`; `source_stage_sha256=48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03` | Clean system ADF and title stage; capture admission only. |

This table is a capture-admission registry, not an equivalence class.  An
archive with the same filename, a direct one-disk container that exposes an
identical leaf, a Spanish DOS release, a modified dump, or another platform
does not satisfy a row unless it is listed with its own full identity.  A v1
manifest has no `adapter` record.  The documentation table is regression
checked against the accepted adapter identifiers and release manifest so it
cannot silently lose a current source boundary.

### Declarative diagnostic boundary map

After a v2 event stream has passed both its own grammar and the complete
outer-release rehash, Eon reports the following exact recovery-map rows. This
is an inspectable declarative function-map-style cross-reference: it says
which documented original-byte boundaries the adapter is allowed to describe.
It does not add a hook, emulate a call, or give the trace authority over the
runtime. A missing or mismatched row rejects the trace rather than printing a
similar platform's evidence.

| Adapter | Reported recovery-map rows |
| --- | --- |
| `millennium-dos-en-startup-v1` | `millennium-dos-launcher`, `millennium-dos-title-flow`, `millennium-dos-game-flow` |
| `deuteros-atari-st-boot-v1` | `deuteros-atari-protected-boot`, `deuteros-atari-first-stage` |
| `millennium-amiga-en-defjam-bootstrap-v1` | `millennium-amiga-defjam-bootstrap`, `millennium-amiga-shared-resident` |
| `deuteros-amiga-en-title-stage-v1` | `deuteros-amiga-main-stage`, `deuteros-amiga-title-handoff` |
| `deuteros-amiga-en-main-copy-loop-v3` | `deuteros-amiga-main-stage` |
| `deuteros-amiga-en-title-display-v4` | `deuteros-amiga-main-stage`, `deuteros-amiga-title-handoff` |

For the two physical-media adapters, the CLI also prints their already
validated `source media` and `source stage` hashes. This makes an independent
reviewer's retained trace report complete enough to identify the bounded disk
input without reading or copying any original data.

### Capture retention and review procedure

The trace pair is intentionally insufficient on its own to reproduce an
observation.  A recorder should retain, outside this repository and subject to
the rights of the media owner, the original archive, emulator binary,
configuration, complete command tail, input timeline, and raw recorder output
whose hashes are declared by the manifest.  Record the exact UTC interval
before post-processing.  Then create the LF-only event file, calculate its
byte count and SHA-256, and write the manifest with the exact adapter row
above.  Do not edit event ordering, normalize an observed result, or replace a
source archive after calculating its identity.

An independent reviewer first supplies the same owned archive under `--data`,
then validates the pair with the explicit game and platform command shown at
the end of this document.  They compare the three opaque capture fingerprints
and retain the CLI report with the capture.  A successful report proves only
that the declared capture is structurally valid and bound to the stated
original bytes; it does not prove emulator correctness, timing, service ABI,
or gameplay behaviour.  The event file and its private preimages are never
copied into an Eon data directory or treated as runtime input.

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
| `interrupt` | `image=2200ad.exe pc=0x0124 int=0x91 ax=0x001f es=cs bx=0xd19e` |
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

### Required capture contract before a Deuteros Amiga runtime increment

The current adapter is intentionally insufficient to advance the title stage:
its result fields establish only that an observation occurred at a hash-pinned
call site.  A future runtime adapter must not infer the missing state from a
successful `exec`, `open-library`, `graphics`, or callback record.  Before it
can model even one additional original title iteration, a retained external
capture must provide all of the following as ordered, raw observations bound
to this same clean system ADF and title-stage hash:

- both `$40450` Exec-vector returns, the subsequent `$1ed80` OpenLibrary
  return, and the exact graphics-vector/custom-register call and return order;
- the callback-registration result at `$1ef74`, then one actual callback
  entry at `$1f056` with the complete caller-owned A0 frame bytes read by the
  recovered local arms (`+4`, `+6`, `+8`, `+a`, and `+c`), not merely an A0
  address;
- the pre/post values of the title response queue at `$1eec0..$1eed3` and
  pending word `$1eed6`, plus the exact callback-table selection source at
  `$1ee20..$1eebf` identified by its existing source hash;
- the incoming D0 at `$1fe7a`, both local `$1feaa` call/return observations,
  and the pre/post dispatch cells `$1f98c`, `$1f98e`, `$1f99c`, `$1f974`,
  `$1f970`, `$1f96c`, `$1f994`, and `$1f998`.

This is a capture-retention contract, **not** an extension of the accepted
v2 grammar. The records must remain outside the repository and are not
runtime inputs. A future adapter must first define bounded field encodings,
source-site ordering, frame lengths and hash checks, then independently prove
the external service and caller ABIs before it can consume any observation.

### Deuteros Amiga title-bridge v3 capture profile

`project-eon-reference-trace-v3` with adapter
`deuteros-amiga-en-title-bridge-v3` is the machine-checked retention form of
the preceding contract. It binds the same exact English Amiga outer archive,
source-media hash, and title-stage hash as v2. It is a diagnostic evidence
profile only: validation records ordered raw observations but never supplies
them to a title-stage session, calls an Exec or graphics vector, or advances a
game/runtime state.

Every event uses the existing LF-only `event<TAB>sequence tick type fields`
envelope with strictly increasing sequence and tick. Its required ordered
segments are two `exec-return` records at `$40450`, one
`open-library-return` at `$1ed80`, one or more nested `graphics-call` /
`graphics-return` or `custom-register-call` / `custom-register-return` pairs,
and `callback-registration-return` at `$1ef74`. It then requires a pre queue
snapshot at `$1eec0`, a callback entry at `$1f056` with the full ten raw bytes
at A0 `+4..+d`, and a post queue snapshot. Finally it requires the `$1fe7a`
selector input, the two observed `$1feaa` local call/return pairs, and pre/post
snapshots of every documented dispatch cell at `$1fbe6`.

Queue records pin the 20 queue bytes, pending word, and the 160-byte
`$1ee20` source-table SHA-256. Graphics/custom pairs preserve their exact
call nesting; a return cannot be reassigned to a different observed call. All
addresses, vectors, widths, raw results, source-table hash, and allowed custom
register/value pairs are checked by the adapter. This admits neither an
emulator's undocumented service semantics nor a replay input.

### Deuteros Amiga main-copy-loop v3 capture profile

`deuteros-amiga-en-main-copy-loop-v3` admits exactly one external PC
observation from the clean main stage (`ADF +0x5800`, length `0x4200`, loaded
at `$20000`):

```text
event<TAB>sequence tick main-copy-loop-pc pc=0x000210d4 opcode=0x51c8
```

### Deuteros Amiga title-display v4 capture profile

`project-eon-reference-trace-v4` with adapter
`deuteros-amiga-en-title-display-v4` is an admission contract for a future,
externally captured title display. It retains the complete v3 title-bridge
prefix, followed by exactly one of each of these ordered raw checkpoints:

| Type | Required fields |
| --- | --- |
| `display-layout` | `site=0x0001eda6`, source/destination addresses, observed display base/list, and a copper-list SHA-256 |
| `bitplane-layout` | `site=0x0001f182`, observed base pointer, four plane pointers, and geometry/modulo |
| `palette-checkpoint` | `site=0x0001eda6`, source/destination, word count, RGB4 and converted RGBA palette SHA-256 values |
| `input-checkpoint` | callback/selector sites and queue/input-timeline SHA-256 values |
| `frame-checkpoint` | observed display base, RGBA dimensions, bitplane and RGBA-frame SHA-256 values |
| `audio-checkpoint` | sample rate, channel/frame count and PCM SHA-256 value |

The fixed source identities are the clean English outer release
`f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`, system
ADF `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`, and
title-stage hash `48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03`.

The schema validates provenance and recorder completeness only. It does not
accept a generated frame, supply a display base or palette, replay audio,
invoke a callback, or advance a title/game session. Until an independently
recorded v4 capture exists outside this repository, the runtime remains at its
documented title-stage boundary.

The mandatory main-stage SHA-256 prevents a PC observation from being
mistakenly attributed to the later title stage, which overlays part of the
same RAM address range. The adapter validates only the exact `DBF` site and
reports one observation. It does not infer registers, a caller, copy count,
source or destination, a completed copy, a visual meaning, or a return.

## Command-line boundary

Use a trace only with an explicit original-media location, game and platform:

```sh
project-eon --data /path/to/original-media --game deuteros --platform amiga \
  --reference-trace /path/to/capture/manifest.eontrace
```

Validation prints provenance only. It rejects combinations with inspection or
verification modes and rejects a missing or mismatched source release. A trace
directory is never copied, unpacked, installed or changed.
