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
