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
versions:

```text
raw-pc <ordinal> cycles=<emulated-cycle> pc=0x<address> opcode=0x<word> d0=0x<value> a0=0x<value> a6=0x<value> sr=0x<word>
```

A line says only that the patched emulator reached that instruction with those
raw register observations. It is not an `event<TAB>…` record, does not carry a
release identity, and cannot be passed to `tools/record_reference_trace.py` or
`--reference-trace`. In particular it establishes no Exec result, graphics
ABI, callback meaning, bitplane layout, input semantic, title screen, audio,
or runtime transition.

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

## Media and execution safeguards

Use only the recognised English Deuteros archive and its clean disk-1/disk-2
hashes listed in [the capture status](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md).
Expose the outer archive, each selected nested disk ZIP and Kickstart archive
through distinct `archivemount -o ro` views. Rehash the outer archive before
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
  --output /home/you/.cache/project-eon-tools/deuteros-amiga-capture-<UTC>
```

It admits only the documented outer ZIPs, reviewed recorder binary, clean
Disk 1/Disk 2 ADFs and Kickstart ROM hashes. It mounts the outer release, each
nested disk archive, and Kickstart archive separately with
`ro,nosuid,nodev,default_permissions`; supplies both FUSE ADFs with
`floppy_write_protect = 1`; and rehashes both source ZIPs after the run.
It rejects repository/media/`/tmp` output paths and headless SDL. Its only
recorder outputs are raw PC and host-input-delivery receipts outside the
repository; a physical input timeline, independent review and trace assembly
remain required before any runtime admission.

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
