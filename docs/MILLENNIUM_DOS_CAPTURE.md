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

### V13 physical key-to-poll chronology (one no-input capture; no input evidence admitted)

The external v13 DOSBox-X build is hash-bound as
`07d80df74d303b519884d37dd474da071b414e98396e8ae030ad89256432521b` and is
selectable only as `--recorder-protocol v13-title-poll`. It preserves the
existing visible-window requirement and produces no input receipt until a
physical SDL key event reaches the emulator. For each new observed key ordinal,
it can retain one bounded raw observation at the original
`TITLES.EXE:$0d0a` `INT 21h/AH=06h` poll. The receipt independently verifies
that the poll's claimed ordinal exists in the host-key file. This proves only
the recorder's ordering of two observations; it is not evidence of key
delivery to DOS, AL/carry output, accepted menu input, frame, palette, audio,
`EXEC`, private return, title state, or playable gameplay.

On 2026-08-31, one fresh operator-visible, write-protected `svga_s3` v13
session completed through the external receipt verifier. The window was
created, but the host-key receipt is absent and the result stream contains no
`title-input-poll`: the reviewed early-stop condition arrived after 0.62
seconds, before the original title's documented poll. Its source archive,
recorder, event stream and raw-result stream respectively retained SHA-256
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`,
`07d80df74d303b519884d37dd474da071b414e98396e8ae030ad89256432521b`,
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda`, and
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`.
The generated 340-byte configuration and 4,606-byte console transcript have
SHA-256 `badb070cce4eee9e531aa9f5d087124f70950f24a327c92e466c827d5225f688`
and `e0500c47370caa57b72ad1c3ca68a3655b465c9dc4f300c1f7e68ee21a3e8938`.
This is only a reproducible negative reachability result: it proves neither a
physical key event nor guest input delivery. A future operator still needs two
write-protected captures containing host-key-and-poll chronology, followed by
separately reviewed display/audio observations, before any reference-trace or
runtime admission can be proposed.

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

Receipt v4 additionally enforces a 64 MiB total-console safety cap. When a
recorder crosses it, the helper keeps draining only until it can terminate the
child, writes an explicit `recorder_console_over_limit=true` receipt, and
rejects the directory as inadmissible evidence. This retains the bounded
diagnostic prefix for review without allowing a known exception loop to spend
the whole physical-observation window on host I/O. A verifier accepts v2 and
v3 historical receipts, while v4 requires `recorder_console_over_limit=false`.

Receipt v2 first bound the retained console prefix as well as the complete
transcript; current Millennium captures write `capture_receipt_version=10`.
V5 additionally validates a present recorder-owned host-key receipt as at
most 256 contiguous ASCII records in the reviewed SDL grammar and records its
count. Scancodes, symbols and modifiers remain opaque host observations until
a genuine DOS poll/result/frame sequence proves their original meaning.
Verify a completed external capture without reading game media:

```sh
python3 tools/verify_capture_receipt.py \
  --kind millennium-dos --capture /absolute/cache/capture-directory
```

Pre-v2 capture directories remain diagnostic-only: their retained console
prefix cannot be integrity-verified. Reproduce the physical capture with the
current runner; do not edit or upgrade an older receipt.

The helper additionally verifies the reviewed recorder executable before it
opens a mount or writes an evidence directory. Its required SHA-256 is
`0ba7a23b75ed543e519e56c6ece7106b81bd1fd8efb3e1b3813b79ca44b71cca`.
An arbitrary DOSBox-X binary, including one with similar visible behaviour,
cannot produce a Project Eon capture receipt.

### CPU-profile discrimination (diagnostics only)

Three additional 15-second, input-free observations establish that the
remaining loop is neither an Eon observer side effect nor solved by choosing
a different CPU label. Each used the same read-only, rehashed English archive
and is external-only evidence:

| Recorder / CPU configuration | Observed result | Consequence |
| --- | --- | --- |
| Unmodified DOSBox-X build, `cputype=auto` | `INT 6` loop; 933,800,899 console bytes, SHA-256 `d10a91cdf2f728b1e833f123718e38d9ea35a170252415676b061c9ea4db6c94` | The loop is not caused by Eon's CPU/DOS/GUI observer hooks. |
| Reviewed recorder, `cputype=286` | Same `INT 6` loop; 856,661,123 console bytes, SHA-256 `f565bcd77f6036f9e3a16f7377561c4b27783f723a2933e9241e4925feafba69` | A 286 label is not a compatible substitute for the default profile. |
| Reviewed recorder, `cputype=8086` | No `INT 6`; instead repeated writes to ROM at `f4725`; console SHA-256 `2d6bb4684f25f73c964b7a23d8dc7fdebcba78c9931faa55593368fd53f9fa8b` | This configuration selects DOSBox-X's 8086 core path, bypassing the recorder's required normal-core pre-instruction hook. Its two-record result is incomplete and inadmissible. |

The first comparison is especially important: it retains no Eon event/result
files and therefore cannot be promoted to a trace, but it isolates the failure
to the emulator's machine/driver behaviour. The `8086` route produced only
the setup and title private-interrupt observations (event-stream SHA-256
`f61dba5b38f7eaa067e1d9f4a943309fb9dd4d9c9231027cd133b9aa28d8bc5b`) and no
raw-result file. Project Eon must keep `core=normal` with its reviewed hook;
it must not switch CPU cores merely to suppress diagnostics.

### Unhandled-interrupt machine-state reconnaissance (not trace evidence)

The reviewed diagnostic recorder now has executable SHA-256
`7b959f7aee3d2db0513db4f14e3075f306e798e25adaeeebd96aedd81aef65da`.
Its first 15-second, input-free run retained the unchanged five-event stream
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda` and,
after the four earlier raw values, exactly one additional raw observation:

```text
fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 ss=0x0a8d sp=0xc9bf \
return_ip=0x1900 return_cs=0x0e70 return_flags=0x7047 code=0x00000000 \
ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101
```

The callback PC is DOSBox-X's default exception-vector handler, not an
original Millennium location. The three following stack words and all-zero
four-byte read are raw callback-context values only; they do not establish a
faulting instruction, a loaded driver segment, a return ABI, or a game result.
The hash-bound linear byte inventory independently maps the observed
`return_ip=$1900` to `TITLES.EXE` file offset `+$1800` under its stated
COM-style candidate origin and renders the first four source bytes there as
zero-valued, unclassified bytes. This corroborates only the recorder's
four-byte value for that candidate mapping; it does not prove that the stack
return uses that mapping, identify an invalid opcode, or explain why the
original execution reaches the exception vector.
This probe confirms that the `INT 6` loop happens after the existing title
prefix and remains an emulator/driver boundary. Its 522-byte raw-result file
SHA-256 is
`7c00214e4461f6d442ea66b2413ae32c8e4a12210cf9dbef67b0cfad3af19f06`;
the input receipt was absent. Neither output is admitted into a runtime adapter
or reference trace.

### Receipt-v2 end-to-end probe (not input or gameplay evidence)

On 2026-08-30, the current runner performed a fresh 15-second input-free,
write-protected capture against the same supplied archive. Its external
directory was independently accepted by
`verify_capture_receipt.py --kind millennium-dos`: source archive, recorder,
generated configuration, optional artifacts, complete console identity and
the retained 1 MiB console prefix all matched receipt v2. It retained the
same five-event stream
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda` and the
same 522-byte raw-result identity
`7c00214e4461f6d442ea66b2413ae32c8e4a12210cf9dbef67b0cfad3af19f06`.

The recorder timed out normally after 15 seconds, created no host-input
receipt, and retained 1,048,576 console bytes. The complete console was
1,017,458,719 bytes with SHA-256
`f22756f5b57ac516a075b44317440a4af3f6a4acc9eff7dcd2dd712773e5cdee`; the
retained prefix SHA-256 was
`8445ff9b71f82d6904408ad2e95e57d303b05915033bed3ed4e786f2e04c1f38`.
This validates the bounded external-evidence route, not a key delivery, DOS
input result, title frame, driver ABI, or gameplay state.

### Receipt-v3 end-to-end probe (not input or gameplay evidence)

On 2026-08-31, the same 15-second, input-free, write-protected route was
repeated after the runner added strict raw-result grammar and count binding.
The external v3 receipt was accepted by `verify_capture_receipt.py`; the
outer archive remained the recognised `e6e7044b…9cab2a123` identity. It binds
the five-record, 522-byte raw-result file
`7c00214e4461f6d442ea66b2413ae32c8e4a12210cf9dbef67b0cfad3af19f06` as one
each at `mill.com:$020e`, `mill.com:$0213`, and the unhandled-interrupt
callback boundary, plus two at `titles.exe:$0129`. No host-input receipt was
created. This is a recorder-format verification only: the fields are not an
interrupt ABI, title-frame, audio, input, or gameplay admission.

### Receipt-v4 console-cap probe (explicitly rejected diagnostic)

On 2026-08-31, the real English DOS archive was rechecked before and after a
fresh write-protected run of the v4 helper. It retained its recognised outer
identity `e6e7044b…9cab2a123`, but the reviewed recorder reached the new
64 MiB total-console cap after 2.57 seconds. The resulting external receipt
records `exit_status=125`, `recorder_console_total_bytes=67210697`, retained
only the 1 MiB prefix (SHA-256
`66beea17c88f9157bfee27e362446a60af8cf2b6a034ca07c74f2dd53c0ab5c0`), and
sets `recorder_console_over_limit=true`. The verifier correctly rejects that
directory as inadmissible.

The recorder-owned raw files still have the pre-existing five-record result
shape and a host-input receipt was present (861 bytes, SHA-256
`ea1ce9317251609bf38bc5863ebbf3502c0c23fbdfc07ed7780c0e809274a854`). The
cap is crossed before an independent guest-side poll/result/frame link can be
observed. Therefore neither that host-side record nor any raw diagnostic is
admitted as an input delivery, private DOS ABI, title state, audio, frame, or
gameplay fact. All raw evidence remains outside the repository and supplied
media.

### Receipt-v5 host-key grammar probe (explicitly rejected diagnostic)

The v5 runner was exercised against the same rehashed English DOS archive. It
reached the 64 MiB console boundary after 2.35 seconds, writing
`exit_status=125`, `recorder_console_total_bytes=67188395` and
`recorder_console_over_limit=true`; no host-key receipt was created. It bound
the same five raw diagnostic records, and the corrected verifier rejected the
directory as inadmissible. This confirms that v5's host-key grammar/count gate
does not weaken the v4+ console-overrun boundary. It supplies no DOS input,
title, frame, audio, ABI, or gameplay fact.

### Receipt-v6 EGA machine-profile probe (explicitly rejected diagnostic)

On 2026-08-31, the v6 helper ran the exact English DOS archive through its
separately declared `machine_profile=ega` configuration. The archive and
reviewed recorder identities remained `e6e7044b…9cab2a123` and
`ab53ed0e…15f50325`; the 335-byte configuration has SHA-256
`bb091ae1d7ea2306019999e1c0616f7e0ae09c18b027402a1e12a9e66b028014`.
The recorder still observed the original `mcga.bin` load request, followed by
the existing title request/return prefix and the same `INT 6` callback shape.
Its 522-byte raw-result file has SHA-256
`978b239dc3823b3b2beed746b1ac5b441b83fc8d4c7157c9b1e10c290507d205`.

The console crossed the 64 MiB cap after roughly 2.03 seconds
(`67,189,462` bytes; retained-prefix SHA-256
`fcf11e2109f8d821aa9c87667aceebdac6813cd0c46fb41fa05dc4e425922a2c), so
the v6 verifier correctly rejected the receipt. This is negative configuration
diagnostics only: `machine=ega` did not force an EGA driver request in this
observed prefix, and neither the request nor the rejected raw output proves a
driver result, a title, input delivery, or gameplay.

### Receipt-v7 private-vector probe (explicitly rejected diagnostic)

On 2026-08-31, the v7 recorder added one bounded IVT observation at the
already byte-locked `TITLES.EXE:$0127` request. With the default `svga_s3`
profile, it recorded `INT 91h` vector `087e:0000` before original interrupt
dispatch. The exact six-record, 622-byte raw-result file has SHA-256
`d98893c7b2da41b611b7445781881839348757305730c698cb79ab34c8fd1a45`; its
reviewed-recorder identity is
`1bacb843a3c1684ce4da78cac809ef6e272b5fdabb7262a01cda2b9b1b571665`.

The same run reached the console safety cap after roughly 2.19 seconds
(`67,187,894` bytes; retained-prefix SHA-256
`081b7bf216671e623f99c703045b717a74b154d54eb2ad4d6015d9e11b77f255`) and
the v7 verifier rejected it. The raw vector endpoint is not evidence that a
specific driver loaded successfully, that bytes at `087e:0000` executed, or
that any handler return, title state, frame, audio, input, or gameplay
occurred. It is retained solely as the next hash-bound runtime-address
boundary for recovery.

### Receipt-v8 private-handler-entry probe (explicitly rejected diagnostic)

On 2026-08-31, the v8 recorder retained one additional raw control-flow
observation. After the same `TITLES.EXE:$0127` IVT snapshot (`087e:0000`),
the normal CPU core reached exactly `087e:0000` before the two previously
observed post-`INT 91h` words at `TITLES.EXE:$0129`. The exact seven-record,
688-byte raw-result file has SHA-256
`4708431edd9552150298e5df64aadcc9bd36f27064c765a5ceec936cc94e361b`; the
reviewed recorder is SHA-256
`6cd6be57b3487d9141b360de209fe9d21205ddd3cefefe2b065b1831be63b2be`.

This configuration again hit the console safety cap (`67,225,827` bytes;
retained-prefix SHA-256
`0e092c09f5e7dc836341695705dfec400f319b17d6ccbf9db8da41a61142617b`) and is
therefore rejected by the v8 verifier. The new edge proves only that the
normal core transferred to the captured IVT endpoint in this raw diagnostic
run. It does not prove a specific loaded-image identity, decode the endpoint,
assign its return ABI, or establish title output, input, audio, or gameplay.

### Receipt-v9 private-handler-return probe (explicitly rejected diagnostic)

On 2026-08-31, the v9 recorder bound the first raw caller re-entry to the
previously observed normal-core handler edge. It recorded the ordered sequence
`TITLES.EXE:$0127` → IVT `087e:0000` → `TITLES.EXE:$0129`, with raw
`AX=$0101` and `FLAGS=$7202` at that first caller re-entry. The exact
eight-record, 786-byte raw-result file has SHA-256
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`; the
reviewed recorder is SHA-256
`7b959f7aee3d2db0513db4f14e3075f306e798e25adaeeebd96aedd81aef65da`.

The run was again rejected after its console exceeded the 64 MiB cap
(`67,200,061` bytes; retained-prefix SHA-256
`30dc6a266fa9c1f1d723e7dcca1f1c93a2474fd3a90ca5a95bb6ca0310e37510`). This
is an ordered raw-state observation only. It does not identify handler bytes,
assign a meaning to `AX` or flags, establish the following title branch, or
prove title rendering, input, audio, or gameplay.

### Receipt-v10 bounded callback-loop stop (diagnostics only)

On 2026-08-31, the capture helper gained an explicit host-side stop condition
for the already documented, recorder-owned `INT 6` callback boundary. It polls
only the recorder's newly completed, bounded `results.raw` file and kills the
external DOSBox-X process after the exact validated raw fault record appears.
It does not install an interrupt vector, write guest memory, alter registers,
inject input, or let a partially written line qualify. It accepts the reason
only for the complete ordered eight-record raw diagnostic sequence already
observed: two `MILL.COM` returns, the `INT 91h` vector/entry/return chain, two
`TITLES.EXE` re-entries, and exactly one fault. A missing, reordered,
malformed, or repeated record remains under ordinary timeout/safety-cap
handling. The helper records
`termination_reason=known-unhandled-interrupt` with exit status `126`; receipt
v10 requires that finite reason/status relationship.

The first fresh input-free, write-protected run stopped after 0.71 seconds,
before any console overrun. The archive retained its recognised SHA-256
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`; the
reviewed recorder remained
`7b959f7aee3d2db0513db4f14e3075f306e798e25adaeeebd96aedd81aef65da`.
Its 786-byte, eight-record raw-result stream is unchanged from the v9
diagnostic (`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`).
The 4,636-byte complete console digest is
`91ba9451979bac2b8ae8c128cd9c25517c64bdcc847103b7550646f2e3a0508f`; it was
not truncated or over the safety cap. `verify_capture_receipt.py` accepted the
external receipt. This verifies that the runner can retain the known emulator
boundary efficiently. It does not make the diagnostic a reference trace or
prove driver behaviour, title pixels, input acceptance, audio, a gameplay
state, or an original-game fault.

On 2026-08-31, the same reviewed recorder and the unchanged, write-protected
English archive were run again through the capture helper after the ordered
v10 sequence gate was introduced. The externally retained receipt was accepted
by `verify_capture_receipt.py` and stopped after 0.71 seconds with
`termination_reason=known-unhandled-interrupt` and exit status `126`. Its
eight-record `results.raw` is byte-identical to the prior observation (786
bytes, SHA-256
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`), and
its five-record event stream is likewise unchanged (367 bytes, SHA-256
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda`). The
new configuration identity is
`cc9c804489a994e4104f07acd70f6e3f3f9db65fb256a7710b99e8e6bb4bbffb` (344
bytes); the bounded 4,633-byte console identity is
`07d42e63460748f842a4875336addcf4412f77ef42e2cd82803f654ef6b1cac9`.
No host-input receipt was created and the archive still hashed to
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` after
the run. This is a reproducibility check of the recorder boundary, not new
evidence for a driver result, title, input, frame, audio, or gameplay state.

### Receipt-v11 exact callback-loop receipt (diagnostics only)

The current helper retains the v10 historical verifier and emits receipt v11
for new runs. It permits the early host stop only when the complete 786-byte
raw result stream is byte-identical to the two independently retained
observations (SHA-256
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`). This
binds the two returns, private-vector chain, fault site, stack words, code
word, and recorded general registers as one external recorder diagnostic. A
change to even one byte is not classified as the known loop and instead
remains subject to ordinary timeout or console-safety handling.

This is a stricter host-process safety condition only. It does not assign an
ABI to the private interrupt, explain the exception, replay the record,
provide a guest result, or establish title, input, frame, audio, or gameplay
behaviour.

On 2026-08-31, a fresh 15-second operator-visible `svga_s3` run exercised the
v11 helper through a new `archivemount -o ro` view of the recognised archive.
The verifier accepted the external receipt. It stopped after 0.72 seconds with
`termination_reason=known-unhandled-interrupt` and exit status `126`; the
source archive retained SHA-256
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` after
the run. No host-input receipt was created. The five-record event file is
again 367 bytes with SHA-256
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda`, and the
complete raw result receipt is again 786 bytes with SHA-256
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`. The
fresh generated configuration has SHA-256
`16fc67bb158262056a1acc0d95d14a620b0fe6ee43c2355d18ba0d94127a1ffe` (336
bytes). This validates the new v11 host stop and recorder-route
reproducibility only; it establishes no guest handler result, title, input,
frame, audio, ABI, or gameplay behaviour.

A second independently prepared v11 directory repeated that exact input-free,
operator-visible route on 2026-08-31. The receipt verifier again accepted the
directory after the source archive was rehashed unchanged. It stopped after
0.81 seconds with `known-unhandled-interrupt` / exit `126`; no host-input
receipt was recorded. Its event and raw-result identities remained exactly
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda`
(367 bytes) and
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`
(786 bytes). The fresh configuration and bounded console identities were
`1183643e54d075ab3c2f455ccd942b249d54ee6d01410fa237b9faba4566d9c8`
(336 bytes) and
`08faffb6881c4311eb80cbcfad219abec8545cc6b1251d0d548fb4be457acc45`
(4,625 bytes). This is a third independently retained reproduction of the
same recorder stop condition, not evidence for an original interrupt result,
title, input, frame, audio, or gameplay transition.

The historical next experiment was built as the separately hash-identified
v12 predecessor recorder below. Its contract retained only normal-core
`CS:IP`, four raw bytes, and a recognised-image flag; it installed no vector,
wrote no guest state, injected no input, and changed no scheduling. The later
v14 history observation extends that same diagnostics-only boundary without
turning any callback tuple into a private-interrupt ABI, title result, or
gameplay claim.

### Receipt-v12 predecessor observation (diagnostics only)

That separate experiment is now retained as the explicit
`v12-predecessor` recorder protocol. Its external DOSBox-X binary is
127,284,720 bytes with SHA-256
`20a5ec331ca71e541d2f6d42c1ab49eca0fec5dabf298b6faf51fa45c63c24ed`.
The helper accepts it only when `--recorder-protocol v12-predecessor` is
spelled explicitly; v11 remains the default and still requires its older
byte-exact early-stop receipt.

The v12 fault record adds exactly four observational fields after the v11
register dump: `predecessor_valid`, `predecessor_cs:ip`, four bytes at that
tuple, and `predecessor_recognised_image`. The recorder stores those values at
the normal-core instruction boundary and only serializes them if its own
unhandled-interrupt callback is reached. It neither maps an additional image
nor changes a vector, guest register, guest memory, input, scheduler, DOS
result, or title flow.

Two separately prepared, operator-visible, write-protected `svga_s3` captures
on 2026-08-31 passed the receipt verifier. Both retained the same five event
records (367 bytes, SHA-256
`eaa6c537373b5a3e118f769c740ba97b59ba78595351685ec2ad79e05f7e0cda`) and
the same eight-record v12 result stream (909 bytes, SHA-256
`ad55dc005728deb5381eb6434259cde2555074ceac1690d9552e44b2af38d393`). The
new final record says `predecessor_valid=1`, `predecessor_cs:ip=f000:ca60`,
`predecessor_code=fe380300`, and `predecessor_recognised_image=0`. The first
and second generated configurations were respectively 340 bytes with
SHA-256 `15783df2e33e0afad5d09136f93d681f1ab6d5d2c52afb0fd78b900b5cb0a8ae`
and `c1a8da432d78de33382d560fa14dae599cc6d196d715466a87108f22be4ac789`.
Neither run produced a host-input receipt; both stopped with external status
`126` / `known-unhandled-interrupt`, and both rehashed the source archive to
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` after
the run.

This reproducibly locates the recorded predecessor outside the recorder's
currently recognised original-image map. It is therefore evidence about the
emulator callback boundary only. It is not evidence for a guest `INT 6`
instruction, a private DOS ABI, a title result, executable handoff, display,
input, audio, simulation, or gameplay state; it is not added to a reference
trace or consumed by Project Eon runtime code.

### Receipt-v14 normal-core history (diagnostics only)

The separate `v14-normal-core-history` recorder is an external DOSBox-X build
of 86,233,096 bytes, SHA-256
`748c1c934a78a28baef083fc352b552644f9665bc27fc032db0fdd7463ee5c63`, from
the reviewed upstream revision
`234797680781567e18c374c9e62da24de5423db0`. It keeps a fixed 16-entry ring
of normal-core prefetch tuples (`CS:IP` plus four raw bytes) in recorder memory
only. The sidecar is created with exclusive, no-follow host-file semantics
only when the already documented default callback reaches `INT 6` at
`f000:ca64`; it neither writes guest state nor changes input, vectors,
scheduling, timing, or the established v13 `results.raw` grammar.

Two fresh read-only `svga_s3` runs on 2026-09-01 passed the receipt verifier.
Their `results.raw` remains the existing eight-record v13-compatible stream
(786 bytes, SHA-256
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`). Both
independently produced the identical 344-byte normal-core sidecar, SHA-256
`248969bc16cfd773f64140ff3e314f6cd465ad7514de0868d24803b399bf4dbb`:

```text
0e70:18e4 .. 0e70:1900 = 00 00 00 00 (15 two-byte normal-core steps)
f000:ca60 = fe 38 03 00
```

This proves only the recorder's immediate execution history: execution reaches
the default callback after a sequence of zero bytes in the observed
`0e70:18e4`–`0e70:1900` context. It does not prove why that segment:offset was
selected, whether it represents original code, or any game behaviour. The
next admissible investigation is a separately reviewed, read-only origin
observer for the control transfer into that context; bypassing the callback or
synthesizing a return remains forbidden.

For an early `known-unhandled-interrupt` stop, the capture verifier now admits
this sidecar only if all sixteen tuples match that twice-observed sequence and
the separate raw fault is the established `f000:ca64` default-callback receipt.
It records the non-semantic boundary label
`observed-zero-context-to-default-callback`. This binds the diagnostic to its
actual evidence rather than treating arbitrary grammar-valid tuples as the
same boundary. It remains capture-tool metadata only and is not a runtime,
reference-trace, loader, mapping, or gameplay assertion.

Read-only static inspection adds one negative mapping result. Under the
already established flat COM-style `IP - 0x100` file-offset convention,
`0e70:18e4` and `0e70:1900` correspond to `TITLES.EXE` offsets `0x17e4` and
`0x1800`. The hash-bound `TITLES.EXE` member (7,022 bytes, SHA-256
`3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6`) has
nonzero bytes at both offsets, so the V14 zero sequence is
not compatible with that specific map. The hash-bound `2200AD.EXE` member
(`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`) has
one zero-filled range at file offsets
`0x124b`–`0x26c3`, which includes both candidate offsets. This is only
compatibility, not attribution: archive inspection does not prove that
`2200AD.EXE` was resident at `CS=0e70`, nor establish a loader, segment, or
return origin. No literal near/far branch encoding to either candidate was
found in the executable members; that syntactic absence is likewise not a
control-flow proof.

### Receipt-v18 INT 93h IVT boundary (diagnostics only)

Two independently prepared read-only V18 captures retain the same anomaly
sidecar, SHA-256 `addd15d01aac89d9f1246fcbe444882a665986f5a03303ebcb9680aa602991f8`.
The fixed 16-tuple predecessor record ends at `0a8d:0134` with bytes
`cd93075d`; the exact English `TITLES.EXE` member maps that instruction to
`TITLES.EXE+0x0034`, where `CD 93` is an original `INT 93h`. The next recorded
normal-core tuple is `0000:0000`. This is evidence that the current recorder
route reaches a zero IVT target after that original software interrupt. It
does not identify the intended INT 93h handler, its ABI, installation path,
or any title/game result. Project Eon does not install, emulate, bypass, or
infer that handler.

Static inspection identifies two potential original installation sites, but
does not order them relative to the captured interrupt. `TITLES.EXE+0x115c`
and `2200AD.EXE+0x416e` both contain `LDS DX,[0x011c]`, `MOV AX,0x2593`,
`INT 21h`: DOS Set Interrupt Vector for `INT 93h`, using an unproven `DS:DX`
pointer. The V18 path reaches `TITLES.EXE+0x0034` first. Consequently neither
site proves that the vector had been installed in the captured process, nor
identifies a handler or supplies an ABI. The next read-only observer must bind
the actual IVT entry at that exact original `INT 93h` call before any runtime
work can cross this boundary.

### Receipt-v19 actual INT 93h vector (diagnostics only)

The V19 recorder reads the same four IVT bytes DOSBox-X will dispatch at the
already proven `TITLES.EXE+0x0034` software interrupt. A receipt is admitted
only when the event reports `vector_ip=0x0000 vector_cs=0x0000` and the
separate V11 raw fault receipt is exact. This confirms the current execution
environment has no installed INT 93h target at that call. It neither supplies
the missing target nor assigns an ABI, title action, display result, or game
state to the interrupt.

The hash-bound `MCGA.BIN` driver (`bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228`)
contains neither the static `MOV AX,2593h / INT 21h` Set Interrupt Vector
signature nor an `INT 93h` opcode. This negative byte-level result does not
identify the intended installation mechanism; it only rejects a direct
in-driver standard-DOS-vector installation claim for this exact file.
The same bounded signature scan found neither encoding in `EGA640.BIN`,
`SCVX.DRV`, `SIBM.DRV`, or `SSBL.DRV`. This rules out only direct standard-DOS
vector setup in those exact byte streams; an indirect installer, loader-side
write, or unreached path remains an explicit preservation boundary.

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
  --machine-profile svga_s3 \
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
hashed. It also binds the post-run source archive, reviewed recorder and exact
generated configuration by SHA-256/byte count, reports raw event/result files
under their 8 MiB bounds, and retains only the first 1 MiB of a recorder
console while hashing/counting its complete transcript. Its output is raw
external evidence only and still requires assembly, independent validation,
and review before any new adapter or runtime route can exist.

The v6 receipt binds one finite machine profile: `svga_s3` (the default) or
`ega`. The selected profile is written as `machine_profile` and independently
checked against the retained configuration. This supports a reproducible
comparison of the original loader's own EGA640/MCGA driver selection without
silently selecting a host fallback. It is still a diagnostic configuration,
not evidence that either driver returned, rendered a title, or accepted input.

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
