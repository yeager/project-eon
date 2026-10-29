# Deuteros Amiga title-display capture status

This record is deliberately a status boundary, not an admitted reference
trace. It records a direct debugger observation made on 2026-08-30 from the
recognised English Amiga release, without storing source bytes, a frame, an
audio stream, a save state, or a generated image in the repository.

## Source and route

| Item | Value |
| --- | --- |
| Outer release SHA-256 | `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` |
| Nested disk-1 ZIP SHA-256 | `7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533` (`449666` bytes) |
| Nested disk-2 ZIP SHA-256 | `b98ee3c36141773485c5e03dd8bb4aa59784eaf08a1363fa6a2951a5eb5fdc0a` (`490962` bytes) |
| System ADF SHA-256 | `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38` |
| Data ADF SHA-256 | `99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a` |
| Kickstart 1.3 SHA-256 | `ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53` |
| Emulator | FS-UAE 3.2.35 |
| Local capture-config SHA-256 | `c2cc8f266b6c8a72b202705d99da78fb7af160d6781897e679fb7a6ee30282fd` |
| Media route | `archivemount` FUSE, mounted read-only; both floppy drives write-protected |

The configuration and every transient debugger dump remain outside the
repository under the project-scoped cache. The three temporary buffers used
solely to calculate the hashes below were removed immediately afterwards.

## Read-only emulator preflight

On 2026-08-30 the configured FS-UAE route was revalidated before an emulator
preflight. The outer Deuteros archive and the Kickstart archive were exposed
only through four nested `archivemount -o ro` FUSE mounts: the outer release,
the two original nested disk ZIPs, and the Kickstart ZIP. The kernel mount
table recorded `ro,nosuid,nodev,default_permissions` for every view. Hashing
the exposed files yielded the three ADF/ROM identities in the table above;
the outer release still hashed to `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`.

An earlier same-day preflight passed the file using `--config=…`. FS-UAE 3.2.35
silently ignored that option: its own log showed no floppy images and its
default ROMs. That invocation is withdrawn as media/ROM evidence. It had never
produced a trace, frame, input, or runtime admission.

The corrected invocation supplies the `.fs-uae` file as FS-UAE's positional
argument. On 2026-08-30 from `04:30:33Z` to `04:30:53Z`, it loaded the recorded
A500 configuration (configuration SHA-256 in the table), both exact FUSE ADF
paths as drives 0 and 1 with `write protected 1`, and `KS ROM v1.3
(A500,A1000,A2000) rev 34.5 (256k) [315093-02]`. Host status `124` is the
deliberate 20-second timeout, not an emulator result. The outer archive still
hashed to `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`
afterwards. This proves only that the documented, read-only media route and ROM
admission reach FS-UAE initialisation. It records no title frame, game input,
audio, callback, emulator result, event stream, or trace admission.

## External recorder no-input preflight

On 2026-08-30, the reviewed FS-UAE raw-observer build ran the same positional
configuration for a bounded 20 seconds with no host input. The initial
headless SDL dummy-video attempt stopped before emulation because FS-UAE could
not initialise an OpenGL context; it produced no raw output. The normal desktop
video route then ran to the deliberate timeout (`124`). Its log confirmed the
two exact FUSE ADF paths, Kickstart ROM, and `floppy_write_protect = 1`; the
outer archive still hashed to `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`
afterward. That initial observer build did not cover A500's cycle-exact CPU
loop, so it created no output file.

After adding the same host-only hook to the actual cycle-exact loop and
correcting its LF writer, a second no-input 15-second run produced 4,096
raw records (SHA-256 `d46d768e9ab16c8154eead16ce82dce60842554fcc89ca1517b9b46d394106ce`)
from a recorder binary SHA-256
`05624790bdf2cce3e34e98309ada9e4ff0ac8d5aa9282ce718f7855719e93e1b`.
They comprise 1,334 hits at `$1fe84`, 697 at `$1fe96`, and 2,065 at `$210d4`;
the 4,096-record cap then stopped further host output. The raw file remains
outside the repository. These are reachability observations only, not title,
input, display, audio, ABI, or reference-trace evidence.

The recorder was then changed to retain at most 128 records per fixed probe
site, so that a loop cannot exhaust the global budget. A fresh no-input run
reached exactly 128 records each at `$1fe84`, `$1fe96`, and `$210d4`, and no
other configured site. It therefore confirms the same bootstrap/loader
boundary with better sampling, but still supplies no input-to-title transition
or evidence for any later runtime path.

After the repository capture receipt was hardened, a new 60-second
operator-visible, no-input run used the reviewed FS-UAE binary
`727bba3ac4bc78558b964d0f572c488a419cd0985d803979e047381d2cf34f93`
through the four read-only FUSE layers. Its receipt binds the unchanged outer
release, Kickstart archive, recorder and generated configuration; the raw-PC
file is 42,132 bytes with SHA-256
`92dcc35ea0b05102e23a96176eb56550b3a4028ac7712de8dc19dd21b4ef2db6`.
It contains exactly 384 records: 128 at each of `$1fe84`, `$1fe96`, and
`$210d4`. The host-input receipt is absent and the captured console is empty.
This independently reconfirms only the bounded bootstrap/loader sites over a
longer real-media run. It does not reach a title/display probe site and does
not establish input, frame, audio, callback, ABI, or gameplay behaviour.

On 2026-08-31, the same physical, write-protected no-input route was extended
to 120 seconds. Receipt v5 was accepted by `verify_capture_receipt.py`; it
retained the same 42,132-byte raw-PC file (SHA-256
`92dcc35ea0b05102e23a96176eb56550b3a4028ac7712de8dc19dd21b4ef2db6`) with
128 records each at `$1fe84`, `$1fe96`, and `$210d4`, no host-input receipt,
and an empty console. This rules out passive duration up to that bounded
window as a path to a title/display probe for this route. It does not identify
the loader's missing condition or establish any title, input, display, audio,
ABI, or gameplay fact.

On 2026-08-31, a separately labelled 30-second `warp` diagnostic was accepted
as receipt v6. It binds `timing_profile=warp` to the generated FS-UAE
configuration, retains the unchanged 42,132-byte raw-PC file with SHA-256
`92dcc35ea0b05102e23a96176eb56550b3a4028ac7712de8dc19dd21b4ef2db6`, and
again records exactly 128 observations each at `$1fe84`, `$1fe96`, and
`$210d4`, with no host-input receipt and an empty console. This does not add a
title/display probe or identify the missing condition. Warp is a
reachability-only diagnostic profile and cannot establish timing, input,
display, audio, ABI, gameplay, or preservation behaviour.

On 2026-08-31, a subsequent 15-second, no-input capture exercised the v3
receipt format. The verifier accepted the externally stored receipt, which
binds the unchanged recognised outer archive, Kickstart ZIP and reviewed
recorder. Its raw-PC observer file is 28,052 bytes with SHA-256
`1e2cdd13d31fb3b368448b4c24b3ca51501ff18876ce9e8df4260c4c29c26d74` and has
256 grammar-checked records: 128 at `$1fe84` and 128 at `$1fe96`. The receipt
records no host input and an empty console. It validates the upgraded
read-only evidence route only; it is not a title/display capture and admits
no input, Exec/graphics ABI, frame, audio or gameplay behaviour.

On 2026-08-31, receipt v7 introduced a separate cycle-exact prefetched IR word
and memory word at the observed PC. A 15-second, no-input `realtime` run was
accepted by `verify_capture_receipt.py` with recorder SHA-256
`59635e876004536273708a04b6109831aa9d4fa6fb4e50663bc5e201cc450697`.
Its 34,196-byte raw-PC file has SHA-256
`22855e20e766df330ee7caf59d37525d5a69fd74d8663b3a1e3d0316a314c16e` and
contains 256 records: 128 at `$1fe84` and 128 at `$1fe96`. At those two
observed addresses the separately recorded IR and memory words agree within
this run (`$7202` and `$7208`, respectively). That result describes emulator
state at the raw hook only. It does not identify original-media instructions,
resolve the overlay/load mapping, establish execution of the statically
disassembled title bridge, or admit any title, display, input, audio, ABI, or
gameplay behaviour.

A separate 15-second, no-input `realtime` repetition on 2026-08-31 used the
same reviewed recorder and four read-only FUSE layers. It was independently
accepted as receipt v7. The recognised outer archive and Kickstart ZIP retained
their required SHA-256 identities after the run; no host-input receipt was
created. Its 34,196-byte raw-PC record is byte-identical to the first v7
observation (SHA-256
`22855e20e766df330ee7caf59d37525d5a69fd74d8663b3a1e3d0316a314c16e`), with
128 records at each of `$1fe84` and `$1fe96`; the recorder console was empty.
The fresh configuration identity is
`4c514bb7efbea8b24e833dbbeb0f9dff9904493289143008edc5018fd2c344c9` (1,111
bytes). This repeat confirms only the bounded raw-observer route and its
reproducibility. It does not upgrade the raw hook to original instruction
provenance or establish title, display, input, audio, ABI, or gameplay
behaviour.

On 2026-08-31, the new receipt-v8 runner was exercised for 15 seconds in
`realtime` mode through the same four write-protected FUSE layers. The
verifier accepted the fresh external receipt. The recognised release,
Kickstart archive, and recorder retained their required identities; the new
configuration identity is
`c05b3a90004d4b779dd4264dc2cf8e149945eb70cbc93ceae62d4393ed57094e` (1,069
bytes). No host-input receipt was produced. The 34,196-byte raw-PC file is
again byte-identical to both v7 observations (SHA-256
`22855e20e766df330ee7caf59d37525d5a69fd74d8663b3a1e3d0316a314c16e`), with
128 records each at `$1fe84` and `$1fe96`. Receipt v8 additionally recomputes
the opaque per-site word-pair summary: `$1fe84` reports `7202/7202` and
`$1fe96` reports `7208/7208` for its separate IR/memory fields. The recorder
console is empty and the run timed out normally after the bounded window.
This confirms the new receipt schema and the existing bootstrap observation
only; it does not prove title execution, instruction provenance, display,
input, audio, ABI, or gameplay behaviour.

A further 15-second, input-free `realtime` repetition on 2026-08-31 was
accepted independently by `verify_capture_receipt.py` as receipt v8. Its
fresh generated configuration had SHA-256
`c4a6161fa0c9d9abcbc188c918d8458c905284748f890204c81082cca262e825`
(1,069 bytes); the recognised outer release, Kickstart archive, and reviewed
recorder retained their required identities before and after the run. The
raw-PC observation was again byte-identical (`22855e20e766df330ee7caf59d37525d5a69fd74d8663b3a1e3d0316a314c16e`,
34,196 bytes), with 128 records at each of `$1fe84` and `$1fe96`, the same
opaque `7202/7202` and `7208/7208` IR/memory pairs, no host-input receipt,
and an empty recorder console. This is repeatability evidence for the bounded
read-only recorder route only. It still does not establish title execution,
instruction provenance, display, input acceptance, audio, ABI, or gameplay.

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
- the mandatory RGB4-to-`rgba8888-rgb4-expanded-nibbles` conversion and a
  canonical `rgba8888-row-major` frame checkpoint;
- a host `s16le-interleaved` PCM capture with sample rate, channel count, and
  frame count;
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

The current external raw-observer design, including its exact FS-UAE source
revision and non-admission boundary, is in
[`DEUTEROS_AMIGA_FS_UAE_RECORDER.md`](DEUTEROS_AMIGA_FS_UAE_RECORDER.md).

Receipt v9, pinned in the former capture runner as a 61,505,560-byte aarch64
binary with SHA-256 `93636a80a9e1124ee6545fe45c0664a1ce07f9450063112c2da5b7a69a0afc8f`,
adds only a read-only atomic ordinal/frame snapshot to each raw-PC observation.
Only a successful, non-playback, non-state-management host-delivery receipt
can advance that snapshot; zero means no such delivery preceded the sample.
On 2026-08-31, the first v9 15-second no-input preflight was independently
accepted by `verify_capture_receipt.py`. Its cache-only receipt retained the
recognised outer release, Kickstart archive, reviewed recorder and generated
configuration identities. The recorder timed out normally with an empty
console, no host-input receipt, and zero input links. Its 256 raw-PC records
(41,876 bytes, SHA-256
`fd52c57cb44a402fc7b9ddbeea0e8d1867dd09e8851f586ef515d6aba8698c39`)
were capped at the existing bootstrap sites: 128 at `0x0001fe84` with
`7202/7202` and 128 at `0x0001fe96` with `7208/7208`. This admits the v9
receipt/verifier route and its zero-link chronology only. It does not prove a
guest poll, input acceptance, title execution, display, audio, ABI, or
gameplay; an operator-led physical-input run remains separately required.

On 2026-09-01, a second 120-second realtime v9 run was independently accepted
by `verify_capture_receipt.py`. It again used the recognised outer release,
Kickstart archive, reviewed recorder and generated configuration, and timed
out normally with an empty recorder console. Its 384 raw-PC records (62,868
bytes, SHA-256
`d8732ec5aab06123147688b19b8bc750b0ee6ca1f9a03cdc68a5787271a1e5b9`)
were capped at three existing bootstrap sites: 128 at `0x000210d4` with
`51c8/51c8`, 128 at `0x0001fe84` with `7202/7202`, and 128 at
`0x0001fe96` with `7208/7208`. The receipt has no host-input delivery, zero
input links and no input chronology. This is longer no-input reachability
evidence only; it does not turn a physical-key attempt into proof of guest
acceptance, title execution, display, audio, ABI results, or gameplay.

Later that day, the same read-only v9 route was exercised after the runner
gained its manual visible-window focus-settle protocol. The independently
verified 120-second capture binds `focus_settle_seconds=10` and
`host_input_observed_during_capture=false` in its external receipt. Its source
release, Kickstart archive, recorder and raw-PC result remain the recognised
identities above; the raw result is byte-identical to the preceding 384-record
observation (`d8732ec5aab06123147688b19b8bc750b0ee6ca1f9a03cdc68a5787271a1e5b9`).
The absence of both a live delivery indication and a final host-input receipt
shows that this session did not dequeue a physical frontend event. It proves
neither a broken game route nor game-input semantics. The known v5 delivery
receipt demonstrates that the reviewed observer can record frontend delivery;
the remaining requirement is an operator interaction that reaches the visible
FS-UAE window, followed by the same independent validation.

## v10 physical-input capture (2026-10-03)

The reviewed external recorder is receipt v10: a 62,014,696-byte trv2 x86_64
binary with SHA-256 `c6422037df6cadeb50ffaee3bb1c1b56d21722a7c687287d6058d4802943f54b`.
It retains the v9 raw-PC and host-delivery contracts, but adds a separate
disabled-by-default receipt that arms only when the existing CPU probe reaches
`$0001eda6`. It then records only bounded CPU/Copper writes to reviewed
display registers.

On 2026-10-03, two visible VNC-operated `realtime` captures used the exact
recognised standalone English disk pair, the pinned v10 recorder, and the
recognised Kickstart archive. Both version-23 receipts independently passed
`verify_capture_receipt.py`. Their source identities were disk 1
`7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533`, disk 2
`b98ee3c36141773485c5e03dd8bb4aa59784eaf08a1363fa6a2951a5eb5fdc0a`, and
Kickstart archive `c9521c114900633c09317ca6ff979db7b9df34d3cb537de062f5d51811c42c04`.
The host-input receipt contains ten grammar-checked records (SHA-256
`9d4543abf2883a93b8ed5d715b3ca9ba5389497d449490c3170262a19d7953fe`) and the
separate operator timelines are retained beside both captures in trv2's
external cache. The input action and state values remain uninterpreted. The
first run's 384 raw-PC records all preceded input (`raw_pc_input_links=0`). In
the second run, the 396-byte host-input receipt has SHA-256
`f6cca69ecbf9ee751c82ace664866ec5da87d946bbb85836aaadaa9c1ac837e0` and eight
records; 128 of the 384 raw-PC observations are chronology-linked to delivered
input (`raw_pc_input_links=128`, last linked ordinal 4). Every nonzero link
passed the verifier.

In the first run, the 62,868-byte raw-PC file hashes to
`3f4f1e1b126785338a05907a66f4398bba0c54781671c08bc6d2affaf85d09f8`; it
reaches its 128-record caps at `$210d4`, `$1fe84`, and `$1fe96` before any
host-input delivery. The second run's 63,124-byte raw-PC file hashes to
`aac6cec0766edcfa104e11db86c43310aaee24bdf4615cc64bca2d68866d8b2f`, with 128
records at each of the same three sites; 128 records in total have verified
links to earlier host-input receipt entries. Neither run produced a title-display receipt, so
`$1eda6` was not observed. The second run establishes frontend delivery before
these bounded CPU probes only; neither run identifies a guest input poll or
acceptance, a frame, bitplane contents, palette conversion, audio, or gameplay.
The next observation must reach the original input poll and its following
display/state boundary.

A third visible VNC run used the same media, reviewed v10 binary and
version-23 runner, with a 30-second visible focus window before the bounded
180-second capture. Its receipt independently passed the verifier. The
901-byte host-input receipt has 18 records and SHA-256
`d569063bbc6b069e7d3ddb04d2ad4d53087590695c9520da5ba0cf1afce18eca`; a
separate operator timeline is retained in the same external cache directory.
The 62,868-byte raw-PC file is byte-identical to the first run
(`3f4f1e1b126785338a05907a66f4398bba0c54781671c08bc6d2affaf85d09f8`), with
the same 128-record caps at `$210d4`, `$1fe84`, and `$1fe96` and zero
chronology links. The `$1eda6` display observer remains absent. This confirms
the sampling-window limit even with repeated visible Space/Return input; it
does not establish guest polling or acceptance. The v11 section below
addresses that bounded-sampling gap; an admitted capture must still reach the
original input poll and following display/state boundary.

## v11 post-input recorder capture (2026-10-03)

The trv2 observer source now gives each reviewed raw-PC site an independent
128-record budget before the first nonzero host-input ordinal and after that
delivery, while retaining the global 4,096-record cap. The recorder rebuilt
successfully as FS-UAE 3.2.35 in its existing external source/build tree. The
candidate is 62,015,024 bytes with SHA-256
`18378aacf2a4bbe4fe3a84c295a1f53a3edd08c1cd4f5dca90f2c4af87c2181d`.
The source remains an externally cached, modified restoration tree; it was
not committed or copied into the repository. The updated runner/verifier
recognise this v11 hash and continue to verify v10 receipts. The bounded,
read-only preflight passed for the exact standalone English disk pair,
Kickstart archive and recorder before creating the capture directory.

The subsequent 180-second, realtime visible-input run used a 30-second manual
focus window. Its v23 receipt passed the updated independent verifier and the
older v10/v23 receipt still passes. The admitted output remains external at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261003-vnc4/`.
The capture ended at the requested time limit (`exit_status=124`) and records
20 host-input events (1,007 bytes; SHA-256
`be8c379af28f600a743b7f40656ec4534ae1516fa77a75e3f23074511463f3d4`). Its
operator timeline is 540 bytes (SHA-256
`a596d65e45957e8f7e4a986866fb16302c6e58d2eca00b887c976bc4e4a9b98a`).
Input fields remain opaque delivery observations; the title logo stayed
visible throughout, with no distinct transition observed.

The 84,372-byte raw-PC file (SHA-256
`a18e739f38702dd53dbd977dc1d2fe24461ff0a90bf11fc0d5e2287f6bbdf89c`) has
512 records: `$210d4` 256, `$1fe84` 128, `$1fe96` 128. The pre-input phase
contains 128 records at each of those sites. The post-input phase contains
128 additional `$210d4` records. Of the raw observations, 128 link to prior
host-input records, with the last link at ordinal 6. The `$1eda6` title-display
probe still did not fire. This demonstrates that the new phase budget works
and samples beyond the previous cap, but does not establish guest polling,
input acceptance or playable gameplay. The next evidence needed is the actual
guest input poll and its following display/state boundary. The recorder build
also emitted compiler warnings, which remain documented for review before any
further recorder changes.

## v12 guest-input-poll capture (2026-10-03)

The recorder was rebuilt in the existing external trv2 FS-UAE restoration
tree after adding hash-bound guest address `$21822` to the bounded raw-PC
allowlist. This address is inside the previously recovered input-read route
`$21822..$21897`. Per-site capacity increased from 15 to 16; the existing
128-record per-phase/site and 4,096-record global limits remain. FS-UAE 3.2.35
compiled successfully, with compiler warnings; the modified source and binary
remain external. The 62,015,024-byte candidate is pinned at SHA-256
`7b3779771dd705aeb313f71c355e06fe6d4f77b836ea95f7b9748baba4eb4f64`.

A 60-second realtime VNC session used the existing standalone English disk
archives and Kickstart archive through read-only mounts. Its v23 receipt passed
the independent verifier. The capture ended at the requested time limit
(`exit_status=124`) after 65 seconds wall time. It records 12 visible manual
host-input events (603 bytes; SHA-256
`9efaa60350b01d61ad396586b7761b2eb13a79dc61e859a0603ea28dd6a5c1b6`). The
84,387-byte raw-PC v9 file (SHA-256
`0a24446b24df40ea5c22adc01a66e93f78ee78f778cd4e8860b127bb60fd98de`) contains
512 records. Before input, `$1fe84` and `$1fe96` each reached their 128-record
caps. After input, `$210d4` and `$21822` each reached 128 records. At `$21822`,
the recorder observed the opaque opcode pair `0839/0839`; 256 raw samples link
chronologically to delivered host input, with the last linked ordinal 6. The
separate `$1eda6` title-display receipt is absent. The title sequence advanced
from credits to the animated Deuteros logo during visible key input, but the
capture does not establish a changed guest input state, a gameplay transition,
or playability. In particular, `$21822` reachability is now evidenced after
input, while the downstream branch and accepted effect remain unproven. Raw
capture and receipt files stay in the external trv2 cache at
`~/.cache/project-eon-tools/deuteros-amiga-capture-20261003-vnc6/`.

## v13 branch-route capture (2026-10-03)

The rebuilt trv2 FS-UAE observer expands the hash-pinned raw-PC allowlist to
22 addresses. In addition to `$21822`, it includes the next-instruction and
route sites `$2182a`, `$2182c`, `$21834`, `$21850`, `$2185e`, and `$21892`.
The 62,014,944-byte binary is SHA-256
`e32f337dafdfb30e655f0aa8eb06e37a5dc445a225ec8473da92cad23cf5eb24`.
Its 60-second realtime VNC run used the same standalone English disk pair and
Kickstart archive through read-only mounts. The receipt version 23 passed
`verify_capture_receipt.py` on trv2. Output is retained outside the repository
at `/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261003-vnc7/`.

The receipt reports 30 opaque host-input records (1,521 bytes; SHA-256
`c3a1e6dbc333bc4b1b6ddfe01b1a07943f5cf6b47813c234aa7fcd11d4fdce2a`). The
212,141-byte raw-PC v9 file (SHA-256
`bb499c49774c8730d63202dfbb3662394b8cfd658de387605f1c921b83951daa`) contains
1,280 records. Before input, it reached 128-sample caps at `$210d4`, `$21822`,
`$1fe84`, `$1fe96`, `$21834`, and `$2185e`. After input it reached the same
128-sample cap at `$210d4`, `$21822`, `$21834`, and `$2185e`. The verifier
chronology-links 512 raw samples to input records, with the last observed
ordinal 10. Opcode pairs include `0839/0839` at `$21822`, `4a39/4a39` at
`$21834`, and `0839/0839` at `$2185e`.

The run still has no samples at `$2182a` or `$2182c`, and no `$1eda6`
title-display receipt. The raw-PC recorder samples only its allowlisted PCs; it
is not a continuous instruction trace, so missing samples at those addresses
do not establish whether either instruction executed. Although `$21834` is a
statically identified route site and appears after input, its samples do not
include the preceding branch PC or the custom-register value; they therefore
do not prove which value was read, why control reached `$21834`, accepted guest
input, or a gameplay transition. The logo remained visible; no playability
claim is supported.

## v14 input-branch probe correction (2026-10-04)

Reviewing the v13 observer call path found a recorder-side exclusion. The
allowlist has 22 entries, with `$2182a` at index 16, but `newcpu.cpp` returned
early whenever `site_index(pc)==16`. Thus v13 could never emit a sample for
`$2182a`; its absence says nothing about whether the guest executed that
instruction. The observer's actual not-found sentinel is 22. The v14
candidate changes the guard to that sentinel and leaves the allowlist and
game-media access unchanged.

The rebuilt trv2 binary is 62,014,936 bytes, SHA-256
`701d11b705dd36934712ab37df4e105a2d68bde4dea2642214f45012d6768acf`. The
prior v13 binary was rebuilt from its preserved source snapshot and retained
separately with its original SHA-256
`e32f337dafdfb30e655f0aa8eb06e37a5dc445a225ec8473da92cad23cf5eb24`. A new
v14 physical-input capture used the read-only media route but ended when the
operator client locked before any manual input was delivered. The runner
rejected it for its missing host-input receipt, so it has no admitted capture
receipt and supplies no guest-input evidence. Its raw-PC file remains external
at `/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261004-vnc1/`
and passes the reviewed v9 raw-record parser: 146,836 bytes, SHA-256
`080ef2feabcdcf417ac3180bc24ad307b401f6df3f7a7bc3e0bab06916033cec`, 896
records with zero input links. Before any input, the samples repeatedly include
`$21822 → $2182a → $21834`; opcode pairs are `0839/0839`, `6608/6608`, and
`4a39/4a39`, respectively. This confirms the v14 probe correction and the
pre-input route only. A fresh visible physical-input capture is still needed
to observe the post-input path and any display/state checkpoint.

## v14 visible-input title and display capture (2026-10-04)

A fresh trv2 VNC session used the v14 recorder above, the recognised English
standalone disk pair, and Kickstart archive through the runner's read-only
mounts. The runner's v23 receipt passes `verify_capture_receipt.py`. The
bounded run used realtime timing, a 45-second focus-settle interval, and a
120-second capture window; the runner completed normally at the requested
timeout (`exit_status=124`). The 18-record visible host-input receipt is 907
bytes with SHA-256
`786c8a8462fc31a3daaee70dd15fe160e758a3977abc5d990bc23beb6b38abf8`.

The 176,615-byte raw-PC v9 file has SHA-256
`672c516260451ad565e020aa5c03f16c130938a1dc0b0145f133a55ed6c03dea` and 1,073
records. It contains 201 chronology-linked post-input observations through
input ordinal 4. Input-linked samples now include `$21822`, `$2182a`, `$21834`,
and `$2185e`, with opcodes `0839/0839`, `6608/6608`, `4a39/4a39`, and
`0839/0839`. The later linked sequence reaches `$40450`, `$4046c`, `$1ed80`,
`$1eda6`, `$1f182`, `$1ef74`, `$1f056`, and `$1fbe6`. This establishes those
allowlisted PCs in recorder order after host-input delivery; it does not make
the sampled hook a continuous instruction trace or prove that a particular
input value was accepted.

The separate title-display v10 evidence file is 265,649 bytes, SHA-256
`488871110e9100e8020622c37ed915a309a01299ffbabf634748a6c020d3390a`, with
2,049 records and 2,048 custom-register writes. All 2,049 observations are
linked chronologically to input ordinal 4. This gives a real input-linked
title-display register stream, including the `$1eda6` title-display site;
there is still no canonical frame artifact, audio checkpoint, or admitted v4
gameplay trace. External raw data and receipt remain under
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261004-vnc3/`.

The next evidence step is a fresh visible run from the language selector with
manual input continuing through the title and into an actionable game state.
Do not infer language selection, game start, accepted controls, or playability
from the host event receipt or the post-input PC samples alone.

## v14 follow-up poll capture (2026-10-04)

A second fresh physical-input VNC run used a 10-second focus-settle interval
and a 300-second realtime window. Its v23 receipt also passes
`verify_capture_receipt.py`, and the recorder and source archive hashes match
the preceding v14 run. The six-record host receipt has SHA-256
`42d72d51b972af43c767402f157764684d4b436982b41c23f8020efdf50b039a`.

The raw-PC v9 file is 148,116 bytes, SHA-256
`4c869cb12eeb4caf3883d4d4beb9e13f59220c52e35f34e108e35480c34fb989`, with
896 records and 640 chronology-linked observations through input ordinal 4.
The five post-input sites each reached their 128-record cap: `$210d4`,
`$21822`, `$2182a`, `$21834`, and `$2185e`; `$1fe84` and `$1fe96` were observed
only before input. The title-display file is absent. The visible session was
locked before a follow-up screen check, so this run establishes the repeated
poll route under delivered host events but no selector outcome, title
initialization, display, or gameplay transition. Raw evidence remains under
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261004-vnc4/`.

Continue from a manually unlocked VNC client. Keep the emulator visible and
verify each game's screen transition after each ordinary key or mouse action;
do not treat receipt presence by itself as an accepted game control.

## v15 hash-bound post-load probe (2026-10-04)

The clean disk 1 image was read directly from its existing ZIP member on trv2
without extraction or media writes. Its SHA-256 is the documented
`6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`. At ADF
offset `+$79da6`, the bytes begin `20 39 00 01 2f f4`, the six-byte 68000
`MOVE.L $12ff4,D0`, and the 80-byte span retains its documented SHA-256
`d6b37bc6431a1fe9145ae9403a5165028ccfd856a6529d1752f824b166807223`. Therefore
the next PC `$1edac` is a finite post-load sampling point whose D0 may expose
the value read from `$12ff4`; it does not establish that the value is a valid
display pointer or that a frame was drawn.

The corrected external trv2 v15 recorder is 62,014,944 bytes with SHA-256
`7160dfafbfe67b17db931065ab6f9853874591ea4af6c33ad51059b2b0f703df`. An
earlier unpinned build was rejected during independent review and retained as
`fs-uae-v15-review0`; it is not in the locator registry. No v15 capture has
been run with physical input. A no-input diagnostic is recorded below; the next
useful evidence still requires manually operated, visible VNC input through
the title path.

### v15 visible VNC no-input diagnostic (2026-10-05)

The v15 executable was located by its pinned digest and run visibly on trv2's
VNC display `:2` for 90 seconds with realtime timing, zero focus-settle time,
and the explicitly declared `diagnostic-no-input` intent. The exact clean
standalone disk ZIPs and Kickstart archive were mounted read-only. Receipt
version 23 passes the independent verifier after the verifier's no-input
phase-summary fix; no host-input receipt was produced. The run-status file is
2,070 bytes and binds recorder SHA-256
`7160dfafbfe67b17db931065ab6f9853874591ea4af6c33ad51059b2b0f703df`.

The raw-PC v9 file is 146,836 bytes, SHA-256
`080ef2feabcdcf417ac3180bc24ad307b401f6df3f7a7bc3e0bab06916033cec`, with
896 records. Its 128-sample pre-input caps were reached at `$210d4`, `$21822`,
`$1fe84`, `$1fe96`, `$2182a`, `$21834`, and `$2185e`; the observed opaque
IR/memory opcode pairs include `$21822:0839/0839`, `$2182a:6608/6608`,
`$21834:4a39/4a39`, and `$2185e:0839/0839`. There is no `$1edac` sample and no
title-display receipt. This narrows the current no-input startup path to the
poll/branch loop before the display-base load; it does not identify a guest
input meaning or show that any control is accepted. Raw evidence remains
outside the repository at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-v15-diag01/`.

The first verifier attempt exposed a receipt-format mismatch: a diagnostic
with no post-input samples correctly writes an empty
`raw_pc_post_input_site_counts` summary, but the verifier rejected all empty
values. The verifier now allows that one summary to be empty and continues to
reject empty identities and malformed fields. Its focused 25-test module and
the independent verification of this receipt pass.

### v15 visible VNC physical-input capture (2026-10-05)

A fresh 300-second realtime run used the same pinned v15 recorder and the
operator-controlled visible VNC desktop. The independent v23 receipt verifier
accepted the run. `run-status.txt` SHA-256 is
`25b800156a5f379588d26f615c5cf952a204bb274cb16ff370bb6b0698c984ed`; the
host-input receipt is 805 bytes / 16 records, SHA-256
`9febdea038b001663f4aa5b9d7d4b30361f30aed83b48f2d6eda798e58cda07a`.
The v9 raw-PC stream is 214,291 bytes / 1,295 records, SHA-256
`86bce3f52542e73050b7903f3843a997a5c9d127f3639aaf20b4f188c74f8870`;
the v10 title-display stream is 269,747 bytes / 2,049 records, SHA-256
`f5635865c5f9d342252b55231f49f5fda127962eec697dea9f62184fd177cf20`.
Raw artifacts remain only in
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-user04/`.

The visible operator selected English at the language prompt. The game then
asked for the Deuteros data disk in DF0. Chronology-linked post-input PC samples
include the recurring poll/branch sites `$21822`, `$2182a`, `$21834`, and
`$2185e`, plus title initialization sites `$40450`, `$4046c`, `$1ed80`,
`$1eda6`, `$1edac`, `$1ef74`, `$1f056`, `$1f182`, `$1fbe6`, and `$1fe84`.
The title-display records link to host-input ordinal 10. The final visible
screen changed near the capture timeout, so this evidence does not attribute a
successful disk-prompt response to a particular key or establish an actionable
game state. It does establish that manually delivered host input is followed
by the language/title path under the pinned recorder. The VNC-visible screen,
raw trace, and display stream are separate artifacts with separate evidentiary
limits.

### v15 visible VNC DF0 disk-swap follow-up (2026-10-05)

A further fresh v15 run left DF1 empty and added both read-only disk images to
FS-UAE's removable-media list. Its accepted v23 receipt has `run-status.txt`
SHA-256 `029aafae0676cdbc91560b6dfab58432906967b6da8dfe6dcc218041de2b0dfc`;
the configuration hash is
`51b0d6b48c914d563910201da7f7a4cdd1febc5c4ccf2b60d0e46df7de335cea`. During
visible manual control the FS-UAE menu showed disk 2 available for DF0 and,
after selection, showed `DISK 2 OF 2` as the DF0 medium. The v9 raw-PC stream
is 148,116 bytes / 1,488 records, SHA-256
`6af7d31890f1d67e649231573ee8389848bcd751b0e7575a013e1e28ec100db5`; the
28-record host-input receipt is SHA-256
`e51cd8172b1015c9505a02c7cd84feb48b899bcab28a37628627841e425c42d4`.
The trace remains at the `$210d4` and `$21822/$2182a/$21834/$2185e`
poll/branch sites; no title-display receipt was produced. The disk swap is
verified as an emulator operation, but the visible game did not leave its
title loop, so this run does not show the game accepting disk 2 or reaching an
actionable state. Raw evidence remains only under
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-user07/`.

### v15 title-animation and DF0 media-menu follow-up (2026-10-05)

A new 300-second realtime VNC run used the same pinned recorder and the
DF1-empty configuration with both original disk archives available to the
visible DF0 media menu. The independent v23 receipt verifier accepts the
capture. `run-status.txt` has SHA-256
`f2524f5f2e78e715cd84f35593078c62e3427202cd87fc70a0214434ec5f69d3`; its
host-input receipt is 1,035 bytes / 20 records, SHA-256
`bfe4a532577ac47625ffff7a90ad12f01f55a4badca44ffb904142113a6aa169`. The
raw-PC v9 stream is 254,893 bytes / 1,536 records, SHA-256
`3c0d16220914dcd0af3e1dc5b684b11bda5022506a83527f4e00e3f1bad600e8`.

The visible guest rendered the animated Deuteros logo. The operator used an
ordinary right-click on the logo, opened the visible FS-UAE DF0 menu, selected
Disk 2 of 2, and later right-clicked the guest display again. The receipt
confirms 640 input-linked PC observations through ordinal 4. Its post-input
samples remain at `$210d4`, `$21822`, `$2182a`, `$21834`, and `$2185e`; the
opcode pairs remain `51c8/51c8`, `0839/0839`, `6608/6608`, `4a39/4a39`, and
`0839/0839`. There is no title-display receipt. FS-UAE remained on the logo
animation until the capture timeout. This verifies delivered visible input
and the disk-menu operation, but does not show the guest accepting the disk,
leaving its poll loop, or reaching gameplay. Raw evidence remains outside the
repository at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-user09/`.

### v15 logo-input poll follow-up (2026-10-05)

A separate 300-second realtime run used the same pinned recorder and fresh
read-only media setup. The independent v23 receipt verifier accepts it.
`run-status.txt` has SHA-256
`f2524f5f2e78e715cd84f35593078c62e3427202cd87fc70a0214434ec5f69d3`; the
six-record host-input receipt is 310 bytes, SHA-256
`ecd762eaccac5b3c5f97082824b22ed089e10460ac8026987340d76bf17ec8f9`. The
raw-PC v9 stream is 254,893 bytes / 1,536 records, SHA-256
`4fc93f443855c5f17b1e73b8e73808e117c8863ffd7a1cc19b8176859fcb216c`.

The visible guest remained on the animated Deuteros logo while the operator
delivered two ordinary right-clicks and one left-click. The 640 input-linked
PC observations reach ordinal 2 but stay at the same poll sites as user09.
All 128 post-input `$2182a` samples have Z=0, so none takes the observed
fallthrough to `$2182c`; `$2182c`, `$21850`, and title-display evidence are
absent. Unlike user08, this run therefore shows no guest-side click transition.
Raw evidence remains external at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-user10/`.

### v15 delayed logo-input poll follow-up (2026-10-05)

A fresh 300-second physical-input run used the pinned v15 recorder. The
independent v23 verifier accepts its receipt. The raw-PC v9 stream is 254,893
bytes / 1,536 records, SHA-256
`41eb4b2ba227dfac33cb9d1a13988f6509d269096bcc0260accf8cf06300838e`; 640
observations link to the two-record host receipt (SHA-256
`769b3d30d03fcc87bba9854f02ccc8d112ef3b89f10e7c88f828af43112fd9b0`). The
operator waited roughly two minutes after startup, saw only the animated
logo, and then delivered one right-click.

All 128 post-input `$2182a` observations still have Z=0. The BNE keeps
returning to the poll loop; `$2182c`, `$21850`, and title-display evidence are
absent. Delaying the click until the logo had animated for two minutes did
not reproduce user08's transition. Raw evidence remains external at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-user11/`.

### v15 early credits-click timing follow-ups (2026-10-05)

Two shorter realtime VNC captures checked the initial credits sequence against
user08. Both pass the independent v23 verifier. In user12 the guest visibly
showed the credits; one right-click was delivered at frame 1329, line 255.
Its 2-record host receipt has SHA-256
`4611b27985f1ff0bfdb9a28fb09442dc251bd767feea5d62a8204b61ba1aa549`, and
the 254,253-byte / 1,536-record raw-PC v9 stream has SHA-256
`23db7428ebf4ab5e18ce6a668764944935da028d4570f08de75be1e807e9162f`.
Post-input `$2182a` observations all have Z=0; there is no `$2182c` or
`$21850` sample and no title-display receipt. The capture ended after the logo
sequence.

User13 clicked as soon as the visible credits appeared. The right-click
down/up pair was recorded in the same frame (frame 798, line 255), unlike
user08's down at frame 979 and release at frame 980, line 0. A later left
click was also a same-frame pair at frame 5401, line 191. Its host receipt has
SHA-256 `5408703ea8efc5698c33753a7a544f7d8668f62c493023501278bc8167baa996`;
the 254,893-byte / 1,536-record raw-PC stream has SHA-256
`1c5f8ab87a9717aff624826190e89303046733dbe6eb3962d9c3dcf1e492c22b`. All
post-input `$2182a` samples have Z=0, and there are no `$2182c`, `$21850`, or
title-display records. This supports a button-duration hypothesis for the
single user08 branch observation but does not assign the button a gameplay
meaning. Raw artifacts remain under their separate external paths
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-user12/`
and `/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-user13/`.

### v16 observer protocol boundary (2026-10-05)

The capture runner and verifier now recognize the separately bounded v16 raw-PC
protocol, which adds only `$218cc` to the v15 site set. The v16 trv2 binary is
62,015,016 bytes with SHA-256
`aa4c797fc2e580c887ab6be88a57abe93f6b442ac822870e4840c514898518b4`; source
patch SHA-256 is
`1fa61e7984fe5535b42c07614be4f10a219323bf8536a3dc271a80c0b6f5f361`.
It was built from the exact v15 baseline, with only `newcpu.o` changed in
`libuae.a`; all other archive members were unchanged. The binary reports
FS-UAE `3.2.35`, and v15 remains preserved. Receipt schema 24 binds the exact
v16 identity and permits `$218cc` under an independent site set. Schema 23
continues to verify historical v10–v15 receipts and rejects the v16 identity.
A fresh 180-second, realtime, visible VNC diagnostic used the pinned v16
binary with `diagnostic-no-input`. The independent schema-24 verifier accepts
the receipt. Its host-input receipt is absent, raw-PC v9-v16 output is 146,836
bytes / 896 records with SHA-256
`3c7a8c1e568c23aa80703e5eceff16219e77234a22eb2ee3307717b2a79a0721`, and
`$218cc` has zero samples. The sampled poll sites reached their 128-record
caps; no title-display receipt was produced. This input-free run does not
establish that `$218cc` is unreachable after an operator action or that a
gameplay transition occurred. Raw evidence remains outside the repository at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-v16-diag01/`.

The v16 contract initially used schema 24, which capped each site's combined
pre- and post-input samples at 128. An incomplete physical-input receipt
showed that the reviewed v16 observer can collect 128 samples in each phase
at one site before the capture ends. Schema 26 keeps the same v16 binary,
allowlisted sites, line grammar, input chronology, and phase summaries while
allowing at most 128 samples per site per phase (256 combined) and retaining
the 4,096-record global ceiling. Schema 24 stays unchanged for existing
receipts; schemas 23 and earlier retain their prior contracts. This adjusts
only receipt admission. The incomplete trace remains unadmitted; a separate
fresh capture using schema 26 is recorded below.

### v16 input-to-display capture (2026-10-05)

A fresh, write-protected VNC run used the reviewed FS-UAE v16 binary and the
separate read-only Disk 1, Disk 2, and Kickstart archives. The physical-input
runner completed its 240-second window with exit status 124; schema 26
verification accepts the completed receipt. The raw-PC stream contains 1,173
records (SHA-256
`f126a55dd45c46de22267b80d232b5a5c48b8e382a13d051022d563af815acc3`), with
917 input-linked observations and valid chronology. Its 36-record host-input
receipt has SHA-256
`e2f9f4a770474245cb4b5df0bb63f08e3fccbe8a953baa2a53c8d4f8b3ad6497`.

One `$218cc` sample links to host-input ordinal 17 at frame 2035. The later
`$1ed80`, `$1eda6`, `$1edac`, and `$1ef74` samples link to ordinal 26 at frame
2040. The bounded title-display receipt contains 2,048 custom-chip writes
plus its arm record (2,049 records total, SHA-256
`7fae52c646a76e6b57b34232c347f66aa66316e25c9346444f8e3d28922e1979`); all
records link chronologically to input ordinal 26. The capture therefore
connects visible input delivery through the newly observed `$218cc` route to
title-display register writes. Input action/state values remain opaque.

The visible operator selected English, followed the prompt to insert Disk 2
into DF0 through FS-UAE's removable-media menu, and continued with an ordinary
key. A later FS-UAE view showed a more developed game interface, but no screen
artifact was retained and no subsequent in-game action was validated. The
screen cannot yet be classified as an actionable state. The register receipt
does not include bitplane contents, a frame hash, or audio output, so this is
not yet a complete display ABI or playability result. Raw capture files remain
external at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-codex06/`;
the run-status SHA-256 is
`290f97f27eb8c33834fc44c59959cbb024fe04a0fc00a54c7b5054ad9fc1b4f4`.

### v16 post-input title-selector observations

The schema-26 capture receipt was reverified with the current verifier against
the unchanged external capture files. After input ordinal 36 (frame 11329),
the raw-PC stream records the hash-identified title-stage selector at
`$1fe7a`, both local call sites `$1fe84` and `$1fe92`, and their return PCs
`$1fe88` and `$1fe96`. The 13 samples at raw-record sequence 1161–1173 include
two selector entries, two `$1fe84` returns, and three `$1fe92` returns. The
second entry carries D0=`$00330064`; its following return samples carry
D0=`$00310000` at `$1fe88` and D0=`$00300000` at `$1fe96`. At both call/return
pairs IR and memory opcodes agree. These values are captured observations,
not interpretations of the helper's ABI or the input action.

The verified static selector path ends at `$1fbe6` in the same title-stage
image. The capture has no corresponding `$1fbe6` sample, caller memory
snapshot, accepted control mapping, or transition to the separately loaded
main stage. It therefore extends observed execution after visible input but
does not establish actionable gameplay, state change, or playability. The
raw stream remains external; only its hash, bounded record range, and
register/address facts are recorded here.

The later codex07 v16 receipt adds 128 `$1fbe6` samples, superseding the
absence of dispatch samples noted for codex06 above. In codex07 raw-PC record
sequences 925–955 (input ordinal 28, frame 3853, A0=`$00020634`) and
1064–1160 (input ordinal 30, frame 5673, first A0=`$0001eed5`, then
`$000256c0`), the instruction opcode remains `$4a39`. The first range's 31 D0
low bytes form printable ASCII consistent with the title's language-selection
text. The middle range contains 61 `$20` bytes, and the last 36 low bytes form
the start of a printable disk-insertion prompt. This suggests the associated
text paths were active, but the capture has no screen pixels to prove that the
text was visible. The same receipt later records the selector entry and local helper returns at
sequences 1161–1173 after input ordinal 34, frame 8575. This joins input-linked
dispatch and selector execution in one capture, but includes no dispatch-cell
snapshot or accepted control mapping. The text bytes do not establish a
game-state transition; transition to the separately loaded main stage and
playability remain unproven.

The v3.2.35 action IDs can be resolved against the exact upstream
`src/inputevents.def` enum (tag commit
`4ae7ddaec50b567ed80d71ffbff067cb58e945a3`, with `INPUTEVENT_ZERO` followed
by sequential event entries). In this receipt, action 33 is
`JOY1_FIRE_BUTTON` (also labelled Mouse1 Left Button), 205 is `KEY_1`, 360 is
`SPC_DISKSWAPPER_0_1`, 157 is `KEY_RETURN`, and 149 is `KEY_SPACE`; actions 3
and 4 are `MOUSE1_HORIZ` and `MOUSE1_VERT`. Accordingly, ordinals 29–30 are a
delivered 1-key down/up pair; 31 is a disk-swapper action; 32 is a Return
release; 33–34 are a Space down/up pair; 28 is a mouse/fire release. These
names identify FS-UAE's delivered events only. The capture does not show
which events the game accepted or what state they selected. The exact source
defines `state` as a signed 8-bit value extracted from the packed event; the
axis records' 127 values are therefore raw axis-state observations. The
receipt and raw stream stay outside the repository at
`/Users/bosse/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-codex07-analysis/`;
raw-PC SHA-256 is
`b7456e4ad24ea7984087919e11f62a3f3695857b353eaf26f045b1700a342627`.

### 2026-10-05 trv2 v18 VNC graphics failure

The visible run at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-codex-live-20261005T202450Z/`
started from fresh external-cache output but failed to produce
`host-input-receipt.json` or `run-status.json`. Its 1,024 raw-PC rows all have
`input_ordinal=0` and `input_frame=0`; the file hash is recorded in
`PRESERVATION.md`. The records end at `$21866` and are not a physical-input
capture. FS-UAE reported failure to create a DRI3 screen and load the
`virtio_gpu` driver on the VNC display. The follow-up below shows that llvmpipe
allows the emulator to run despite these warnings; retain this failed run
unchanged.

A follow-up ran the same pinned binary with effective group `render` for that
process only. It completed its capture interval and fell back to llvmpipe, but
retained the same 1,024-row raw-PC file and hash as the prior no-input attempt.
No host-input receipt or `run-status.json` was written. This is evidence that
the graphics fallback keeps FS-UAE running, not evidence of delivered input or
game response. The external output remains under the trv2 cache path recorded
in `PRESERVATION.md`.

### 2026-10-05 v18 visible language-selector capture

Receipt v28 was accepted for the visible physical-input run at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-f10-01/`.
It binds the recognised release, the exact two read-only disk archives,
Kickstart, and FS-UAE v18 (`44477a0f41025a6e3f68098eb093fb7a32aebf3b2d1577fb294337a617154a54`).
The generated configuration SHA-256 is
`7ae0265cc59fa2d5b69c2268f57404063dfd927d3329f89f00fc5ed779fc0d36`.
The host-input receipt contains 30 events; 1,241 bounded raw-PC observations
contain 935 input links. The raw-PC SHA-256 is
`4fe26db68f1066312cc770f6d171ca88d5450b7d514fb6fa6ef04d1882286978`.

The selector observer retained 31 input-linked reads at `$1fbe6` (SHA-256
`b53d287f78369de9851d66076616a1154cd3ead1ed486a1564e32fef2b12f831`). The
configured helper call sites `$1fe84` and `$1fe92` and returns `$1fe88` and
`$1fe96` did not produce the ordered post-input passage needed by the native
admission gate. The UI remained at the language selector; the run did not
reach the Disk 2 prompt. The separate title-display observer recorded 2,049
register observations (SHA-256
`d73180e170a3e43d88c13e5f2a8c26a94e1bcbb731caab0e527cb71766e95010`), not a
complete bitplane/palette/audio artifact or a gameplay frame. The capture
ended at its deliberate 240-second limit (status 124). This run adds
input-linked selector-cell reads, but cannot yet be admitted to route execution.

### 2026-10-05 v18 input-delivery retry

A fresh visible run at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261005-vnc-f10-02/`
was independently accepted as receipt v28 with configuration SHA-256
`39db0c48a7a384127bd0b071d7ccee035109c36a2c9ce7031e16f79509dc65e2`. It
contains 14 host-input events, 1,792 raw-PC observations, and 768 raw-PC input
links; raw-PC SHA-256 is
`5badc1341213c041545894665154e4d44f0ac53fa89118eb3767a5ec3f76316b`. The
only sampled title-helper PCs were `$1fe84` and `$1fe96`; no selector-dispatch
or title-display artifact was produced. The visible screen stayed at the
Deuteros logo through the bounded run. This confirms that host input reached
FS-UAE, but it did not advance the game to its language or disk selector and
does not establish an accepted game input. The run remains diagnostics only.

### 2026-10-06 guided v18 visible-input capture

A fresh 600-second realtime capture used the reviewed v18 recorder and the
updated visible operator instructions at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-vnc-guided-01/`.
The v28 receipt passed `verify_capture_receipt.py --kind deuteros-amiga`.
Original disk archives and Kickstart were mounted read-only. The generated
configuration SHA-256 is
`9032b07f13230d1ac4fdb154e4cb5bf8b51d4c244f2661d4fc7c3af4bb3f85f9`; the
recorder SHA-256 is
`44477a0f41025a6e3f68098eb093fb7a32aebf3b2d1577fb294337a617154a54`.

The receipt records 18 host-input events (SHA-256
`2826fa196da2cefaeda213f13be480306ab471f3cf84f19cbe0457c8febed45b`) and
1,024 raw-PC observations (SHA-256
`457ab41ccc1dea35506723a87b6ab645b12b1ced37776907ac9505ca7d33aeb4`). All
eight configured sites reached their 128-record limits; 768 raw observations
link to delivered input through ordinal 8. No selector-dispatch or title-
display artifact was produced. The visible game remained on its animated
logo; no language selector, disk prompt, or main-stage transition appeared.
The run therefore confirms input delivery and recorder limits only. It does
not establish that the game accepted those inputs or provide the missing
ordered helper-return/selector-read passage. The generated receipt and raw
data remain outside the repository; no original media was copied or changed.

### 2026-10-06 v19 title-input capture

The schema-29 physical-input run at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-vnc-v19-codex01/`
passed the Deuteros receipt verifier. It used the pinned v19 recorder
(`7b46501fc3cf774938fc8ca4e02788586ba22ef440462ea7ecd00396311b73a2`), the
recognised read-only English disk archives, and the read-only Kickstart
archive. The configuration SHA-256 is
`5820aba1e45784c22c9f3ac6d5be128d445102c3e1b842ff742c7bf463cc9f31`.

The receipt contains 6 host-input records and 1,364 bounded raw-PC records
(SHA-256
`daac727fc25e6e28271e1712df354f922f2cc13f2d7b35a489b8e85590106302`), with
768 input links ending at host-input ordinal 2. The separate late-input PC
and selector-dispatch sidecars are absent. The VNC-visible game remained on
its animated title logo throughout the 300-second run, including after one
Space and one Return were sent while the title was idle. This establishes
host delivery and recorder integrity only; it does not establish guest input
acceptance, a language selector, or game progress. Receipt SHA-256 is
`b7043d86c36af48534b16214b65aa31b8208071c0c85624d7b43e0c7e3dcd99b`.
Original media remained in place and unchanged; all capture output remains
outside the repository.

### 2026-10-06 v19 visible Fire-button follow-up

A fresh 90-second realtime capture used the same pinned v19 recorder and
read-only recognised disk and Kickstart archives, with output under
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-vnc-v19-firehold01/`.
Receipt v29 passes the independent verifier. The host-input receipt contains
two delivered down/up pairs for action 33 (`JOY1_FIRE_BUTTON`), at frames 888
and 1142. The v9 raw-PC stream contains 1,024 observations (SHA-256
`050f99a2f5dbf82ec6d7aa096bbaa56b30cbcf1fc6752ee8d67f90fb74c12f2f`), with
768 chronology links through input ordinal 4. At the title loop, the capture
repeatedly reaches `$21822/$2182a`, `$21834`, and `$2185e/$21866`; sampled
pre-instruction status words at `$21822`, `$2182a`, `$21834`, `$2185e`, and
`$21866` remain `$0008`, `$0008`, `$0008`, `$0004`, and `$0000`. The exact
hash-bound bytes at `$21866` are `66 08` (`BNE.B $21870`), immediately after
`BTST.B #6,$bfe001` at `$2185e`. Thus Z=0 at `$21866` means the branch was
taken on all 128 samples, including samples linked to both Fire down/up
pairs. This establishes the sampled branch outcome, not a direct CIA value
or a visible state transition; intervening paths may also write guest state.
The visible screen remained on the animated logo and no selector/display or
late-input sidecar was produced. The two click pairs were each delivered in
one frame. Original media remained unchanged.

The next useful disassembly step is to establish a bounded, hash-identified
control-flow pass through `$21870..$21897` and return to `$21822`, tracking
writes and tests of title bytes `$2171e`, `$21720`, and `$21721`. Do not infer
a control meaning or visible accepted transition from host event IDs or SR
flags alone.

### 2026-10-06 v19 held-Fire language-selector capture

A fresh 75-second visible realtime capture used the same v19 recorder, exact
recognised read-only disk archives, and Kickstart archive. Receipt v29 passed
the independent verifier. Output remains external at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-vnc-v19-fire-drag-long01/`.
The host receipt has 24 events. A visible left-button drag held action 33 from
ordinal 7 (frame 1689) through ordinal 24 (frame 1699). At ordinal 9/frame
1690 the hash-bound `$21866` sample has opcode `$6608` and SR `$0004`, so Z=1
and `BNE.B $21870` falls through. Static bytes at `$21868` then execute the
store of one to `$21720`; this particular store is a control-flow deduction,
not a recorded memory-write value. Later held-drag samples at ordinals 15 and
19 again have Z=1; the release-linked sample at ordinal 24 has Z=0 and takes
the branch. This is the first capture joining a sustained visible Fire input
to the active-low poll outcome.

The same receipt has 1,914 raw-PC samples (SHA-256
`062ff0d21c3c65679c3026d1fc917f3212f698b9308ddf96450128c8892e0f45`) and 34
late-PC samples. The late stream reaches `$218cc`, then title-stage PCs
`$40450/$4046c`, display setup at `$1ed80/$1eda6/$1edac`, and selector code at
`$1fbe6`, all linked to input ordinal 24. The selector sidecar records one
`$1fbe6` read with cells `$1f98c=$00` and `$1f98e=$00` (SHA-256
`698d1e4c979fb00572de895d0c9835bbb91cdcf2aa154989c40093185bfd303c`). The
same sample has D0 low byte `$31`; this is a byte observation, not proof of a
selected language. The bounded display sidecar has 2,048 writes (SHA-256
`31c59f4a028b7aa762940b611b01455e085d8d2d0d7e967b1ba0ae2e3e4b2fe0`). During
the visible run, the language selector appeared. A subsequent `1` key attempt
is absent from the host receipt, so this capture does not prove English was
accepted. No actionable game state, complete title frame, or gameplay is yet
established. Raw captures and original media remain outside the repository;
the media archives were not changed.

### 2026-10-06 v19 visible language-confirm follow-up

The fresh visible capture at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-vnc-v19-language-confirm01/`
passes schema-29 receipt verification. It uses the pinned v19 recorder and
read-only recognised English disk archives and Kickstart. The host-input
receipt (SHA-256
`2bfc55ffa8b65f82d6bffa9d90321e9f7afb2f366d0e4a5a384507fb3fc5554e`)
contains 30 records, including key `1` down/up at ordinals 29/30. Raw-PC has
1,063 records (SHA-256
`8cd605f68def7b531d12596e40f2d12cba71cf1655ebc67eeca2ebc9f116e735`); the
bounded late-PC sidecar has 24 records (SHA-256
`d2e5debe02484d33221c9fea0fa3b651ab5fffd5f67a6eb991c10037e55e9f83`).

The language selector was visible before key `1` was delivered. The joined
selector-dispatch sidecar has two observations (SHA-256
`6b2973baa492336d0753695ba70269fcd01eae35a226321c6c5117f3e276e6b3`). The
first, linked to ordinal 24, has D0 low byte `$31` and A0 `$20634`; after the
key event, the ordinal-30 sample has D0 low byte `$20` and A0 `$1eed5`. The
selector cells are `$1f98c=$00` and `$1f98e=$00` in both samples. Static
address mapping identifies `$1eed5` with the disk-insertion prompt text
region, but that pointer sample alone does not prove the prompt was rendered.
The display sidecar has 2,048 writes (SHA-256
`1ffb81a951b004a3192f5227583225feb91245d42f3da64157dae6d99606b6ae`); the VNC
view later appeared black and no visible disk prompt was confirmed. This run
therefore strengthens the guest-side path evidence after a delivered key but
does not establish language acceptance, disk insertion, a complete frame, or
gameplay. Original media remained read-only and unchanged.

Next, map the exact bounded instructions and display writes between the second
selector-dispatch sample and the black view. Obtain the ordered helper returns
and selector reads required for route admission before extending native
execution. Do not synthesize a disk prompt or disk-swap result.

### Static disassembly of the observed zero-mode candidate

The exact Disk 1 bytes at ADF offset `$7ac22` (runtime `$1fc22`) were decoded
as a 256-byte linear candidate, SHA-256
`9f1ecf6524f3e88e4124cde39fed5a01fb9008a44871ea7907aa78c029f0ecd3`. The
external listing is
`/home/trv2/.cache/project-eon-tools/deuteros-branch-static-20261006-01/zero-route-1fc22-100.md`
(whole listing SHA-256
`acb1b80bb9465fdbf89ee61c0561ec4cd64152b6ec3aedca73feba3c4048a580`). It
confirms `$1fc22` tests `$1f98e` and branches to `$1fd0a` when nonzero; when
zero, `$1fc2c` subtracts `$20` from D0's low byte, indexes eight bytes at
`$1f99c`, merges source words from `$1f970/$1f96c`, and writes four-plane bytes
through `$1f974`. The capture's ordinal-30 sample at `$1fbe6` had D0 low byte
`$20`, A0 `$1eed5`, and both selector cells zero. Static control-flow therefore
predicts the zero/zero branch if those sampled cells persist until their
original tests, but neither execution at `$1fc22` nor any of its memory reads
or writes was captured. The A0 pointer and D0 byte do not establish a visible
prompt or a game action.

Next recorder work should add bounded probes at `$1fc22` and its sibling
`$1fc9c`, with enough receipt capacity for the post-input helper-return and
dispatch sequence. Admit a route only from an ordered physical capture of the
original source reads; keep table, mask, destination and stride values as
explicit observations. No planar output from this static candidate has been
added to runtime or presented as captured pixels.
