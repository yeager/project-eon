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
| Reviewed trv2 v10 binary | x86_64 Linux, 62,014,696 bytes, SHA-256 `c6422037df6cadeb50ffaee3bb1c1b56d21722a7c687287d6058d4802943f54b`; restoration/review metadata in `CAPTURE_RECORDER_RESTORATION.md` |
| Rebuilt trv2 v11 recorder | x86_64 Linux, SHA-256 `18378aacf2a4bbe4fe3a84c295a1f53a3edd08c1cd4f5dca90f2c4af87c2181d`; separately bounded 128-record/site pre/post-input windows; v23 capture verified 2026-10-03 |
| Rebuilt trv2 v12 recorder | x86_64 Linux, 62,015,024 bytes, SHA-256 `7b3779771dd705aeb313f71c355e06fe6d4f77b836ea95f7b9748baba4eb4f64`; adds `$21822` input-read probe; v23 capture verified 2026-10-03 |
| Rebuilt trv2 v13 recorder | x86_64 Linux, 62,014,944 bytes, SHA-256 `e32f337dafdfb30e655f0aa8eb06e37a5dc445a225ec8473da92cad23cf5eb24`; expands the bounded raw-PC allowlist to 22 sites, including `$2182a/$2182c/$21834`; v23 capture verified 2026-10-03 |
| Rebuilt trv2 v15 recorder | x86_64 Linux, 62,014,944 bytes, SHA-256 `7160dfafbfe67b17db931065ab6f9853874591ea4af6c33ad51059b2b0f703df`; adds bounded post-load PC `$1edac`; no-input and physical-input VNC captures verified 2026-10-05, but do not reach the new site |
| Rebuilt trv2 v16 recorder | x86_64 Linux, 62,015,016 bytes, SHA-256 `aa4c797fc2e580c887ab6be88a57abe93f6b442ac822870e4840c514898518b4`; exact v15 baseline plus one observer site `$218cc`; schema-24 no-input diagnostic verified 2026-10-05, but does not reach the new site |
| Rebuilt trv2 v17 recorder | x86_64 Linux, 62,016,168 bytes, SHA-256 `8d7255b20a6f9867a9329541cdf5e0a590d2d507f639da313dd8964c7f02fd5e`; adds a bounded selector-cell snapshot at pre-instruction `$1fbe6`; no capture has been run |
| Rebuilt trv2 v18 candidate | x86_64 Linux, 62,016,152 bytes, SHA-256 `44477a0f41025a6e3f68098eb093fb7a32aebf3b2d1577fb294337a617154a54`; adds only raw-PC site `$21866`; schema 28 checks memory opcode `0x6608`; no capture has been run |
| Rebuilt trv2 v19 candidate | x86_64 Linux, 62,020,024 bytes, SHA-256 `7b46501fc3cf774938fc8ca4e02788586ba22ef440462ea7ecd00396311b73a2`; adds separate bounded late-input raw-PC and selector-cell sidecars; no capture has been run |
| Rebuilt trv2 v20 candidate | x86_64 Linux, 62,020,224 bytes, SHA-256 `071f1c949409be9ff3faa128d0acc98fcde9136c0aa09ab6a6edb058e7fbc397`; adds late-input-only PCs `$1fc22/$1fc9c`; receipt schema 30; no capture has been run |
| Rebuilt trv2 v21 recorder | x86_64 Linux, 62,030,288 bytes, SHA-256 `2fc7f47425d0fa005bb59bf41eaeccf32d1cba284dee4f227e7e723b853e1b35`; adds complete zero-route memory observations; schema 31; physical04 receipt verified, but no accepted game action established |
| Rebuilt trv2 v22 recorder | x86_64 Linux, 62,031,592 bytes, SHA-256 `eb0995c70f7f355f674d448b08c0f3e647430562ffde7d179e5aeb12e5abca71`; adds a separate later-input display-write sidecar after host-input ordinal 20; schema 32; visible no-input diagnostic verified 2026-10-07, no game action established |
| Rebuilt trv2 v23 recorder | x86_64 Linux, 62,039,312 bytes, SHA-256 `e4e46e84cd75eceffb28d26882fef0062582c251b0ef26baa0fccf6aff2e8dd1`; adds bounded observation of the exact `$21868` write to `$21720`; schema 33; visible attempt rejected 2026-10-07 for no host input |

The v16 source patch SHA-256 is
`1fa61e7984fe5535b42c07614be4f10a219323bf8536a3dc271a80c0b6f5f361`.
The build used the exact v15 source and binary baseline. In `libuae.a`, only
`newcpu.o` changed; all other archive members matched. `fs-uae --version`
reported `3.2.35`. The v15 executable remains preserved alongside v16 in the
external trv2 cache. The observer adds only bounded raw-PC site `$218cc`; its
allowlist is separate from v15, whose parser continues to reject that address.
The schema-24 verifier binds records to the exact v16 digest and size, while
schema-23 remains compatible with admitted v10–v15 evidence. This is protocol
This tooling support was followed by a verified v16 no-input diagnostic. No
`$218cc` sample appeared, so runtime reachability remains unresolved.

The v17 incremental source patch, applied on the reviewed v16 source tree, has
SHA-256 `37967e26f9cc4ae4ead2814c8656e940a4cdc0351c4c48e0ca3456708f5c7f95`.
The v17 binary was built on trv2 from that tree with
`./configure --disable-jit --without-libmpeg2 --disable-cdtv` and `make -j2
fs-uae`, using only the external Project Eon cache. The hook reads bytes at
`$1f98c` and `$1f98e` only when the pre-instruction PC is `$1fbe6`; each read is
limited to allocated chip RAM. A separate selector-dispatch receipt is capped
at 128 records before and 128 after the first recorder-confirmed host input.
Each record carries its raw-PC ordinal/cycle and the existing host-input
ordinal/frame link. Schema 27 recomputes those joins and rejects missing or
inconsistent samples. Input action values remain opaque; no action-ID meaning
is inferred here. No emulator was launched and no capture or original-media
access occurred during this build and review.

The v18 patch SHA-256 is
`7c1f253bc05e5aac81769d44905af01459769d771ccfdf37dd17bb092499439c`. It adds
only `$21866` to v17's finite raw-PC observer site set and resizes its bounded
per-site counters. An independent review confirmed that no guest state, input
path, or output format changed, and that only `newcpu.o` differs in the
175-member `libuae.a`; the v17 executable remains preserved in the external
cache. Receipt schema 28 binds this exact binary and rejects a `$21866` sample
unless the memory opcode is `0x6608`, and requires that sample to immediately
follow `$2185e` with the same host-input ordinal and frame. The observer still records pre-instruction
SR and does not read CIA input directly. No emulator was launched and no
capture or original-media access occurred during this build and review.

The v19 patch SHA-256 is
`f36706a6a4ce8e1c2ffa011392407c9418359052ccac3106a81ea02343eb8b9c`. It was
applied against the separately retained v18 observer header and CPU source;
the patched pre-build files have SHA-256
`675972bb134dba2d6a05641492aea8c72d7d5af86f228073b3026ccb80f9538a` and
`475b728a50db55653f9ebf3dab2a26a4a476d57443fbe56749e8ff9b06e4b30c`,
respectively. The x86_64 result reports FS-UAE `3.2.35`. Its `libuae.a`
member list is identical to v17, and the retained archive comparison reports
only `newcpu.o` differs. The patch, binary, and build log remain in
`/home/trv2/.cache/project-eon-tools/v19-late-window-20261006/`; no emulator
was launched and no game media was accessed during this build review.

V19 preserves the existing raw-PC and selector-dispatch files and writes
additional observations to separate `late-input-pc.txt` and
`late-selector-dispatch.txt` sidecars. The late raw-PC file admits at most 96
samples per reviewed site and 2,496 total; each must link to a distinct
host-input ordinal starting at 9. The selector sidecar is limited to 96 rows
and checked as a one-to-one chronological join to reached `$1fbe6` raw-PC
samples and the host-input receipt. Receipt schema 29 recomputes hashes,
grammars, caps, and joins. These bounds address the earlier input-phase
128-per-site cap while retaining historical v18 receipt grammar. They
establish recorder chronology and observed cell bytes only: delivery does not
prove that the guest polled or accepted an event, and cell bytes do not prove
a branch meaning, display update, or playable state.

The FS-UAE configuration file must be passed as the positional command-line
argument. `--config=…` is not a FS-UAE configuration-file option and is known
to fall back to default media; see
[the capture-status correction](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md#read-only-emulator-preflight).
The read-only capture configuration maps F10 to FS-UAE's
`action_drive_0_insert_floppy_1`. A visible operator can press it at the Disk 2
prompt to insert the already-mounted second image into DF0 when a browser-based
VNC client intercepts F12. This key changes emulator media selection only; it
is not treated as a game action. Disk 2 remains read-only, and the operator
must press F10 manually in the visible emulator window.

## Hook contract

The raw-PC prototype has the same pre-instruction hook immediately after
`r->opcode = r->ir` in both 68000 routes (`m68k_run_1` and the A500-selected
cycle-exact `m68k_run_1_ce`). It only reads the current PC, original
opcode, D0, A0, A6, SR and emulated cycle counter. It tests a fixed finite
site set:

- main-copy loop `$210d4`;
- input-read route `$21822` and following branch/join sites `$2182a`, `$2182c`,
  `$21834`;
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
The runner accepts either the recognised outer release ZIP, or both standalone
disk ZIPs as separate inputs. For the outer release, it exposes the outer
archive, each selected nested disk ZIP and Kickstart archive through distinct
`archivemount -o ro` views. For standalone input, it mounts each supplied disk
ZIP directly through its own read-only view. Both routes hash and size-check
the exact disk-1/disk-2 archives before mounting the contained ADFs, rehash
the physical source ZIPs after the run, and record the ordered identities in
`run-status.txt`. The standalone receipt labels its source layout and the
canonical content-release identity separately; it does not claim an outer
release ZIP or an invented combined physical hash. The receipt verifier
compares the identities to the reviewed constants without reopening user
media. Replay-fixture manifests still require an actual recognised physical
outer ZIP, so standalone-layout receipts are not admissible as replay-fixture
sources. Verify every FUSE mount reports
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

When the exact recognised disk ZIPs are supplied separately, replace
`--source-release` with both ordered arguments:

```sh
python3 tools/run_deuteros_amiga_capture.py \
  --disk1-archive '/absolute/path/to/Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).zip' \
  --disk2-archive '/absolute/path/to/Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 2 of 2).zip' \
  --kickstart-archive '/absolute/path/to/Kickstart v1.3 r34.005 (1987-12)(Commodore)(A500-A1000-A2000-CDTV)[!].zip' \
  --recorder /absolute/path/to/reviewed/fs-uae \
  --capture-intent physical-input \
  --output /home/you/.cache/project-eon-tools/deuteros-amiga-capture-<UTC>
```

The two disk-archive options are required together and are mutually exclusive
with `--source-release`. Disk order is fixed by the two option names; swapped,
missing, altered, symlinked or otherwise unrecognised sources fail before a
FUSE mount is started.

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

It admits only the documented outer or ordered standalone disk ZIPs, reviewed
recorder binary, clean Disk 1/Disk 2 ADFs and Kickstart ROM hashes. It mounts
the selected physical archive inputs separately with
`ro,nosuid,nodev,default_permissions`; supplies both FUSE ADFs with
`floppy_write_protect = 1`; and rehashes both source ZIPs after the run.
Disk 1 starts in DF0. Disk 2 is present in FS-UAE's removable-media swap list
but DF1 starts empty, so a visible operator can insert disk 2 into DF0 when the
game asks for it. This keeps both original images read-only and follows the
guest's explicit drive request.
It rejects repository/media/`/tmp` output paths and headless SDL. Its only
recorder outputs are raw PC and host-input-delivery receipts outside the
repository. `run-status.txt` binds the selected physical source layout, the
separate canonical content-release hash, the applicable post-run outer ZIP
or ordered standalone disk ZIP identities, Kickstart, reviewed recorder and
generated configuration; it reports the
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

New captures write `capture_receipt_version=23`. They bind both the complete
console-stream identity and the retained-prefix identity, validate the raw-PC
observer grammar, contiguous ordinals, monotonic cycles, reviewed probe-site
set, and finite per-phase counts before recording a non-semantic site-count
summary, and enforce a 64 MiB total-console safety cap. For v9 CPU lines, the
runner now bounds each reviewed site separately before the first nonzero
input ordinal and after delivery, at no more than 128 samples in either
phase; it records both phase/site summaries and the verifier recomputes them
when present. Existing single-window receipts remain verifiable. The rebuilt
v11 recorder has separate 128-record counters per site before and after
input, while retaining the global 4,096-record cap. Its verified v23 capture
is recorded in `DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md`. A cap crossing writes
`recorder_console_over_limit=true`, stops the recorder, preserves the bounded
prefix for review, and rejects the directory as inadmissible evidence. Receipt
v23 also binds the required declared capture intent to the recorder-owned
host-input receipt; it makes no claim about original-game input semantics.
The v12 build adds hash-bound guest address `$21822` to the fixed observer
sites (array capacity 16). Its 60-second VNC capture reached this input-read
route 128 times after visible host input and passed the independent receipt
verifier. The v13 build expands the allowlist to 22 sites, adding the next
branch and route sites without reading the custom register a second time. Its
verified VNC receipt records `$21834` after input but no samples at `$2182a`
or `$2182c`; this does not establish the branch condition or accepted game
state. Details and raw-file identity are in
`DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md`.
The v15 recorder additionally observes `$1edac`, immediately after the
hash-bound six-byte `MOVE.L $12ff4,D0` at `$1eda6`. Its D0 field can expose the
register value after that load if execution reaches the site. This remains a
raw pre-instruction sample and does not establish pointer validity or display.
The verified 90-second no-input VNC diagnostic reached the capped poll/branch
sites but not `$1edac`; only visible manually operated input can advance the
next evidence attempt through the selector.
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
tools built/extracted only in the scoped external cache.
No system package was installed, and neither dependency is part of Project
Eon or a release artifact. The restored recorder has since completed two
receipt-verified v23 physical-input runs on trv2; see
[`DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md`](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md)
for their bounded findings and unresolved guest poll/display boundary.

For any new recorder revision, review the complete diff against the exact
commit above, build outside the checkout and game-media directories, hash and
independently review the resulting binary, then pin it before a read-only
preflight. Retain output with its configuration, command, and operator-input
timeline preimages. Only independently reviewed observations can support a
new strict adapter revision.

## v20 selector-target recorder review

The v20 patch SHA-256 is
`cc55f243c54421719ed5b954df47a855d8e5c146a59f315806c1c3cdecfd91de` (3,357
bytes). It and the 62,020,224-byte x86_64 binary are retained under
`/home/trv2/.cache/project-eon-tools/v20-selector-targets-20261006/`. The
binary SHA-256 is
`071f1c949409be9ff3faa128d0acc98fcde9136c0aa09ab6a6edb058e7fbc397`; it
reports FS-UAE `3.2.35`.

The fixed observer table adds `$1fc22` and `$1fc9c` after the existing 26
sites. Both are admitted only in the bounded post-input sidecar, which now
allows 96 samples per site and 2,688 total. The ordinary raw-PC stream and
selector-cell sidecar retain their existing site sets and grammars. The
source diff changes only `src/eon_observer.hpp` and `src/newcpu.cpp`; a
content comparison of all 180 `libuae.a` members against v19 found only
`newcpu.o` changed. Schema 30 pins the executable and enables only these two
late-input sites; schema 29 and earlier continue using their former grammar.
No emulator was launched and no capture was run during this build/review.

## v22 later-input display recorder review

The v22 observer patch is retained externally at
`/home/trv2/.cache/project-eon-tools/v22-late-display.patch` (SHA-256
`60ecb8dbac4bb347f996e3ca24492775761ed4be02d91b121eaace439c49e180`). It was
applied to the already reviewed v21 source/build tree for FS-UAE v3.2.35 and
compiled on trv2 with the existing `--disable-jit --without-libmpeg2
--disable-cdtv` configuration. The x86_64 binary is
`/home/trv2/.cache/project-eon-tools/v22-late-display/source-tree/fs-uae`,
62,031,592 bytes, SHA-256
`eb0995c70f7f355f674d448b08c0f3e647430562ffde7d179e5aeb12e5abca71`.

The patch changes only the external `src/eon_observer.hpp`. It preserves the
existing title-display log and adds `late-display.txt`, independently opened
with the observer's exclusive, no-follow, mode-0600 output contract. It records
only the existing display-register allowlist, only after host-input ordinal
20, with 2,048 total rows, at most 64 rows per register, and a 512 KiB output
cap. Schema 32 pins the executable and recomputes this sidecar's grammar,
register counts, bounds, hashes, and frame/ordinal links to the host-input
receipt. These links establish capture chronology only; they do not establish
that an input caused a display write or that a particular frame was presented.

The binary contains the new bounded output path and format string. Its
allocated ELF section changes are recorded in
`/home/trv2/.cache/project-eon-tools/v22-late-display/loaded-section-diff.txt`
(SHA-256
`b3a5ff35303c0153c7dfc97fb7a4809546b989277deeced73af401e4823bbda0`). The
v22 executable has compiled and was attempted in a visible no-input diagnostic
at `/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261007-v22-physical02/`.
The run produced 1,024 raw-PC samples, all with input ordinal zero, then the
runner rejected it because the required physical-input receipt and completed
sidecars were absent. It is incomplete and does not establish a game action or
later display write. The original media remained read-only and was neither
copied nor modified. A completed visible physical-input capture and
independent receipt verification are still needed to learn whether later
inputs produce display writes.

## v23 main-stage latch-write observer review

The v23 patch is retained outside Git at
`/home/trv2/.cache/project-eon-tools/v23-latch-write-observer-20261007/v23.patch`
(SHA-256 `47c5dddc5d22761824b8a14d53947686d615be874746a2fefd6410617793e50d`).
It was applied to the v22 source tree at FS-UAE v3.2.35 commit
`4ae7ddaec50b567ed80d71ffbff067cb58e945a3`. Only `src/eon_observer.hpp` and
`src/newcpu.cpp` change. The x86_64 executable is
`/home/trv2/.cache/project-eon-tools/v23-latch-write-observer-20261007/source-tree-v22/fs-uae`,
62,039,312 bytes, SHA-256
`e4e46e84cd75eceffb28d26882fef0062582c251b0ef26baa0fccf6aff2e8dd1`.

At the hash-addressed main-stage code span `$21822..$21897`, the observer
checks the instruction at `$21868` (`13fc 0001 00021720`) and the following
PC `$21870`. It samples mapped chip RAM immediately before and after that
instruction and emits only a `0x00` to `0x01` change to `$21720`, paired with
the same host-input ordinal/frame. The sidecar is lazy-opened, limited to 64
records and 16 KiB, and performs no guest write, device access, or input
injection. Schema 33 pins the binary and strictly recomputes this record
grammar and chronology. A matching row would establish the original
instruction changed the latch during a host-input event; by itself it would
not identify an action or prove a rendered frame/gameplay result. The visible
visible operator-driven attempt on display `:6` ended without host input and
was rejected by the runner. Its incomplete external output is
`/home/trv2/.cache/project-eon-tools/deuteros-latch-evidence-20261007/capture-operator03/`;
it has no run receipt and proves no input acceptance. A later human-driven
capture is still required.
