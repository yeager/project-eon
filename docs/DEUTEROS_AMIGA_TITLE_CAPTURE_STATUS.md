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

The next reviewed recorder is receipt v9, pinned in the capture runner as a
61,505,560-byte aarch64 binary with SHA-256
`93636a80a9e1124ee6545fe45c0664a1ce07f9450063112c2da5b7a69a0afc8f`. It adds
only a read-only atomic ordinal/frame snapshot to each raw-PC observation.
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
