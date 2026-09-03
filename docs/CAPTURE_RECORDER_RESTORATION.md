# Capture recorder restoration

Project Eon has installed normal emulators (`dosbox-x`, `dosbox`, and
`fs-uae`), but ordinary emulator output is not preservation-admissible runtime
evidence. It may be used only for visible troubleshooting. It must never be
substituted for a reviewed recorder, interpreted as a trace, or used to infer
an original ABI, input result, frame, audio result, or game state.

## Required external recorders

The recorder binaries, their source trees, patches, raw output and build
directories remain outside this repository, outside original media, and
outside packages. Restore one of these exact executables to a scoped external
cache before asking Project Eon to run an evidence capture:

| Game | Protocol | Required SHA-256 | Role |
| --- | --- | --- | --- |
| Millennium DOS | `v21-int93-installation` | `18ec0ead7d08deeca694fbbe8155d5f5e6a99562adaea22fe914a691961fe1f1` | Read-only DOSBox-X observer for the current vector-installation boundary |
| Millennium DOS | `v13-title-poll` | `07d80df74d303b519884d37dd474da071b414e98396e8ae030ad89256432521b` | Host-key to original title-poll chronology only |
| Deuteros Amiga | reviewed FS-UAE v10 | `0e0bfb1fe73a6f37dc38992b39e34e355564adc516106c399c8be86fb38232ec` | Raw PC, host-delivery and title-armed display-write observer |

The default restoration location is a new directory under
`/home/yeager/.cache/project-eon-tools/`, for example
`/home/yeager/.cache/project-eon-tools/recorders/`. Do not place a recorder in
the checkout, `~/.projecteon`, an original-media directory, `/tmp`, or a
package staging tree.

## Locate before running

Never provide a path based on a filename or version string. Locate the restored
binary by its pinned digest:

```sh
python3 tools/locate_capture_recorder.py \
  --kind millennium-dos \
  --recorder-protocol v21-int93-installation \
  --root /home/yeager/.cache/project-eon-tools

python3 tools/locate_capture_recorder.py \
  --kind deuteros-amiga \
  --root /home/yeager/.cache/project-eon-tools
```

An empty result is a hard preservation boundary. It is not permission to use
`/usr/bin/dosbox-x`, `/usr/bin/fs-uae`, AUTOTYPE, debugger input, guest-memory
injection, a screenshot, or a hand-transcribed CPU window as a replacement.

## Millennium DOS capture once restored

Use the recognised English source archive, not an extracted copy:

```sh
python3 tools/run_millennium_dos_capture.py \
  --source-release /absolute/Downloads/Millennium-Return-to-Earth_DOS_EN.zip \
  --recorder /absolute/path/reported/by/locator/dosbox-x \
  --machine-profile svga_s3 \
  --capture-intent physical-input \
  --output /home/yeager/.cache/project-eon-tools/millennium-dos-capture-YYYYMMDD-NN
```

The output directory must be new and must not already exist. The helper mounts
the archive read-only, creates no game-data files, and requires a visible
display. Enter keys only in the focused, visible emulator window during the
capture window. It has no automated input route.

Verify the completed evidence before any recovery work:

```sh
python3 tools/verify_capture_receipt.py \
  --kind millennium-dos \
  --capture /home/yeager/.cache/project-eon-tools/millennium-dos-capture-YYYYMMDD-NN
```

## Deuteros Amiga capture once restored

Use the exact runner arguments documented in
[`DEUTEROS_AMIGA_FS_UAE_RECORDER.md`](DEUTEROS_AMIGA_FS_UAE_RECORDER.md).
The same rules apply: a fresh external output directory, read-only original
media, visible operator-driven input only, then
`tools/verify_capture_receipt.py --kind deuteros-amiga` before use.

## Current recovery boundary

As of 2026-09-03, both locators again returned no matching recorder under the
project cache: Millennium's `v21-int93-installation` search and Deuteros
Amiga's reviewed-FS-UAE search both produced the explicit empty result. Project
Eon must retain that fact and continue unblocked work, but it must not ask
again for generic emulator installation: the installed normal emulators are
known and insufficient. The next required input is either an absolute path to
an already pinned recorder, or an external reviewed source/patch/build which
produces the exact documented digest.

## Recorder restoration state machine

Recorder work is an explicit state machine. A transition may only advance when
its listed evidence exists; a failure remains recorded at its current state and
does not weaken an admission rule.

```text
LOCATOR_EMPTY
  -> RECORDER_SOURCE_READY
  -> BASELINE_BUILD
  -> BASELINE_VERIFY
  -> OBSERVER_PATCH
  -> INDEPENDENT_REVIEW
  -> PINNED_RECORDER
  -> VISIBLE_CAPTURE
  -> RECEIPT_VERIFIED
  -> TRACE_ADMITTED
  -> NATIVE_ENGINE_RECOVERY
```

`BASELINE_VERIFY` proves only that a known source revision can make an
executable on a stated host. `OBSERVER_PATCH` must be a minimal read-only
observation change. `INDEPENDENT_REVIEW` must verify that it neither injects
input nor guest state, changes guest timing, or records an unbounded/private
payload. Only `PINNED_RECORDER` may change one of the required SHA-256 values
above, and only after the review record is available. An experimental baseline
or observer must never be passed to a capture helper.

### 2026-09-02 baseline provenance

State reached: `BASELINE_VERIFY` (not `PINNED_RECORDER`). The external cache
source was DOSBox-X revision `234797680781567e18c374c9e62da24de5423db0`, built
with `g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`. The resulting local baseline
SHA-256 was
`b4a0727a4229d7581e584536b02488796ad0f1a8e7b84dfdd2f3841dab8eb509`.
The upstream static-SDL link required an explicit trailing `-lGL`; no source
file was changed. This digest deliberately does not match an approved recorder
digest, so the locator must continue to reject it. All source, build and
temporary paths were under `/home/yeager/.cache/project-eon-tools/`; no
original media, project file, package, or `/tmp` path participated.

### 2026-09-03 FS-UAE baseline provenance

The externally cached FS-UAE source at reviewed upstream commit
`4ae7ddaec50b567ed80d71ffbff067cb58e945a3` (tag `v3.2.35`) now builds on
the current aarch64 Linux host.  Bootstrap completed with the distribution
Autotools toolchain; the configured baseline used `--disable-jit`, then built
successfully with `make -j2`.  The resulting external executable was
61,647,256 bytes with SHA-256
`1c07a01833922ad3afd53f87380a32d68f832301234be1a72262266d582c9370` and
reported version `3.2.35`.

This is a `BASELINE_VERIFY` result only.  It establishes a reproducible source
and host-toolchain boundary for the separately approved recorder work; it does
not replace the reviewed v10 observer binary, whose hash, bounded observer
patch and independent review remain the admission contract.  The source and
all build outputs remain outside the repository at
`/home/yeager/.cache/project-eon-tools/recorder-recovery/fs-uae-source/`.
No game data, capture output or `/tmp` path participated.

Maintainer decision, 2026-09-03: this exact external FS-UAE executable is
approved as the recorder-development baseline. The approval is bound to the
upstream revision and executable SHA-256 above; a filename, a different build
of `v3.2.35`, or the system-installed FS-UAE is not interchangeable. This
authorizes work on the bounded observer protocol, but does not assert that
the unmodified baseline emits the reviewed v10 raw-PC/host-delivery records.
Accordingly it remains `BASELINE_VERIFY` for capture admission: no capture or
native recovery claim may use it until the observer output is implemented,
reviewed, and pinned for that protocol.

### 2026-09-02 experimental observer provenance

The first candidate reached `OBSERVER_FIX_REQUIRED` (not `INDEPENDENT_REVIEW`
and not `PINNED_RECORDER`). A first minimal V21 observer reconstruction was applied
only to the external source tree named above. Its patch is retained outside the
checkout at
`/home/yeager/.cache/project-eon-tools/recorder-recovery/observer-smoke-20260902/recorder-v21-experimental.patch`
with SHA-256
`479c043171fe9c5351340723a034bd2a80019d38e947fd1002d1b6b0775b0574`.
The first locally built experimental executable SHA-256 was
`5b9dd55d1c5eab1a34edd9561c60f7f11066ddd9492033cea50734a0dad60f51`.

An independent review rejected that first candidate before admission because
its opcode literals and record newline were escaped incorrectly, its preimage
start was one byte too early, and its output handling needed stricter path and
short-write handling. It must never be used.

A corrected follow-up candidate is retained at
`/home/yeager/.cache/project-eon-tools/recorder-recovery/observer-smoke-20260902/recorder-v21-experimental-v2.patch`
with SHA-256
`72e0e931cfda96f1a5cf786cd59f761975b5b71a1a6021b8f64c18e594282679`.
Its experimental executable SHA-256 is
`3c5e205e163bd6166fa517dadea8298bfec91bb965843852ff868ff6dc7be69f`.

The follow-up review also rejected v2 before admission: it read `DS:DX`
before exact candidate identity had been established and removed a failed
output by pathname after closing it. A third candidate moves that read behind
the exact mapped CS:PC and opcode checks, and fail-closes on a write failure
without pathname removal. Its external patch is
`/home/yeager/.cache/project-eon-tools/recorder-recovery/observer-smoke-20260902/recorder-v21-experimental-v3.patch`
with SHA-256
`eb9f21bd22b6d7105137b1c0495d87b02a894d4a5a2d8533d1dce81ba6aa793c`;
its experimental executable SHA-256 is
`26acf29a06ef53abb876b04d155540e38370daf5beb85fc8c51ffcd08bb98fce`.

The delta records an entry-CS map for the two exact 8.3 executable names,
arms only for the documented software `INT 21h AX=2593h` instruction
preimages at the mapped CS:PC locations, and emits one bounded host record
only after DOSBox-X's existing `RealSetVec` has installed vector `93h` with
the expected `DS:DX`. It does not add guest writes, register/flag assignment,
callback installation, input handling, debugger calls, scheduler calls or
guest file operations. The output requires an absolute fresh path and is
opened with exclusive, no-follow, owner-only permissions; the corrected
candidate also rejects `..` path components and fail-closes if a full write
cannot complete. A no-media `-version` smoke test with the output
environment variable set produced no output file.

The v3 candidate completed an independent static review on 2026-09-02 before
the functional integration check. The review rechecked the exact base revision and
both v3 hashes, opcode/PC identity, post-`RealSetVec` verification, one-shot
disarming, bounded host output and the absence of added guest writes, register
or flag changes, input, scheduling, callbacks and guest-file operations. A
short or failed write intentionally leaves an invalid bounded sidecar rather
than performing a pathname deletion; the globally disarmed observer cannot
retry it. This review did not pin the candidate.

The 2026-09-02 explicit experimental no-input run exposed an integration
defect: this source tree contains only the new v3 observer on vanilla
DOSBox-X, not the older recorder's normal-core/default-callback hooks. It
therefore reproduced the documented unhandled `INT 6` console loop and
generated neither legacy result streams nor an installer sidecar before the
64 MiB console safety cap. The run is retained at
`/home/yeager/.cache/project-eon-tools/millennium-dos-experimental-observer-20260902-02`;
its receipt explicitly says `experimental-observer-not-for-recovery` and is
not admissible. The current state is consequently `OBSERVER_FIX_REQUIRED`
(not `PINNED_RECORDER`). The candidate is not the approved `18ec0e…`
recorder, cannot be located by the Project Eon capture tools, and must not be
used for a native recovery claim. A separate pin decision must assess the
persisted review record, exact binary and release-identity mapping before any
candidate can be pinned.

### 2026-09-03 clean DOSBox-X observer rebuild

The interrupted worktree attempt was retained outside the repository as an
invalid external artifact after its Git metadata and checked-out source files
were observed to be zero bytes. A fresh ordinary clone was then made outside
the repository from the already reviewed DOSBox-X revision
`234797680781567e18c374c9e62da24de5423db0`. The existing v3 observer patch
was applied cleanly and the source was bootstrapped with Autotools and
configured with `--enable-debug=heavy --enable-sdl2 --prefix=/usr
--disable-sdltest --disable-opengl`. Explicit `--enable-sdl2` is required by
this upstream revision; merely having `sdl2-config` on `PATH` is not enough.

The resulting external executable is 121 MiB and has SHA-256
`35eca0e9248d42a8b682d67cc7e112193be51360728b740e9938f96c504cebaa`.
Its `-version` output identifies DOSBox-X `2026.08.02` with SDL2. This is a
reproducible development build only, not an approved recorder: its different
toolchain/configuration hash and its missing legacy normal-core/default-
callback observers mean the locator and normal capture runner must continue
to reject it. The next recorder change is limited to restoring those
read-only observation hooks atop this fresh source before a new independent
review and pin decision.

### 2026-09-03 V22 callback-probe review

The first V22 follow-up was rejected before any emulator run or capture. It
added one host-side INT6 callback record to the clean v3 development build,
but its synchronous filesystem I/O occurred inside DOSBox-X's default callback
before the existing return. That can perturb wall-clock timing and host-event
scheduling even though it does not directly write guest registers or memory.
It also lacked a release/site/opcode gate, consumed its one observation on an
unrelated or unwritable occurrence, emitted a non-canonical literal `\\n`,
and was not portable to the project's Windows DOSBox-X build surface.

### 2026-09-03 independent re-review of the V21 development rebuild

State remains: `OBSERVER_FIX_REQUIRED` (not `INDEPENDENT_REVIEW` and not
`PINNED_RECORDER`). A clean external development rebuild of the existing V21
v3 patch was independently inspected again. Its source base was
`234797680781567e18c374c9e62da24de5423db0`, the dirty source diff matched the
external v3 patch SHA-256
`eb9f21bd22b6d7105137b1c0495d87b02a894d4a5a2d8533d1dce81ba6aa793c`, and its
aarch64 executable SHA-256 was
`783502776b2a5acb856395445a60296ff2c7160bda41398598146a5ed4f52bba`.
It was built externally with Ubuntu g++ 15.2.0.  A no-media `-version` smoke
test with the observer-output environment variable set created no sidecar.

The re-review rejects the binary for capture admission. Although the source
gate still checks the mapped CS:PC and exact nine-byte installer preimage
before reading `DS:DX`, its `open`/`write`/`close` sidecar operation occurs
inside `DOS_21Handler` while guest execution is active. That synchronous host
filesystem work can perturb wall-clock timing or event scheduling. The
candidate also identifies an image only by its two canonical executable names,
entry CS and instruction bytes; it does not bind a recognised outer release or
loaded-image digest. Finally, its unconditional POSIX output code has not been
reviewed for the Windows DOSBox-X build surface. These are admission failures,
not evidence gaps that a capture runner may waive.

The successor must retain at most one bounded POD observation in recorder-owned
memory during guest execution, bind the recognised release plus loaded
image/site/opcode identity before arming, and flush only at a separately
reviewed host-safe point after guest execution has stopped. It must have a
reproducible build record before any new independent review or pin decision.

The reviewed candidate host-safe point is DOSBox-X's terminal-only path in
`src/gui/sdlmain.cpp`, immediately before `GFX_ShutDown()` (the current source
location is line 10556). All restart, BIOS and guest-OS paths jump back to
`fresh_boot` before that point. `DOSBOX_RunMachine()` is explicitly unsuitable:
it is recursive and can return from callbacks, interrupts and page-fault
paths while guest execution continues. DOS shutdown events and generic exit
callbacks are unsuitable for the same reason or run after the required
recorder state may have been torn down.

The next external patch must therefore make the `INT 21h` boundary write only
one zero-initialised recorder-owned POD slot. It may copy the already verified
scalar source CS:PC, DS:DX, target preimage and installed vector values, then
publish `captured` last. It may not inspect the environment, allocate, log,
open, write, close, lock, schedule work or read additional guest memory at
that boundary. A host-only serializer may consume that slot only at the
terminal point above, before callback/memory teardown; a forced process kill
must produce no record. The output path must be parsed and validated before
guest execution, not fetched from an environment variable while an interrupt
is being handled.

This design remains incomplete by intent. The present name-plus-entry-CS-plus-
opcode predicate is not a recognised-release identity. Before arming, the
successor also needs a configured recognised outer-release identity and a
reviewed loaded-image fingerprint bound to the same entry CS and mapped site.
Until those two identities, the host serializer and a visible graceful-stop
route are independently reviewed together, the candidate remains development
only and cannot be passed to a capture helper.

The V21 runner now writes the already rehashed recognised outer archive digest,
direct-media-set digest, and the two manifest expected loaded-image SHA-256
values and sizes into a recorder-only `[project-eon-recorder-v21]` config
section. The normal capture receipt already binds the generated configuration
hash, so this is stronger and more reproducible than an environment variable.
It does not change the current pinned recorder protocol and a historic recorder
may ignore the unknown section. A successor must parse it before guest start,
treat it solely as expected identity configuration, recompute a bounded loaded-
image fingerprint itself, and refuse to arm on any mismatch. These configured
values are not guest-provided evidence and do not make the development observer
admissible.

The reviewed DOSBox-X integration order is also fixed. A successor registers
`project-eon-recorder-v21` with `Config::AddSection_prop()` in `sdlmain.cpp`
immediately after `DOSBOX_SetupConfigSections()` and before the first
`ParseConfigFile`. Every identity property is `OnlyAtStart` with an empty
default. It validates strict lowercase hexadecimal values and exact sizes only
after configuration parsing has completed but before `DOSBOX_RealInit` can
start guest execution. The ordinary DOSBox-X environment configuration pass
must explicitly skip this recorder-only section: otherwise a crafted host
environment can override a value that the runner's retained config hash was
supposed to bind. Unknown configuration sections are discarded by DOSBox-X,
so the successor must register this section; it cannot rely on a generic
unknown-section reader.

The V22 binary and source remain external rejected artifacts. They must not
be pinned, located, run through a capture helper, or used for native recovery.
The only acceptable successor records a bounded callback fact in recorder-
owned in-process storage without filesystem work at the callback boundary,
then flushes it from a separately reviewed host-safe point after guest
execution has stopped. It must also bind the exact release/image/site/opcode
identity and fail closed without consuming a later eligible observation.

### V22 successor callback contract

The reviewed successor has one narrow callback contract. DOSBox-X reaches
`default_handler` only after it decodes its default stub `FE 38 03 00`; the
permitted callback point is therefore `f000:ca64`, with stub origin
`f000:ca60` and callback index `3`. It must require all of those literals,
`lastint == 0x06`, and a prior exact Millennium title-prefix arm. A
non-matching callback cannot consume the recorder slot.

Only after that gate, the callback may copy the three already-pushed real-mode
IRET words at `SS:SP`—`return_ip`, `return_cs`, and `return_flags`—plus the
listed scalar registers into one preallocated recorder-owned POD slot. They
are raw exception-frame facts, never a title ABI, function return, mapping, or
game result. The canonical bounded record contains the schema identifier
`unhandled-int6-v2`, interrupt, callback CS/IP/stub/index, SS:SP, those three
raw words, and AX/BX/CX/DX.

The callback may not allocate, log, open or write a file, inspect environment,
write guest memory, alter registers/vectors, request a stop, or schedule work.
An independently reviewed host-safe shutdown/flush point remains required;
the in-memory slot alone cannot survive a forced process kill.

### V22 successor implementation boundary

The first implementation attempt must not turn the existing INT 93 installer
observer into the required title-prefix arm. Its two executable-name/CS/PC
checks are for a different request (the `INT 21h AX=2593` vector-installation
observation), and that request is absent from the observed title-to-INT6
route. Likewise, a filename, an `INT 6`, a default-callback address, or an
old recorder receipt is not a release identity or causal arm.

Before the successor may capture a slot, it therefore needs two independently
reviewed additions: a configured recognised-archive identity plus a mapped
loaded-image/site/opcode predicate, and a state transition that proves that
predicate occurred on the relevant Millennium title prefix. Until both exist,
the callback receiver remains deliberately unarmed and records nothing.

Once such an arm is proven, the receiver belongs beside DOSBox-X's CPU callback
code as a zero-initialised, preallocated POD slot with the states `empty`,
`captured`, and `flushed`. It tests the arm and all literal callback gates,
copies the bounded words, and marks `captured` last. It has no strings,
environment lookup, logging, allocation, locking, or host/guest writes.

The host may serialise a captured slot only on DOSBox-X's terminal shutdown
path after `DOSBOX_RunMachine()` has returned and before callback/memory
teardown. The current Project Eon runner's early `process.kill()` path cannot
be used: forced termination cannot execute that flush and must yield *no
record*, not a synthetic one. A future recorder invocation consequently needs
a separately reviewed visible graceful termination path and an exclusive,
regular, mode-0600 output sink validated outside guest execution.

### 2026-09-03 V23 POD callback foundation

An external clean DOSBox-X source copy at
`/home/yeager/.cache/project-eon-tools/recorder-recovery/dosbox-x-v23-pod-observer/`
was created from revision `234797680781567e18c374c9e62da24de5423db0` for the
next recorder implementation. It contains a zero-initialised, fixed-size
`ProjectEonInt6Slot` in `src/cpu/callback.cpp`. At the literal callback site,
the slot can only copy the bounded exception frame and scalar registers, and
publishes its captured state last. It performs no output, environment lookup,
allocation, logging, guest write, register/vector change, input handling or
scheduling.

The slot is deliberately unarmed: the reviewed release identity, loaded-image
fingerprint and title-prefix predicate do not yet exist. Therefore no `INT 6`
event is eligible to capture, there is no serializer, and no trace artifact
exists. The CPU component compiled on the configured external host. A full
recursive Autotools build is not recorded because interrupted tool execution
left competing make children; this is an implementation foundation only,
below `OBSERVER_FIX_REQUIRED`, never a recorder, pin, trace or capture target.

### Experimental observer runs

The reviewed v3 candidate may be run only with the explicit
`--experimental-observer` capture-runner switch and the
`v21-int93-installation` protocol. This is a visible, read-only emulator
observation run, useful for testing the candidate's own bounded sidecar against
real supplied media. Its receipt records
`recorder_admission=experimental-observer-not-for-recovery`; the normal
verifier rejects it. A maintainer may inspect its integrity with
`verify_capture_receipt.py --allow-experimental-observer`, but that command
does not promote it, alter a pinned hash, admit a trace, or authorize native
recovery. The ordinary pin prerequisites above remain mandatory.

### 2026-09-03 V21 configuration registration build

The external DOSBox-X development tree was rebuilt sequentially with `make
-C src -j2` after a clean. The resulting executable is 128,692,024 bytes and
has SHA-256
`53f2f569b7cf44df50cfe2eb55dd770765e1ee1ae617dffd60c2f2bad83910ef`.
This rebuild includes an inert registration of the runner's
`[project-eon-recorder-v21]` configuration section immediately after
`DOSBOX_SetupConfigSections()`. `strings` confirms the section and its two
release-set identity keys are present in the output.

This establishes only that the configuration registration itself compiles on
the stated external source/toolchain. It does **not** validate the values,
prevent environment override, fingerprint a loaded executable, arm an
observer, provide the POD callback slot, or provide the graceful host-only
serializer. The source also retains the previously rejected synchronous-I/O
observer changes. Consequently this hash is an `OBSERVER_FIX_REQUIRED`
development artifact, never a recorder locator value, capture target, or
native-recovery evidence. No media was mounted or read during the rebuild.
