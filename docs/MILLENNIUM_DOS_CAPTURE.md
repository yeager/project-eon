# Millennium DOS startup capture recipe

This document records the current acquisition boundary for the first
Millennium 2.2 DOS runtime evidence.  It is intentionally a capture recipe,
not an emulator setup guide and not a claim that the game is playable.

Four external captures have been assembled and independently CLI-validated under
this recipe. That validation proves only the declared release identity,
provenance hashes, ordering, and narrow event grammar. In particular, no event
in this repository represents an emulator observation, and validation does
not prove a DOS/private-driver result, title transition, input result, frame,
audio checkpoint, or playable game state.

## First trace-validated capture (diagnostics only)

On 2026-08-30, the reviewed external recorder was built from the pinned
DOSBox-X revision and run against the English archive through an
`archivemount -o ro` FUSE mount. The mount was verified as
`ro,nosuid,nodev`, and the archive hash was identical before and after the
run. The first recorder build emitted one raw driver-load request before a
20-second headless smoke run was stopped. After correcting its interrupt hook
to match the actual `INT` opcode at `0x020c` while retaining the declared
`0x0209` setup-site identifier, the rebuilt recorder emitted this two-event
sequence:

```text
event 1 1 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin
event 2 2 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000
```

The raw event files remain outside this repository. The corrected two-event
file has SHA-256
`dc4a67ce61ed6bcd32767c2bf354444f525176ba81308cb9374d7718d1b7aa9a`; its
recorder executable has SHA-256
`122869702f46b5eda8f9f3ded1032c2e466dd6b0bfafaa460742ef4cd5712dc0`.
The explicitly configured 2026-08-30 capture was assembled with the source
archive hash checked before and after the run, and CLI validation accepted its
two ordered events through `millennium-dos-en-startup-v1`. The private
assembled manifest SHA-256 is
`79b77f399ebc0dd14494e9a6c0f0c7d55c13bbd864bba008f0f7f29cd7c885fa`.

| Private capture field | SHA-256 / value |
| --- | --- |
| Capture UTC | `2026-08-30T03:49:37Z` to `2026-08-30T03:50:00Z` |
| Explicit recorder configuration | `ba56ee4f77e3e982e5ddb36680190ec17b109e496bad8452cc0a0a4c895d3291` |
| Literal command tail | `ff9c433831db4975cefa7ee23b646cee2eba1ad676aad4f8df2a48ce36b72ca8` |
| Empty guest-input timeline | `f8a02c12d4fed2b0f8374ac5137e6565d1d9ca520cfe49d9e194376e7d7ad704` |
| Recorder source-review anchor | `234797680781567e18c374c9e62da24de5423db0` |
| Recorder-reported version | `DOSBox-X 2026.08.02 source e522642` |

The FUSE mount was `ro,nosuid,nodev`, and it was unmounted after the run. The
newly assembled directory contains private copies of the exact configuration,
command-tail, input-timeline, metadata, event stream, manifest, and receipt;
it contains no original release bytes. The CLI's `REFERENCE TRACE VERIFIED`
report is diagnostic provenance only and does not authorize Eon to replay the
capture or cross any result boundary.

The preliminary one-event file remains retained with SHA-256
`402b411a0a6958e2fd425bdc6dcdf4cedb1cee3a9fac9437745d6fa7b63e1c76` and
its executable has SHA-256
`5c1132e7a78b36703aa24347e08ba5d8c48fd4fb385a2412f9ce1818d895af09`.
An additional alternate video-mode run emitted the bounded `ega640.bin`
driver-load request (event-file SHA-256
`1a7c4cee85ba64a159d167948ca0162fc1425ba72f4738bae9dcb7d57f21c874`).
The retained DOSBox-X log for the corrected observation is
`6bf278b48d4d7fd05211afce73433775c21fc9995dd998bf6e9d90c749eb84ef`.
Every run was intentionally interrupted before a complete request sequence,
return value, input result, title state, frame, audio checkpoint, or game
state was observed. It is therefore a trace-validated diagnostic provenance
record, not a runtime input or a gameplay/parity claim.

### First raw result reconnaissance (not a v2 event)

The same exact archive and explicit read-only route were used for a separate,
private result probe on 2026-08-30. Static 8086 bytes establish that the
`INT 21h` opcode at `$020c` returns to `$020e`; the probe reads `AX` only when
the normal core reaches that instruction, before the original `PUSH CS`.
Its raw output SHA-256 is
`d09a4c840a4d9f1877b033a6e67f9a96b9ca7201ca5fddf932ddcaa2ed60c391`:

```text
raw-result 1 1 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591
```

The probe executable hash is
`cf0f3d67d2f9ca8857f9daf749136b19ed625918764ba23fd02b284c65423f78`.
The next caller-connected point is the return from original `CALL $0511` at
`$0210`, observed before `AND AL,AL` at `$0213`. No `$0213` record appeared
before the host ended this input-free run. This does **not** establish that
the call cannot return, nor any function meaning, result flag, branch,
driver outcome, title, input, frame, audio, or game state. The raw result has
no v2 grammar entry, is not assembled, and is not a runtime input.

### Command-tail sound-choice continuation (diagnostics only)

A second, separately assembled run on 2026-08-30 used the same unchanged
archive and read-only FUSE route, but recorded the literal original guest DOS
command tail `mill.com 0`. It injected no keyboard, pointer, controller, or
other guest input. This is an explicit, reproducible original command-tail
condition, not a replacement asset, host-hardware policy, runtime input, or
claim that any input path is generally recovered.

The recorder executable SHA-256 was
`cf0f3d67d2f9ca8857f9daf749136b19ed625918764ba23fd02b284c65423f78`.
The external event stream SHA-256 was
`cb55a2ad7935da29ff2b698be0abb162a1d61532a750d4cb585b1cd4a366c89f`, and
the assembled private manifest SHA-256 was
`e7779652cef7d89eb56682ece688968b926a80534318b2f33f20b9a6856683ac`.
CLI validation accepted these three ordered diagnostic observations:

```text
event 1 1 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin
event 2 2 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000
event 3 3 interrupt image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4
```

The private raw result probe additionally observed `AX=$0000` at `$0213`,
the original continuation immediately after `CALL $0511`; its SHA-256 is
`8eaf872a3bcf02c8d10b29a361236694306a2241a72ecec52b503538cb903406`.
That raw result remains outside the v2 event grammar and is not replayed or
consumed by Eon. The sole new admitted v2 fact is the title wrapper request
at `TITLES.EXE:$0127`. It neither establishes its return, title rendering,
input result, frame, audio, child execution, nor playable state.

| Private capture field | SHA-256 / value |
| --- | --- |
| Capture UTC | `2026-08-30T04:00:43Z` to `2026-08-30T04:01:06Z` |
| Explicit recorder configuration | `a0c5302a719fc8f070b79ed57ffa3095b0c8e66ab0cc208b2d30cabb484a2f9c` |
| Literal command tail | `adb9588ac15cecf16445efacb9bc94696b857b264479d2f73a4801f720eef39e` |
| Explicit no-device-input timeline | `2b1edf6ba255c41fb7099e3b3a610b9591014aa691d9e0f55a84ab6397f0deb1` |
| Recorder source-review anchor | `234797680781567e18c374c9e62da24de5423db0` |
| Recorder-reported version | `DOSBox-X 2026.08.02 source e522642` |

The archive SHA-256 was the required
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` before
and after the run; the FUSE mount was unmounted afterwards. All raw events,
result observations, and provenance preimages remain outside this repository.

### Title private-vector return reconnaissance (not a v2 event)

A third private run on 2026-08-30 retained the same literal guest command
tail `mill.com 0`, no device-input injection, unchanged archive, and
read-only FUSE route. Its separately opt-in result observer reads only `AX`
when original `TITLES.EXE` reaches `$0129`, the first instruction after its
observed `INT $91` opcode at `$0127`. The raw file SHA-256 is
`4f26dcaa3320de13d3118f202bf5cad2750b68a887969e7f44d311b361a988e6`:

```text
raw-result 3 3 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0101
raw-result 4 4 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0000
```

These are two ordered raw observations at one return site; they do not assign
meaning to either word, infer interrupt flags, record-buffer writes, branch
selection, rendered output, or an input/game state. The observer did not
change guest registers, memory, vectors, flags, timing, or input. Its
executable SHA-256 was
`2343c8ce86ef4fa3b07a7655ae28baba1a00bbcd0f08dad1dee2c9aac4775164`.

The same three v2 diagnostic events were separately assembled and accepted;
the private manifest SHA-256 is
`3d4ffdf3b566ec8e2c77d54cdbc4a246c2708542b0404720cea2942644e9f2dc`.
The capture ran from `2026-08-30T04:06:59Z` to `2026-08-30T04:07:22Z`; its
configuration, command-tail, and input-timeline SHA-256 values are,
respectively,
`64d592591591ee34d1d8cea68053b7a4602ac5f3888d1fce35b519d257bded2f`,
`2c35ec229f5b76975246f28b70429e23667197147bb6268f3a4493c9d4250687`, and
`2b1edf6ba255c41fb7099e3b3a610b9591014aa691d9e0f55a84ab6397f0deb1`.
The archive hash was checked unchanged before and after, and the mount was
unmounted. No raw result or preimage is stored in this repository or consumed
by the runtime.

### First strict title-init v2 trace (diagnostics only)

The fourth private run on 2026-08-30 promoted only the already observed,
ordered title-wrapper records into the dedicated
`millennium-dos-en-title-init-v2` capture grammar. It used the same original
guest command tail `mill.com 0`, no device-input injection, normal CPU core,
read-only FUSE route, and unaltered English DOS archive. Its candidate event
stream SHA-256 is
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda`:

```text
event 1 1 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin
event 2 2 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000
event 3 3 interrupt image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4
event 4 4 private-return image=titles.exe pc=0x0129 int=0x91 ax=0x0101
event 5 5 private-return image=titles.exe pc=0x0129 int=0x91 ax=0x0000
```

The external assembler and Eon CLI accepted all five ordered records. The
private manifest SHA-256 is
`c1081a1107c7f7bdd047c237bba8b4564321ca949d5d429d92899caf4c4664fd`; the
capture interval was `2026-08-30T04:17:32Z` to `2026-08-30T04:17:55Z`. The
recorder executable SHA-256 was
`bc8796acc3748db743352beac7a77797bd4f633d1ff30f0d90b282882691d695`.
Its exact configuration, command-tail, and input-timeline hashes were,
respectively,
`0b0a0ce24ed1b2bbfda0b9fe558aec63d340be3b2ef8f0a97af32445c808c866`,
`96a17ae1c4bb95d2290bf430c358bd16f7a3c95961be037f09b8a19ab2a5ad3c`, and
`2b1edf6ba255c41fb7099e3b3a610b9591014aa691d9e0f55a84ab6397f0deb1`.

This strict v2 acceptance is still diagnostics-only. It proves the declared
raw records and provenance, not the `INT $91` ABI, flags, record writes,
branch consequences, title pixels, input semantics, audio, EXEC, or a game
state. The trace is never replayed and neither return word is supplied to Eon
runtime code. The archive was rehashed unchanged before and after the run and
the FUSE mount was unmounted; raw outputs and all preimages remain external.

### Independent recorder repeat (diagnostics only)

On 2026-08-30, a fresh 45-second normal-core observation repeated the same
literal guest command tail, `mill.com 0`, through a newly created
`archivemount -o ro` view of the exact English DOS archive. It used the later
host-key-receipt recorder build (SHA-256
`0ba7a23b75ed543e519e56c6ece7106b81bd1fd8efb3e1b3813b79ca44b71cca`) but
did not configure, inject, or claim any host input. The read-only mount was
confirmed `ro,nosuid,nodev`, and the archive retained SHA-256
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` before
and after the run.

The new external event stream is byte-identical to the strict five-record
candidate above (SHA-256
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda`). Its
separate raw-result file is likewise byte-identical to the previously retained
four observations (SHA-256
`4f26dcaa3320de13d3118f202bf5cad2750b68a887969e7f44d311b361a988e6`): the
post-DOS `MILL.COM:$020e` word, the post-call `MILL.COM:$0213` word, and the
two `TITLES.EXE:$0129` words. This repeat corroborates recorder stability for
that bounded, no-input condition only. It supplies no flags, memory writes,
delivery receipt, poll result, frame, audio, `EXEC`, title state, or playable
path, and no new record is admitted into runtime code. The raw files,
configuration, and emulator log remain outside the repository and supplied
media.

### Explicit keyboard-delivery probes (not input evidence)

Two further private probes used DOSBox-X's built-in `AUTOTYPE` mapper command
to schedule one ordinary `Enter` keypress after, respectively, five and
fifteen seconds. Both runs used the same normal-core recorder, a freshly
mounted `archivemount -o ro` view of the supplied archive, and a 28--30 second
host timeout. The archive SHA-256 was unchanged before and after each run;
each FUSE mount was `ro,nosuid,nodev` and was unmounted immediately afterwards.

Neither probe is admitted as an input capture. Both produced exactly the
existing five-event candidate stream (SHA-256
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda`) and
the existing four raw return observations (SHA-256
`4f26dcaa3320de13d3118f202bf5cad2750b68a887969e7f44d311b361a988e6`). In
particular, neither emitted the recorder's bounded `TITLES.EXE:$0d0a` input
poll, `$1a12` exit, or `MILL.COM:$0337` EXEC observation. The current
headless recorder has no independent host-side delivery acknowledgement for
that mapper request, so identical output must not be interpreted as proof
that the key reached the original program, was polled, was accepted, or chose
any title/game action.

The private command timelines, raw files, and compressed emulator logs remain
outside this repository. The emulator repeatedly reported an x86 segment
limit violation while the title wrapper remained within its existing private
ABI boundary. Inspection of the pinned recorder's generated reference
configuration establishes that its default `segment limits=true` rejects a
word transfer at `ES:DI=$ffff` rather than applying the original real-mode
wrap. This is recorder configuration behaviour, not evidence of an original
game fault or input semantic.

The operator-facing capture helper now pins `segment limits=false` together
with the existing normal core. The configuration is written and hash-bound in
every external capture directory, so it is reviewable and cannot silently
change Eon's runtime or supplied bytes. It does not inject input, alter guest
memory, or promote a capture into an input result. A future input capture must
still provide a physical host-key receipt and a trace schema that binds a real
poll/result/frame sequence; Eon does not retry the prior AUTOTYPE timings as a
substitute.

### Wrap-compatibility probe (not input evidence)

A fresh 15-second input-free probe used that explicit configuration against the
same archive through a newly verified read-only mount. The source archive
remained `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`.
It retained the existing five-event stream
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda` and the
existing four raw results
`4f26dcaa3320de13d3118f202bf5cad2750b68a887969e7f44d311b361a988e6`; no
host-input receipt was created.

The segment-limit loop did not recur, which isolates it as a recorder
configuration defect. The recorder then emitted repeated unhandled `INT 6`
diagnostics instead. This is a separate emulator/driver configuration boundary
already observed during sound-selection research, not an original-game crash,
input result, title frame, or playable handoff. The probe is not admitted into
any runtime adapter.

The capture helper now drains the emulator console into a fixed 1 MiB external
prefix while recording the full transcript's SHA-256 and byte count in
`run-status.txt`. This prevents an emulator exception loop from exhausting a
terminal or cache while retaining an auditable identity for the complete raw
diagnostic stream. It does not drop or reinterpret the hash-bound event and
result records.

## Audited local route

The only source release eligible for the current English DOS adapter is the
user-owned outer archive with this identity:

| Field | Value |
| --- | --- |
| Game/platform/language | `millennium` / `dos` / `en` |
| Outer archive size | `328383` bytes |
| Outer archive SHA-256 | `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` |
| Required adapter | `millennium-dos-en-startup-v1` |

On the capture host examined on 2026-08-30, the installed `/usr/bin/dosbox`
was DOSBox `0.74-3-5build3` for arm64, with binary SHA-256
`12ddd009fcb0372b492d90632cf8394ef5adbe04fbacb733dd622d872d38e1a8`.
Its documented command line provides normal configuration, directory/disk
mounting and media captures; it does not provide a documented machine-readable
CPU, interrupt, EXEC, or register-return recorder.  Its capture directory is
therefore not evidence for the reference-trace adapter.

The release is a ZIP archive containing a directory tree.  DOSBox 0.74's
`MOUNT` command accepts a host directory, not that archive.  A previously
created writable extracted duplicate in the local tools cache is *not* an
eligible media route: it violates Eon's no-unpack/no-copy rule.  Do not mount,
repair, re-chmod, or use that copy for a capture.

`archivemount` is available on this host and explicitly supports `-o ro`.
It can expose the owner-supplied archive through a read-only virtual directory
without extracting or modifying it.  That makes it suitable only as the lower,
original-data layer.  A fuller run may write a save or other guest state, so a
separate disposable copy-on-write layer must be provided before the game is
allowed to write.  Do not point a writable DOSBox mount at the archive, its
read-only VFS, the real-data directory, or any extracted cache copy.

## Required recorder, not just DOSBox 0.74

Use an emulator build with a debugger/instrumentation hook at instruction
execution and DOS dispatch.  A debugger can help establish the first sites,
but a screenshot, a hand-transcribed register window, or a generic CPU dump
is not itself an admissible event stream.  The recorder must write the exact
LF-only event records at the moment it observes them and retain its unmodified
raw log separately.

The locally cached DOSBox-X source/build is a possible investigation tool: it
contains instruction and interrupt breakpoints and optional heavy CPU logging.
That is not an endorsement of its emulation behaviour, and its generic heavy
log is not Eon's event format.  If it is used, retain its exact executable
SHA-256, source revision, configuration, command tail, and raw log alongside
the capture.  A small, reviewed recorder hook is still required to emit only
the schema below; it must observe and serialize values, never alter registers,
interrupt vectors, memory, file results, input, or guest timing.

The first hook must identify the loaded image by its original DOS name and
observe only these byte-locked request sites:

| Image | PC | Observation |
| --- | --- | --- |
| `mill.com` | setup site `0x0209`; `INT` opcode `0x020c` | `INT 21h`, `AX=0x2591`, `DX=0x0000` |
| `titles.exe` | `0x0127` | `INT 91h`, `AX=0x0000`, `ES=CS`, `BX=0x1ac4` |
| `2200ad.exe` | `0x0124` | `INT 91h`, `AX=0x001f`, `ES=CS`, `BX=0xd19e` |
| `titles.exe` | `0x0d0a` | title input poll: `INT 21h`, `AH=0x06`, `DL=0xff` |
| `titles.exe` | `0x1a12` | `INT 21h`, `AX=0x4c00` |
| `mill.com` | `0x02cf` | driver-load request for `ega640.bin` or `mcga.bin` |
| `mill.com` | `0x0337` | `INT 21h` EXEC request for `titles.exe` or `2200ad.exe` |

These are request observations only.  They do not establish a DOS return,
private `INT 91h` result, driver behaviour, selected display mode, child
execution, input outcome, or navigable state.  Capturing any such result
requires a new, separately reviewed schema and evidence boundary before Eon
can consume it.  Do not add guessed return values to the current adapter.

## Safe capture procedure

All capture material remains outside the repository and outside the
user-supplied media tree.  Use a new, scoped path such as
`/home/yeager/.cache/project-eon-tools/millennium-dos-capture-<UTC>`; never
use `/tmp`.

For the reviewed external DOSBox-X recorder, the repository includes a
preflight helper that creates this read-only route but does not contain,
build, or modify the recorder itself:

```sh
python3 tools/run_millennium_dos_capture.py \
  --source-release /absolute/path/to/Millennium-Return-to-Earth_DOS_EN.zip \
  --recorder /absolute/path/to/reviewed/dosbox-x \
  --output /home/you/.cache/project-eon-tools/millennium-dos-capture-<UTC>
```

It rehashes the exact archive before and after a fresh `archivemount -o ro`
mount, requires `ro,nosuid,nodev`, forces DOSBox-X's normal CPU core, and
sets exclusive external paths for event, raw-result, and host-input-receipt
files. It refuses `/tmp`, repository/media output paths, headless SDL, and a
missing visible X11/Wayland display. The operator must press keys in the
visible emulator window; the helper has no AUTOTYPE, mapper, debugger, or
guest-memory input path. `run-status.txt` records whether the recorder
actually created a host-input receipt, and hashes it only when present; an
absent or empty receipt remains explicit no-input evidence rather than a
generated empty timeline. The receipt is also capped at 64 KiB before it is
hashed. Its output is raw external evidence only and still requires
assembly, independent validation, and review before any new adapter or runtime
route can exist.

1. Hash the owner-supplied outer archive before and after the run.  Both
   values must equal the table above.  List the ZIP directory with a read-only
   tool only; do not extract it.
2. Create a new empty capture directory and a separate VFS mountpoint beneath
   that scoped cache path.  Mount the archive with `archivemount -o ro` (and,
   where supported, select only the archive's game-root subtree).  Record the
   exact `archivemount` executable/version and mount command in the retained
   command-tail preimage.  Verify the mount is read-only before starting an
   emulator.
3. If the observed path reaches a guest write, stop the run unless a separate
   reviewed copy-on-write union presents the read-only VFS as its lower layer
   and a newly empty disposable upper layer as its only writable target.  Hash
   and retain the union tool/configuration.  The upper layer is capture
   by-product, never original media and never an Eon input.  DOSBox 0.74 alone
   does not provide this merged read-only-plus-COW game directory.
4. Configure the instrumented emulator with explicit CPU, memory, machine,
   video, sound, timing, mount, and `autoexec` settings.  Mount only the
   verified VFS or reviewed COW view as the DOS game drive.  Retain the exact
   configuration text; do not use an implicit per-user default configuration.
5. Start `MILL.COM` from that mounted original directory.  The hook writes an
   unmodified raw recorder log and, only for the seven accepted site shapes,
   an LF-only candidate event file.  Stop if loaded image identity, registers,
   operand bytes, or event ordering disagree with the table.  Do not fill a
   missing event with a static-disassembly conclusion.
6. Retain an input-timeline preimage, including an explicit empty timeline if
   no keys were supplied.  Record UTC start/end times before normalizing
   nothing.  Hash the emulator executable, config, literal command tail, and
   input timeline with SHA-256.
7. Assemble the external evidence using
   [`record_reference_trace.py`](../tools/record_reference_trace.py) and the
   `millennium-dos-en-startup-v1` adapter.  Then validate it against the same
   owner-supplied archive using the CLI.  The CLI report is provenance-only;
   it must not be used to advance the game session.

The candidate event records must exactly follow the registered schema in
[`REFERENCE_TRACE_FORMAT.md`](REFERENCE_TRACE_FORMAT.md#event-stream-v2-millennium-dos-adapter).
The assembler must receive those exact configuration, command-tail and
input-timeline files as well as their hashes described in
[`REFERENCE_TRACE_ASSEMBLY.md`](REFERENCE_TRACE_ASSEMBLY.md). It checks and
retains their evidence copies in the private capture directory. Commit neither
the archive, mount, saved state, screenshot, audio, raw recorder output, or
trace pair.

The exact reviewed DOSBox-X hook locations, normal-core restriction, program
identity map, and non-mutation requirements are in
[`MILLENNIUM_DOS_DOSBOX_X_RECORDER.md`](MILLENNIUM_DOS_DOSBOX_X_RECORDER.md).
That design is an external recorder implementation contract, not an admitted
trace or a runtime integration.

## Acceptance boundary

The two strict v2 captures now retain five ordered observations: the startup
driver-load and DOS request, then the title private-vector request and its two
observed returns. They do not satisfy work-queue rank 1, whose acceptance
requires observed interrupt/EXEC/far-return and driver results through a real
navigable state. The remaining capture must use a real focused emulator input
route, record key press/release timing separately, and retain the title poll,
local exit, parent `EXEC`, child/far return, and device results. Recorder-side
keyboard injection, guest memory edits, and fabricated return values remain
inadmissible.
