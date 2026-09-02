# Deuteros Amiga FS-UAE raw recorder design

This is a design and review record for an external, local FS-UAE probe. It is
not Project Eon runtime code, a distributed emulator patch, a reference trace,
or a release artifact. All emulator source, build products, raw output,
configuration and supplied media remain outside this repository.

## Exact source boundary

| Field | Value |
| --- | --- |
| Upstream source | `https://github.com/FrodeSolheim/fs-uae.git` |
| Reviewed tag | `v3.2.35` |
| Reviewed commit | `4ae7ddaec50b567ed80d71ffbff067cb58e945a3` |
| Local package used for configuration preflight | Ubuntu `fs-uae 3.2.35-2` |
| CPU route | A500-compatible cycle-exact 68000 loop, `src/newcpu.cpp:m68k_run_1_ce` |
| Recorder activation | Exclusive new output named by `PROJECT_EON_FS_UAE_RAW_RECORD` |
| Reviewed local v10 binary | aarch64 Linux, 61,449,016 bytes, SHA-256 `0e0bfb1fe73a6f37dc38992b39e34e355564adc516106c399c8be86fb38232ec` |

The FS-UAE configuration file must be passed as the positional command-line
argument. `--config=…` is not a FS-UAE configuration-file option and is known
to fall back to default media; see
[the capture-status correction](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md#read-only-emulator-preflight).

## Hook contract

The raw-PC prototype has the same pre-instruction hook immediately after
`r->opcode = r->ir` in both 68000 routes (`m68k_run_1` and the A500-selected
cycle-exact `m68k_run_1_ce`). It only reads the current PC, original
opcode, D0, A0, A6, SR and emulated cycle counter. It tests a fixed finite
site set:

- main-copy loop `$210d4`;
- Exec / OpenLibrary / graphics / custom-register / callback sites `$40450`,
  `$4046c`, `$4069a`, `$1ed80`, `$1ef74`, `$1f056`;
- observed title display sites `$1eda6`, `$1f182`;
- selector and local routes `$1fe7a`, `$1fe84`, `$1fe88`, `$1fe92`,
  `$1fe96`, `$1fbe6`.

It writes at most 4,096 LF-terminated records, with at most 128 records per
fixed probe site, to a new `0600` host file opened
with `O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW`. The environment value must be an
absolute path with no `..` component. An existing path, symlink, malformed
path, short write, or I/O error disables observation. It never overwrites an
output, follows a final-component symlink, writes to guest memory, alters
registers or flags, changes vectors, inserts input, changes event scheduling,
or writes a disk.

The raw grammar is intentionally outside all Project Eon reference-trace
versions. Recorder v9 adds a delivery chronology snapshot, not guest input
state:

```text
raw-pc <ordinal> cycles=<emulated-cycle> pc=0x<address> ir_opcode=0x<word> memory_opcode=0x<word> d0=0x<value> a0=0x<value> a6=0x<value> sr=0x<word> input_ordinal=<0..256> input_frame=<frame>
```

A v7/v9 line distinguishes the cycle-exact core's prefetched IR word from the
word currently read at its observed PC. Neither field is promoted to an
original-media instruction assertion without separately hash-bound load and
execution evidence. A line says only that the patched emulator reached that
observation point with raw CPU values. It is not an `event<TAB>…` record, does not carry a
release identity, and cannot be passed to `tools/record_reference_trace.py` or
`--reference-trace`. In particular it establishes no Exec result, graphics
ABI, callback meaning, bitplane layout, input semantic, title screen, audio,
or runtime transition. `input_ordinal=0 input_frame=0` means that no
recorder-confirmed host delivery preceded that sample. A nonzero pair names
the exact prior `host-input` receipt ordinal and its frame. The snapshot is
published only after that bounded receipt write succeeds and immediately
before the existing `amiga_send_input_event` call. It is an atomic packed
observer value so a CPU record cannot combine a newer ordinal with an older
frame. It does not copy action/state into the CPU record, claim an original
poll, or imply acceptance by the guest.

## Title-armed display-write receipt

Recorder v10 optionally writes `PROJECT_EON_FS_UAE_DISPLAY_RECORD`. It
arms only at the reviewed title site `$1eda6`, then records at most 2,048 CPU
or Copper writes to a finite allow-list: COP1LC, DIW, DDF, bitplane-pointer,
BPLCON0, BPL1MOD/BPL2MOD and COLOR registers (at most 64 writes/register).
The protected host file is capped at 512 KiB, enough for the full bounded
fixed-format receipt:

```text
display-arm 1 cycles=<emulated-cycle> site=0x0001eda6 input_ordinal=<0..256> input_frame=<frame>
display-write <ordinal> cycles=<emulated-cycle> vpos=<u16> hpos=<u16> origin=<cpu|copper> register=0x<word> value=0x<word> input_ordinal=<0..256> input_frame=<frame>
```

The sequence, cycles and input ordinals are monotonic; a nonzero input pair
must match the finite delivery receipt. The runner/verifier hash-bind the arm,
write/register and input-link summary. A present receipt does not prove that a
title frame was displayed or establish bitplane contents; an absent file is a
valid v10 result and says only that this observer did not trigger.

## Physical-input delivery receipt

The reviewed local probe also has a distinct, disabled-by-default receipt at
`src/fs-uae/main.c:input_handler_loop`. It observes an action only after the
FS-UAE frontend has dequeued it from `fs_emu_get_input_event`, and records it
only after `fs_uae_process_input_event` has rejected port-configuration and
state-management actions and immediately before it calls
`amiga_send_input_event`. The playback route is explicitly excluded. Thus it
records delivery to the Amiga core, not an arbitrary host key event and not a
recorder-created action.

Set `PROJECT_EON_FS_UAE_INPUT_RECORD` to a new absolute path. The observer
uses the same `0600`, `O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW` safeguards as the
raw-PC observer and writes at most 256 LF-terminated records:

```text
host-input <ordinal> frame=<emulated-frame> line=<scanline> action=<FS-UAE-action> state=<state>
```

This receipt is deliberately not an Eon reference-trace event and does not
claim that the game polled or accepted the event. It only supplies the missing
reviewable delivery side of a future user-operated capture. A physical user
press/release must still be retained as a separate input timeline and linked
to matching title-poll and frame observations; recorder-side injection,
playback, debugger commands and guest-memory edits remain inadmissible.

## Visible-window focus protocol

The capture runner starts FS-UAE windowed and reserves a ten-second manual
focus-settle period before its configured capture duration. During that period,
the physical operator must click the visible FS-UAE window themselves, then
press and release ordinary mapped keys in that window. Terminal keystrokes,
`xdotool`, autotype, debugger commands, playback, and guest-memory injection
are forbidden.

While FS-UAE is running, the runner reports `HOST INPUT DELIVERY OBSERVED`
only after the recorder has begun its protected host-input receipt. This is a
live usability signal, not evidence admission: the completed receipt is still
bounded, grammar-checked, chronology-checked, and hash-bound after the process
stops. A run without that signal is explicitly a no-delivery observation; it
must not be treated as a game-input or title-display capture. The optional
`--focus-settle-seconds 0..120` setting is bound into `run-status.txt`.

## Media and execution safeguards

If the reviewed external FS-UAE recorder has been restored to a cache but its
path is unknown, identify it by its pinned hash before capture. This lookup is
read-only and does not accept a normal FS-UAE installation:

```sh
python3 tools/locate_capture_recorder.py \
  --kind deuteros-amiga --root /home/you/.cache/project-eon-tools
```

Use only a returned path as `--recorder`; no match is an explicit capture
boundary rather than permission to rebuild or substitute a recorder.

Use only the recognised English Deuteros archive and its clean disk-1/disk-2
hashes listed in [the capture status](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md).
Expose the outer archive, each selected nested disk ZIP and Kickstart archive
through distinct `archivemount -o ro` views. The runner hashes and size-checks
each nested disk ZIP before it mounts the contained ADF, then records the two
nested identities in `run-status.txt`; the receipt verifier compares them to
the reviewed constants without reopening user media. Rehash the outer archive before
and after the run and verify every FUSE mount reports
`ro,nosuid,nodev,default_permissions`. Configure both drives with
`floppy_write_protect = 1`; do not rely on FS-UAE's overlay mechanism as a
substitute for write protection.

The first safe run is a bounded, no-input preflight. A later interactive run
must record physical key/button press and release timing in a separate input
timeline. Debugger commands, injected host events, guest memory edits and
recorder-side input are not admissible controls.

The repository's capture preflight helper prepares the interactive, physical
route without including, building, or modifying FS-UAE itself:

```sh
python3 tools/run_deuteros_amiga_capture.py \
  --source-release /absolute/path/to/Deuteros-The-Next-Millennium_Amiga_EN.zip \
  --kickstart-archive '/absolute/path/to/Kickstart v1.3 r34.005 (1987-12)(Commodore)(A500-A1000-A2000-CDTV)[!].zip' \
  --recorder /absolute/path/to/reviewed/fs-uae \
  --timing-profile realtime \
  --capture-intent physical-input \
  --output /home/you/.cache/project-eon-tools/deuteros-amiga-capture-<UTC>
```

`realtime` is the default and the only timing-faithful capture profile. New
runs must also state `--capture-intent physical-input` or
`--capture-intent diagnostic-no-input`. A physical-input run is rejected
unless the recorder retains a non-empty observed host-input receipt; a
no-input diagnostic is rejected if it retains host input. This is an
operator-procedure boundary only, never proof that Deuteros accepted input.
The
finite `warp` profile is allowed solely for separately labelled diagnostic
reachability work; it is receipt-bound but cannot establish original timing,
gameplay, or title-screen behaviour.

It admits only the documented outer ZIPs, reviewed recorder binary, clean
Disk 1/Disk 2 ADFs and Kickstart ROM hashes. It mounts the outer release, each
nested disk archive, and Kickstart archive separately with
`ro,nosuid,nodev,default_permissions`; supplies both FUSE ADFs with
`floppy_write_protect = 1`; and rehashes both source ZIPs after the run.
It rejects repository/media/`/tmp` output paths and headless SDL. Its only
recorder outputs are raw PC and host-input-delivery receipts outside the
repository. `run-status.txt` binds the post-run outer-release, Kickstart,
reviewed-recorder and generated-configuration identities; it reports the
optional raw-PC file by hash/size (with an 8 MiB ceiling), and the optional
title-armed display receipt by hash/size and bounded grammar summary (with a
512 KiB ceiling), and explicitly says
whether a receipt was created, hashing it only when nonempty and capping it at
64 KiB. It also records a SHA-256 and byte count for the complete FS-UAE
console while retaining at most the first 1 MiB in `recorder-console.log`.
Consequently a no-input preflight cannot silently look like an empty
physical-input timeline, and a defective recorder cannot make the terminal or
evidence cache grow without bound. A physical input timeline, independent
review and trace assembly remain required before any runtime admission.
Every FUSE mount is now checked by its exact mountpoint on cleanup; a failed
unmount rejects the run rather than silently leaving a read-only source view
inside a later evidence directory.

New captures write `capture_receipt_version=11`. They bind both the complete
console-stream identity and the retained-prefix identity, validate the raw-PC
observer grammar, contiguous ordinals, monotonic cycles, reviewed probe-site
set, and finite per-site counts before recording a non-semantic site-count
summary, and enforce a 64 MiB total-console safety cap. A cap crossing writes
`recorder_console_over_limit=true`, stops the recorder, preserves the bounded
prefix for review, and rejects the directory as inadmissible evidence. Receipt
v11 also binds the required declared capture intent to the recorder-owned
host-input receipt; it makes no claim about original-game input semantics.
v6 also binds the finite `realtime` or diagnostic-only `warp` timing profile
to the generated configuration, and validates a present FS-UAE host-delivery
receipt as at most 256
contiguous ASCII records in the reviewed `host-input` grammar and records its
count. Receipt v7 additionally requires the separate `ir_opcode` and
`memory_opcode` raw-PC fields; it rejects a legacy single-opcode line rather
than silently reinterpreting it. Receipt v8 recomputes a canonical per-site
summary of the observed IR/memory word pairs from the raw-PC file. It does not
assign those words an original-media, instruction, or ABI meaning. The action
and state integers remain opaque: this binds delivery-file
integrity, not a game input meaning or acceptance result. Receipt v9 additionally
requires the raw CPU file to use the v9 grammar and recomputes every nonzero
CPU snapshot against the finite host-delivery receipt. It rejects a missing,
mismatched, nonmonotonic, or hand-edited ordinal/frame link. State save/restore
and playback are excluded from both the receipt and snapshot. The resulting
link proves only that an observed host-to-core delivery preceded an observed
probe sample. Receipt v10 retains v9 raw-PC chronology and validates an
optional title-armed display-write receipt without claiming a displayed frame.
Receipt v2–v4 remain
verifiable as earlier evidence, but they do not contain the newer fields.
Verify a completed external capture without opening its game media with:

```sh
python3 tools/verify_capture_receipt.py \
  --kind deuteros-amiga --capture /absolute/cache/capture-directory
```

Pre-v2 capture directories remain diagnostic evidence only: their retained
console prefix was not hash-bound, so they cannot be verifier-admissible.
Repeat a physical capture rather than upgrading or editing its receipt.

On 2026-08-31, the reviewed v9 binary completed a fresh 15-second realtime,
input-free preflight against the exact recognised outer release and Kickstart
archive. Its external cache receipt was accepted by
`verify_capture_receipt.py` after the verifier recomputed the v9 (rather than
v7) IR/memory opcode grammar. It retained an empty console, no host-input
receipt, and no delivery-to-CPU links. The bounded raw-PC observation is
41,876 bytes with SHA-256
`fd52c57cb44a402fc7b9ddbeea0e8d1867dd09e8851f586ef515d6aba8698c39`: 256
records, split 128/128 between `0x0001fe84` (`7202/7202`) and `0x0001fe96`
(`7208/7208`). This validates the read-only receipt path and no-input
chronology only; it does not admit guest input, title execution, display,
audio, ABI, or gameplay evidence.

On 2026-08-31, a fresh 15-second input-free run against the recognised clean
outer release and Kickstart was accepted by that verifier as receipt v2. It
recorded a 28,052-byte, 256-record raw-PC observation with SHA-256
`1e2cdd13d31fb3b368448b4c24b3ca51501ff18876ce9e8df4260c4c29c26d74` and an
empty, hash-bound FS-UAE console. No host-input receipt was created. The
record cap stopped at the existing observer sites `0x1fe84` and `0x1fe96`;
this validates the read-only v2 evidence route only. It does not establish a
title entry, physical control, Exec or graphics return, bitplane, palette,
frame, audio checkpoint, or interactive game state.

Immediately after the v3 grammar/count gate was added, another 15-second
input-free run through the same read-only route was accepted as receipt v3.
Its 28,052-byte raw-PC output has the same SHA-256
`1e2cdd13d31fb3b368448b4c24b3ca51501ff18876ce9e8df4260c4c29c26d74` and
exactly 256 grammar-validated records: 128 at `0x0001fe84` and 128 at
`0x0001fe96`. The host-input receipt remains absent and the console is empty.
This proves that the new receipt gate describes the existing bounded observer
without widening its evidence: it remains only bootstrap/loader reachability,
not title entry, input, ABI, display, frame, audio, or gameplay evidence.

On 2026-08-31, the same write-protected preflight was repeated with receipt v4
and independently accepted by `verify_capture_receipt.py`. The recognised
outer release and Kickstart archive retained SHA-256
`f4dc8dd1…e470e04` and `c9521c11…11c42c04`; the run timed out normally after
15 seconds with an empty console and
`recorder_console_over_limit=false`. Its raw-PC result remained the same
28,052-byte, 256-record file with SHA-256
`1e2cdd13d31fb3b368448b4c24b3ca51501ff18876ce9e8df4260c4c29c26d74`, split
128/128 across `0x0001fe84` and `0x0001fe96`, and no host-input receipt was
created. This verifies v4's bounded evidence route only; it does not add a
title, control, Exec/graphics, bitplane, palette, frame, audio, or gameplay
fact.

The v5 route was then exercised against the same unchanged source release and
Kickstart archive. `verify_capture_receipt.py` accepted its external receipt:
the bounded raw-PC file remained the same 256-record bootstrap observation,
the console remained empty, and `recorder_console_over_limit=false`. This run
also contained a 709-byte, 15-record host-delivery receipt (SHA-256
`88368f6cd6c696af79f11835028b38139e9404d46aa50f76e1451d8b69fd1cbc`), whose
strict ordinal and signed-integer grammar was independently revalidated by
v5. The records are intentionally opaque frontend-to-core deliveries. They
are not attributed to a particular physical control and do not establish an
original poll, input acceptance, title transition, display, audio, or gameplay
fact.

The v4 route was also repeated after the checked-unmount contract was added.
The receipt verified with the same bounded raw-PC result and no host-input
receipt, both original archive hashes remained unchanged, and all four exact
outer/disk/ROM mountpoints were absent after cleanup. This is lifecycle
evidence for the capture tool only, not an additional title or gameplay
observation.

On 2026-08-30 the new delivery observer passed an eight-second no-input
preflight. The raw-PC observer produced its expected 384 site-capped records;
the delivery receipt path did not create a file, so FS-UAE's startup
port-configuration actions were not misclassified as core input. The external
recorder executable SHA-256 was
`727bba3ac4bc78558b964d0f572c488a419cd0985d803979e047381d2cf34f93`.
The supplied outer archive was rehashed after the run and remained
`f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`.

## Current build boundary

The external source is clean at the reviewed tag before the local probe patch.
Its Linux configuration needs the normal FS-UAE development dependencies. On
the current host, the reviewed source now configures and builds out of tree
with `--without-libmpeg2 --disable-cdtv` using OpenAL Soft 1.24.2 and gettext
tools built/extracted only beneath `/home/yeager/.cache/project-eon-tools/`.
No system package was installed, and neither dependency is part of Project
Eon or a release artifact. This clears the build prerequisite only: no raw
capture has yet been created or admitted.

When the external build prerequisites are available, review the complete diff
against the exact commit above, build outside the checkout and game-media
directories, hash the resulting binary, run the read-only preflight first,
then retain raw output together with configuration, command and input
preimages. Only independently reviewed observations can be used to design a
new strict v3/v4 adapter revision.
