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
| CPU route | A500-compatible 68000 loop, `src/newcpu.cpp:m68k_run_1` |
| Recorder activation | Exclusive new output named by `PROJECT_EON_FS_UAE_RAW_RECORD` |

The FS-UAE configuration file must be passed as the positional command-line
argument. `--config=…` is not a FS-UAE configuration-file option and is known
to fall back to default media; see
[the capture-status correction](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md#read-only-emulator-preflight).

## Hook contract

The prototype has one pre-instruction hook immediately after
`r->opcode = r->ir` in `m68k_run_1`. It only reads the current PC, original
opcode, D0, A0, A6, SR and emulated cycle counter. It tests a fixed finite
site set:

- main-copy loop `$210d4`;
- Exec / OpenLibrary / graphics / custom-register / callback sites `$40450`,
  `$4046c`, `$4069a`, `$1ed80`, `$1ef74`, `$1f056`;
- observed title display sites `$1eda6`, `$1f182`;
- selector and local routes `$1fe7a`, `$1fe84`, `$1fe88`, `$1fe92`,
  `$1fe96`, `$1fbe6`.

It writes at most 4,096 LF-terminated records to a new `0600` host file opened
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
