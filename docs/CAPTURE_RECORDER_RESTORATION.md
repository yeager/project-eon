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
| Deuteros Amiga | reviewed FS-UAE v10, trv2 x86_64 | `c6422037df6cadeb50ffaee3bb1c1b56d21722a7c687287d6058d4802943f54b` | Independently reviewed restoration of the same bounded grammar |

The default restoration location is a new directory under
`$HOME/.cache/project-eon-tools/`, for example
`$HOME/.cache/project-eon-tools/recorders/`. Do not place a recorder in
the checkout, `~/.projecteon`, an original-media directory, `/tmp`, or a
package staging tree.

## Locate before running

Never provide a path based on a filename or version string. Locate the restored
binary by its pinned digest:

```sh
python3 tools/locate_capture_recorder.py \
  --kind millennium-dos \
  --recorder-protocol v21-int93-installation \
  --root "$HOME/.cache/project-eon-tools"

python3 tools/locate_capture_recorder.py \
  --kind deuteros-amiga \
  --root "$HOME/.cache/project-eon-tools"
```

For recorder-restoration work, append `--diagnose` to report only aggregate
scan facts (`roots`, `hashes-checked`, and `reviewed-matches`). This can show
that an external cache was actually searched without exposing unmatched binary
paths, contents, or digests. It is operational troubleshooting data, not
capture evidence and never changes admission:

```sh
python3 tools/locate_capture_recorder.py \
  --kind millennium-dos \
  --recorder-protocol v21-int93-installation \
  --root "$HOME/.cache/project-eon-tools" \
  --diagnose
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
  --output "$HOME/.cache/project-eon-tools/millennium-dos-capture-YYYYMMDD-NN"
```

The output directory must be new and must not already exist. The helper mounts
the archive read-only, creates no game-data files, and requires a visible
display. Enter keys only in the focused, visible emulator window during the
capture window. It has no automated input route.

Verify the completed evidence before any recovery work:

```sh
python3 tools/verify_capture_receipt.py \
  --kind millennium-dos \
  --capture "$HOME/.cache/project-eon-tools/millennium-dos-capture-YYYYMMDD-NN"
```

## Deuteros Amiga capture once restored

Use the exact runner arguments documented in
[`DEUTEROS_AMIGA_FS_UAE_RECORDER.md`](DEUTEROS_AMIGA_FS_UAE_RECORDER.md).
The same rules apply: a fresh external output directory, read-only original
media, visible operator-driven input only, then
`tools/verify_capture_receipt.py --kind deuteros-amiga` before use.

## Historical recovery boundary (2026-09-28)

As of 2026-09-28, both locators returned no matching recorder under the
provisioned host's `/home/trv2/.cache/project-eon-tools` cache:
Millennium's `v21-int93-installation` search and Deuteros Amiga's reviewed-
FS-UAE search each reported `roots=1`, `hashes-checked=35`,
`executables=35`, `size-rejected=0`, and `reviewed-matches=0`. A metadata-only
search of the Eon workspace and scoped cache found no Eon capture directories
or receipts. These locator diagnostics are not capture evidence and do not
change admission. Project Eon must continue any unblocked work, but it must
not ask again for generic emulator installation: normal installed emulators
are known and insufficient. The recorder boundary can move only when an
already pinned executable is restored to the scoped cache, or when the exact
documented digest is produced by the specified reviewed restoration path.

## trv2 FS-UAE restoration (2026-09-29)

The Deuteros recorder reached `PINNED_RECORDER` on trv2 (x86_64 Linux).
The historical aarch64 pin remains valid. Runner, locator and receipt verifier
share the finite reviewed-build registry; an arbitrary system FS-UAE still
fails admission. Millennium DOS recorder availability is unchanged.

All source, patches, test fixtures, review and binaries remain beneath the
external cache `~/.cache/project-eon-tools/recorder-recovery-20260929/`.
The baseline uses upstream revision
`4ae7ddaec50b567ed80d71ffbff067cb58e945a3`, GCC 15.2.0, bootstrap and configure
flags `--disable-jit --without-libmpeg2 --disable-cdtv`, then `make -j8` with
`TMPDIR` beneath that cache. Baseline SHA-256:
`14bcef8b7982d6c866bd905f71fb68b7aa038647bc2d556493e4cc414fe7f2d7`.

The complete external patch `fs-uae-observer-v10-trv2.patch` has SHA-256
`ec5eb3e3ee13305070b78277f21ce58c4f88dfe098ca2fe025910959b9366851`.
It adds only the bounded observer header and hooks in `newcpu.cpp`,
`custom.cpp` and `fs-uae/main.c`. CPU samples directly read the canonical
mapped chip-RAM allocation, reject other banks and compose SR without
mutating cached flags. Live frontend delivery publishes the packed atomic
ordinal/frame only after a successful receipt write; playback, state actions
and port-selection routes cannot publish a delivery. Display hooks distinguish
CPU and both Copper routes and exclude custom-register read fallback.

An independent source review checked the CPU/header/frontend changes and
the single emulation-thread guarantee for the required A500 configuration.
Separate reviewers checked the display hooks. The external review record is
`fs-uae-trv2-review.md`, SHA-256
`c34f4b6114d2ea8c00173449207f9d1c279de91699b9c31f5466f928a33eb616`.
It approves the exact binary
`c6422037df6cadeb50ffaee3bb1c1b56d21722a7c687287d6058d4802943f54b`,
62,014,696 bytes. Recompiling all three modified translation units reproduced
that digest, and reverse-applying the complete patch passed a dry check.

The actual observer header has SHA-256
`270c8e5fee262c12df951e9431ba9f19d8b1a63384a6af47e7964fc69215f939`.
Its external C++ harness source has SHA-256
`1f18198849495c809994a6bd5074ac9da58bb1e2c37ea84e74586ac43609033d`.
Runner parsers accepted bounded protocol fixtures and rejected a real injected
short write. Tests covered 128 raw samples/site, 256 input deliveries, 2,048
display writes, 64 writes/register, title arming, register filtering, atomic
input links, failed input publication, unset/relative/parent-component paths,
existing-file and symlink protection, and `0600` permissions. These fixtures
are module tests only; they contain no original media and are not captures.
A no-media `--version` check produced no observer files.
The reproducible external parser driver `harness/eon_observer_test_driver.py`
has SHA-256 `9c9093aba7f962709b1c681a241f20ef5b8438119bcb191e2e5aa64fea6ef40a`;
its passing metadata summary `harness/eon_observer_test_summary.json` has
SHA-256 `52e20eba26d76906454db89fa7e6f17733f562677a6525a5b7b56733bc97f833`.

After pinning, the locator found this exact executable. A fresh visible
15-second realtime `diagnostic-no-input` run then passed receipt verification
using the user's standalone disk archives and existing Kickstart ZIP, all
mounted read-only and rehashed afterward. The external v23 `run-status.txt`
has SHA-256 `cad2d29e7d4642296fe515e20bd60d98fd2e23c05954ea41a6ac792c6225a34c`.
It records 256 raw samples (128/site at `$1fe84` and `$1fe96`), no host input,
no title-display receipt and an empty console. See `PRESERVATION.md` for the
raw-output identity. This reaches `RECEIPT_VERIFIED` for the no-input diagnostic
only; manual input, trace admission and native recovery remain separate steps.

## trv2 DOSBox-X identity foundation (2026-09-29)

The exact upstream revision `234797680781567e18c374c9e62da24de5423db0`
now builds on trv2 with GCC 15.2.0. The external source and artifacts are under
`~/.cache/project-eon-tools/recorder-recovery-20260929/`, with compiler
`TMPDIR` set to its `tmp/` directory. Configuration uses
`--enable-debug=heavy --enable-sdl2 --prefix=/usr --disable-sdltest
--disable-opengl`. Installing SDL2_net/FFmpeg development packages and repeating
configure resolved the missing-header and stale-link-library failures without
upstream source changes. The clean baseline is 132,860,704 bytes, SHA-256
`2a823db6bde49f449c9d9904c6e41bf3f195e038424bc4411067f4fb054eacca`.

A separately built identity-only development patch now registers and validates
the runner's six exact v21 configuration fields before guest initialization;
`Config::ParseEnv` explicitly excludes the recorder section. Its COM loader
check hashes only the existing successful read buffer, requires the original
full size to equal the returned read size, and matches the exact name, size
and SHA-256 of `TITLES.EXE` or `2200AD.EXE`. MZ loads are excluded. It adds no
file reads, seeks, guest writes, input, capture output or observer arm.
Identity is cleared on every execute attempt, termination (including TSR),
and machine initialization. Parent identity is deliberately not restored after
nested termination; a complete process-lifetime map remains future work.

The complete external patch `dosbox-identity-foundation.patch` has SHA-256
`65a3980bc9b3eed948d050a8e1e79dcc605b6da5972b96493048d529ca936cb3`.
Its executable is 132,895,488 bytes, SHA-256
`7face6cfc41f48523c0478f5677cff83b6fea15289ad4d3a744b8d9e226a5a96`.
Reverse-patch checking and full compilation passed. Independent harnesses
checked the exact configuration helpers/environment parser and portable SHA
padding boundaries, failure gates and both genuine DOS leaves read in place.
The trv2 identity-test hash manifest is
`2f4f7d6681f407cb3805575008cc3dcc5554340943f4da8733743426265ae98a`.
Detailed build/review metadata remains in `dosbox-identity-build-record.md`
beside the patch; test sources/results are in `config-harness/` and
`identity-harness/`. No emulator run used game media.

This remains `OBSERVER_FIX_REQUIRED`, not a pinned recorder. Exact normal-core
and callback predicates, bounded POD observation storage, terminal-only
serialization and a reviewed graceful-stop route still have to be integrated
and independently reviewed. Existing recorder pins and native admission are
unchanged.

### Bounded title-prefix and INT-6 observation foundation

The subsequent trv2 development build integrates normal-core prefetch and
software-interrupt predicates with one recorder-owned POD slot. Arming requires
the SHA-256-bound loaded `TITLES.EXE`, its dynamic load CS, normal real-mode
execution at IP `0134`, the exact `cd93075d` preimage, and the following software
INT `93h` at return IP `0136` with a zero IVT entry. It does not substitute the
`2200AD.EXE` CRC tuple for this observed title prefix.

The later default INT-6 callback must match index `3`, CS:IP `f000:ca64`, and
stub bytes `fe380300` at `f000:ca60`. The slot records only bounded scalar
register/frame values and publishes its captured flag last. Memory inspection
requires direct low-memory RAM or ROM page-handler identity, allocation bounds
and unchanged page aliases; it bypasses neither a hardware handler nor a slow
path that could change mapping state. Stack words use wrapped 16-bit starting
offsets with contiguous two-byte reads. Execute/termination invalidates pending
arming but preserves an already captured slot; fresh-machine initialization
clears the entire slot.

Full compilation and reverse-patch checking passed on trv2. The external
`dosbox-pod-foundation.patch` has SHA-256
`227a0759a644f40a376968962c0ed7fd2b1acd844e69bb19e8b2644e15f29ac6`.
The preserved executable `dosbox-x-pod` is 132,908,416 bytes, SHA-256
`524f60359f03c315d44475e3f2d0ea851622d79cdf533fe9b6afab4ce4bbba60`.
An actual-source harness in the external `observation-harness/` directory
passed ordered arming, once-only capture, rejection gates, lifecycle reset,
stack-boundary and memory-boundary cases. Its generated source SHA-256 is
`08250c0db2dd799b059ddc65b2449ffb260e8e15fcf0f100ded7fdadf15bde0a`;
its executable SHA-256 is
`bba0e4a52ebe3da2f9d61965f5eee24e8f14e8bf57f7e851b2c920195bad0881`.
These checks used no emulator run or original media.
Independent source review also checked the direct page-handler predicates,
normal-core/interrupt ordering, callback allocation and exception-stack layout;
it found no blocking guest-side effect or mapping issue. The complete build
record remains external in `dosbox-pod-build-record.md`.

This remains `OBSERVER_FIX_REQUIRED`. There is no terminal serializer in this
build, no emitted observation, and no recorder pin or capture admission. The
existing SDL window-close route reaches the terminal shutdown point, but its
integration with a successor runner and serializer still requires review.

### Terminal output development integration

A subsequent external build connects a bounded serializer only to the final
`sdlmain.cpp` path immediately before `GFX_ShutDown()`, beyond the restart
branches. The new `OnlyAtStart` `terminal_output_path` field shares the
environment-excluded recorder section. Before guest initialization, the caller
requires an absolute path beneath trv2's scoped
`recorder-recovery-20260929/terminal-captures/` cache directory. The serializer
opens each parent component without following symlinks and retains the final
directory descriptor; that directory must belong to the effective user and
must not be group/world writable. No destination file is created at this stage.

The terminal caller requires a captured POD, a prepared sink and an accepted
SDL2 window-close event. That event is a raw host fact, not proof of physical
input. Exclusive mode-0600 creation and bounded serialization occur only after
guest execution stops. Write, file/directory sync and close failures leave the
requested run at exit status `86`; a leftover file is never sufficient. Only
successful serialization followed by a written/flushed terminal stdout marker
can clear that failure status. A future consumer must bind the exact marker,
successful process exit and output hash together. Reboot initialization clears
the observation and accepted-close state.

Full compilation and reverse-patch checking passed. The preserved external
`dosbox-x-terminal` is 132,919,880 bytes, SHA-256
`aeba73d5df374b20479e20bb75c13200fbea1e39ae91464c6f50b77eea036564`.
The complete `dosbox-terminal-foundation.patch` has SHA-256
`6964e331729e19aac14bfe65fd01bb4c69ea2a67306728fbe57384f7f1869d1b`.
Actual-header POSIX/non-POSIX harnesses passed path bounds, symlink/collision
rejection, parent-directory anchoring, ownership/permission checks and injected
write/sync/close failures. The header SHA-256 is
`c04ae69b764f58dd2698cda50390d917cbedd2f2d65ebf067719e76b3cdb4109`;
the harness source SHA-256 is
`841573e4778ca327dc8cf79cd5cff02422a815b3fb1a06dd9a63f1697060f59c`.
Build details and test artifacts remain in the external
`dosbox-terminal-build-record.md` and `terminal-harness/`.
Independent integration review identified and verified fixes for internal
`fresh_boot` reset and accepted close events in the menu/focus/pause paths.
Actual-helper tests also passed success, absent/invalid configuration, absent
close, and stdout-failure cases; the latter retained status `86` despite an
existing output file. The glue harness source SHA-256 is
`cf1accf99029bf3698178d1efb11d04986b4052d407469d0811c3886ae9018ec`.
The seven-property configuration harness confirmed the new path's empty
default, startup-only mutability and exclusion from environment overrides.

This remains `OBSERVER_FIX_REQUIRED`, with no media run or new pin. The emitted
schema is explicitly development-only. Host-input receipts and the successor
runner/validator contract still require implementation and complete review.

### Bounded SDL input development integration

The next external build retains up to 256 SDL2 key events in a fixed POD
buffer. Each wrapper calls the existing poll/wait function exactly once,
returns its result unchanged, and copies scalar fields only after a successful
dequeue. The existing call sites in `sdlmain.cpp`, `sdl_gui.cpp`,
`sdl_mapper.cpp` and `debug_gui.cpp` use these wrappers; SDL1 remains a direct
passthrough. No extra SDL call, clock lookup, allocation, output or guest write
occurs in the append operation. Input chronology is retained across internal
reboots, independently of the reset title/INT-6 observation.

The `ticks` field contains the original SDL2 event's `key.timestamp` bits.
It is not a fresh `GetTicks()` sample, physical-origin proof, guest-delivery
proof or causal link to a title action. The existing host-key grammar accepts
this representation, but a successor protocol must explicitly declare the new
timestamp provenance. The 257th event sets overflow; terminal formatting then
rejects the entire receipt rather than publishing a truncated chronology.

Both distinct output paths must be prepared before guest initialization. The
new `host_input_output_path` has the same external-root, descriptor, ownership,
permission and environment protections as the observation path. Terminal
publication requires both files to succeed before the success marker/status
can be emitted. An empty, successfully observed input stream produces an empty
receipt; it does not independently establish that physical input was absent.

Full compilation and reverse-patch checking passed. External `dosbox-x-input`
is 132,945,208 bytes, SHA-256
`776a02c687951fdc88a65834719ba7cf0124c879c371b278f8117f203cd4375f`.
The complete `dosbox-input-foundation.patch` has SHA-256
`23951d5c7cab7d18206f7f15eac352bc2901ab8bbd56b9693e621796aeb9efe5`.
Actual-header tests passed empty/full/overflow/reset, high-bit fields, bounded
formatting and 0–64 KiB output limits. The host-input header SHA-256 is
`9a19882773be2996ad5b0d1b00c12f9ce57e0a2a3f5110abfb400482a55cdd12`.
The eight-property configuration harness passed both output-path defaults,
startup-only mutability and environment exclusion, with the six identity
checks unchanged. Tests used synthetic unit-test records, not emulator runs or
substitutes for original media. This is still `OBSERVER_FIX_REQUIRED`; there is
no new recorder pin, capture or native-recovery admission.

Independent coverage review found no remaining SDL2 dequeue bypass in the
configured Linux `src/` build and no background poll/wait caller. That
single-producer conclusion is configuration-specific; alternate Windows/VS
PDCurses input paths require separate review. Internal clipboard `GenKBStroke`
and `PushDummySDL` producers can enqueue synthetic key events that reach the
same wrappers. Consequently the receipt means **SDL-queue key delivery, origin
unclassified**. A populated receipt cannot by itself promote a session to
physical-input evidence, and no synthetic event is silently filtered out.
An independently generated actual-source wrapper/glue harness passed inactive,
null, empty and non-key events, down/up/high-bit fields, unchanged event bytes,
one underlying call and unchanged returns. Missing/identical output paths,
input/observation short writes, absent observations and stdout failures all
remained rejected. Its external result manifest
`terminal-harness/hostinput-integration-results.txt` has SHA-256
`d784a2d6bdf5c70c2fd036ee30f54c4f9ffb84024c2652448b7393726946f3d1`.
The complete build record is external in `dosbox-input-build-record.md`.

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
temporary paths were under the scoped external cache; no
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
patch and independent review remain the admission contract. The source and
all build outputs remain outside the repository in the scoped external cache.
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
checkout in the scoped external cache.
with SHA-256
`479c043171fe9c5351340723a034bd2a80019d38e947fd1002d1b6b0775b0574`.
The first locally built experimental executable SHA-256 was
`5b9dd55d1c5eab1a34edd9561c60f7f11066ddd9492033cea50734a0dad60f51`.

An independent review rejected that first candidate before admission because
its opcode literals and record newline were escaped incorrectly, its preimage
start was one byte too early, and its output handling needed stricter path and
short-write handling. It must never be used.

A corrected follow-up candidate is retained in the scoped external cache
with SHA-256
`72e0e931cfda96f1a5cf786cd59f761975b5b71a1a6021b8f64c18e594282679`.
Its experimental executable SHA-256 is
`3c5e205e163bd6166fa517dadea8298bfec91bb965843852ff868ff6dc7be69f`.

The follow-up review also rejected v2 before admission: it read `DS:DX`
before exact candidate identity had been established and removed a failed
output by pathname after closing it. A third candidate moves that read behind
the exact mapped CS:PC and opcode checks, and fail-closes on a write failure
without pathname removal. Its external patch is retained in the scoped
external cache
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
the scoped external cache;
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

An external clean DOSBox-X source copy in the scoped external cache
was created from revision `234797680781567e18c374c9e62da24de5423db0` for the
next recorder implementation. It contains a zero-initialised, fixed-size
`ProjectEonInt6Slot` in `src/cpu/callback.cpp`. At the literal callback site,
the slot can only copy the bounded exception frame and scalar registers, and
publishes its captured state last. It performs no output, environment lookup,
allocation, logging, guest write, register/vector change, input handling or
scheduling.

The exact external source delta is retained in the scoped external cache
with SHA-256
`71ee7d92bd6ff25f7f16dd5e6218b79629f7efcbb8c48f41d904c9b7c53b8419`.

The slot is deliberately unarmed: the reviewed release identity, loaded-image
fingerprint and title-prefix predicate do not yet exist. Therefore no `INT 6`
event is eligible to capture, there is no serializer, and no trace artifact
exists. The CPU component compiled on the configured external host. A full
recursive Autotools build is not recorded because interrupted tool execution
left competing make children; this is an implementation foundation only,
below `OBSERVER_FIX_REQUIRED`, never a recorder, pin, trace or capture target.

### 2026-09-08 V24 callback-grounding rebuild

An independent external development copy of the documented DOSBox-X revision
`234797680781567e18c374c9e62da24de5423db0` was rebuilt sequentially after the
unarmed POD receiver was tightened. The external executable SHA-256 is
`14aed260f8761b816575c3ec7b7c8cc1fc820b675b1bd55d19184cdf73e2e3ab`.
The callback predicate requires the exact default INT-6 callback index and
stub, and the POD now retains the canonical callback CS/IP/stub fields plus
16-bit-wrapped exception-frame word offsets. It performs no output,
allocation, logging, host lookup, guest write, register/vector mutation,
input handling, scheduling, or stop request while guest execution is active.

Both recorder arms remain false: no configuration identity, loaded-image
fingerprint, title-prefix predicate, serializer, capture output, pin, or
admission exists. This is `OBSERVER_FIX_REQUIRED`; the binary and its source
delta remain only in the scoped external cache and must not
be passed to the locator or capture runner.

### Experimental observer runs

### 2026-09-09 V24 configuration-gate rebuild

The external V24 development copy was extended with a dedicated
`[project-eon-recorder]` section and an `OnlyAtStart` `development arm` flag.
Its configuration callback resets the retained POD on every application and
can control only the first development gate. The independently required
release/image/title-prefix gate is private, hard-coded false, and cannot be
changed through configuration. Therefore `development arm=true` still cannot
record, serialize, emit, pin, locate, validate, or admit a capture.

The full `make -C src -j4` rebuild completed in the scoped external cache; its
external executable SHA-256 is
`1d68e2f04a6569242bb15adf82e21c3508a48e8ab60fc868b332339ebbbe78dd`.
This is an `OBSERVER_FIX_REQUIRED` development baseline, not a recorder
candidate. Its source, binary, and any future build output remain external and
must not be passed to a capture helper.

### 2026-09-09 V24 loaded-image grounding rebuild

The same external V24 development tree now binds its first development gate to
the image tuple that DOSBox-X itself records while loading a DOS process:
`RunningProgramHash[1]` (loaded byte count) and `RunningProgramHash[2]`
(CRC-32). The comparison is fail-closed: enabling the development flag without
both non-zero expected values cannot enable the first gate, and an INT-6
candidate whose current DOS image tuple differs cannot pass the site predicate.
This is a loaded-image check, not a substitute for the capture runner's
outer-archive SHA-256 verification.

For the recognised English DOS corpus, the future reviewed configuration may
identify the observed `2200AD.EXE` image only by the exact tuple `54391` bytes
and CRC-32 `440994f9`. These values were independently derived from the
supplied, hash-verified leaf; no media was copied, altered, or placed in the
repository. The title-prefix gate remains private and hard false, and there is
still no serializer, output path, pin, locator result, capture, or admission.

The external source and binary remain in the scoped external cache.
A sequential `make -C src -j1` followed by `make -C src -q` completed on the
configured host. Its external executable SHA-256 is
`67954bc31e054f435d00c43bbaa26471bcf3117e71f6a3cd5330f5238caa7e38`.
This remains `OBSERVER_FIX_REQUIRED` and must never be supplied to a locator
or capture helper.

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
