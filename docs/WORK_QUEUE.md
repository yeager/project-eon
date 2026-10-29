# Project Eon P0 work queue

## Playability progress (2026-10-12)

### Deuteros callback: bounded title-event and queue prefix

The bounded M68k executor follows the hash-verified callback from `$1f056`
through its initial raw-event guards, mirrors the captured event byte to
`$1ef2e`, then follows the observed event-one branch to `$1f0f0`. With captured
A0+4=`1`, A0+6=`1`, and A0+8=`$8000`, it stores `$8000` at `$1ee0e` and loads
`$1` from A0+6 into D0. The callback's indexed byte transfer at `$1f13a` uses
brief 68k index extensions; the bounded executor now applies D0/D1 word indices
to the original `$1ee20` table and `$1eec0` queue. Direct-media coverage runs the
full hash-locked callback with a synthetic raw-key event and return frame while
keeping the exact stage source bytes for its latch, count, selector and table.
The verified v28 callback-memory sidecar also records entry A0=`$00c037f2`,
A1=`$0001ef40`, A7=`$00c0372a`, and RTS caller `$00fe6228` for input ordinal
54; the native execution test now uses those captured register addresses and
return PC.
Raw usage 1 with qualifier `$8000` returns to the captured caller `$00fe6228`,
writes `$31` to `$1eec0`, increments `$1eed6`, mirrors class 1 at `$1ef2e`,
stores the selector at `$1ee0e`, and updates the prior-key latch. This matches
the observed callback return at input ordinal 54. It is interpreter coverage,
not an SDL-connected callback or an `input.device` lifecycle claim. The separate
raw-mouse qualifier path executes through `$1ffd4`; codes `$0068` and `$00e8`
pass `ANDI.W #$7f`. A raw no-button mouse event executes the coordinate stores
at `$1ee10/$1ee12` with signed `-2/+3` deltas and stops at the `MoveSprite`
call boundary `$1f0b8`; no library result is fabricated. `OpenDevice`,
`SendIO`, Exec handler lifecycle and event-to-game meaning remain unmodeled.

The local consumer is now executed from its exact `$407b4` callsite using the
hash-verified stage as instruction source and the mutable stage image as data.
The bounded run calls `$1f238` inside the queue helper, shifts the owned
response queue, updates `$1eed6`, then follows the `$407ba` feedback tail to
`$4078c` or `$407e4`.
Direct-media coverage chains the executed callback and consumer for raw key 1
(`$31`, no control-word write) and the comma physical usage (`$2c`, control
word `$00ff`). These are source-executed local effects, not evidence that Eon
has registered `input.device`, delivered SDL events, or rendered an accepted
game action.

The complete linear candidate disassembly fixes an important call-order
boundary. `$407b4` is inside the `$4077e` response loop: it calls
`$1f238`, the nonzero-count continuation of the `$1f230` queue helper, and
`$407d6` branches back to `$407b4`. No independent scheduler call from the SDL
input handler is supported. The compatibility adapter therefore commits the
callback's queued byte and leaves it pending until an owned runtime path reaches
the original response loop. The source-execution test composes producer and
consumer as a bounded regression; it does not authorize that composition for
each host event.

That host route is now implemented. SDL physical scancodes map to the original
Amiga raw-key matrix; SDL modifier state maps to the corresponding public
`InputEvent` qualifier bits. SDL left/right mouse transitions now also map to
raw-mouse `$68/$69` and their `$80` up forms, preserving the current `$4000`
left-button and `$2000` right-button qualifier bits. The original class-two
callback writes this compact state to `$1ffd4`; the native session test
verifies left down/up as `0→1→0` and the bounded callback test verifies right
down. Pointer motion runs the recovered local coordinate routine but stops
before the unresolved `MoveSprite` result. The title session's input contract
admits only these typed keyboard, button, and motion events. An Eon-owned
`input.device` compatibility session checks the hash-bound registration
profile, executes the original callback bytes with the captured v28 register
context and an ephemeral host event/return frame, then atomically commits only
the callback's changed title bytes. The response queue remains pending for the
original `$4077e` path; the SDL event handler does not consume it directly.
Runtime integration tests confirm that raw key 1 queues `$31` and comma
queues `$2c`, with the count pending and
`$407e6` unchanged. The input callback is an Eon compatibility behavior: it
does not issue the original `OpenDevice`, `SendIO`, or Exec handler calls.
The meaning of `$00ff`, any visible screen update, and the effect after
`$1ffd4` reaches later game code remain unverified.

The raw-key queue still has no source-proven route from SDL to a visible state
change. A read-only literal-address scan across 11 local Amiga ADFs found all
four `$407e6` references in the English response loop at runtime `$40782`,
`$4078e`, `$407c2`, and `$407de`; duplicate English images match those offsets.
The queue consumer and tail are source-bounded to local dequeue/count and
control-word effects, but no live caller to `$407b4` or visible-state reader is
established. Keep SDL responses pending; do not turn composed test events into
runtime actions. Exact hashes and scan limits are recorded in
`PRESERVATION.md`.

The title hit-test scheduler at `$225ce..$22760` is now hash-addressed in
`PRESERVATION.md`. It evaluates runtime D3/D4 against the active 12-byte table
at `$222b2`, switches through `$222b6` for `$fffe` rows, and has a separate
`$ffff` path that calls an A1-derived target at `$226b2`. No active runtime
step currently reaches this scheduler. Derive its register and table
preimages from an actual title transition before implementing the local path;
keep the indirect call as an external boundary until its handler effect is
established. D3/D4 come from owned `$1ffc8/$1ffce` after the `$1ffd4` loop
gate, and callback `$20118` updates those words before its unresolved
`MoveSprite` call. The remaining gap is a verified outer-loop PC and active
`$222b2` table at runtime, not a host-side mapping from SDL pixels to a menu row.
Do not use `$223b2` alone as that PC witness: it names the title hit-test in
the `$13000` title image, but after the main image loads at `$20000` the same
address contains different clear instructions. The two hash-addressed spans
and their ADF offsets are recorded in `PRESERVATION.md`; establish which image
is resident along with the transition before scheduling either path.

Raw mouse motion now runs the exact hash-locked `$1f056` callback and its
local `$20118` coordinate routine through the instruction before graphics
vector call `$201b6`. The host's signed x/y deltas are copied into the original
`$1ee10/$1ee12` words; the original bounds logic updates `$1ffc8/$1ffce` and
computes the recovered `MoveSprite` arguments. The callback reports the exact
external-service boundary and commits only its local title-stage writes. No
`MoveSprite` return, sprite/Copper-list change, or visible pointer motion is
claimed. SDL motion arrives in window-space units, so input scaling to the
native viewport now uses SDL's window-to-render conversion and the actual
display rectangle. X scales to 640 raw units across the 320-pixel canvas,
matching the recovered caller's final divide-by-two; Y scales one-to-one to
the 200-line canvas. Fractional renderer deltas accumulate between SDL events
instead of being discarded. Visible pointer/click response and gameplay still
need verification.

The English Millennium DOS selected-driver path now executes the exact
hash-bound `INT 21h/AH=25h` request at `MILL.COM:$0239` as an Eon DOS
compatibility service. It writes INT `$95` to the process-local virtual IVT
with offset zero and the admitted driver allocation segment, labelled
`eon_dos_compatibility_service`. A fresh full-image linear report confirms
that `MILL.COM:$0100` sets `SP=$0100`, the selected-driver helper call at
`$0240` pushes its return address, and `MILL.COM:$032f` stores the resulting
`SP=$00fe` to `DS:$05f7` after `$023b` set `DS=CS`. The runtime now applies
that hash-bound local stack write and reaches its existing `TITLES.EXE`
compatibility child and startup path. This does not claim an original DOS
observation or an original EXEC return. Millennium now stops at the title's
first private `INT $91` result boundary. A typed dispatcher now validates the
exact `$1b95` function-zero call, the observed INT `$91` vector, and the
complete owned driver image before resolving table entry zero. It advances
only to the selected driver's first explicit `INT $10h` request; it does not
invent the interrupt result. BIOS continuation, rendering and gameplay
remain unproven.

### Millennium first game-process private function

The first game startup request is private function `$1f`, whose hash-verified
EGA/MCGA driver routines read DS:`$008a` / DS:`$00ac` and set AH to `$04` /
`$01`. A reachable `TITLES.EXE` call at `$1b95` invokes function `$00`, whose
already modeled producer writes that same local byte after explicit BIOS
results (and, on one MCGA path, an explicit private INT `$92h` result). The
source-level producer-consumer relation is now established. The active title
runtime validates the first INT `$91` dispatch against the observed vector and
owned loaded driver image. It accepts only ordered, callsite-matched explicit
INT `$10h` results and commits the exact hash-checked driver cache write. The
MCGA `CX=1` path reaches the instruction before RET `$024b` and commits
DS:`$00ac=1` to owned driver memory. Its typed stop records `SS:SP` and the
minimum `$0da0`-paragraph stack extent, but allocation does not initialize the
return word at `$d9ec`; runtime checks showed it absent. The exact TITLES
startup and wrapper path now materialize only known CALL/INT words in owned
memory. On the MCGA single-count route, the runtime validates the near return
`$0012`, handler branch state, IRET frame, and wrapper return `$1b98`, then
completes the handler store, IRET, wrapper epilogue and title return. FLAGS and
SI/DI/BP remain unknown. The title proceeds to its selected mode-one private
function `$04` boundary. The EGA function-four result now advances into its
ordered startup palette loop; MCGA multi-count INT `$92h` remains open. MCGA
function `$04` now executes its exact
`ES:BX` byte mask and state-byte exchange, validates the reused wrapper stack,
and reaches the following INT `$10` palette boundary at `$046d`; the BIOS
palette result is the next stop. EGA's post-`$047c` caller remains open. See
`PRESERVATION.md`.

The hash-bound MCGA single-count branch now advances from its `CX=1` target
`$023d` through the literal calculation, DS:`$00ac` store, and AH=`$01` setup,
stopping immediately before RET `$024b`. The active title runtime consumes
ordered explicit BIOS results for this path and commits the local byte to
owned DOS memory. The admitted startup and wrapper code now provide the known
stack words at `$d9ec`, `$d9ee`, `$d9f0`, `$d9f4`, `$d9fc`, and `$d9fe`; unknown
FLAGS/SI/DI/BP remain absent. Runtime validates and consumes the MCGA RET,
handler cleanup, IRET and wrapper return. It reaches the next mode-one
function `$04` INT `$91` boundary with AX `$0101` and flags explicitly unknown.
The bounded MCGA function `$04` code at `$0815..$081f` now reads the exact
TITLES byte at CS:`$1ac5`, masks it with 3, and exchanges it with driver byte
`$00af`; the known mode-zero initial state remains zero. The MCGA wrapper and
outer near return are validated and title execution reaches the palette
`INT $10` at `$046d`. Its BIOS result and later game handoff remain open. Preserve
the explicit INT `$92h` boundary and keep the EGA post-`$047c` caller stopped
until its return and continuation inputs are modeled.

The EGA function-four route now starts the hash-checked startup palette
session from the admitted owned driver/vector path. The active DOS process
accepts the 16 ordered INT `$10` returns only with exact AX `$1000`, observed
BX register/value, child CS and return IP `$0478`, then stops at RET `$047c`.
It then follows the owned `$da05` selector and verified `$d1c3..$d1d0` caller
through shared setup to DOS `INT $21/AH=$4a` at `$d201`. This does not expose a
rendered EGA frame; the device/render path and later startup remain open.

After a typed English `2200AD.EXE` process-entry transfer, the first function
`$1f` call advances from the verified loaded INT `$91` vector and owned
driver-local state. The runtime verifies the exact startup request, driver
dispatch prefix, function leaf, and non-mutable loaded bytes, then resolves the
source-backed function `$04` path. MCGA advances to its `$0455` allocator
boundary; EGA validates all 16 ordered BIOS palette returns and stops at
`$047c`. Unknown register inputs and BIOS returns remain explicit boundaries.
Neither branch proves a rendered game screen or playable state. See
`PRESERVATION.md`.

Deuteros Amiga now routes the initial CIA-A bit-6 sample and selector-zero
resource load through the SDL/native path. The first local `$217f8->$22a5a`
service is executed from its hash-verified bytes using the existing `$22ab8`
software sound-descriptor model; the caller stores at `$217fe` are committed
atomically with that operation. This advances to the first graphics-library
return at `$21310`. The consecutive `LoadRGB4` returns at `$21310/$2132a` now
require exact A0/A1/D0/A6 call registers matching the owned caller plans
before accepting typed D0 returns; wrong call metadata is rejected without
memory changes. The later recurring `LoadView` return at `$216ee` likewise
checks the selected `$12e00/$12f00` View pointer in A1 and the owned graphics
base in A6. This still does not emulate the graphics.library/Exec device
effects or establish an input-linked game action. The title's `$1ef74` call is
Exec `SendIO` (`-$1ce`); request command 9 and callback pointer `$1f056` are
hash-bound, while the concrete opened device and callback lifecycle remain
unproven. No admitted callback-to-response-queue action sequence is available;
do not synthesize one from title clicks.

### First Deuteros outer-loop input poll (2026-10-12)

The SDL bridge now selects the first recovered POTINP poll at `$21822` from
`next_instruction_address`; subsequent polls continue to use
`pending_read_instruction`. The runtime checkpoint represents these two
forms differently, so reading only the pending-read field could skip the
first sample. The existing exact-address whitelist still controls which
candidate becomes a host port observation. A small selector helper has a
native regression check for the initial, recurring, and empty cases. This
closes one skipped-input edge in the already recovered main-stage loop; it
does not prove title callback delivery or general game playability.

### Deuteros profile-two main-stage re-entry

The exact profile-two `MOVE.W $12a34,D0` at `$12b14` derives D0.W=`2` before
the loaded stage jumps to `$21734`. The runtime now advances that hash-bound
local step without waiting for an unneeded full-register observation, while
still validating any optional receipt. The driver reaches the next external
Exec call at `$2174a` and stops for its return. This closes one deterministic
handoff gap; the OS/device returns and an input-linked visible game-state
change remain the next playability evidence required.

## Deuteros next captured boundary

### Host mouse ABI and implementation boundary (2026-10-12)

The title callback's raw-mouse constants are now checked against the AmigaOS
Input Device documentation and the NDK `devices/inputevent.h` listing:
`IECLASS_RAWMOUSE=$02`, `IECODE_LBUTTON=$68`, `IECODE_RBUTTON=$69`,
`IECODE_NOBUTTON=$ff`, `IECODE_UP_PREFIX=$80`, and button qualifier bits
`LEFTBUTTON=$4000`, `RBUTTON=$2000`, `RELATIVEMOUSE=$8000`. The Input Device
documentation identifies `input.device` as the handler-chain owner and
describes `IND_ADDHANDLER` / `IND_REMHANDLER`. The hash-locked registration
body also prepares IORequest `$1eefa`, passes name pointer `$1edfc`, clears
D0/D1, and calls Exec `OpenDevice` at `$1efb0`; its later command-nine
request passes descriptor `$1ef48` to Exec `SendIO`. The official Input
Device interface identifies command 9 as `IND_ADDHANDLER` and `io_Data` as an
`Interrupt` pointer ([AmigaOS Input Device documentation](https://wiki.amigaos.net/wiki/Input_Device)).
This establishes the static ABI route, but not the OpenDevice or SendIO
result, installed-handler lifecycle, or a live callback invocation.

SDL raw-mouse motion now sets `RELATIVEMOUSE=$8000` alongside the held-button
bits. The signed deltas alone were insufficient to reproduce the original
InputEvent qualifier. Unit coverage checks all four left/right button states;
this corrects the host event ABI but does not establish visible pointer motion.

The runtime now has a byte-layout primitive for the exact first fourteen
bytes of the callback's Amiga `InputEvent`: big-endian next pointer, class,
subclass, code, qualifier, and signed X/Y words. Its native test covers
two's-complement negative deltas. It now also maps USB HID physical key usages
to classic Amiga raw key codes, including the `$80` key-up bit. The mapping is
based on the AmigaOS Keyboard Device table and deliberately does not convert
locale-specific characters or guess qualifier state. Tests cover letters,
digits, punctuation, keypad positions, Space, Return, F1, key-up, and an
unsupported usage. The SDL event loop
does not yet pass these events through an owned input-device session. A
bounded M68k replay now executes the hash-locked raw-key callback from
`$1f056` to `$1f150` using an explicit event/return frame and exact loaded
stage state; it reproduces the captured `$31` queue write for raw key 1. This
is interpreter evidence, not live `input.device` registration or SDL delivery.
The raw mouse-motion route still stops before `MoveSprite` returns. Next,
connect bounded SDL raw events to an owned registration/event lifecycle, then
model the graphics call's owned effect.

The existing SDL path already drives the hash-bound main-stage CIA polls
(`$21822`, `$2185e`, `$218be`) from held mouse/keyboard/gamepad state. At the
main-stage boundary, the native loop reads left-button/fire on CIAA bit 6 and
right-button on POTINP bit 10; this only proves the host-to-port wiring and
hash-bound local branch execution, not that either control produces a useful
game action. Mouse pointer motion is not consumed by those port reads. A live
title callback path is still absent: registration at `$1ef74` calls Exec vector
`-$1ce`. The bounded interpreter verifies the local producer with a synthetic
frame, but runtime does not yet own `input.device` registration or schedule
callback invocations. Next, connect SDL raw movement/button/key state through
a bounded registration, event-frame, invocation and callback-return lifecycle.
Mouse deltas must use Amiga signed-word semantics; button-up codes must retain
the `$80` prefix; qualifier/button state must be explicit. The `$20118`
path also requires an owned graphics.library `MoveSprite` return before any
sprite overlay can be presented. An upstream manual-input capture remains
unavailable, so this code path must be source/hash-bound and runtime-checked
without presenting agent-operated input as admitted preservation evidence.

The raw-key translator now includes standard USB-HID punctuation and keypad
usages needed by the callback table. On the canonical English Deuteros ADF,
usage `$36` (comma) maps to raw key `$38`, which the original table maps to
response `$2c`; usage `$37` (period) maps to raw key `$39` and response `$2e`.
The hash-bound `$407ba` feedback tail applies its exact low-byte updates to
`$407e6` for those responses. The direct-media regression now composes comma
through the callback table, one queue dequeue and the local `$407ba` tail; an
explicit `$1b` helper response ends the tail after `$407e6` becomes `$00ff`.
This remains a source-level path with supplied helper output, not proof of a
runtime callback, live helper return, SDL delivery or visible gameplay
action. Runtime integration must still own the callback lifecycle and provide
captured or otherwise source-validated helper semantics before controls can
be called playable.

The agent-operated v31/schema-40 time-control exploration
`deuteros-amiga-v31-time-advance-20261012-01` passed receipt verification,
but its targeted display probe waited for input 76 and its broad display
trace capped at input 4. The visible click at input 41–44 did not alter the
observed zeroed response queue; there are still no `$4077e` caller samples or
complete consumer sequence. A runtime adapter now validates and commits a
complete queue pass when such observations exist, but this exploration cannot
drive it. Its summary and hashes are in `PRESERVATION.md`.
Because agent-driven UI input is not the required manual operator input, this
exploration is unadmitted and must not be used to wire a click into gameplay.
The next useful admitted input-to-display capture needs visible manual input
and a bounded display window armed after a baseline action.

The fresh v30/schema-39 follow-up
`deuteros-amiga-v30-title-callback-20261012-02` also reached “THE SUN” and
passed receipt verification, but ended with 38 host-input records, no sample
at the v30 target ordinal 76, and no service-boundary sidecar. Ten callback
pairs were linked, with one unmatched callback entry at ordinal 36 and no
caller sequence. It confirms visible panel input reaches `$1f056`/`$1f14e`,
but does not change the current next step: instrument the identified callback
caller and response-queue consumer through one complete input-linked pass.
Details are in `PRESERVATION.md`.

The v31/schema-40 visible capture `deuteros-amiga-v31-response-queue-20261012-06`
now advances beyond the title logo: a held left click spans emulator frames,
the `$21866` poll sees `SR=$0004`, the `$21868` latch write is recorded, and
execution reaches `$218cc`. Visible English selection and the Disk 2 prompt
are also reached; F10 advances to “PRESS ANY KEY WHEN READY.” That bounded
capture ends before a later key is recorded. A second capture,
`deuteros-amiga-v31-response-queue-20261012-07`, records the ready-key press
and visibly reaches the “THE SUN” game view. Its mouse clicks link to
`$1f056` / `$1f14e` callback pairs, but there are still no `$4077e` caller
samples or response-queue sidecar. Exact hashes and limits are in
`PRESERVATION.md`. Fresh local static disassembly now pins the byte-two gate
and `$00ff` call path: sample `$1f094/$1f09a/$1f0a2/$1f0a6/$1f0a8/$1f0b0`,
the `$20118` entry, its graphics-vector call/return `$201b6/$201ba`, local
return `$201be`, callback continuation `$1f0be`, and linked post-input display.
The schema-42 v33 observer now captures the service entry, MoveSprite call and
return, local return, caller return, coordinate inputs, registers, and bounded
viewport/sprite windows; its receipt compares the observed call arguments to
the hash-bound local arithmetic. A verified diagnostic run is recorded in
`PRESERVATION.md`; it observed two complete local argument matches. The run was
agent-operated through noVNC, so it is not admitted as manual capture evidence
and cannot establish a preservation claim or gameplay action. It also does not
capture the upstream gate and selector path in the v32 contract, so neither the
callback's caller context nor a player-action-to-frame link is established.
The runtime adapter for
`$407b4` → `$1f230` → `$1f23e` → `$1f258` → `$407ba` → `$407e4` now exists, but
requires its own full ordered pass before it can be used. Do not claim an
actionable game control from callback entry alone.

### Deuteros service-boundary observer ready

The v33/schema-42 recorder is now built, independently reviewed, and
hash-pinned. Its bounded sidecar samples the `$20118` → `$201b6` → `$201ba` →
`$201be` → caller-return sequence at up to 32 host-input ordinals, with exact
A0/A1 memory windows at `$201b6`. This repairs the earlier ordinal-76 blind
spot and permits partial sequences without losing later observations. No
admitted manual capture has been run with v33. A verified agent-operated
diagnostic capture found two local argument matches; see `PRESERVATION.md`.
The run adds no admitted runtime or gameplay evidence. The
schema-42 receipt parser now preserves observed D0/D1/A0/A1/A6 call values, the
six coordinate/bound words and two caller deltas read by `$20118`, and the two
raw object windows at `$201b6`. Receipt verification recomputes the recovered
coordinate arithmetic and compares its D0/D1/A0/A1 arguments to the call-site
sample. A match would validate local arguments only; the MoveSprite library
effect remains unmodeled.
Capture still requires visible manual emulator input under the recorder
restoration rules. Continue with static disassembly and native-path work; do
not treat agent-operated UI events as admitted capture input or synthesize
guest input in the recorder.

The v33/schema-42 diagnostic `deuteros-amiga-v33-selfsolve-20261012-04` passes
the independent receipt verifier. It contains 46 host-input records, seven
complete `$20118`/`$201b6`/`$201ba`/`$201be` sequences, and seven local argument
matches. Callback snapshots contain ten `$1f056`/`$1f14e` pairs but no
`$4077e` samples, no complete caller sequence, and no response-queue records.
The targeted display probe was not armed before this session, and the broad
display trace remained linked to startup input only. This run used
agent-operated noVNC input and is diagnostic only. It does not identify an
actionable control or establish a state or frame change. Do not repeat the
same pointer probe; continue static analysis from the hash-bound callback
caller and response-queue path, and preserve the manual-input admission
boundary. Exact hashes and parser summary are in `PRESERVATION.md`.

Follow-up with the already running uninstrumented noVNC session measured its
canvas mapping: viewport `(391,202)` maps approximately to framebuffer
`(363,224)`. Two holds were followed by counter changes `.02→.03` and
`.03→.04`; a later hold had no visible effect, and no recorder receipt linked
any change to input. Treat this only as a diagnostic clue. Do not use it to
infer the time control's event byte or state effect. The v31 runner starts a
fresh recorder process and cannot attach to the already running session; any
new capture must still satisfy the reviewed recorder and visible-input rules.

A separate uninstrumented, read-only FS-UAE session on trv2 showed the counter
change from `3100 000.02` to `.03` after a 1,000-ms left-button hold at
noVNC viewport coordinates `(391,202)`, over the blue time-advance icon.
The page canvas is 1280×800, displayed at 1152×720 with its left edge at x=64;
the click therefore maps approximately to guest framebuffer `(363,224)`.
After reopening the noVNC page visibly, a 2,200-ms hold at the same viewport
coordinates changed `.03` to `.04`; a later 10-second idle interval left
`.04` unchanged. A further 2,200-ms hold also left `.04`, so delivery/effect
is not consistent across attempts. The visible counter changes are suggestive
of a working time control but remain unverified without a linked input
receipt.
These were agent-operated diagnostics, not admitted captures; none links the
control to the response queue, a canonical campaign-state checkpoint, or a
rendered-frame receipt. The session config and both ADFs/Kickstart were read
from `archivemount` FUSE mounts with `ro,nosuid,nodev`; no save action was
taken. See `PRESERVATION.md` for hashes and limits.

### Millennium DOS ambiguous patch-span analysis

The exact English `2200AD.EXE` has a six-byte zero-filled span at
`$740f..$7414`. A new bounded in-place reference scan found three linear
candidate writer groups (`$742e/$7434/$743a`, `$92a4/$92aa`, and
`$b83c/$b842/$b848`) and corresponding reads that restore bytes or words to
the `$75a6/$75a8/$75ac/$75ae` cells. This is a repeated state-save/restore
pattern, not evidence that the span is executable code or that any group is
reached. A bounded worklist from image entry `$0100` also found no candidate
route to these groups even with conditional and direct call/interrupt-return
assumptions; the function-table transfer is indirect and remains outside that
graph. That result does not prove unreachability. The next required boundary is
an observed `$d40a → $76f1` dispatch identifying the live index and handler;
the new bounded context disassembly records surrounding candidate instructions
without classifying them as code. Do not decode a guessed patched stream or
claim an action from the static table alone. All report identities are in
`PRESERVATION.md`.

## Current DOS descriptor continuation

Deuteros FS-UAE v24 is pinned as a separate schema-34 observer candidate. Its
bounded late-display windows can sample up to four observed input ordinals;
skipped ordinals are allowed and are not interpreted as physical frames. The
synthetic harness and verifier tests pass, but no v24 emulator capture has
been made. This adds no game-action, displayed-frame, or playability evidence.

Mode one's palette source is now corrected to TITLE.LIB `[$25f9,$28f9)`.
The original LDS restores library-base:$0006; the former `$4865` overread
was a relocated-pointer error. The complete 768-byte copy now reaches its
real BIOS request at `$0fd8`, AX=$1012/CX=$00ff. A separate typed BIOS result
permits return to the shared setup. Hardware-dependent graphics selection
still needs captured evidence. The active runtime now owns a hash-locked
EGA640/MCGA loader session behind typed selector and DOS-result observations,
and unit tests cover the explicit open/seek/allocate/rewind/read/close path
through the `$020c` SetVect result. Those tests use synthetic observations;
they do not prove the original selector, IVT contents, or INT $91 dispatch.
An optional typed INT $91 IVT tuple may now follow that raw DOS result. It
preserves the observed interrupt, offset, and segment verbatim, including a
mismatch with the loaded image address. That comparison is correlation only;
it does not establish successful installation, handler dispatch, or service
execution, and it does not select a private service. No private service can
yet be selected from the driver profiles. Title mode alone does not establish
which driver is installed.
The hash-bound English function-$00 profiles validate exact instruction spans
through their match-continuation opcodes. The standalone session accepts the
conditional cache query result, set-mode result, and verify-query result as
typed external inputs; it stops at the successful continuation or mismatch
RET opcode after applying the proven local zero-AX operation. Focused real-
media tests cover both original leaves and rejected/reordered results. No BIOS
behavior, caller return, IVT contents, or private-handler dispatch is
established. The bounded successful postlude below records only hash-bound
local instruction effects and typed stop points.
The separate success-postlude step models EGA's local `$0192` write and
register clamp. Its next prefix records an explicit pending `PUSH SI` stack
write, then stops before one-count `POP AX` `$022e` or VGA `OUT` `$0207`. The
one-count path advances through its paired POP and, with explicit DS input,
records the pending `$008a` store and stops at RET `$0234`; continue only with
an exact caller/return observation. The multi-count EGA path now accepts
explicit DS and records each bounded descriptor write and `$1f40`-byte plane
clear through the balancing POP at `$022e`. The shared local-pop/store path
then restores SP and records the `$008a` write through RET `$0234`; its RET
destination is accepted only as a typed stack word at that exact `SS:SP`.
The session records the near-RET IP/SP effect but does not execute the target.
The loop outcome retains the preceding VGA `OUT` as a port-write intent,
without modeling device effects.
The zero-count input decrements to `$ffff` in the original and is rejected by
this bounded step rather than expanding into an unbounded clear loop.
MCGA's branch step stops at the `$023d` single-count target or before INT
`$92` at `$021f`. No graphics or private-service behavior is inferred.
The standalone English function `$1f` session now requires the explicit
runtime DS-local byte and stops at its hash-bound RET instruction for EGA640
and MCGA. It does not use either file's initialized zero or establish caller
reachability.
Function `$13` also has a standalone poll-loop session; continue it only with
ordered raw reads from VGA status port `$03da`. A completed poll does not prove
an MCGA interrupt postlude or select an installed driver.
The standalone INT `$91` function-`$13` session now hash-binds the EGA/MCGA
dispatcher prefix and table route, carries the typed `$0129` interrupt frame,
and models MCGA's `$01e4` clear plus explicit `$01e5` conditional byte. It
continues the nonzero `$01e5` path through the hash-bound local callback
prefix at `$0d22`. Its typed byte and word reads select only the proven branch
destinations `$0c94`, `$0d10`, or fallthrough `$0d35`. From `$0c94`, a typed
nonzero `$0c92` counter is decremented and the local `$01e4/$01e5` flags are
cleared before stopping at RET `$0d10`; a zero counter now records the exact
eight register pushes from `$0ca2` through `$0ca9`, accepts a typed
`CS:$0c8c` far pointer at `$0caa`, and pushes its offset. The zero-counter path
now validates the exact `$0cb0..$0cd5` routine, accepts the descriptor
word/byte and count as ordered typed observations, then executes its bounded
`REP MOVSB` rotation, descriptor writeback, and register setup to the VGA-read
boundary `$0cd8`. The source span is capped at 765 bytes; 16-bit pointer
wrapping is preserved, with no buffer-safety or runtime claim. The `$0d35`
branch records `$01e4=1`, accepts
explicit register values and `SS:SP`, and records the nine ordered stack
pushes. It accepts a fresh CS:`$0d18` word at `$0d44`, a far pointer from
CS:`$0d1a` at `$0d49`, then DS:`SI+8` at `$0d4e`, stopping at `$0d54` or
`$0dad`. From `$0d54`, it reads the video pointer at CS:`$0d1e` and the byte at
DS:SI, selecting `$0dcd` for 1, `$0dcb` for 2/3, or `$0dad` for 0/4..255.
The bounded `$0dad` model applies one authenticated decrement/`SI += $0c`/`LOOP`
iteration and accepts a fresh typed word at `$0d4e`; it stops at `$0db5` when
CX reaches zero. The `$0dcb` route records its jump to `$0e46`. Its
hash-bound local setup now accepts typed descriptor bytes/word at `$0e49`,
`$0e55`, and `$0e62`, computes the row count and 16-bit source/destination
registers. The hash-bound pixel loop accepts its ordered source/destination
reads, conditionally applies the threshold reduction, records memory writes,
and returns to `$0e71` between pixels or reaches the final-pixel epilogue at
`$0e8f`. Explicit ordered SS:SP reads
restore SI/DS, then the descriptor count is decremented; its zero branch clears
the descriptor byte, and both branches reach the jump boundary `$0d6a`. These
stack and memory values are scenario inputs, not captured runtime observations.
The palette-descriptor prefix from `$0d6a` now updates the index/count fields
and records the first VGA `OUT DX,AL` effect at `$0d82`. Explicit status reads
from `$03da` are accepted at `$0d94` until bit 0 is set. The palette pointer at
CS:`$0d1e` is then observed, and all three byte/write pairs to `$03c9` are
modeled per bounded step. The `$0da1` LOOP updates CX and either begins another
triple or reaches `$0da3`; the hash-bound descriptor epilogue is modeled
through `$0db5` with explicit SI/DS/CX stack words and a branch to `$0d4e` or
`$0db5`. Nine final register pops and both CS flag clears are modeled through
the RET at `$0dca` to the hash-bound `$0021` CALL return at `$0024`, then reaches
the callback-path IRET boundary. This proves only the static route; runtime
reachability remains unobserved. Other VGA port I/O remains outside the model.
The `$0d10` RET also returns to the hash-bound `$0024` CALL continuation;
the zero-counter branch stops before its `$0cb0` descriptor read. The zero `$01e5` path still
stops at IRET. These scenario
inputs do not authenticate the installed vector or prove title execution
selected this handler.
Successful original MCGA/EGA640 initialization returns title mode 1/4,
respectively. The existing global-mode-2 descriptor regression uses explicit
arithmetic observations; it is not evidence that either supplied driver
reaches that path. Mode one's repeated palette now uses the current library
bytes, selects `$1c07->$1ac6`, and enters its own bounded descriptor
postprocessing. All 37 records translate 368 decoded bytes in place before
the shared `$1967` return. This execution accepts the actual library allocation
instead of requiring the test fixture's `$3000` segment. Earlier BIOS/private
results remain explicit inputs; graphics-loader ownership and EGA mode-four
postprocessing remain priorities before claiming a complete driver path.

The owned mode-two path now addresses the complete 37-record loop using the
actual relocated directory at TITLE.LIB+$4813. The first record is
$3294:$0001; the former $5050 mapping and lookup alias are rejected.
All header, payload and lookup reads remain within the verified leaf.
Each finite quantum preserves exact segmented addresses and commits session
plus memory together. After the loop count reaches zero, the owned caller
continues through RET $1967 to $1c20, reads runtime word CS:$1896, shifts it
right once, and enters $1931. The child executable image already supplies
this word; the continuation reads current native memory rather than assuming
its initial value. It stops at private INT $91 at $0127, requested by
$1937 with AX=$0013. Recover this service and its caller-specific return
before advancing into the $1917 patch helper or title input.

The EGA640 mode-four descriptor continuation reads the exact `$14f0`, `$14fc`,
and `$1500` header operands, then emulates the hash-bound `$15c5..$163a`
planar loop using initialized decoded bytes and the exact record table. A
real-media regression now drives the body for all 37 records, checks stride,
clear, planar writes and resulting emulated memory, and uses only the admitted
child image to seed native memory. Its startup INT 91h, DOS and BIOS returns
are synthetic fixture inputs, not captured observations; captured runtime and
decoded-pixel parity remain unverified. Keep mode four separate from MCGA
mode-one translation and the synthetic mode-two regression; keep mode three
distinct and leave private function `$0013` unexecuted until its result is
evidenced.

The title's hash-bound AX=`$0006` caller now has standalone EGA640/MCGA
entry-side clipping models. They accept only ordered descriptor-word reads,
preserve each driver's distinct signed/unsigned 16-bit branch behavior, and
stop before source pointers, helper calls, VGA I/O, or pixel writes. This does
not select the installed driver or establish a captured `$0006` dispatch; real
call reachability, descriptor provenance, rendering, and frame comparison
remain open.

MCGA static follow-on spans now have a standalone observation model for
`$071d..$072d`: read the descriptor byte at `ES:BX+$12`, derive `DI=$b0+byte`,
read `DS:[DI]`, and record its store at `DS:$07b9`. DS and CS remain distinct
types because later code reads `CS:$07b9`. This model is independent of the
clip-prefix model and proves no runtime reachability. A second standalone
model now covers `$072d..$073a`, including the recorded DS stack write, ordered
descriptor/source far-pointer reads, source header word, DS changes and stack
pop. It stops before `$073a`. Neither model is chained to the clip prefix.
`$073a..$076f` now has a standalone typed register/stack observation model,
starting from an explicit 16-bit register snapshot and ordered descriptor
reads, with no assumed reachability from either earlier span. It stops before
opaque helper `$0666` and assigns no helper/blit meaning. Its real-media native
test checks signed multiply, 16-bit register arithmetic, all five `SS:SP`
writes, and the no-wrap bound BX <= `$ffee`. The MCGA loops
`[$078b,$079d)` and `[$07a1,$07b5)` now have a hash-bound register/address
observation session. It records the word/byte REP phases, DF-directed SI/DI
stepping, per-row BX/DX strides, and BP/JNZ iteration. DF, segments, and
registers remain explicit inputs. A separate bounded executor now reads only
initialized bytes from `NativeRuntimeMemory`, takes the real-mode A20 mapping
explicitly, preserves ordered MOVSW/MOVSB reads for overlapping copies, and
commits unique final destination bytes in one atomic batch. Its real-media
test covers both DF directions, overlap, an A20 alias, and rejection without
partial mutation when source bytes are absent. It accepts the completed
hash-bound caller result directly, validates the returned loop against the
driver bytes, and refuses outcomes without the `$88` branch and matching RET.
This is still not wired into release runtime: the DOS guest does not feed it
live registers or memory, and neither runtime reachability nor pixels are
established. A new complete linear candidate scan of the exact English game
image locates three AX=6 near calls to the private wrapper at `$0124` (`$5d7c`,
`$6b0c`, and `$ad15`); the third call uses 16-bit IP wrap. See the caller
facts and report hash in `PRESERVATION.md`. This is static caller evidence, not observed
reachability or an installed-driver proof. Next, capture the caller registers,
descriptor bytes, private INT `$91` return and loaded driver identity at one
of those sites; then connect only that call path to guest-owned state, resolve
A20 mapping from the running DOS environment, and compare visible output
against a capture.

An integrated MCGA caller observation now connects the setup and helper
outcomes across `CALL $076f`, explicit caller stack reads, `CS:$07b9` and the
two `$88` copy branches. It feeds the selected state into the corresponding
hash-bound copy-loop address model and records the outer RET word at `$079c`
or `$07b4`; a helper carry path instead models `$079d` cleanup and `$07a0`
RET. A non-`$88` CS byte now exports the recovered `$07b5` register snapshot
to a bounded byte-effect model for `[$07b5,$07c7)`: ordered `DS:SI` reads,
`ES:DI` reads/OR writes, DF-aware SI, forward DI, row strides and SS:SP
push/pop observations. The snapshot and branch remain caller-supplied facts;
no game reachability or pixel interpretation is established.

The following paragraphs retain earlier recovery history. Their intermediate
stops and historical source assumptions are superseded by the current
[title initialization contract](MILLENNIUM_DOS_TITLE_INITIALIZATION.md).

The Millennium DOS non-mode-1 title path now observes both external vector
pairs and atomically installs the exact timer and video hooks. Its verified
caller continuation enters the repeated mode call `$1c02->$1ada`. Mode one
has a distinct BIOS boundary at `$0fd8` after its corrected palette copy.
The repeated `$1ada` call now consumes a fresh typed
INT `$91` result and sixteen fresh typed BIOS palette results, returns through
the caller setup, and stops before `$1c0e->$135e`. The next DOS evidence job
is recovering that callee without inventing setup or driver behaviour.
The `$135e` callee is now native and atomically binds its selected allocation
pointer into title state. The `$1c11->$0ff3` request now reaches typed private
INT `$91` function `$0019` at `$0127`. Continue only with its raw result;
its setup ABI remains unproven. The raw result is now retained separately and
the caller stops before `$1c17->$1725`; recover that callee next.
The `$1725->$1390` route is now native through its exact pointer setup and
stops at the typed two-word far read `$13aa`. Supply only the genuine words
from the reported relocated source before continuing.
The genuine `$0006/$0000` words are now provenance-checked and the normalized
pointer is committed. Continue with the typed record word at `$13cd`, source
`$3000:$001e`; do not infer loaded record contents.
An attempted automatic continuation from the admitted `TITLE.LIB` allocation
was rejected during focused native testing: later record-byte values cannot yet
be established solely from that allocation in this caller route. They remain
typed external observations until a hash-bound provenance mapping proves their
source; do not use generic DOS physical aliasing as a substitute.
The first record word `$0140` is now admitted through the dedicated
single-word facade. Continue at `$13d0` with genuine word `$00c8` from
`TITLE.LIB+$001c`.
That `$00c8` word and its exact multiplication effects are now native. Continue
at `$13e2` with the external `$3000:$001a` word.
That genuine zero word and adjusted-product write are native. Continue at the
typed byte boundary `$13e9`, source `$3000:$0007`.
The genuine `$23` byte is now admitted through all production facades and its
incremented `$24` is committed. Continue at `$13f2`, source `$3000:$000a`.
The genuine zero byte and complete local return/request build are now native.
The raw function-`$0006` result resumes into function `$001a`; its raw AX,
FLAGS, and ten-byte `CS:$0fdf` record are now admitted atomically. The first
`$1941` title-loop iteration advances both output pointers and stops at the
typed two-word `$13aa` read from relocated `TITLE.LIB+$000f`. The genuine
`$0503/$1f02` pair is now provenance-checked and normalized to
`$5050:$0003`. The raw runtime words at `$5050:$001b` and `$5050:$0019`
are now admitted through all single-word facades. Their exact unsigned
product/store sequence is native.
The third typed runtime word from `$5050:$0017`, the byte from `$5050:$0004`,
and their exact arithmetic/store sequences are native. The typed byte at
`$13f2`, source `$5050:$0007`, and both deterministic branch continuations
are also native. The one/two branch owns its first payload byte and exact
prefix through `$1427`; the complete hash-bound `$1437..$1487` escape, run,
lookup, high/low-half, and extended mode-two output loop is now native with
atomic writes. The `$1488` post-record mode dispatch is native and stops at
typed first-header bytes `$14a9`, `$14f0`, or `$1647`; continue from those
genuine ordered runtime observations. Mode two now owns its genuine `$1647`,
`$1653`, and `$1657` inputs and exact setup through typed source byte `$16b3`;
continue there without assigning lookup or pixel semantics.
The full typed `$16b3..$16e8` nested byte-pair loop is native, including
lookup boundaries, atomic destination writes, repeated row/column edges, and
return. It can now consume bytes directly from owned native memory, including
physical-equivalent DOS segment aliases, under a finite transactional cap.
Its `$16e8` return is caller-connected through exact `$1740..$1763`, genuine
embedded `$170c/$170e` table words, raw callee-word copies, and the complete
private function-six boundary at `$1764->$0122`. Continue from that typed
private result; do not infer pixel or palette meaning. The later function-six
return is now state-distinct, advances the title loop's output pointers to
`$02e0`, and stops at the next descriptor pair `$13aa`, source `$3481:$001b`.
That one pair is now automatically sourced from hash-admitted
`TITLE.LIB+$482b` after an exact segment-to-physical proof and normalizes to
`$32a1:$0006`. Continue only with the typed runtime word at `$32a1:$001e`;
do not generalize this one proven alias into generic DOS memory semantics.
The same leaf-relative proof now admits that record's complete fixed header
atomically. It additionally admits the exact first payload byte at
`TITLE.LIB+$2a32` and executes `$1419..$1427`, including the write to
`$4000:$02e0` and the non-zero loop edge. The exact next stream byte `$10`
and its two admitted lookup bytes now execute both decoder nibbles, producing
`$00/$01` at `$4000:$02e1..$02e2`. A bounded transactional stream driver then
admits only the canonical relocated second-descriptor byte at `$32a1:$0024`
(`TITLE.LIB+$2a34 == $1e`); it rejects physical-equivalent aliases and
commits no partial transition on a contradictory leaf. That byte and its
canonical little-endian word `$201e` now execute `$144a..$146b`, writing the
three repeated bytes at `$4000:$02e3..$02e5` and returning to `$1428`, source
`$32a1:$0025`. Its exact high-nibble continuation and lookup then return to
the ordinary stream boundary `$32a1:$0026`. The same 16-observation bounded,
transactional native continuation now validates the exact 6-byte
`TITLE.LIB+$2a34..+$2a39` prefix (SHA-256
`f5e49eddff72cad076c01cc7d4b884404224ea78a24ebe659e320951bca14b29`),
executes its two low/high lookup pairs and the selected mode-two run, and
returns to `$1428`, source `$32a1:$002b`. Do not infer codec semantics or
generalize this relocation into DOS-memory aliasing.
The other-value branch owns the second descriptor and first two raw words plus
their product and subtraction; continue at `$13e9`, source `$3c80:$0001`. Do not assign
graphics or codec semantics to these fields.

The Millennium Atari config loop now has all three D0-indexed iterations, D7
termination, and its deterministic epilogue native through RTS. The saved-
register `MOVEM.L (A7)+` at
`$2b562`; its external frame must be typed before caller execution continues.
The typed frame now restores all 15 registers and returns through `$2aac8`.
The caller-connected `$2aa68` prefix is native through XBIOS selector `$26`;
the next exact boundary is `TRAP #14` at `$2aa72`.
Selector `$26` now has a typed return; cleanup, RTS, and caller D7 setup are
native. The absolute `$2aa0c->$2a5aa` call and its argument pushes are native.
The next exact boundary is GEMDOS selector `$3d` `TRAP #1` at `$2a5b4`;
continue only with a generation-owned typed Fopen result.
The typed raw result is now admitted and stored exactly. Nonnegative results
load the exact literal arguments and stop before `JSR $2a5c2` at `$2aa28`.
Negative results reach the verified self-loop at `$2a632`. The positive callee
is the next executable evidence job.
The positive callee's argument setup is now native through GEMDOS selector
`$3f`; the next boundary is `TRAP #1` at `$2a5d0`. Its return and any bytes
written to `$7d42` require an explicit typed observation.
The raw Fread result is now typed; because original code does not branch on
it, no buffer observation is needed to reach Fclose. Execution now stops at
selector `$3e` `TRAP #1` `$2a5e6`; its return is the next boundary.
The Fclose return and the only four Fread bytes consumed before the next call
are now typed and native. The next exact boundary is `JSR $2b2be` at `$2aaec`.
The correctly mapped `$2b2be` setup is now native through its atomic D6/D7
stores. The next boundary is `MOVE.B (A4)+,D0` at `$2b2de`, source `$2c250`.
That first source byte and all four exact dispatch outcomes are now native.
The production facade now binds the prefix and token observations to the
resident hash-admitted `MILL22D.INF` bytes; synthetic token streams remain
confined to focused state-machine tests and cannot enter runtime memory.
The normal path now owns the typed pair at `$2c251..$2c252` and executes the
hash-bound D6/D7/D5 run, row and plane continuation at `$2b2f2..$2b321`.
It stops only when the next pair/token needs source bytes or at the routine
RTS. The repeated-byte, swapped-pair, and extended 14-bit run paths at
`$2b338`, `$2b376`, and `$2b3b8` are now native with typed payload bytes and
atomic destination effects. The next large Atari job begins with the caller
continuation after `$2b2be` returns. That caller and the `$2b448` clear/copy
prefix are now native through `$2b486`, with the 96-byte original source and
all 32 longword writes hash-bound and atomic. The 16-by-3 palette arithmetic
loop is now native with typed existing destination words and atomic byte/word
effects through `TRAP #14` `$2b4ac`. Continue with a typed XBIOS selector-6
return and the largest deterministic portion of the following timing loop.
That raw return, stack cleanup, 20,000-iteration D0 delay and first D7 `DBF`
edge are now native through the corrected recurrence boundary at `$2b46e`.
All six recurrent palette passes, their typed selector-6 returns, delay loops,
D7 transitions, terminal selector-6 return, and local RTS `$2b4c6` are now
native. Continue with a typed RTS destination and the largest deterministic
caller continuation. The exact `$2ab04` return is now typed; its caller loads
the corrected D7 pointer `$2a634`, enters the reused `$2aa0c` helper, and stops
at the second configuration file's GEMDOS selector-`$3d` boundary. Continue
with that typed Fopen result and its bounded success/failure caller path. Both
are now native: negative returns reach `$2a632`, while nonnegative returns
atomically retain the handle and reach the existing `$2a5c2` Fread boundary.
Continue with the second-config Fread/Fclose results and prove its caller
return separately from the first configuration path. Typed Fread and Fclose
results now reach helper RTS `$2a5ec`; a typed `$2ab10` return owns the exact
caller through XBIOS selector `$26` at `$2ab24`. Continue with that typed
firmware result and the deterministic cleanup/RTS continuation. The raw result,
cleanup, RTS `$2ab28`, typed return `$77042`, and staged-PRG caller are now
native through GEMDOS selector `$3d` at `$77056`. Continue with that typed
service result and its exact success/failure branches. Both branches are now
native: failure stops at `$77060`, while success reaches GEMDOS selector `$3f`
at `$77074` with count `$20000` and buffer `$11e00`. Continue from the typed
Fread result. Typed Fread and Fclose results now reach the two atomic constant
writes and local RTS `$770ba`. Its typed even 24-bit stack destination is now
the terminal preservation boundary: the staged target was entered by `JMP`,
so there is no statically encoded caller continuation to recover. Do not infer
filesystem, firmware, or wall-clock effects from static bytes.

This is the ordered execution queue for the completion plan. It is a
preservation tracker, not a list of compatibility claims. A task moves only
when its acceptance evidence is committed; a missing capture is a boundary,
not permission to synthesize a result.

The queue is deliberately organized by its contribution to the first genuine,
playable vertical slice. Presentation, packaging, and broad platform work stay
behind the first proven input-to-frame-to-state loop.

## 2026-09-04 native execution checkpoint

### Later native batch

The English Millennium DOS title path now continues past its BIOS palette
loops through the exact DOS memory allocation sequence and the complete
`title.lib` loader helper. It opens the hash-verified original leaf, admits
nine bounded reads into observed allocated segments, closes it, applies the
recovered header relocation fields, and stops at `$0f6b` for mode 1 or
`$0f6a` for other modes. Raw DOS returns and failure paths remain explicit.

The clean Deuteros Amiga title path now executes both caller-connected
`$41bb4` paired dispatches from genuine ADF bytes. The second `$004e` route
uses the proven fixed `$4128e` descriptor, consumes its complete 229-byte
payload, observes only the 64 genuinely preexisting final row/plane words,
and atomically completes all 320 merge writes through `$4051e`. Decoded sparse
memory is not yet a renderer or parity claim.

The Millennium Atari Equinox path now crosses XBIOS selectors 2, 3, 4, Line-A,
selector `$15`, and selector 6 through typed observations and deterministic
local continuations. It stops at `JSR $2b55a`. A previously considered byte
sequence maps to `$2b57c` under the exact admitted `$2a500` load, so Eon now
rejects that 22-byte-shifted candidate instead of executing it.

Player-visible game-text presentation now has a declarative, source-parity
tested map with complete leaf SHA-256, offset, and length. It covers ten
English DOS launcher strings plus all 41 celestial labels from both the exact
English and Spanish static-data leaves: 92 source-bound definitions and 51
catalog messages. Every message is present in all shipped PO catalogs for both
presentation modes. This is infrastructure and current-string coverage, not a
claim that unrecovered game text has already been extracted.

The mechanical disassembly inventory is complete for all eight declared
releases: 19 code images, 21 admitted ranges, and 764,867 source bytes are
accounted for. This is byte coverage, not semantic recovery or parity.

The active English Millennium DOS path owns the selected sound-driver load
in a bounded native paragraph arena, then stops at the `MILL.COM` INT 95h
SetVect instruction `$0239`. Continuation requires the post-interrupt PC
`$023b` and the actual INT 95h IVT words pointing to the loaded driver; only
then can the parent-stack and `TITLES.EXE` EXEC boundaries be reached. Native
tests cover that typed tuple but there is no capture for it. No DOS PSP,
parent `EXEC` return, display result, or game-state transition is inferred.

The normal SDL and CLI startup route no longer depends on a test-only injected
child code segment after the original sound selection. Choices `0`, `1`, and
`2` now bind the exact supplied `SIBM.DRV`, `SSBL.DRV`, and `SCVX.DRV` leaves
by hash and enter their shared bounded file-loader prefix. Choice `0` was
previously a dead end despite being a literal original option. Each choice
creates a labelled Eon compatibility process at `$e000`, then lets the
existing deterministic loader consume only the selected hash-admitted leaf.
That segment is engine-owned state, never an original allocation claim; the
first original-dependent parent-stack result and every later driver/device
ABI boundary remain blocked pending evidence. This does not claim SIBM
initialization or audio playback.

The clean Deuteros Amiga path now accumulates the zero/zero route and all three
remaining deterministic non-negative `$1fbe6` planar routes into a sparse
320x200 four-plane Original surface. SDL presents only pixels whose four source
plane bytes are admitted; every other pixel remains invalid and transparent.
This is not a complete title frame and supplies no title-input semantics.

The Millennium Atari Equinox path now materializes the complete exact
`MILENIUM.TOS` TEXT+DATA+BSS image at an Eon-owned address, applies all 227
relocations, reads the exact `MILL22A.inf` payload through a narrow read-only
GEMDOS compatibility service, and follows its native JSR/JMP chain to
`$2aa88`. It stops before `MOVE SR,D0`; the original status/privilege value and
the resulting branch remain explicit observations.

The corrected Millennium Amiga first stage is native from `$41000` through
its register-save/vector setup, two typed frame-complete ILLEGAL exception
entries. Vector-9 tracing decrypts and executes the exact ADDX plus ten
unconditional branch steps through `$411d8`, then the deterministic LEA,
MOVEQ, table-word and ADD register prefix. A typed group-0 frame admits the
24-bit bus-error route, and a typed custom-chip/ExecBase observation advances
the deterministic setup. The typed graphics.library returns at `$41780`,
`$417b4`, `$417c6`, and `$417d2` now commit the bounded bitmap/pointer setup;
the exact custom-chip effects at `$42546` and `$4254c` are retained as typed
records. Execution stops before `$42552 -> $41d4a`, whose device-call result
and visible display remain unproven.

The English DOS native-process adapter now connects only after a typed
`2200AD.EXE` child-entry observation at the title-to-game boundary. Its
first and second `$0129` returns require explicit, strictly ordered AX
observations and are visible through copy-only checkpoints. The API does not
provide those values or establish original-process reachability; rank 1 still
requires a genuine title/EXEC capture.

The new diagnostics-only trace profile
`millennium-dos-en-title-exec-entry-v4` pins the title-exit stack segment,
the exact `MILL.COM` game `EXEC`, and the `2200AD.EXE` entry registers. Its
validator and manifest writer are present, but no reviewed recorder emits the
ten-row sequence and no trace can yet advance the active runtime. A dedicated
observer must capture both image identities, the ordered DOS/title returns,
and the child `CS:IP`/`SS:SP` from one run before the native game-entry gate
can consume it.

| Rank | Work package | Exact current evidence | Required acceptance evidence | Status / boundary |
| --- | --- | --- | --- | --- |
| 1 | Millennium DOS: capture the launcher/title/`2200AD.EXE` handoff and private DOS ABI | English DOS release `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`; CLI-validated diagnostics-only title-init v2 profile binds the `MILL.COM:0x02cf` driver-load, setup-site `0x0209`/actual-`INT` `0x020c`, `TITLES.EXE:0x0127` request, and two raw returns at `$0129`; v6 additionally binds `svga_s3`/`ega` machine-profile selection to its exact config, while the genuine EGA diagnostic still requested `mcga.bin` and hit the console-capped `INT 6` boundary; v7 records IVT `INT 91h` endpoint `087e:0000`, v8 records a normal-core transfer to it, v9 binds the first raw caller re-entry (`AX=$0101`, `FLAGS=$7202`), v11 terminates the host recorder only after the complete twice-observed `INT 6` diagnostic receipt matches byte-for-byte, and v12 independently repeats an immediate predecessor at `f000:ca60` outside the recognised original-image map. V13's no-input preflight ends at the same eight-record `INT 6` receipt; later experimental capture 07 records complete MCGA.BIN and SIBM.DRV buffers with their DOS allocation segments, but no program EXEC or title handoff; [read-only physical capture runner](MILLENNIUM_DOS_CAPTURE.md#safe-capture-procedure); [external recorder status](MILLENNIUM_DOS_DOSBOX_X_RECORDER.md#prototype-status) | Hash-bound genuine trace of each interrupt, EXEC/far-return and driver result through one navigable state | The title-init prefix and physical capture route have strict contracts. Capture 07 is explicitly experimental and has no host input; its matched driver buffers do not prove DOS success, driver initialization, title handoff, rendering, audio, or game state. |
| 2 | Millennium DOS: admit the GX startup bridge | Same release; `2200GX.EXE` SHA-256 `093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb`; caller and record bounds in `PRESERVATION.md#millennium-dos-gx-startup-record-boundary`; strict ten-record admission builds a call-free overlay state after independently pinned trace validation; an engine-owned successor now requires the exact active English release to have independently reached title handoff and publishes only a terminal value checkpoint | A genuine hash-bound ten-record capture plus the rank-1 driver/title handoff; active admission must end at the second private-INT boundary and reject any missing/reordered/altered record without changing the prior session | Runtime ownership, byte lifetime, state, rejection, reset and revocation contracts are implemented. The current English path still stops before title handoff, so the successor remains unavailable; no genuine GX trace, frame, input or gameplay is implied |
| 3 | Deuteros Amiga: capture title initialization and display ABI | English Amiga release `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`; clean disk 1 `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`; hash-bound `$389e2` caller establishes start disk offset `$14000`, buffer `$26cc0`, and `$5800` total; the `$208c0` loop plus `$20902` vector helper gate four ordered `-$1c8` calls, each followed by `io_Error` at IORequest+$1f; both selector copy spans are checked inside that completed read buffer; [capture status and latest receipt](DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md) | Capture bitplane/palette/audio checkpoints and compare them with native Original output; verify a subsequent guest action changes state | Focused direct-media CTest passes against the supplied archive. The gate admits request order/status and copy-address containment only; active disk identity and returned payload remain unobserved. No captured bitplanes, frame hash, audio, selector meaning, or validated actionable state exists. |
| 4 | Deuteros Amiga: define and admit title-display capture evidence | Title-stage/main-stage recovery-map entries; strict v4 24-record contract and v5 artifact contract; an engine-owned checkpoint now reopens, rehashes and revalidates all evidence at consumption time; separately, the native runtime publishes bounded Original frames from recovered title/main-stage code | A genuine, write-protected v5 capture satisfying every event and artifact checkpoint; then compare captured display and ABI results against the native runtime | The v16 capture validates a bounded register-write receipt only; it does not contain the complete v5 display artifact. Immutable v4/v5 checkpoint ownership, state transition, rejection, reset and revocation are implemented. Full capture-backed display validation and parity remain open. |
| 5 | Millennium DOS: recover first actionable state and controls | Hash-identified `TITLE.LIB`, `GX.LIB`, video-driver and title-flow profiles; the typed post-overlay continuation covers `$d39d..$d412`, fifteen exact call returns, explicit AL/runtime-byte branches, poll cycle and terminal dispatch-call boundaries; the `$d3db → $0f05` DOS AH=06h leaf is now independently byte/hash anchored | Replay of a real input → canonical state → frame/audio checkpoint | F6/F7/F10 runtime ownership, typed forwarding, copy-only checkpoints and revocation are coded and real-media tested at independent recovery boundaries. The leaf proves only the DOS function request, not the host keyboard mapping or empty-queue AL value; SDL is not connected to the native action poll. Positive active dispatch remains unavailable until genuine predecessor evidence exists. No dispatch resolution, call return, input, handler meaning or frame is invented |
| 6 | Deuteros Amiga: recover first actionable state and controls | Clean ADF loader, bundle, VM, opening-frame and title-stage profiles; bounded native main-stage execution publishes recurring Original frames at the recovered visibility boundary; the installed `$224cc` vector target has a hash-bound 30-byte prologue at ADF `+$7ccc`, and its statically reached `$22816..$229e7` span is hash-bound at ADF `+$8016`; active-runtime worker entry validates generation, PC, owned nested-BSR stack return, vector, and resident code before applying translated writes to owned software memory; v19 visible capture records held-Fire poll outcome at `$21866` (Z=1, BNE falls through), the title-stage `$218cc` path, display setup, and two `$1fbe6` selector reads with `$1f98c/$1f98e=$00`; a host-delivered `KEY_1` is followed by a selector-code sample at A0=`$1eed5`; v23 capture 06 links physical input to `$1fbe6/$1fc22`, a complete zero-route invocation and 32 chip-RAM writes/readbacks, and observes `$21868` set `$21720` from 0 to 1; a hash-bound native continuation executes `$21870..$21892` with typed lazy reads of `$2171e/$21720/$21696`, recording exact branch PCs and SR | Replay of a real input → canonical state → bitplane/palette/audio checkpoint | The v23 capture proves the selector/zero-route and latch memory effects occurred after physical input, but the route's selector cells remain zero and there is no later display update, changed frame, or accepted gameplay action. The earlier v19 screen became black after `KEY_1`; A0=`$1eed5` lies in a zero-filled source span selected by the hash-bound main-stage load profile and does not identify a prompt string. These captures do not prove main-stage handoff or canonical state change. The native `$21870..$21892` continuation's exact reachability from the captured path remains unproven. Custom-register outputs remain intents and do not establish device/audio effects or cadence. Capture-backed state equivalence, interrupt cadence and audio timing remain open. |
| 7 | Millennium DOS: establish video/audio device contracts for that slice | `EGA640.BIN`, `MCGA.BIN`, `SSBL.DRV`, `SCVX.DRV`, and VOC source-byte profiles | Captured calls plus pixel/sample hash comparison; no guessed hardware behavior | Depends on rank 1 |
| 8 | Canonical game-core subsystems for the two proven slices | Only rule paths that have a caller-connected code proof or trace | Deterministic long-run replays, including state transitions and edge cases | Cannot begin from strings, assets, or inferred genre mechanics |
| 9 | Remaining Amiga/Atari ST execution adapters | Millennium Amiga `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400`, Millennium Atari `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01`, Deuteros Atari `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653`; the latter has a hash-gated static bootstrap checkpoint and a non-admitted Hatari `Floprd` shape cross-check | Per-release recorder-backed bootstrap/device traces and explicit shared/divergent replay checks | Deferred until a playable vertical slice establishes the right core boundary; ordinary emulator output remains diagnostics-only |
| 10 | Final UX, localization, packages, and release audit | Existing card route, i18n catalogues, CI package recipes, and preservation contracts | Real-session menu/CLI equivalence, 20-language checks, clean-package scans and end-to-end replay | Continuous maintenance only; never substitutes runtime recovery or authorizes a release |

Capture 07 refines rank 6's selector evidence: two Return press/release pairs
precede `$1f98c=$ff`, `$1f98e=$00` samples at `$1fbe6`, and a later Space pair
precedes `$00/$00`. It does not record the negative branch destination or the
`$1fc24`/`$3fbf8` return. The next trace should add those exact boundaries;
playability remains unproven.

### 2026-10-08 AH=48 return sidecar readiness

An experimental DOSBox-X recorder now has an exact binary pin and a bounded
runner/verifier contract for register snapshots at the four DOS sites
`$1b26`, `$1b2d`, `$1b38`, and `$1b3f`. Its C++14 harness and full recorder
build pass on trv2. No capture has been run. The next useful evidence requires
a fresh visible `physical-input` run with manual operator input and an orderly
window close; its pre-IRET handler snapshots will still not prove guest IRET,
allocation memory, title handoff, or gameplay. Rank 1 remains open until a
genuine trace connects the launcher, title, and `2200AD.EXE` through one
navigable state.

### 2026-10-08 Millennium DOS title-entry scenario follow-up

The context-sensitive analyzer now follows the two recorded `TITLES.EXE`
`INT 91h` AX values (`$0101`, `$0000`) as an explicit static scenario. It
reaches the candidate BIOS `INT 10h` at `$046d` after the `$044c` initializer;
the call sets AH/AL to `$10`, while the other request registers and BIOS
effects remain unmodeled. `$0134`/`$125c` are not reached under this scenario.
The report is external at
`/home/trv2/.cache/project-eon-tools/millennium-title-context-reachability-20261008-v10.tsv`
(SHA-256 `6e9afca1fead092f539def88698f9d2a9d47f2e1695cb74c4d0dd45bf0cad252`).
This narrows the next capture to BIOS input/output at `$046d`; it does not
establish that this caller route ran or that the title/game is playable.

A separate hypothetical BIOS-return scenario now follows only `$046d` to
`$046f`, clearing tracked register/flag facts and leaving BIOS effects opaque.
It reaches `$1ad6` after the candidate `$044c` return, then stops because the
DS-relative write at `$0107` cannot be proven disjoint from the live return
word at `$1bb0`. The updated external report is
`/home/trv2/.cache/project-eon-tools/millennium-title-context-reachability-20261008-v13.tsv`
(SHA-256 `c9ecba7a83791125ad7237ef3d7ec639af5d1ff776366dbc29db696a240710f9`).
The scenario does not establish a BIOS return or runtime execution. The next
useful DOS observation is the exact DS/stack relation at `$1ad6`, along with
the BIOS return at `$046d`; no memory write is modeled from this analysis.

The analyzer now distinguishes a source `PUSH` from a register write and can
optionally preserve the known DS value across the exact wrapper pop at
`$012d`, subject to an explicit saved-stack assumption. Its DS-absolute alias
proof now uses that fact only after proving the access disjoint from active
return words. With additional explicit BIOS-return/DS and DOS-return scenarios,
the path passes `$1ad6` without applying its store and reaches the next DOS
`INT 21h` at `$1b2d`, with AH=`$48` and BX=`$fa00`. The allocator call remains
opaque; no return value, memory-manager effect, runtime route or playable state
is established. The report is
`/home/trv2/.cache/project-eon-tools/millennium-title-context-reachability-20261008-v16.tsv`
(SHA-256 `1f4750d18d5df57e8bc9161808345ebad139fd0462f55ae88fa76f286626c95a`).
The next DOS observation should bind the `$1b2d` return and the resulting
allocation segment before following any memory writes.

The context analyzer now also tracks candidate absolute-memory operand
read/write direction and includes `$104a`/`$1247` in its reachable-target
summary. The hash-bound v18 run still finds both loader candidates unreachable
under the named static assumptions and finds only candidate reads, not
absolute writes, to `$011c`; the direct `$1082 -> $1247` jump remains an
unconnected candidate edge. The external report hash is
`f998bb0fda0b1a1e37ce412aae3337d82d4e25bb84794898ca970705d3d44852`.
This narrows the static picture but does not justify a title/driver runtime
fix. Resume at the missing DOS allocator result at `$1b2d`, with its return
registers and allocation effects captured before admitting following writes.
A schema-4 follow-up also records 18 byte-start candidates for indirect or far
transfers (report SHA-256
`f7141ab07041147c982a5f5ee46b7d06d1179521b6c737c7be43c8ca8a443f43`). The
`$1068` far-jump candidate overlaps the `$1066` immediate, and the `$1299`
candidate has no resolved target; neither supplies a loader edge or runtime
evidence.

The English DOS startup continuation now records a typed BIOS return at the
hash-checked palette `INT 10h` site `$0476` only when the raw return CS:IP is the
expected child segment:`$0478`. Its receipt preserves observed AX/FLAGS and
sequence ordering without applying BIOS/palette effects or continuing the
loop. Focused direct-media coverage exercises the accepted boundary and rejects
wrong INT site, interrupt number, return PC/segment, stale order, and duplicates;
this does not replace rank 1's genuine capture requirement.

The same direct-media `TITLES.EXE` inspection has narrowed the next DOS capture
after `$1b2d` to the caller's ordered allocation sequence: `$fa00` paragraphs,
free that returned segment, `$1000` paragraphs, open/seek-end/close of the
CS-relative NUL-terminated `title.lib` at `$0e4e` to compute its rounded size,
allocate that size, then one paragraph. The exact ten-byte name at file offset
`$0d4e` hashes to
`62bfc3e4275f23097edf305a3e1144d3eac79b4a4c75cc35cfbb3eb0b9255aed`. The
file-size helper `$1af6..$1b1e` hashes to
`4fd3a9694c9ea36d7baf33607ed0b70ac764bb1f27bb6b686c3401bce5ef6b3d`; the
caller span `$1b28..$1b7f` hashes to
`b014a155ccc31c33a5f74e530634a09abfb00f5ec2de3686307dbf4d50ffc406`.
This is a capture target, not a runtime result: no allocator/file return,
title-library result, driver installation, or playable state is admitted yet. Preserve
the precise DOS return order and carry outcomes before extending native runtime
execution.

### 2026-09-03 native media-admission update

The English Millennium DOS native session now admits a read-only catalogue of
the fourteen executable-named original VOC leaves after exact media
verification and decoding. It retains filename/hash/sample-rate/sample-count
facts only and discards PCM bytes. This closes a resource-admission task, not
an audio-playback task: event-to-index mapping, driver ABI and timing remain
outside the recovered engine boundary.

### 2026-09-03 split-container Deuteros admission

A read-only scan of the user-supplied `~/.projecteon` collection found real
single-disk ZIP containers whose inner disk hashes match existing Deuteros
parser leaves but whose outer ZIP hashes are not the one combined-release
archive currently represented by `ReleaseArchive`. In particular, the Atari
ST pair contains the existing first-stage leaf
`aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee`
(Replicants disk 1) and the existing killer-boot leaf
`5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193`
(clean disk 2). The clean Amiga disk 1 likewise contains the existing
`6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`
leaf.

The scanner now admits two declarative, ordered split-container sets. Each
required ZIP occurs exactly once; its outer hash/size and its specific inner
disk hash/size are independently verified before an ordered-set digest binds
the logical release identity. The pair is rejected when incomplete,
ambiguous, altered, or mixed, and runtime admission reopens every selected
container instead of trusting scan-time state. No disk is copied, unpacked,
or substituted.

The admitted Atari ST set is a bootstrap-only native session: it proves static
loader facts but not XBIOS/raw-read results, title, input, frame, audio or
gameplay. The admitted clean Amiga pair supplies both hash-addressed ADFs to
the existing native recovered-opening session, which now reaches `READY` from
the user collection. That is not parity: title-display ABI, player controls,
and game state remain governed by ranks 3–6.

### 2026-10-25 synthetic ST candidate screening

The locator's synthetic FAT12/STX fixtures are permitted as search probes and
exercise only parsing/chain-consensus logic. A fresh read-only scan of local
`Downloads/` and `~/.projecteon` finds the original English-labelled Millennium
STX pair and alternate Robocop/4AFC one-disk images. Both alternates contain
the same 7,506-byte `MILL22A.INF`, 84,720-byte `MILL22B.INF` hash
`e315b0ec…121d53e9`, and later C–F resources as the admitted Equinox disk,
but their 221-byte startup PRGs and media geometries differ. They therefore
do not satisfy the current Equinox-only startup profile, and no runtime
admission was widened. Locator JSONL is kept in the external analysis cache;
original archives were read in place. Next compare the bounded startup PRG
control paths against shared config entry requirements, then admit an
alternate only if the complete source chain is established. This is candidate
search evidence, not a gameplay result.

### 2026-10-25 Deuteros display-buffer producer search

The hash-bound `tools/scan_deuteros_address_literals.py` was run read-only on
the supplied Deuteros ADF set for absolute address `$12ff4`. The canonical
Disk 1 contains 12 overlapping absolute-long/24-bit encodings. They include
the already mapped bootstrap graphics-memory module at ADF `+$3400`, which
stores `$ab00` at `$12ff4` and can select `$8ab00` after its typed service
results. Later title-stage spans at ADF `+$6e000` and `+$79000` read this same
cell, including at the `$1eda6` boundary. The modules are distinct images at
the overlapping runtime base `$13000`; the static reference search does not
prove that the bootstrap writer executes in the active opening-to-title
generation or that its value survives the intervening loads. Next establish
that caller-connected image transition and carry only an actually owned
`$12ff4` value into the later title session. No memory value or service return
was fabricated.

The STX Disk 1 follow-up checked the next 16, 32, and 64 bytes from the
`EXEC.TOS` copy source against both disks' authenticated FAT-consensus files
`MILL22A.INF`–`MILL22F.INF`; none occurs in those resources. On Disk 1, the
actual `EXEC.TOS` occupies one 512-byte sector and the sector bytes after its
221-byte EOF are zero padding. These bytes do not establish the program's
post-TEXT runtime memory, so the 79-word copy prefix still stops at the same
boundary. The read-only probe and its output remain in the scoped external
analysis cache.

### 2026-08-31 capture-route update

The rank-1 v13 Millennium DOS operator-visible no-input capture is independently
receipt-verified. It ends at the same eight-record `INT 6` diagnostic receipt
(SHA-256 `8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`)
with no host-key receipt and no title-input poll. The rank-3 Deuteros Amiga v9
15-second realtime no-input preflight is likewise receipt-verified: its 256
raw-PC records have SHA-256
`fd52c57cb44a402fc7b9ddbeea0e8d1867dd09e8851f586ef515d6aba8698c39` and
zero host-delivery links. These are capture-route/no-input facts only. They
do not establish guest input, title execution, display, audio, ABI results,
or gameplay, and the next operator-led capture remains the required P0
evidence.

### 2026-09-01 Millennium execution-history update

Two independently verified `v14-normal-core-history` captures of the same
write-protected English DOS archive retain an identical 16-entry normal-core
sidecar (SHA-256
`248969bc16cfd773f64140ff3e314f6cd465ad7514de0868d24803b399bf4dbb`). It
records zero-byte fetches at `0e70:18e4` through `0e70:1900`, followed by the
already known DOSBox-X default-callback opcode at `f000:ca60`. This isolates
the current P0 boundary to the unproven transfer into the `0e70` context. Two
later V20 transfer-observer receipts independently retain the same one-step
normal-core adjacency, `0000:0001 ca00f00e` to `0e70:fffe 00000000` (sidecar
SHA-256 `b4434953ad218801db9b3966d9d2be226b0261c7d4a87316c58feb8599472236`).
The first independently verified V21 capture reached the existing eight-record
`INT 6` boundary in 0.61 seconds and retained `int93_installation=absent`.
Thus neither reviewed original installer site executes on that route before the
known stop. The observer remains prepared to retain one verified original
`INT 21h/AH=25h` vector-$93 transaction at either site if a later capture
reaches it. Absence is explicit rather than synthesized and demonstrates no
installation, handler, or dispatch.
On 2026-09-01, the tightened V22 `diagnostic-no-input` runner independently
reproduced that same stop: its eight raw results retain SHA-256
`8d01223e76a7f5b8497c7a2d8c727452a6d25928002eff06df8265c460e851e7`, with
`host_input_receipt=absent`, `host_input_observed_during_capture=false`, and
`int93_installation=absent`. The V22 receipt is a procedure check as well: it
proves the runner rejected host input for the declared no-input diagnostic. It
does not prove guest polling, key acceptance, a private ABI result, rendering,
audio, title execution, or gameplay.
It does not authorize a callback bypass, guest-memory repair, inferred mapping,
or a gameplay claim: the exact original vector installation/dispatch path and
a navigable trace remain the rank-1 missing evidence.

On 2026-09-01, a second independently receipt-verified Deuteros Amiga v9
realtime run lasted 120 seconds. Its 384 raw-PC records (SHA-256
`d8732ec5aab06123147688b19b8bc750b0ee6ca1f9a03cdc68a5787271a1e5b9`)
remain capped at three known bootstrap sites and retain zero host-delivery
links. It is additional no-input reachability evidence only, not a guest
input, title, display, audio, ABI, or gameplay claim.

### 2026-09-02 recorder restoration state

The external Millennium DOS v21 observer reconstruction is now
`OBSERVER_FIX_REQUIRED`. Its v3 delta was independently reviewed for
post-`RealSetVec` observation, bounded host output and absence of
guest/input/scheduler changes, but an explicit read-only experimental run
proved the build contains only that delta on vanilla DOSBox-X. It lacks the
older normal-core/default-callback recorder hooks and reaches the known
unhandled-`INT 6` console loop before it produces the legacy receipt streams
or a v21 installer record. The next work item is therefore to recover and
review the complete base-recorder patch provenance, then integrate v3 on top
of it; pinning, locator admission, capture recovery and hash substitution are
all forbidden until then. See
[`CAPTURE_RECORDER_RESTORATION.md`](CAPTURE_RECORDER_RESTORATION.md) for exact
candidate provenance and the retained negative observation.

## Operating rule

The Deuteros title chain now advances from its dedicated fail-closed
OpenLibrary return boundary through the proven nonzero local call chain and
stops at `$1eda6`. A typed genuine observation can now supply the external
display-base value read from `$12ff4`; the engine advances the local
palette/clear plan and stops at `$40498`. The next increment must preserve the
custom-chip write boundary—do not substitute host hardware or claim display
output from the bounded plan. Four exact custom-chip observations now advance
the local callback-registration plan to `$1f04a`; the next increment requires
an explicit return observation for its Exec vector `-$1ce` and must not infer
the service or callback semantics. That typed return is now admitted and the
local RTS reaches `$404b6`; the next continuation must resolve or explicitly
observe the `$206d4` boundary rather than assigning it invented behavior.
The `$206d4` prefix and its explicit `-$126` return now advance the first local
descriptor plan to `$20708`. Continue only with an exact `-$162` observation;
do not infer either Exec service or manufacture its result.
The exact `-$162` return and its local pointer/link setup now reach `$2072e`.
Continue with a typed `-$1bc` return only; its service and branch result remain
unresolved.
The `-$1bc` return is now an explicit branch boundary: nonzero stops at the
original loop, while zero reaches `$20776` with the earlier observed D0 value.
Continue only through an exact second `-$162` return at `$2077a`.
That return and its local second-pointer setup now reach `$2079c`; the next
required boundary is the exact `-$1bc` return from `$207a0`.
That final return now completes the hash-proven `$206d4` routine and reaches
`$404bc`. Continue by recovering `$206be`; do not infer its returned D0 or
pointer effect.
The fully local `$206be` controller transfer and `$403e6` literal pointer seed
now advance to `$404ce`. The next boundary is the `$403f4` service batch; each
opaque callee must return through explicit evidence before later setup runs.
The following boundary-session milestones describe transitions exercised
with explicit test observations; they are not yet connected to the
launch-linked active driver. Static ABI lookup identifies its first vector
`-$c0` as graphics.library `LoadRGB4`, called with ViewPort `$12e12`, colors
`$1ed24`, and count 20. A prior direct-media checkpoint audit at `$404ce`
found no owned
runtime memory. A direct-media checkpoint audit at `$404ce` found no owned
bytes at `$12e12..$12e21`, only 10 of 12 bytes at `$12ec4..$12ecf`, and all 40
source color-table bytes at `$1ed24`. Recover the missing structure through
hash-bound original instructions before adding the launch-linked service and
regression. The initializer is already recovered in the alternate
`$13000` bootstrap image: `$1310c` writes `$12ec4` to `4(A1)` with A1=`$12e12`,
thereby setting `$12e16`. That ordered native layout path is modeled, but the
active profile-one title launch jumps to `$40426` without running it. Connect
the correct bootstrap lifecycle and its typed graphics dependencies to title
launch; do not directly reseed the ViewPort, reuse the distinct main-stage
16-color LoadRGB4 handler, or fabricate later graphics-vector returns and the
`$20276` value. The original first-entry word is confirmed as source `$0000`
at `$404ce` (ADF `+$7b276`, two-byte SHA-256 recorded in `PRESERVATION.md`).
Full boot-track admission owns the genuine zero longword at `$12ff4`. At the
opening handoff, the active runtime now executes the hash-pinned `$13000`
bootstrap graphics prefix through its exact native-compatibility OpenLibrary
return and the source-owned `$12ff4=$ab00` store, then publishes the
`$13028/-$d8` AvailMem boundary. A typed observation requires both D0 and SR;
the authentic BNE uses the observed Z bit to stop at either the second
AvailMem request `$13034` or graphics call `$13058/-$168`. The CLI reports the
current exact external requirement. The typed second AvailMem return executes
the authentic CMP.L/BLT path: signed D0 below `$80000` retains `$ab00`, while
all other signed values select `$8ab00`, then both paths stop at
`$13058/-$168`. After the required AvailMem observation, the active route
invokes the existing Kickstart-ROM-hash-backed InitView and InitVPort
implementations as explicitly labelled Eon native-compatibility services.
It executes only authenticated caller bytes and stops at `$1306c`, the
exclusive end of the admitted 108-byte source span. It invents neither an
AvailMem result nor an original graphics-service return. InitBitMap at
`$1309e` lies outside this source span and is not executed.
Read-only searches of retained Deuteros captures 07, 08, 25, and 27 found no
startup records at `$13028`, `$13034`, or `$13058`; those traces begin after
the bootstrap graphics setup. Their captured Copper BPL1PT `$ab00` confirms a
later displayed buffer address, but does not recover either AvailMem D0/SR
return or prove the startup branch. Do not substitute that later address for
the missing service returns.
The first `$20094` graphics return now advances to `$200b0`. Continue only
with the exact `-$198` return using the same observed library base; do not
invent its D0 byte or descriptor effect.
The formerly listed `$200f4` graphics boundary is crossed through its exact
same-library `-$1a4` return, and the caller-connected chain now includes the
tail, command path, and both `$41bb4` merges. The first tail chain now commits
only hash-proven local effects to private native memory: the `$20276->$2027c`
word, `$20094`'s status/pointer/descriptor stores, the four observed tail
words and `$ffff` literals, both bounded selection results, and `$404da`'s
two observed table longwords plus `$204c8`'s descriptor stores. Graphics and
Exec vectors remain typed external boundaries; none are emulated. From authoritative boundary
`$4051e`, the next deterministic service prefix commits seven exact effects.
Its typed `$20e6a -> $1fb9a` return now reloads the owned selector, adds
`$00a0`; its typed `$20e7a -> $1ff08` return then selects immutable table
longword `$127a3980` and commits it to `$1378e`. Its typed
`$20e96 -> $22bca` return now enters `$20ba8`; ordered observations of
`$13008/$202bc` resolve the first loop branch. The clear-carry route stops
before `$20bd6 -> $41a68` (D0 `$0048`, D1 `$0010`); carry/zero skips at
`$20bea`. Exact typed `$41a68` returns and local skip routes now complete all
eight bounded iterations and return through `$20bf0`. The typed
`$20bf4 -> $1f9b8` return and exact three-read pointer chain now select and
atomically write `$00b0/$00bd` to `$417a2`. Selector `$005c` follows the
direct `$41c32` route, and both distinct `$74576/$76e24` streams are now
hash-bound, fully decoded, and atomically written through the typed `$1f168`
destination. Exact typed `$20c4c->$41ad2` returns now complete the bounded
12-entry descriptor-bit loop and atomically store `$00bd` at `$416b4`.
The mutable `$20a10` byte now has an exact typed observation; its low-byte
addition atomically adjusts `$416b4`, and D0 becomes `$004b`. Continue at
`$20c7a->$41bb4` by proving the adjusted descriptor route selected by that
observed byte; do not reuse the unadjusted `$00bd` stream implicitly.
The genuine `$03` observation now selects a separately hash-bound `$00c0`
descriptor and completes its 68-by-168 decode as 22,848 atomic byte writes
after one typed destination-pointer read. Caller `$20c80` now owns the typed
`$19d1e` pointer and exact zero branch. The qualifying nonzero object gate
now owns typed `$ee`/`$f0` bytes and immutable table loads through the first
`$20ca8->$41ad2` call. Both helper returns and the second table pair are now
owned through local RTS `$20cb8` without assigning helper effects. The typed
stack frame selects only caller `$40530`; its repeated `$20ba8` local-service
call now consumes two ordered runtime words, all eight typed `$41a68`
returns, exact counter effects and DBF iterations, then returns through
`$20bf0` to known caller address `$40536`. The caller now loads literal A0
`$20cfe`, admits its typed return, reads typed long `$12fe4`, shifts it right
three bits, and atomically stores the low word at `$1f42a`. A typed return from
`$4054c->$37180` now admits the exact longword copy `$1378e->$1c26c` and a
typed `$4040e` mode selects `$40566->$36a8c` or `$4056e->$1fb9a`. Continue
from that selected external call; do not
treat the sparse decoded memory as a renderer surface.
Both selected returns and their join at `$40574` are exposed through every
runtime facade with replay and revocation checks. The following `$222c0` and
`$23e4e` calls now require typed returns, after which the exact word-change and
60,000-count gate is recovered atomically. The optional `$405b6->$4069a`
return is typed, clears `$40410` atomically, and joins the not-due route at
`$405c6`. A nonzero typed `$1bf36` byte now admits `$405d0->$1f9a4` returning
after its exact inline byte stream at `$405de`, followed by typed word `$22a0`
and `$405e4->$1fe88` return. Three more typed service returns and their exact
conditional word reads recover the caller through `$4062c`: rejected
conditions join at `$40638`, while the selected tail stops at external jump
`$37f56`. At `$40638`, the first `$1f238` return is typed. A non-`$43` low
byte loops to `$40574`; `$43` atomically XORs word `$1bf36` with `$0101`,
selects exact colour word `$00f0` or `$0f00`. The repeated `$40662->$1f238`
returns are now observation-driven: each iteration atomically writes the
selected word to `$dff180`, non-`$43` repeats at `$40656`, and `$43` exits
through `$40670` to the recovered `$40574` loop.
The alternate selected gate now types returns from `$3880a` and `$204fa`,
then atomically copies exactly `$9392` hash-bound original-stage bytes from
`$13006` to `$66000`, overlaying any earlier admitted source mutations from
the sparse runtime ledger. No caller-supplied replacement bytes are accepted.
Local `$37f7a->$37f9a` is now caller-connected through its initial service,
the five-call equal or seven-call unequal Exec route, typed comparison and
the exact profile-two bootstrap writes. Its `$12800` jump now follows the
opcode-validated `$12a4e` dispatcher, `$12b44->$12b1c` profile route and
atomically reloads all `$4200` genuine main-stage bytes at `$20000`, stopping
at `$21734`. Continue with a re-entry session that owns the exact incoming A1
controller and typed D0 stores before the first main-stage service boundary;
do not assign a gameplay meaning to profile two. That entry prefix is now
atomic; typed Exec `-$96` and `-$9c` returns now advance through literal
`$7fff0`, followed by ordered typed returns from `$20068` and `$2013a`. The
recovered pointer copies reach `$20510/$20c20`. Ordered `$22a5a/$22bea/$22bea`
returns, four literal custom-register writes, and both typed pointer chains now
reach `$2197a/$2197e`. The `$217d8->$20994` edge now follows its exact
hash-bound local prefix, clears A1, reloads the Exec base from `$0004`, and
types the `$2099e` vector `-$126` return, atomically applies its five exact
big-endian writes, and stops at the `$209ca` vector `-$162` boundary (return
`$209ce`). Its return is now typed; two exact longword writes commit atomically
before the next boundary at `$209f0`, vector `-$1bc`, return `$209f4`.
That return now preserves both exact branches: nonzero terminates in the
`$209fa` spin, while zero atomically writes `$2094c/$2093a`, returns to
`$217de`, loads D1 with `$20000`, and reaches an SDL-backed `$217e4`
stateful-bit sample on `$bfe001`. That operation atomically sets raw bit 1,
validates the resident `$21704` source word, and mirrors it through local
`$21926` at `$21704/$21706`; the matching `$21932` resource loader then copies
only the selected ADF bytes into owned runtime memory. `$22a5a` and `$22ab8`
now execute as a hash-checked deterministic local service, followed by the
exact `$217fe` stores that clear `$21720/$2171e` and set `$210f2` to one. The
hash-bound `$21276` native-body candidate requires an owned `$32a24` record;
the caller's typed re-entry D0 supplies that selector through `$21704` and
`$21926`. Selector zero now drives the exact `$21708` table entry, bounded ADF
probe/body transfer, and observed released `$2196e` hardware retry into owned
`$2ad24/$32a24` memory. The earlier typed OpenLibrary return now owns `$12fec`
only through its exact post-return store, so `$21276` can consume both owned
sources. Ordered typed returns at `$21310` and `$2132a` now continue to the
local `$2132e->$20888` call and its four native stores. Continue from the
typed `$208b0` ExecBase read and `-$a8` return, then the owned `$2126a`
conditional: `$2133c->$22330` when nonzero or local `$21342` when zero. The
nonzero branch now executes the complete native optional-resource initializer,
bounded maximum scan and 15-record relocation transaction before joining the
same `$21342` record loop.
The zero branch now executes the complete native record construction and
first processing pass, typed buffer selection, full buffer clear, and initial
no-draw record walk and owned view selection through `$216ee/-$de`. Continue
with the now-typed view return and resumable hardware wait into the next
processing pass. Its owned `$214aa` local interpreter now executes sound
descriptor staging and the original random routine; the real initial pass
reaches the second record's palette boundary `$214ee`. The native prefix,
ordered library-return admission and saved-position scheduler resumption now
continue through the second buffer/view pass to the outer `$21822` gate.
The ordered outer port samples and native latch/control logic now re-enter
the scheduler and select the first sprite. Native opaque rendering now writes
its exact bitplanes and covers all 142 original resource bitmaps in tests.
The masked `$20cc6/$20fb2` cache/merge path now renders the following real
buffer and is cross-checked on all 142 original bitmaps. Native saved-scanline
save/restore now covers those same bitmaps, including bottom-clipped restores
with the original source-plane stride. Alternate-resource selector `$fe` now
executes the bounded `$20580` command classes directly against owned native
memory inside the atomic frame transaction, including its original global
video cursors, selector tables, embedded glyph rows, and four bitplane writes.
The distinct `$218cc`, `$21892` and `$21982` cleanup/transition
prefixes now execute native selector decisions and software sound reset;
the nonzero-resource fade now performs its 16-color RGB4 steps, ordered
dual palette calls and owned 32-step counter. Its two final native buffer
clears, exact fade return, `$224a2` cleanup writes and software sound reset
are implemented. Ordered asynchronous counter observations now continue both
wait routes to `$208ba`, retaining the original snapshot at `$12fe4` where
required. The `$208ba` service now continues through its typed Exec return,
then either the raw restart wait to `$217f6` or the `$20a74` request writes,
typed return and native selector increment. The reached `$21926` resource
loader and `$21a4c` special transition are implemented. The complete
`$219f8` boot-disk check is hash-gated and has a bounded native
success/mismatch/message/input state machine. It is now connected through the
production launcher, revocation-aware host, native-session state gate, and
coordinator with explicit system/data selection and atomic memory/session
publication. Direct coordinator/facade tests reject unknown media, wrong
boundaries, out-of-order palette/input callbacks, and source revocation
without changing the native-memory checksum. Recurring resource
0/1 loads now verify the owned table, transfer original bytes in native
memory and follow the retained primary return into fresh loop preparation;
branch-entered unknown returns remain at `$21978`.
The state-two `$21a4c` transition now conditionally clears its fixed buffer,
admits both ordered Exec returns and publishes handoff metadata before
reaching `$12800`. The `$12800` re-entry now passes five typed Exec
returns and native request setup, reaching the original error spin or the
`$13000`/`$12932` caller boundary. Continue those routines and profile dispatch.
The configured bootstrap route now executes `$12932` request initialization,
its ordered Exec return and the caller's `$12a92` return, then dispatches via
the owned profile table at `$12a36`. Profiles 0–4 now execute their fixed
native load bodies and prepare the `$12ad2` read request;
profile 5 selects its distinct `$12b46` body.
The exact profile-five source block is now independently parser-gated across
all 204 bytes at `$12b46..$12c11`, with its four service sites, two literal
read ranges, mutable `$66000->$13006` copy, and common `$12ada` tail recorded
as preservation metadata. The native runtime now executes its helper and
four private phases, performs both hash-locked media transfers and the
owned-memory copy, then joins the three-return common cleanup and reaches the
real `$13000` program entry with profile 5 retained. Failed or reordered
returns roll back their pending effects.
The restored `$13000` JMP is now a distinct typed program-entry boundary for
both the initial profile-one title load and the later profile-five reload, not
the bootstrap graphics routine that previously occupied that address. Its
explicit local advance validates owned runtime bytes/checksum, applies only
the selected profile's controller/mode stores, resets stale title receipts,
and rejoins shared title startup at `$40456`. The initial handoff installs the
exact admitted title span into native memory atomically before publishing the
boundary. The SDL runtime advances either deterministic profile before its
next presentation query so production cannot remain stranded at the JMP.
The successful `$12ad2` path now transfers the exact parsed main/title range
into private native memory, clears the request length and issues command 9,
then admits the separate `$12aee`, `$12afc` and `$12b0a` returns. Final RTS
uses the retained loaded destination rather than the final service's D0.
The `$20000` entry now reaches native setup at `$21734` below, and the `$13000`
title reload rejoins shared title startup. Failed or partial device effects
and profile-five execution remain distinct unfinished paths.
The loaded `$20000` jump now enters `$21734` natively, stores its actual
controller/profile, establishes stack metadata and crosses the two ordered
Exec services. Native `$20068` now handles the graphics-library return and
its zero-result spin; success continues through `$2013a/$2008e` to the
typed graphics return and both pointer stores at `$21768/$21772`. The reached
`$2177c -> $22a5a` call and two `$22bea` audio calls are implemented. The loaded
title at `$13000` remains a separate program-entry boundary.
Native re-entry now performs that sound reset and both `$22bea` consumers,
retaining the four ordered DMA write intents and reaching `$2178e`.
Both full four-channel register receipts now cross the coordinator atomically
as metadata with owned-range hashes and a generation. The only recovered
production pair is sound zero with zero volume, so its fail-closed host render
is empty; sound-table entries 1/2 are not autoplayed.
The consumer implements all four descriptor channels, delayed silence,
countdown, period changes, tail selection and ROM-dependent random modes.
The `$2178e` custom-register writes and owned graphics pointer chains are
implemented below. Recover an explicit later audible consumer edge before
extending playback; raw register writes do not prove playback, and `$224cc`
cadence remains unknown.
Random audio modes require their original owned ROM byte at `$ff0000+index`.
The `$2178e` native prefix now writes the four raw custom-register values,
follows both owned graphics pointer chains and enters `$20994`. Three ordered
Exec returns build the task/port/device request, then reach either the original
error spin or the typed `$217e4` CIA observation. The CIA operation and
`$217f2 -> $21926` return to `$217f6` are implemented below. Graphics roots
must be produced by the earlier graphics setup; controlled unit fixtures are
not a runtime fallback.
The typed `$217e4` CIA sample now drives native byte BSET and the owned
`$21704` selector read; the bounded interpreter path also checks the resulting
MOVE.W condition codes and X/upper-SR preservation before its call boundary.
Its retained `$217f6` return is recognized by the existing resource-0/1
transfer transaction, which resets loop-local receipts and reaches fresh
`$21276` preparation. Unproven selectors and unknown return addresses remain
separate boundaries; no alternate media is substituted.
The unconfigured bootstrap `$12a76 -> $13000` route now crosses library
opening, both memory-query branches and the two initial graphics calls,
reaching `$1306c`. The first query requires the returned Z flag explicitly;
it is not inferred from D0. Continue native view/viewport structure setup
from `$1306c`, followed by the remaining graphics allocation/assembly calls.
Do not confuse this bootstrap routine with the separately loaded title whose
destination is also `$13000`.
Both native view/viewport layout paths now continue through their ordered
bitmap, raster-port, viewport-build and merge returns. The first view alone
has the original load call; the second returns to `$12a7a -> $1330e`.
The caller's structures, shared 20-word palette and four plane pointers are
owned writes. The `$1330e` routine now performs both request-service calls
and transactionally transfers the hash-locked `$b000/$1800` original-media
span to `$1fe00`. Its ByteKiller routine is now translated natively with exact
input/output/checksum bounds; the resulting palette and 320x200x4 image are
written to owned memory, deinterleaved into the four original planes, and
continued through the typed `LoadRGB4` return to `$12a7e`. Continue the now
reached configured bootstrap/profile dispatch. The accepted palette return
now atomically publishes the hash-locked 320x200 Original frame through the
coordinator, host and SDL renderer, superseding the stale opening preview.
Library-generated list pointers are still
not inferred from return values, and the final `$6e0` decoded bytes remain
unlabelled until a consumer proves their role.
The unconfigured `$13000` graphics initialization remains a distinct local
call within the initial bootstrap track. Opening acquisition now retains that
entire genuine track (`ADF +$2c00`, 0x1600 bytes to `$12800`) in owned memory,
including the source `$12ff4` zero and the profile table. A real-media
regression checks every retained byte and the whole-track SHA-256; this proves
the boot-block load payload only, not its execution or any branch/service
result. Later mutable bootstrap state must still come from reached native
instructions and admitted service results.
The restart route
at `$217f6` now executes sound/flag reset and re-enters `$21276` using fresh
per-loop continuation state and effect identifiers. Continue verifying its
new ordered service returns without reusing previous observations.
The owned recurring main-stage framebuffer and accepted RGB4 palette now pass
through the coordinator, revocation-aware host and SDL renderer. A completed
buffer remains pending until the original LoadView/bit-5 visibility boundary;
tests cover four successive publications and genuine planar hashes. The
`LoadView` vector boundary also requires the exact counter-selected A1 view
pointer and owned A6 library base before accepting its typed return. It does
not simulate library effects or interpret raw return D0. The
reached outer control/input routes now have typed port, counter, and service
boundaries; do not create those observations in the host. The initial
`$21782/$21788` `$22bea` calls are already connected to the native descriptor
consumer. Opcode `$0b` still stages records outside that initialization path,
but the asynchronous `$224cc` caller cadence and channel state at each
consumer invocation remain unproven. Continue only with a caller-connected
trace of `$224cc`, its ordered invocation timing, the relevant `$22a20`,
`$22a16`, `$22a30` and AUDx state, and the DMA/register observations that
select the consumer branches. Do not run the consumer once per command or
frame, or assign playback-loop timing from descriptor writes.
The caller-connected ordinary, masked, saved-scanline, restored and `$fe`
sprite routes are implemented; do not invent another renderer for the
remaining asynchronous `$224cc` audio/control interrupt path. Do not reuse
unrelated observed bases or assign graphics, audio, scheduler, input, or
resource semantics.

The runtime now has a bounded main-stage deterministic driver. Each SDL pass
runs already-proven local transactions until the next typed external
observation, then stops idempotently. It never creates an Exec, graphics,
custom-register, counter, input, or media observation. A nonzero step cap
guards accidental local cycles, and every constituent transition retains its
copy/validate/commit rollback contract.
The SDL path now uses the encompassing deterministic session driver rather
than special-casing `$13000` and invoking the main-stage pump separately. It
reports an exact typed stop reason and boundary address across title load,
profile reload/re-entry, title-stage local work, and main-stage local work.

For every row, commit only source code, metadata, hashes, bounded offsets,
tests, and documentation. Keep raw captures, ROMs, original media, generated
pixels, and user saves outside the repository. When an item remains blocked,
record the exact missing observation in `PRESERVATION.md` and continue with the
next unblocked item.

### 2026-10-03 Millennium Atari Fcreate boundary

The typed `MILL22B.INF` Fopen return checks the dynamic RTS destination. Only
if that observed address is `$11fe6` does it follow the hash-identified local
prefix through GEMDOS selector `$3c` at `$11ff6`; otherwise it stops after the
typed Fopen result. The 18-byte continuation hash is
`352b6ca9a375e016e129667085dbd6fcd89d1e2eece201d75d97972c60b94cd4`; the
prepared `2200AD.PRG` argument frame is not a disk write, and no Fcreate
result is synthesized. A typed raw result can now cross the hash-bound
`$11ff8..$12003` suffix and store its D0.W value before returning to the
stack-derived caller address. A recorder-backed trace has not established
that `$11fe6` is the runtime destination. The supplied Equinox roots also
lack `2200AD.PRG`, so its Fcreate result, program bytes, and later gameplay
remain unresolved. Next, obtain the real returned PC/service result from an
admitted trace or locate hash-identified supplied media, then continue on
that path.

### 2026-10-05 Deuteros v16 no-input boundary

The bounded v16 observer adds only `$218cc` to the v15 raw-PC site set.
Its visible, realtime, 180-second no-input capture passes schema-24 receipt
verification, contains 896 raw-PC records, no host-input receipt, no title-
display receipt, and no `$218cc` sample. The PC observer and receipt grammar
are ready, but this run does not advance the title or gameplay state. Next,
obtain a visible physical-input capture linking the relevant guest route to
`$218cc` and display/state checkpoints before advancing native execution.

### 2026-10-05 Deuteros v17 physical-input attempt

The pinned v17 recorder was launched visibly on trv2 with a 30-second focus
settle and a 300-second realtime physical-input window. FS-UAE produced raw-PC
output, but no non-empty host-input receipt. The runner therefore rejected the
capture with `physical-input capture requires an observed non-empty host-input
receipt` before writing `run-status.txt`; this attempt is not admitted evidence
and establishes no game response or selector-cell value. Repeat only with
visible manual input in the VNC window, then require a complete receipt before
using any resulting observation.

The follow-up codex09 run used the same pinned v17 binary with a 120-second
focus settle and a 600-second realtime physical-input window. It reached the
active window and ran FS-UAE visibly, but again ended without a host-input
receipt and was rejected before `run-status.txt` was written. The only changed
condition was the longer visible window; no new game-state evidence was
produced.

### 2026-10-05 Millennium Atari media recheck

A metadata-only search across local Downloads, local `~/.projecteon`, trv2
Downloads and trv2 `~/.projecteon`, including archive member listings, found
no `2200AD.PRG`. The supplied alternate Equinox root contains `TDS.TOS` with
the exact active Equinox `MILENIUM.TOS` SHA-256
`4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686`; neither
root supplies `2200AD.PRG`. Do not substitute or synthesize the missing child
program; its Fcreate result, bytes and later gameplay remain unresolved.

### 2026-10-05 Millennium Atari bytecode boundary correction

Exact-media execution now follows only the first selector-3 macro command at
`$1decf`: bytes `16 01 06` store `$00081e01` at `$1ff66`, and execution stops
at the next token `$10` at `$1ded2` (dispatcher return `$20026`). The native
runtime now follows the full hash-bound 44-byte macro through its NUL, runs
the `$10`/`$11` handlers and positive-mode text raster from the actual
selector-4 word and typed `$5f00` screen source, then stops at selector-7
trap `$11ebc`. Next, obtain the external Crawcin return/branch observation and
continue the already hash-identified caller path. The test's patterned buffer
proves only mechanics. The Equinox disk-member test passes
when the original image is streamed directly from its user-supplied ZIP; no
media is extracted. The missing `2200AD.PRG` boundary still prevents claims of
Atari gameplay.


### 2026-10-05 Millennium DOS child image availability

A metadata-only Downloads/archive recheck confirms the English DOS package
contains `millennium-return-to-earth-2-2/2200AD.EXE`, 54,391 bytes, SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`. The
member streamed from the user-supplied archive matches the existing
`~/.projecteon` file byte for byte; no extraction or copy was performed. This
removes child-image absence as a static DOS blocker. Continue only from the
existing typed parent EXEC/process-entry path: dynamic private-driver returns
remain unobserved, so the executable's presence is not evidence of gameplay.
This DOS `.EXE` is not the missing Atari ST `2200AD.PRG`.

### 2026-10-05 Linux real-media CTest coverage

A Debug/Ninja build on trv2 compiled the current source snapshot. In direct-
media mode, with `EON_REAL_DATA_DIR` unset and `EON_DIRECT_DATA_DIR` pointing
to the existing `~/.projecteon`, CTest passed 21/21 tests, including split-container admission,
Deuteros Amiga worker scenarios, Millennium DOS video-driver profiles, DOS
title/runtime and MCGA function-six paths, and the DOS title EXEC entry. The
separate full-corpus mode cannot pass its scanner count because the available
recognized archive set is incomplete; its real platform-start test also reports the canonical
Millennium Amiga release (`2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400`)
and Atari ST release (`ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01`)
missing or substituted. The direct Millennium DOS directory and its title
EXEC path pass; no English archive or observed runtime from another platform
was substituted for the absent releases.

### Deuteros Amiga title selector dispatch

The title-stage session now consumes the captured selector passage followed
by exact ordered raw reads of `$1f98c` and `$1f98e`, validates the original
branch PCs, and reports the next local PC for each of the five static routes.
Native direct-media coverage tests all five mechanics and rejects missing or
misplaced secondary reads. Fixture values are synthetic test inputs, not
captured game state. The runtime facade now checks these bytes against any
already-owned memory and atomically admits them into guest memory, returning the
computed next PC. A verified physical-input capture now contains 31 input-linked
reads at `$1fbe6`, but it lacks the ordered `$1fe84 → $1fea8 → $1fe88` and
`$1fe92 → $1fea8 → $1fe96` helper-return passage required before those reads
can be admitted. A later visible physical-input run on 2026-10-06 passed the
language selector and Disk 2 prompt and displayed the main game interface.
Its verified schema-30 receipt linked three selector samples at `$1fbe6` to
input ordinals 22, 24 and 28; both selector cells remained zero. The late
post-input sidecar recorded three executions at `$1fc22` and none at `$1fc9c`.
This establishes zero-route reachability after physical input, not the
pattern/mask reads, destination writes, displayed playfield contents or
gameplay behavior. The first bounded capture was removed after receipt
verification to reclaim cache space; its full sidecars are no longer
available. A repeated 2026-10-06 run is now preserved under
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-v20-selector-preserved01/`;
its schema-30 receipt verifies and its sidecars retain the input chronology.
The repeated run again captured only three `$1fc22` hits, at input ordinals
18, 20 and 24, with no `$1fc9c` hit. Next, extend the bounded external
observer to record the zero-route's ordered chip-RAM operands and bitplane
writes, then capture the required helper returns and selector passage. Do not
enable native route writes until those exact ordered effects are observed.
Continue only along admitted paths while retaining the external service stop
at `$3fbf8`.
Millennium DOS still lacks an admitted private-INT driver return and live
function-six caller. Neither game's playability target is complete.

### 2026-10-07 Millennium DOS LOADNGO identity diagnostic

A fresh external no-input diagnostic on trv2 compiled the experimental
DOSBox-X observer and recorded complete `LOADNGO` identity for the first two
COM members. `MILL.COM` entered at `0812:0100`, and `TITLES.EXE` entered at
`0a8d:0100`; both full COM hashes match the separately hash-identified direct
media. Twelve ordered private-INT driver returns use the observed MILL.COM
segment `CS=0812`. Captures 08–10 subsequently showed that the saved `IP` was
truncated: the actual core fetch at the INT-6 stop is `CS=0e70:EIP=0x000a1900`,
linear `$b0000`, not a `0e70:1900` flat-file offset. Capture 10 traced the
first high EIP from `$fffe` to `$10000` in a 16-bit real-mode code segment,
then execution advanced beyond the recorded `$ffff` CS limit into the VGA
aperture. Capture 13 also records the preceding path: `TITLES.EXE+0x34`
executes `INT 93h` with a zero IVT target; bytes in the DOSBox-X INT-0 callback
at physical zero execute as `PUSHA; RETF $f000`, using preexisting `DI:SI` to
resume at `0e70:fffe`. This establishes the current emulator route and missing
INT-93 target, not the original handler or intended behavior. Do not synthesize
the vector or treat DOSBox's callback bytes as game logic. Raw sidecars, build,
and helper remain external under the trv2 cache; hashes, sizes, and bounded
metadata are in `PRESERVATION.md`. A hash-gated full linear disassembly of
`MILL.COM`, `TITLES.EXE`, `2200AD.EXE`, and `2200GX.EXE` now finds the GX
`INT 93h` wrapper at member offset `$51` in addition to the title and game
wrappers. Both supplied text drivers have a hash-verified, identical
31-byte INT-93 dispatcher and ten-entry AH table at `$27`; the dispatcher
uses absolute near offsets in the active CS and has no AH bounds check.
Their AH0/AH6 shared-RET routes and AH3/4/5/7/8/9 handler effects now have a
separate hash-bound typed profile/session and real-media test. The session
requires explicit driver-load and vector observations and is not connected to
runtime, because the captured TITLES route has not established those
observations. AH1/AH2 draw into VGA memory or EGA planes, whose visible results
are not established here. Exact member, dispatcher,
table, and handler hashes are in
`PRESERVATION.md`. The linear listing also
contains a candidate title-loader fragment:
it reads a mode value from INT 91h, selects a text-handler file, and reads it
through the far pointer at `CS:$011c`. An independent worklist from the
declared title entry does not reach the fragment's `$125c` Set-Vector site, so
this listing does not establish that the loader runs or installs a handler.
The supplied `VGATXT.BIN` (1,024 bytes; SHA-256
`c31cb760d5f62a21b3baf9c09a6be413514780bd88eeac0273620e81b5d69318`) has an
interrupt-shaped dispatch prefix and IRET, but no runtime handler contract is
proven. The diagnostic capture does not show whether the candidate loader ran.
A bounded
candidate graph from the title entry decoded 1,346 instructions but reported
the `$125c` Set-Vector site as unreached; its code/data and indirect-transfer
limitations are recorded in `PRESERVATION.md`. Next correlate the INT 91h
mode result, chosen file, read completion, Set-Vector result, and first INT
93h call in a reviewed interactive trace before implementing behavior. This
remains diagnostics-only and cannot satisfy either game's playability target.

### 2026-10-06 Deuteros v21 zero-route capture

The v21 visible capture now verifies one complete 147-record zero/zero planar
invocation from `$1fc22` through `$1fc9a`. Its receipt is retained at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-v21-physical04/`;
the zero-route sidecar SHA-256 is
`1290bcb23017c86d4f115b281a4cb0a8315efb42c9757803a09635e3379891ff` (54,723
bytes). The parser binds all rows to delivered host input and validates the
32 `$1fc74` intended byte writes against the `$1fc76` readbacks. The VNC view
reached the interface labelled “THE SUN”. Independent source and build review
against pinned FS-UAE v3.2.35 is complete: a rebuild from the exact observer
patch matches all 26 loaded ELF sections of the pinned binary byte-for-byte.
The normal Deuteros receipt verifier accepts this receipt without an
experimental-observer flag; that flag is not a Deuteros admission gate. The
recorder identity is now reviewed and pinned, while the capture remains
diagnostic and does not prove an accepted game action or playable state.

The physical04 run fixes two omissions found during live validation: the
runner now passes `PROJECT_EON_FS_UAE_ZERO_ROUTE_RECORD` to v21, and the
schema-31 grammar accepts selector-cell samples at `$1fc28` as well as the
start/entry sites. Focused parser and receipt tests pass. Review confirms the
existing owned runtime API already has a bounded, atomic zero/zero planar
write path via `observe_command_planar_write`; it derives only the 32
hash-bound route effects and the pointer update, and publishes a surface only
after memory admission. The separate generic variant API remains gated from
zero/zero without its own typed evidence. Reinspection of physical04 shows
input ordinal 26/frame 5031 traversing `$1fe7a`, both ordered `$1fea8` calls,
their `$00310000`/`$00300000` returns, `$1fbe6`, and `$1fc22`; this satisfies
the existing captured-selector-passage checkpoint but does not explain the
opaque input action or establish a screen/state change. The title-display
recorder had exhausted its 2,048-write budget by input ordinal 20. Next,
reserve an additional bounded display-register sidecar for later input-linked
writes, then use a visible capture to bind action and display/state change.
Keep selector and planar runtime checkpoints tied to their existing owned,
hash-bound APIs; do not claim general playability from these local passages.

Historical note: the Deuteros v21 external FS-UAE observer was initially
unexercised. It was subsequently run with visible manual input; see the
2026-10-06 capture section above for the verified diagnostic result and its
gameplay evidence boundary.

### 2026-10-06 local validation and remaining playability evidence

The current shared source snapshot configures and builds with CMake/Ninja on
macOS when the installed Xcode 26.5 SDK is selected explicitly. CTest passes
22/22 in direct-media mode against the existing `~/.projecteon` collection;
the full Python preservation suite passes 455 tests (6 skipped), and the
repository artifact verifier, Gitleaks, and `git diff --check` pass. Both the
Millennium DOS and Deuteros Amiga CLI launch checks report `READY`; these are
media/configuration checks, not playability evidence. No runtime fix is
justified by the current traces: Millennium still needs the first real INT
`$93` vector change and subsequent call/return context, while Deuteros needs
captured post-selector display state and ordered helper returns. Continue with
visible, physical-input captures and do not synthesize missing guest state.

On 2026-10-06 an external experimental DOSBox-X observer was built against
the pinned upstream commit. A clean x86-64 Linux build of the same patch was
completed on trv2 on 2026-10-07; its exact binary identity is recorded in
`MILLENNIUM_DOS_DOSBOX_X_RECORDER.md`, and the local runner/verifier now bind
the macOS and Linux binaries to separate hash/size identities under the same
experimental protocol. Its bounded sidecar can record the six exact
`MILL.COM` post-INT `$21` return sites and a digest of the read buffer at
`$0315`. The prototype has no reviewed v21 event/result/input hooks, and no
game run or capture has been performed. Next, use its explicitly experimental
runner mode for a visible, operator-closed diagnostic to distinguish
file-service returns from the later `INT 6` path. Compare any observed read
digest with the hash-identified original driver without retaining guest
bytes. This does not replace either game's capture-backed playable-state
requirement.

The later origin-observer candidate was rebuilt on trv2 with the exact
experimental identity recorded in `MILLENNIUM_DOS_DOSBOX_X_RECORDER.md` and
run visibly against the unchanged English archive
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`. The
window remained black with DOSBox-X title `TITLES`. Closing it spawned a
`zenity` confirmation helper that inherited the recorder console pipe; the
180-second runner timeout killed DOSBox-X but then waited indefinitely for
that helper to close the pipe. The retained external run
`/home/trv2/.cache/project-eon-tools/dos-origin-experiment-runs/origin-20261007-rebuilt02/`
has `failure_reason=timeout`, no success marker, and no raw receipt. The
runner now starts DOSBox-X in its own process group and kills the complete
group at timeout or console safety cap, so inherited UI helpers cannot strand
cleanup again. This corrects the failure mode, not the emulator's black
screen; no runtime or playability claim follows from the attempt.

Direct, non-materializing inspection of the hash-matched member found its
first word is `$1f0e`, not either DOS MZ signature. DOSBox-X's pinned loader
source classifies an unrecognised header as a COM image and chooses PSP:`$0100`
as its initial CS:IP. This is a source-path prediction, not an observation of
this run's EXEC parameters or a guest-image mapping. The observer's first
recorded fetch for CS `$0e70` was `$fffe`, not `$0100`; the origin observer
also does not retain the time or full instruction history between the DOS
loader handoff and that fetch. The next recorder increment should capture the
hash-validated DOS `TITLES.EXE` identity and exact `LOADNGO` CS:IP together
with the first `CS=$0e70` normal-core fetch, keeping their chronology explicit.

Source inspection identified DOSBox-X's default `[dosbox] quit warning=auto`
as the cause of the hidden zenity prompt. The experimental-only origin runner
now sets `quit warning=false` in its hash-bound generated configuration; the
general Millennium capture profile is unchanged. The manually closed retry
completed and its metadata/raw-record digest passed the updated verifier. It
records a last fetch at `0000:0001` (opcode `$ca`) before the first observed
fetch at `0e70:fffe` (opcode `$00`). Both addresses are outside the declared
flat candidate interval for the 7,022-byte `TITLES.EXE`, so those bytes are
not attributed to original code. This remains only last-fetch context: it does
not identify a DOS EXEC image, prove a causal transfer, or explain the black
screen. The next DOS boundary is an exact observation of the DOS EXEC load
image and entry context before that out-of-range fetch; no model may consume
the `$00` as an original instruction.

The 2026-10-06 guided v18 run again stayed on the animated Deuteros logo.
Its verified v28 receipt has 18 delivered host-input events but only 1,024
raw-PC samples, capped at eight sites; no selector-dispatch or title-display
artifact exists. The new operator prompt did not close the title-to-selector
evidence gap. Continue from a visibly reached language selector and capture
the ordered helper returns, dispatch read, and selector-cell values together;
do not add guessed runtime behavior.

### 2026-10-06 Deuteros v19 late-input recorder

The v19 FS-UAE executable is hash-pinned at 62,020,024 bytes. Its source patch
and archive-member comparison are documented in
`DEUTEROS_AMIGA_FS_UAE_RECORDER.md`. Receipt schema 29 and focused parser tests
cover distinct late-input samples and the joined selector-cell sidecar. The
visible v19 captures pass receipt verification. Early runs delivered Space,
Return, and short Fire clicks but remained on the animated logo. A held Fire
drag then produced Z=1 at `$21866` while action 33 was held; static bytes fall
through to a store of 1 at `$21720`, after which the language selector became
visible. The selector sample read cells `$1f98c=$00` and `$1f98e=$00`.

A follow-up run visibly reached the selector and delivered key `1` at input
ordinals 29/30. The following selector-dispatch sample (ordinal 30) has D0 low
byte `$20` and A0 `$1eed5`. Both selector cells were still `$00`. A source-span
check maps A0 to a zero-filled region, so the earlier prompt label is
withdrawn. The display later appeared black;
the capture does not prove that the prompt was drawn, that English was accepted,
or that the game reached a playable state. Its raw-PC, late-PC, dispatch and
display data remain external under
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261006-vnc-v19-language-confirm01/`.

The next Amiga step is to identify the bounded code and display writes between
that second dispatch and a visible disk prompt, then capture the ordered helper
returns and selector reads needed by native route admission. Do not synthesize
the prompt or advance a disk swap without visible, input-linked evidence.

Hash-locked disassembly of Disk 1 ADF offset `$7ac22` / runtime `$1fc22`
(256 bytes, SHA-256
`9f1ecf6524f3e88e4124cde39fed5a01fb9008a44871ea7907aa78c029f0ecd3`)
confirms that the zero/zero route subtracts `$20` from D0's low byte, indexes
an eight-byte pattern at `$1f99c`, and performs original four-plane merges
through `$1f974`. The v19 ordinal-30 dispatch sample has D0 low `$20`, A0
`$1eed5`, and both cells zero. This predicts the route only if the sampled
cells persist to the later tests; no `$1fc22` execution or plane write is
captured. Next recorder build should probe `$1fc22/$1fc9c` and retain the
ordered selector-helper/dispatch passage. Do not treat `$1eed5` as proof that
the prompt was displayed.

FS-UAE v23 and receipt schema 33 now add a bounded observer for the exact
main-stage store at `$21868` to `$21720`; the receipt verifies the fixed
instruction, 0→1 byte transition, and matching input frame. A visible
operator-driven attempt on trv2 ended with no host-input receipt and was
rejected, so it yields no new game evidence. The next Amiga step still needs a
human-driven capture beyond the selector. The v23 build and incomplete output
remain outside the repository and supplied media.

Millennium DOS still lacks an admitted private-INT driver return and live
function-six caller. Neither game's playability target is complete.

### 2026-10-07 Millennium DOS zero-context disassembly

The `TITLES.EXE` context around the V14 zero-byte history was re-disassembled
from the exact direct-media member, without extraction. Under the existing
flat COM-style `IP - 0x100` candidate mapping, the captured `$18e4..$1900`
range maps to file `+$17e4..+$1800`. Those title bytes are nonzero; `$18e4`
lands inside the instruction linearly decoded from `$18e3`, and `$1900` lands
on the final displacement byte of the instruction beginning at `$18fd`. The
same 30-byte file span in `2200AD.EXE` is zero-filled. Captures 08–10 later
proved that the exception stack's 16-bit IP did not identify the effective
fetch address, so this apparent match is retracted and does not support
mapping the observed fetch to `2200AD.EXE`.
The complete linear report and exact span hashes are recorded in
`PRESERVATION.md`; raw disassembly remains outside the repository. Captures
08–13 supersede the flat-offset candidate mapping: the effective fetch comes
from the null-INT-93 route described there, and no `2200AD.EXE` identity is
established for it. Next DOS work must locate the original INT-93 handler or
installer using a reviewed read-only observer. Under the retracted
`2200AD.EXE` candidate, the 15 zero-opcode pairs would decode as
`ADD byte ptr [BX+SI],AL`; this must not be treated as evidence that this image
was executing. Any further observer must retain bounded register,
effective-address, and pre/post-write evidence alongside full EXEC
image identity; PC samples alone cannot explain the path. The exact source
zero run is `2200AD.EXE+$124b..+$26c3` (5,241 bytes), with nonzero bytes
resuming at `+$26c4`; this is file structure, not runtime mapping evidence.
The reproducible direct-transfer scan tool finds no instruction-aligned
relative CALL/JMP target among the observed IPs; tests reject byte matches
that begin inside another instruction. This still leaves indirect transfers
and computed returns open.
The tool and focused unit tests are now in `tools/scan_dos_transfer_targets.py`
and `tests/test_dos_transfer_scan.py`; they revalidate the direct-media set
and selected file hashes and write mode-0600 metadata only under the external
cache. The reproduced empty result is hash-recorded in `PRESERVATION.md`. The
full Python suite passes 463 tests (6 skipped), repository artifact policy
passes, Gitleaks reports no leaks, and `git diff --check` passes. These static
checks refine the DOS fault hypothesis but do not advance either game's
verified playable state.
Do not infer executable identity or runtime behavior from zero-byte
compatibility, stack contents, or a candidate offset mapping.
Deuteros Amiga still needs the ordered selector/helper passage and a captured
state-changing control leading beyond the observed “THE SUN” interface. Both
games remain short of verified playability.

The follow-up `tools/analyze_dos_reachability.py` starts at `$0100` and `$d2b0`;
all six English DOS candidate addresses are reachable only in an
over-approximation with 24–34 assumed near-call returns and two assumed
`INT $91` returns. Neither entry has a direct-control-only route to those
sites. Hashes, route classes, and decoder limits are recorded in
`PRESERVATION.md`; the caller models remain unconnected to runtime.

The external CMake/Ninja build completed and CTest passed 22/22. The full
Python preservation suite passed 470 tests (6 skipped), repository artifact
policy and Gitleaks passed, and `git diff --check` is clean. Neither title has
yet produced verified gameplay evidence.

### 2026-10-07 Deuteros later-input display observation

Rechecking the v21 schema-31 capture found that input ordinal 26 reaches the
hash-profiled selector passage and `$1fc22`, with selector cells still zero.
The established title-display sidecar ended at ordinal 20 after using its
2,048-write budget, so it could not determine whether that later passage
changed display registers. FS-UAE v22 is now compiled and hash-pinned in the
external cache, adding an independently bounded display sidecar for writes
after ordinal 20. Schema 32 verifies its exact binary identity, register
allowlist, per-register and total caps, frame/ordinal links, and file hash.
Focused tests cover these constraints and a full synthetic schema-32 receipt;
the full Python suite passes 472 tests (6 skipped). The prior v21 physical
capture still verifies normally. A visible v22 no-input diagnostic at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-capture-20261007-v22-physical02/`
produced 1,024 raw-PC samples, all with input ordinal zero, then failed
preflight because no physical-input receipt or completed sidecars existed.
It supplies no new evidence of a game action or visible state change. Next,
run a visible physical-input capture with v22 and inspect later-input display
writes together with the selector route; do not infer gameplay from
display-register writes alone.

A new v22 no-input diagnostic now passes receipt verification after using
`LIBGL_ALWAYS_SOFTWARE=1` on trv2. It loads the recognised media and records
only the bounded no-input polling loops; it supplies no title or gameplay
evidence. The earlier locator miss came from trv2's stale helper copy; the
current pinned-hash locator finds the exact v22 binary. A 1,024-byte
hash-bound continuation disassembly also confirms the character-blit caller
and the zero-route helper. Source mapping retracts the earlier interpretation
of A0 `$1eed5` as static prompt text. The command parser reads its stream via
A4 at `$1fa0a`; static code sets table pointer `$1f97c=$1c482` and later
dispatches entry `1`, but no capture establishes that initialization or call.
The next visible trace should bind A4 and its bytes to glyph calls and display
writes. Continue with visible manual input only; never replace it with
scripted guest input.

### 2026-10-07 Deuteros Exec interrupt-server boundary

The hash-bound post-Exec call at `$204f4` uses Exec LVO `-$a8`, which the
Exec Autodocs identify as `AddIntServer(D0, A1)`. The call registers a handler
on a priority-ordered Paula interrupt chain; the handler then runs when its
interrupt is dispatched. This rules out treating the call as a no-op typed
return, but does not identify its actual D0 interrupt number, A1 structure,
registered code, or invocation cadence. Model this OS list and interrupt path
only after those operands and handler effects are tied to the admitted guest
state. No new gameplay behavior is admitted by the LVO identification.

### 2026-10-08 Deuteros VNC follow-up

The v23 physical-input capture initially ran on Xvnc `:6` without its required
`XAUTHORITY`; the emulator window was therefore not visible and the runner
rejected the no-input result. The capture helper now probes an available
`xdpyinfo` before mounting media, with focused tests for authenticated,
inaccessible, and headless displays. A retry with the correct auth file shows
the FS-UAE window in VNC, but the guest image remains black and no physical
input is delivered. The recorder reaches its sampled `$21866` poll with
input ordinal zero. FS-UAE's retained log confirms a Mesa llvmpipe OpenGL 4.5
context and a started emulation thread, so missing X11 authentication and a
failed GL context are no longer the leading display hypotheses. Preserve the
current evidence boundary: the 1,024-record raw-PC budget is full with 128
samples at each of eight sites, all in the input-zero phase; none are at the
allowlisted title-display or selector sites. Trace the startup/entry and
guest-side display setup before the repeating poll, then capture manual input
and a subsequent state/frame change. This finite sample set is not a global
reachability proof. No game-control meaning or playability is inferred from
reaching the poll.

The hash-bound secondary-input continuation now commits the sampled CIAA
`$bfe001` byte and `$21720` latch only after its bounded branch succeeds, so
failed paths cannot leak partial input state. Native assertions cover the
committed port value and the real-media selector routes; the focused real-media
CTest passes. The 23 tests independent of the two missing-platform corpora
also pass on trv2. This validates the bounded continuation only; a visible
input-driven state change and a playable Deuteros session remain unverified.

A separate normal FS-UAE visual troubleshooting session is running on trv2
VNC `:6` from `/home/trv2/.cache/project-eon-tools/vnc-troubleshoot-20261008-02/`.
The original disk archives and Kickstart are mounted read-only and their
contents match the recognised English release hashes. The window is present
on the X display; no operator input has been sent in this session. This normal
emulator display is not capture evidence. A read-only comparison of capture
07's title and late Copper streams maps action 149 to Space and finds that,
after a single palette-phase mismatch, the remaining 31 normalized display
passes match exactly; both selector cells remain zero at the last sample.
That narrows the evidence: Space did not produce a persistent Copper-program
transition in the sampled window, but the data does not prove whether another
game state changed or whether Space was accepted. Resume with visible manual
input and retain the instrumented v23/v24 recorder only when a physical-input
capture is being collected; no unattended no-input retry is useful here.

When launching FS-UAE with an explicit configuration file, pass the file as
the positional argument (`fs-uae /path/to/session.fs-uae`); `--config=...`
was ignored by the installed FS-UAE and opened its AROS fallback instead.
This host-launch detail was verified by the visible troubleshooting session
and is not game or capture evidence.

The normal FS-UAE log's `Illegal instruction: 4e7b at 00FC0564 -> 00FC0582`
message was checked against the exact mounted Kickstart 1.3 bytes. It is
consistent with the ROM's 68000 CPU-detection probe, which temporarily installs
an exception continuation, tries a 68010-only opcode, then restores the old
vectors and registers. This log line alone is not evidence of a crash: the
separate normal-session VNC snapshot shows the intro logo. The black image was
observed in separate v23 capture attempts and remains unexplained. Keep tracing
the guest startup before its repeating input poll, and continue to require a
visible input-linked state change before claiming Deuteros playability.

### 2026-10-08 Millennium DOS loader and INT 93h cross-reference

The exact English `TITLES.EXE` now has an external full linear candidate
listing and bounded instruction-start transfer scans. A file-selection/read
fragment at `$104a..$1082` reads a far pointer from `CS:$011c`; the candidate
installer at `$1247..$1265` uses DOS `GetVect`/`SetVect` for INT `$93` around
that pointer. The source bytes at `+$001c..+$001f` are zero. The listing records
`$1061 -> $1066` and `$1082 -> $1247`, but no edge proves entry into `$104a`
from the modeled `$1b80` startup. No handler identity or runtime pointer write
is established. See the matching hash-addressed evidence in
`PRESERVATION.md#2026-10-08-millennium-dos-int-93h-installer-disassembly-cross-check`.

The call-context scenario that assumes the recorded INT `$91` returns plus
named BIOS/DOS returns reaches the DOS memory request at `$1b2d` with
`AH=$48`, `BX=$fa00`; its carry, returned registers and memory effects are
unknown. Next DOS work must capture the exact EXEC child and the actual
allocation result, then trace who writes `CS:$011c` and which indirect/far
transfer reaches the candidate loader. Do not synthesize the DOS return,
handler vector, or child state.

The exact `MILL.COM` linear candidate also identifies DOS EXEC function
`$4b00` at `$0336`, called from `$0240` for the literal `TITLES.EXE` request
and conditionally from `$024c` for `2200ad.exe` after the first helper returns
AL=0. This is static caller/dataflow evidence only. The next recorder should
bind each site to the actual `DS:DX`, parameter block and hash-identified
child image, then capture the DOS return and caller re-entry. This links the
existing `LOADNGO` identity work to a specific runtime boundary only when
those values are observed; see the parent EXEC record in `PRESERVATION.md`.

The v23 Deuteros capture helper also had a schema-33 bookkeeping defect: it
activated the zero-route observer but omitted its status fields from the final
receipt. A pinned-recorder hash allowlist now drives both activation and
serialization. Visible-input captures now pass receipt verification and link
physical input through selector dispatch to a complete 147-sample zero-route
invocation; the route's 32 intended byte writes are each followed by a matching
readback. One run also records the latch write at `$21868` setting `$21720` to
1. Capture 07 records the F10 swapper request (host action 360), but its
retained FS-UAE log does not show Disk 2 insertion, and later display samples
are all from one frame. Disk acceptance and a changed playable state remain
unverified. Continue tracing an input-linked display/state change before
claiming Deuteros playability. The main-stage entry observation also carried
an unconstrained D0 field into a `$21704` memory write, while the reviewed
capture receipt did not record that register. D0 is now optional in the typed
observation; absence is rejected without a partial memory commit, and explicit
zero remains distinct from a missing value. The current receipt still cannot
advance this boundary on its own.

Capture 08 confirmed that its physical F10 event could not request Disk 2:
the loaded FS-UAE configuration omitted `keyboard_key_f10`, and its log shows
no Disk 2 insertion. The runner now binds F10 to
`action_drive_0_insert_floppy_1`; a follow-up startup run loaded that option but
had no manual input and therefore produced no admissible physical-input
receipt. The swap fix is configured but not yet verified end to end. Continue
from the visible intro with operator input; at the visible Disk 2 prompt,
verify the actual DF0 insertion and a linked changed screen/state before
claiming progress toward playability.

The next 10-minute VNC `:6` session (capture 10) also expired without any
host-input receipt. Its config contains the corrected F10 mapping, but logs
show only Disk 1 inserted. The runner rejected the capture before writing a
final status file, and the original media hashes remained unchanged. No guest
progress is established; a manual visible session from the intro through the
language selector and Disk 2 prompt remains necessary for this evidence path.
Capture 11 reopened the noVNC page visibly on port 6082 and confirmed the X
display was connected, but again had no host-input receipt at its 10-minute
deadline. Do not keep launching equivalent unattended sessions; resume this
path when physical operator input is available.

Capture 12 used the connected recorder display `:5` for its full 600-second
window but also received no physical input. Its 1,024-record raw-PC trace is
byte-identical to the earlier no-input poll trace, and the runner rejected it
without a final status receipt. The stale diagnostic VNC `:6` service was
reopened separately with normal FS-UAE and visibly reaches the intro, but that
session is not capture evidence. The instrumented recorder remains the only
admitted route for this evidence, and another no-input run is not useful.

A read-only hash-bound disassembly maps main-stage startup `$21734` through the
display-list setup and recurring `$217f6` update/input loop. Startup calls the
already-modeled `$21926` resource reader before `$21276` treats `$32a24` as a
record base. It probes a four-byte length into `$2ad24`, then uses that value
for the chunked payload transfer to `$32a24`; for admitted selectors 0/1, a
successful transfer overwrites the bytes that otherwise overlap a linear
candidate instruction at `$32a22`. Capture 12 lacks the entry D0 and transfer
results, so resolved records and rendered output remain unverified. Next trace
the runtime read returns, record/list values, and a guest-linked frame around
first entry and a manual input. Detailed hashes and uncertainty are in
`PRESERVATION.md`.

The exact experimental Millennium Linux loader-return observer has now been
restored on trv2 from the pinned upstream revision and patch; its executable
matches the documented hash/size. A first visible no-input run was rejected
because DOSBox-X flooded stderr with its known INT 6 loop. The capture config
now directs only emulator logs to `/dev/null` and disables the CPU log group;
the focused runner suite passes 28 tests. The next run showed no CPU log flood,
but still lacked the required manual window closure to finalize its sidecar.
It was rejected without `run-status.txt`, so it provides no DOS-return or
gameplay evidence. Capture 03 used the pinned experimental binary on VNC `:5`;
its observation window was ended with an X11 window-close operation that
destroyed the resource, and DOSBox-X continued emitting `BadWindow` errors until
the runner's shutdown bound expired. It produced neither
`driver-load-return.raw` nor `run-status.txt`; config SHA-256 is
`df87a5b6aa4bf51b54f81d94f0a6e30890a3888ca700f66f3f6a4279e9a2a4d1` and
bounded-console SHA-256 is
`07c89ec14a68c1063de06266079028a9194749985d553a01a6fc01dc1b2985b2`.
Capture 04 targeted the DOSBox-X client window, which advertises
`WM_DELETE_WINDOW`, but the close operation destroyed the resource instead of
delivering that client message. It was also rejected without the sidecar or
status receipt while the emulator continued X11 operations;
its config SHA-256 is
`a4ceb0642a07055428615b8180e02ffb2e2fe295bbc93ca0f6902e4bd26db1a9` and
bounded-console SHA-256 is
`93061440ca8260e2850d8601c8cc1edca8dbe579de5b8d86339436d9b2a53a77`.
Both attempts are rejected and add no gameplay evidence. A fifth no-input
attempt on the unchanged v1 binary added `quit warning=false` but still lacked
SDL2 `SDL_WINDOWEVENT_CLOSE` handling; it produced neither sidecar nor final
receipt. Its config/console hashes are
`5827277f4e93af7543e49621a46def3dbf3bece16c101fd07ed095c421d058f0` /
`fa8aa750ca8260b9ad4a9de0d6a94b74cae496fe3bc6fefac2777e070c0c86d3`.

An independently reviewed, separate v2 host-close patch and exact Linux
binary pin were then built. The runner and receipt verifier keep v2
experimental, separate from v1, and require `quit warning=false` in its
generated config. Capture 07 verified normal shutdown and sidecar integrity
after a real `WM_DELETE_WINDOW` message; the receipt remains diagnostic
provenance, not gameplay evidence. The candidate and its patch hash are documented in
`MILLENNIUM_DOS_DOSBOX_X_RECORDER.md`. Neither game's playability target is
complete.

The one bounded v2 diagnostic on trv2 display `:5` did not test its close
branch: `xdotool windowclose` destroyed the X11 resource instead of sending
`WM_DELETE_WINDOW`, leaving DOSBox-X to issue `BadWindow` requests. SDL did not
receive `SDL_WINDOWEVENT_CLOSE`. Capture 06 has no driver sidecar or final
receipt; config/console hashes are
`c282f49485874301c2f29b19f84013a433f4cae29ad5138689665449eb04b784` /
`db371049f5bda0d6124ae92ac1565a646f3078d81a9bd93e6bed3a60f5204ecf`.
The runner unmounted the archive, which remains unchanged. Capture 07 used a
real `WM_DELETE_WINDOW` client message. The DOSBox-X process exited normally
and its receipt passed the independent verifier with experimental opt-in.
The receipt hash is
`1c25be2f50c8e22670fd7653356356653290dccebc7cea2a034f0c9bd2f00a6d`; its
2,176-byte driver sidecar hash is
`77e301c8fe0d2678c8a9754ea3d157f069352bad3146c8f34a9f13b66668f180`.
It records 12 observations over six sites (two each) and zero host input.
This confirms recorder shutdown and diagnostic sidecar integrity only. The
receipt remains explicitly non-admitted for recovery; it supplies no DOS
success, guest progress, or gameplay evidence.
Neither game's playability target is complete.

An additional read-only digest comparison against the original archive
identified capture 07's two `$0315` guest-buffer snapshots as complete members:
`MCGA.BIN` (4,366 bytes, hash
`bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228`) and
`SIBM.DRV` (2,871 bytes, hash
`f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1`). The
observed allocation segments `$087e` and `$0990` accompany those reads. This
binds the two data buffers to supplied files for that no-input run; it does
not prove driver initialization or behavior. Metadata is recorded in
`PRESERVATION.md`.

The v24 recorder pin in the capture runner had a transposed digest substring;
it now matches the reviewed 62,039,936-byte binary and has an exact-hash
regression assertion. A 180-second visible VNC attempt on trv2 was rejected
because no physical input arrived. Its raw-PC hash is byte-identical to the
prior no-input trace and adds no game evidence. Do not repeat this capture
until operator input is available; details are in
`PRESERVATION.md#2026-10-08-deuteros-v24-visible-input-retry`.

The Millennium DOS context analyzer passes the `$1b44` `CS:[BX+2]` write only
in a named static scenario bound to the exact `TITLES.EXE` hash and the pinned
DOSBox-X successful-COM-loader model. That model sets `DS=SS=CS` and supplies
the complete initial SP interval `$1d6e..$fffe`; static `MOV BX,$010c` makes the
write target `$011e`, disjoint from all modeled active return words. This is
not an observed stack, general DOS guarantee, or evidence of AH=48 success.
Starting from COM entry `$0100`, the analysis follows `$0104->$1b80`, with the
captured INT 91h values and existing assumption-only BIOS/DOS returns
combined. It passes `$1b44`; the next stop is the third DOS `INT 21h/AH=48h`
at `$1b4f`. Its allocation result and effects remain unknown, and
`$104a/$1247` remain unreached. The report and derivation are recorded in
`PRESERVATION.md#2026-10-08-millennium-dosbox-x-com-stack-interval-scenario`.
Capture 07's sidecar itself still records SS:SP only for `MILL.COM`; its
observation boundary is documented in
`PRESERVATION.md#2026-10-08-millennium-dos-capture-07-stack-sidecar-boundary`.
An experimental DOSBox-X observer is now prepared specifically for the
`TITLES.EXE` call at `$1b4f` under protocol
`millennium-dos-ah48-site-1b4f-v2`. It has not been run against the guest; its
binary pin and site-only grammar are documented in
`PRESERVATION.md#2026-10-08-experimental-dos-ah48-site-1b4f-observer`. Next,
obtain a visible, manually operated capture, then use any observed handler
result to continue the bounded analysis. Keep helper calls and returned
allocation state opaque until captured.

### 2026-10-08 Deuteros profile-two re-entry

The profile-two entry's D0.W no longer waits for an unrecorded full-register
value. Hash-bound `$12b14` loads `$12a34.w`, which the selected profile-2
dispatcher sets to `2`; the return and `$20000 -> $21734` jump do not change
it. The runtime commits `$12a34.w=2`, verifies the owned 14-byte instruction
span at `$12b0e` against its hash, confirms `$21734` was reached, and stores `2`
at `$21704`. An optional full D0 observation is checked only for a matching
low word; its upper word remains unknown. Native tests cover the missing-D0
route, a contradictory snapshot, the owned instruction span, profile word and
`$21704` result. CMake/Ninja, CTest (23/23), Python preservation tests (519
passed, 6 skipped), repository-artifact audit, gitleaks, and `git diff --check`
pass. The next boundary is the typed `$2174a`
service return; no game action or playable state is yet proven. The exact
remote disassembly metadata is recorded in
`PRESERVATION.md#2026-10-08-deuteros-profile-two-re-entry-d0w-derivation`.

Follow-up inspection hash-locks the `$21734..$217a4` caller range and maps
`-$96/-$9c` to `SuperState/UserState` using the Amiga Developer CD autodoc.
The first D0 result is overwritten by literal `$7fff0` before `UserState`, and
the second result is overwritten when `$20068` starts. Neither D0 result is a
software-state dependency, although both calls have supervisor/stack side
effects which the native runtime does not emulate. Exact ordered call/return
observations remain required; Kickstart V33/34 `UserState` behavior needs
separate verification because the autodoc records a defect fixed by V1.31
SetPatch. Details and source hashes are in
`PRESERVATION.md#2026-10-08-deuteros-main-stage-exec-vector-semantics`.

The Deuteros negative-selector service gate is now statically mapped through
`$1fbfe`, `$1fc08`, `$3fbf8`, and the `$1fc12` skip path. The v25 recorder and
schema-35 verifier are prepared and pinned, but no guest capture has run.
Next evidence remains a visible, manually operated capture; do not claim the
service result or gameplay progress from synthetic harness records.

The native command session consumes schema-35 rows individually: dispatch,
branch, gate, service/skip, epilogue, and caller. Route, cycle order, selector,
and physical-input links remain attached at every phase. The command stream
stays blocked until caller `$1fac6`; tests cover both service and skip paths
and reject an attempted next command before caller. Harness rows are
synthetic. The physical route and resulting gameplay state remain unobserved.

A visible v25 attempt reached the language selector, but no manually delivered
input arrived in its 300-second window. The runner rejected it without a
host-input receipt, negative-selector sidecar, or final receipt. Do not reuse
its no-input trace as gameplay evidence; repeat only when a human operator can
control the visible noVNC window.

## Millennium DOS observed child continuation

The title EXEC contract now has a usable external route: admit the observed
child CS:IP after `$0336`, copy the exact hash-verified `TITLES.EXE` bytes to
that segment's `$0100` entry in owned runtime memory, then accept the exact
entry-prefix observation and stop at the first private INT `$91` result. The
native media test verifies this full image copy but uses synthetic observations.
No capture has exercised it. Next, obtain the pinned manual DOSBox-X recorder
receipt for successful EXEC, child CS:IP, and the `$0100 -> $1b80` prefix; then
continue only with observed private/DOS/BIOS results. The ordinary compatibility
path remains separately provenance-labeled.

### 2026-10-08 Millennium DOS allocation continuation

Using the exact direct-media `TITLES.EXE` on trv2, a new static scenario
assumes only a stack-preserving return from the fourth `AH=48h` call at
`$1bca`; all returned registers, flags, allocation and memory effects remain
unknown. The site-entry route reaches the next DOS interrupt, AH=`$49` at
`$1bd5`. In the composed analysis an alternate branch still stops at the
opaque stack-pointer write `$1c60`, with two software-interrupt and two
stack-write stop sites in total. This narrows the static route but proves no
DOS allocation, title initialization, game action or playability. The exact
span hashes, report hashes and remaining capture requirements are in
`PRESERVATION.md#2026-10-08-millennium-dos-fourth-allocation-site-follow-up`.

Next, extend the reviewed DOSBox-X observer to record `$1bca` and `$1bd5`
under a distinct protocol, then use a visible manually operated capture. Keep
the assumption-only analyzer scenario separate from admitted runtime evidence.
The newly hash-bound `$1ba5`/`$1c60` save/restore shape both uses `DS:$1aa0`;
the manual runtime route must observe DS, SP and that word at each site rather
than assume DS preservation or memory continuity across DOS calls. Existing
title-handoff v3 stores only offset/value for its `$1c60` row; it cannot bind
the effective address. Keep that historical grammar intact and version any
future runtime-admission trace to include the observed DS segment.
For this update, CMake/Ninja reported no pending build work; CTest passed 23/23,
the Python preservation suite passed 535 tests with 6 skipped, the repository
artifact audit passed, gitleaks found no leaks, and `git diff --check` passed.

The separate experimental DOSBox-X observer for `$1bca` (AH=`$48`) and `$1bd5`
(AH=`$49`) is now built and hash-pinned as
`millennium-dos-ah48-sites-1bca-1bd5-v3`. Its harness and parser tests pass;
the pre-IRET register snapshots remain raw diagnostics only. Next, run it only
through the visible, manually operated capture workflow, then use observed
registers and preservation-bounded memory observations to continue the DOS
startup path. Do not promote the protocol to recovery evidence or claim
playability from its existence; no guest capture has been made yet. Details
and the binary hash are in
`PRESERVATION.md#2026-10-08-experimental-dos-allocator-free-observer`.
The follow-up static pass confirms the AH=`$3d` helper at `$0549` saves and
restores DS/ES around its DOS call, but a composed assumption-only run still
stops at `$1c60`. The AH=`$0549` DS-preservation scenario was removed because
there is no runtime observation for it. SSH access was restored and the
external roots were inspected: no AH48 v3 run directory, receipt, or sidecar
was found in the searched capture/tool caches. The nearby driver-load run is
separate diagnostic evidence only; the v3 runtime result remains unavailable.
Details are in
`PRESERVATION.md#2026-10-08-millennium-dos-ah3dh-helper-cross-check`.

### 2026-10-08 Millennium DOS SetVect loop fix

The first title-runtime test exposed a non-advancing loop at the external
`$0239` SetVect result boundary. The compatibility runner now exits its local
automatic loop there, commits the staged startup state and returns the
boundary. The typed return requires the post-INT address `$023b` and the
observed INT 95h IVT tuple to point to the loaded driver. CF is not interpreted
as a SetVect status because the DOSBox-X implementation does not define it as
one. An initial CF-clear gate was removed after this source check. This
tightens the observation boundary but does not provide a captured DOS result.
The existing direct-media test supplies the IVT observation; the
direct title-runtime CTest passed in an earlier run. All 22 CTest entries that do
not require the incomplete archive corpus pass, including both games' current
direct-media startup checks.

Next, continue Millennium through the private INT `$91` result boundary and
Deuteros through observed Exec/graphics/callback returns and title input. The
CLI confirms runtime admission for both releases, but neither current startup
diagnostic proves gameplay. SSH key access and the visible trv2 VNC path have
since been verified for Deuteros. Millennium still needs its pinned, reviewed
DOSBox-X capture; do not infer results from static or synthetic observations.
Details are in
`PRESERVATION.md#2026-10-08-millennium-dos-setvect-compatibility-loop-boundary`.

### 2026-10-08 Deuteros callback response producer

The reviewed v27 FS-UAE recorder now captures bounded callback-entry and
pre-RTS snapshots. A verified visible run links the manual `1` selection and
Space press/release pairs to the original callback: it returns with `$31` and
`$20` respectively in `$1eec0` and pending count 1 in `$1eed6`. A later panel
input reaches the callback's distinct `$A0+$6 = $00ff` frame branch, but its
downstream graphics call and any visible effect remain unobserved. The capture
does not include a complete `$4077e` response-loop sequence or establish an
actionable game control. Continue by tracing the callback's `$20118` call,
capturing its bounded return and linked display window, then follow one
observed main response-loop pass. Full receipt hashes and the evidence limits
are in `DEUTEROS_AMIGA_TITLE_CAPTURE_STATUS.md`.

The v30/schema-39 service-return recorder fixes the reviewed stage-four caller
return gate and passed independent source/protocol review. Its unique binary
pin is `45b0fde4ffd354793c16cf72fa19891e08b9aee3f2d21b837ac40b46c3323877`.
A physical-input attempt on trv2 yielded no valid host-input receipt because
the VNC session displayed black while FS-UAE ran; it is rejected and supplies
no game evidence. Diagnose the visible display path before retrying. The
`$20118` return and any game action remain open boundaries.

### 2026-10-09 Deuteros software-rendered VNC and schema-39 receipt

The display fault is resolved for Xvnc `:6`: FS-UAE becomes visible in noVNC
port 6082 when launched with `LIBGL_ALWAYS_SOFTWARE=1`. A 15-second no-input
v30 run passed receipt verification, but establishes no input or game action.
The first diagnostic exposed that the v30 recorder hash was missing from the
runner's inherited zero-route allowlist; the hash is now included, and the
corrected fresh diagnostic receipt passes. The corrected-runner physical-input
attempt at
`/home/trv2/.cache/project-eon-captures/deuteros-amiga-playability-v30-service-return-20261009-03/`
received no physical input and was rejected without a final receipt. A second
600-second run used the corrected runner at
`/home/trv2/.cache/project-eon-captures/deuteros-amiga-playability-v30-service-return-20261009-04/`.
The title art was visible, but no physical input arrived; the runner rejected
the attempt without a host-input receipt or final schema-39 receipt. The
input-linked `$20118`/`$4077e` effects remain unverified. Do not repeat an idle
capture; resume when the visible VNC session can receive human input.

### 2026-10-09 Deuteros v30 visible input trace

The `:6` noVNC path visibly renders FS-UAE and accepts host-input deliveries.
A fresh, hash-verified schema-39 physical-input capture at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-playability-v30-20261009-01/`
contains ten host deliveries and six late-input PC samples through ordinal 10.
It did not reach the language/disk prompt, title callback, or targeted display
window. Continue only from a visibly confirmed prompt and capture the
callback's `$20118` return plus its linked post-input display. Millennium has
an experimental DOSBox-X observer on trv2; its result and limits are recorded
below.

A second fresh 600-second run at
`/home/trv2/.cache/project-eon-tools/deuteros-amiga-playability-v30-20261009-02/`
passed schema-39 receipt verification. Software rendering kept the animated
title visible, but Space and Return caused no visible transition and no
language/disk prompt appeared. It produced no callback or display sidecars;
playability remains unproven.

### 2026-10-09 Deuteros owned callback producer continuation

The v28/schema-37 callback snapshots support a bounded typed runtime path for
the callback's byte-one queue producer. The title-stage session accepts only
an ordered `$1f056` entry / `$1f14e` return pair with caller event byte 1, the
observed frame words, and complete before/after snapshots of the queue,
pending count, selector word and event mirror. It cross-checks the expected
table byte against the hash-locked ADF source and requires the before-image to
match owned runtime memory before atomically committing the four original
writes. This does not map a host key or infer an event meaning. The raw v28
sidecar stays external. Tests were not run for this source change.

The runtime now has a gated adapter for a complete v31 response-queue sequence
`$407b4` → `$1f230` → `$1f23e` → `$1f258` → `$407ba` → `$407e4`. It compares
the captured queue/count preimage with owned title memory and commits only the
hash-bound 20-byte shift and pending decrement. Its dedicated real-media test
uses constructed observations and does not stand in for a capture. No complete
input-linked live pass has been captured, so the caller's returned byte still
has no mapped event or gameplay effect. The callback's `$00ff` arm still stops
at `$20118`; the v30 service observer concerns a separate caller route and has
no valid input-linked service-return receipt for this callback. Next capture
one manual panel action with the complete ordered queue sequence and linked
pre/post state or display observations; then trace the returned byte through
the caller to a documented state and frame change. Do not infer a game action
from queue production or callback entry alone.

The schema-40 v31 capture now verifies after a visible noVNC run from the
language selector through the Data Disk prompt to the “THE SUN” panel. Its
callback snapshots link two visible panel clicks to `$1f056` / `$1f14e`, but
the finite trace contains no `$4077e` consumer sample or response-queue
sequence. Next, capture a visible panel action with a linked pre/post display
or state observation and the complete ordered
`$4077e`/`$407b4`/`$1f230`/`$1f23e`/`$1f258`/`$407ba`/`$407e4` pass; do not
infer a game action from callback entry alone.

### Millennium DOS: follow the caller after AH48/AH49

The experimental trv2 v6 observer supplies a verified SDL host-input
receipt, handler snapshots, and caller-after-IRET registers/FLAGS through the
title-library reads. The earlier AH48/AH49 scratch calls still fail, but
`TITLE.LIB` opens successfully when the original archive is mounted read-only
as a DOSBox-X CD-ROM drive. Nine reads return the full library, and visible
Space input advances from the title logo to the `OUTER SYSTEM` planetary
view. The prior black display was caused by the game's AL=`$02` read/write
open against a read-only directory mount. The capture is experimental and
does not establish all gameplay actions or parity; see `PRESERVATION.md` for
the exact identities and limits.

### 2026-10-09 Millennium DOS allocation-chain boundary

The trv2 schema-v4 static scenario advances the hash-matched TITLES path past
the BIOS palette interrupt and the helper's resize/allocate/free calls only
under explicitly stated return assumptions. Its `$1bca` AH48 and `$1bd5` AH49
sites are already observed in the v5 post-IRET capture: both return CF set
(`$0008` and `$0009` respectively). The static report adds no runtime result.
The later v6 capture loads `TITLE.LIB` and visibly reaches `OUTER SYSTEM`.
Continue from there by tracing the DOS executable/action-poll path, then bind
one visible gameplay action to its input and resulting state. Do not repeat an
idle capture. Full report identities and assumptions are in
`PRESERVATION.md`.

### 2026-10-12 Deuteros title input cross-check

The published Deuteros manual says the title accepts a right mouse click to
show the opening sequence and a left mouse click to skip directly to the game;
the keyboard is reserved for naming craft and saves. See the
[manual transcription](https://github-wiki-see.page/m/DeuterosX/DeuterosX/wiki/Deuteros:Manual),
which identifies itself as an edited transcription of the original manual.
The current trv2 FS-UAE process on X display `:6` uses a 640×512 window and
is still showing the animated logo. Exploratory left/right mouse clicks and
Space/Return/F10 delivered through the visible session and XTest did not
visibly change that process's screen; the process is no longer producing a
linked capture receipt, so these probes cannot locate the failed edge. This
does not show a general title-input failure: the earlier v31/schema-40 capture
`...-06` records a held Mouse1-left action (action 33, frames 3171–3322), a
linked `$21866` poll with `SR=$0004`, the `$21868` latch write, and execution
at `$218cc`; the visible route reached English selection and the Disk 2
prompt. Capture `...-07` then reached “THE SUN” after the ready-key input.
Those captures establish the earlier startup route, not a game action or a
reason the current process ignores XTest. Next compare its exact launch/runtime
state with the verified capture setup before extending the gameplay path; do
not repeat idle captures or infer behavior from the exploratory probes.

The schema-42 receipt now retains the `$20118` coordinate inputs, `$201b6`
registers, and bounded A0/A1 windows, and checks local coordinates against the
callback sample. A typed native adapter now validates that same ordered route
against Eon's owned title-stage RAM and commits only its observation sequence
number; it performs no original memory writes or graphics calls. The project
build now completes with the Xcode 26.6 toolchain and the pinned macOS 26.5
SDK in `/Users/bosse/.cache/project-eon-captures/build-playability-xcode`; the
older build directory still selects Command Line Tools linker inputs from the
macOS 27 SDK and fails at link time. The adapter is not backed by a live
schema-42 capture, so it establishes no input, rendered frame, or playable
action. Local read-only `--inspect` confirms the English Millennium DOS set
and Deuteros Amiga disk pair remain hash-verified under `~/.projecteon`.
Continue by connecting a verified manual capture to the adapter before
considering any runtime effect.

### Deuteros hash-bound callback source access

The title-stage session now exposes the exact 250-byte callback as a
read-only span only after checking it against the callback hash admitted by
the registration profile. This lets a future bounded interpreter consume the
original bytes directly without extracting or copying executable media. The
accessor does not invoke the callback or claim an Exec/input.device lifecycle.
Next compare the callback's instruction path against the bounded 68k executor
and identify the minimum supported memory ranges before connecting host input.

### Millennium MCGA title palette continuation

The exact mode-one BIOS call at `$046d` is now serviced internally only for
INT `$10`, AX=`$1010`, BX=`0..15`, the exact `$046f` return site, the matching
`TITLES.EXE` source byte address, and RGB6 components. The following `$0fd8`
AX=`$1012` block call is serviced only after verifying its exact ES:DX source
and 255 RGB6 triples in owned memory. The checkpoint records 271 ordered DAC
writes and leaves AX/FLAGS unknown. The active DOS compatibility driver then
continues to the first DOS vector result at `$10f4`. Regression coverage checks
the exact TITLES/TITLE.LIB source bytes. Next, connect this runtime DAC state
to presentation and continue the title/game handoff while keeping DOS
allocation provenance explicit. This does not prove a rendered screen or
gameplay.

### Millennium DOS vector setup

The two hash-bound title `INT 21h/AH=25h` sites at `$1106` and `$111d` now
install the title handler `$1124` into Eon's process-local IVT for vectors 0
and 4. Checkpoints label both writes as `eon_dos_compatibility_service`.
The preceding `AH=35h` GET calls at `$10f4` and `$110b` remain explicit
environment results; they are not replaced with zero or another default. The
next boundary is BIOS `INT 15h` at `$1ab9`. This advances deterministic DOS
setup only; original interrupt chaining and gameplay are still unproven.

### DOS IVT reads with unknown saved values

The two-word reads at `TITLES.EXE:$115d` (`0000:$0070`) and `$12ad`
(`0000:$0024`) can now advance with each source word explicitly marked
unknown. Their saved-vector destinations `$10dc/$10de` and `$1266/$1268` keep
the already loaded image bytes; the unknown source values are not replaced by
zero and are not committed. The hash-bound code then deterministically installs
its own INT 1Ch and INT 9 vectors. Real-media runtime coverage confirms those
installations and continues to the selected video setup path. This does not
model the prior handlers or their chaining. The next private `INT $91` result
remains a separate environment boundary, so this setup advance is not a
gameplay or playability claim.

The MCGA function-four return service also runs from the post-video mode
hook. Its native-memory transaction identifiers now include the TITLES
sequence as well as the loaded-driver generation, so the second call cannot
collide with the first call's committed stack and driver effects. A real-media
coordinator regression follows the post-video call with explicit synthetic
DOS vector returns and unknown saved IVT words, then confirms the second call
reaches the `$046d` BIOS palette boundary with FLAGS still unknown. The next
environment results and original interrupt chaining remain unresolved.

### Deuteros input queue scheduling boundary

The SDL-to-title callback commits only the callback's own bounded writes. It
leaves raw-key response bytes in the original queue; it no longer invokes the
separate `$407b4` consumer immediately after each host event. The consumer is a
game-code call whose scheduler entry has not been linked to a captured runtime
PC, so running it from the SDL event handler consumed events out of order and
could change `$407e6` without the game's caller reaching that path. The direct
consumer helper remains covered by source-based tests, but runtime queue
draining waits until an owned scheduler boundary can establish that the
original caller reached it. Tests check that `1` and comma input enqueue `$31`
and `$2c` while the control word remains untouched. This correction preserves
the input producer path; it does not establish visible actions.

### 2026-10-24 Deuteros `MoveSprite` ROM source map

The supplied Kickstart 1.3 r34.005 ROM's resident `graphics.library` vector
table maps LVO `-$1aa` to ROM entry `$fc9b24`; that stub calls the ROM body at
`$fced90`. The entry and body are hash-addressed in `PRESERVATION.md`. Static
analysis now identifies the local SimpleSprite coordinate writes and the
subsequent internal helper call, but the guest's live A6 vector mapping and
the routine's full viewport/sprite/hardware preimages are not owned by the
runtime. Keep the callback at `$201b6` until those effects can be executed
against bounded owned memory. This closes the code-identity question only; it
does not prove visible pointer movement or a playable action.

The recurring Deuteros `LoadRGB4` calls at `$21310/$2132a` now advance through
a bounded native service. It checks the owned call registers, copies the 16
source RGB4 entries into the owned ViewPort ColorMap, updates the SDL palette
after the second call, and carries no fabricated D0 return. This reaches the
ExecBase read at `$208b0`. The `$208b4/-$a8` call is `AddIntServer(5,
$2086a)`: its `NT_INTERRUPT` node has priority `$c4` and handler `$207ec`.
The exact handler decode is recorded in `PRESERVATION.md`. An explicit
`DeuterosAmigaVertbSession` now begins after the observed `$208b4->$208b8`
AddIntServer return. Each later supplied entry at `$207ec` runs a bounded
transaction over owned counter, flags, records, pointers, and—when enabled—the
hash-checked `$22bea` audio consumer. It returns ordered AUD/custom-register
intents and commits only its owned memory writes. This API creates no timer or
SDL cadence. Captured registration/interrupt delivery and the ROM CopperList
effects remain unrepresented; the native path is short of a verified
input-linked game action.

### 2026-10-24 Millennium game function-`$04` wrapper decompilation

Read the exact English `2200AD.EXE` in place and hash-addressed its 12-byte
`$0124` private-`INT $91` wrapper plus the two startup callers that request
function `$04` with `ES=CS`, `BX=$d19f`. This pins the wrapper's five saved
registers and its `INT`/`IRET`/`RET` shape. It does not supply the child stack's
live values, the `$d19f` runtime byte, or the private handler's return chain.
The existing function-`$04` continuation models `TITLES.EXE` and cannot be
reused for this game wrapper. Next, source-execute the game wrapper and driver
dispatch against a fully owned child image, stack, and loaded driver only
after those preimages and exact handler effects are established; until then,
retain the external boundary at `$0129`.

### 2026-10-24 Millennium game function-`$04` startup continuation

The active native process now executes the second startup call when its exact
English `2200AD.EXE`, observed INT `$91` vector, and owned EGA640/MCGA driver
image match. It dispatches the exact function-four leaf, applies only the
driver-state/MCGA handler writes, and records known stack words through the
caller helper boundary. Unknown SI/DI/BP/FLAGS remain absent. The MCGA route
stops before the local `$0455` RET; EGA stops before palette `INT $10` at
`$0476` (loaded by `$0473`). Focused real-media coverage was added but not run. This removes one
startup return boundary; it does not establish rendered or playable gameplay.
The MCGA DOS allocation return at `$d201` is a typed observation gate
requiring the exact child segment, INT `$21`, AH `$4a`, return IP `$d203`, and
increasing sequence. Its raw result advances only across the literal
`MOV BX,$fa00; MOV AH,$48` instructions to request boundary `$d208`; DOS
allocation effects and the later allocator return remain unmodeled.

### 2026-10-24 Deuteros `$3fbf8` owned-memory translation

The profile-five title loader reads ADF `+$79000`/`$55400` to runtime `$1e000`,
so runtime `$3fbf8` maps to ADF `+$9abf8`; its 184-byte body hash matches the
existing candidate. A bounded native C++ translation validates either the
`$1fc08` selector call or the `$40786 -> $3f7a8 -> $3fbf8` tail-call, including
return word, D0/D1, code, and required preimages in owned runtime memory, before
returning ordered effect batches. It remains unwired to `$4077e`: the
scheduler's active runtime reachability is still unproven. The
Millennium DOS path independently remains at `$d201` until the child block and
DOS allocation result have native provenance. Full local build and CTest passed
after repairing the new focused test targets; the Python suite passed 543
tests (six skipped), artifact policy, gitleaks, and `git diff --check` passed.

### 2026-10-24 native response and EGA palette continuations

Deuteros now has a one-pass native C++ model for the exact `$407b4` response
loop call, bounded `$1f238` queue helper and `$407ba` feedback tail. It checks
the loaded stage, exact call/helper/tail bytes, caller return on owned stack,
and a canonical nonempty queue before applying queue and control-word writes.
The helper maps to ADF `$7a238` (not `$7b238`). This pass remains disconnected
from the SDL callback scheduler, so it does not establish a delivered action.
An in-place hash-locked scan of the canonical Disk 1 title stage found no
direct branch, PC-relative/absolute call, LEA absolute pointer, or pointer
literal to `$4077e`. A runtime-computed/indirect call or another loaded region
must supply the missing native caller edge; keep queued SDL responses pending
until that edge is derived from owned state.

The follow-up static audit rules out fallthrough from `$40708..$4077e` because
its 118-byte ADF span at `$9b708` (SHA-256
`8d0f12daaae7ad14632bd358d5e214a40d15826322cfdb5fac51a8a2a1a851c2`) ends in
`RTS`. Three static indirect tables also do not contain `$4077e`: 16 words at
`$212b0` / ADF `$7c2b0` (32 bytes, SHA-256
`1e6458b695bd8f0ed58e33d7d55a1b78133026aaf4eeb9052f8e27fedf472a93`), 24 at
`$237d8` / ADF `$7e7d8` (48 bytes, SHA-256
`d7e5dd166e7d322becc40360cd02c8ae4684bfa9e94746247aa2eddc4bd80fb7`), and 20
at `$22d36` / ADF `$7dd36` (40 bytes, SHA-256
`7bf893cfb79d3f09280670b3a8ec41fb6f15113599987008b31c56324451a561`).
Remaining candidates depend on owned dynamic values, especially global pointer
`$222a2` and the A1-supplied table used at `$226b2`. Next, recover those values
at their source transition from owned memory/register state; do not call the
response consumer from SDL event receipt alone.

The nine source sites that appeared to write a dynamic D0/D1 value to
`$222a2` have now been checked in the exact canonical ADF. In every bounded
window the relevant register is explicitly cleared before the store, so none
is a dynamic `$4077e` callback producer. The source windows, ADF offsets, and
hashes are recorded in `PRESERVATION.md`. This closes those nine candidates;
it does not prove the runtime caller at `$223dc` is reachable or identify a
different indirect edge. Keep the response queue pending until an owned
instruction path supplies that edge.

Millennium's EGA function-four startup now has a native adapter session for
the 16 ordered BIOS palette requests from `$0476` through RET `$047c`. It
preserves unknown initial register contents and accepts only the hash-checked
request site, AX/BX values and increasing sequence. It does not yet connect
those writes to a rendered frame or establish the later game handoff. Full
CTest passed 27/27 after correcting the Deuteros helper's linear ADF offset.
The Python suite passed 543 tests (six skipped), repository artifact policy
passed, gitleaks found no leaks, and `git diff --check` was clean.

The MCGA startup's `$d201` `INT $21/AH=$4a` return is now retained as a raw
observation. The allocator continues with literal `MOV BX,$fa00; MOV AH,$48`
and reaches its next DOS request at `$d208`. The checkpoint stops at that
`INT $21/AH=$48` boundary; it does not infer allocation effects or reach the
later allocator return and `$d2ee` DX test. That return remains future work.

The native DOS observer now accepts an explicitly supplied `$d208`
`AH=$48` return only at the owned child CS and return IP `$d20a`. It preserves
that DOS result raw, applies only the verified `$d20a` store of returned BX to
`CS:$d12e` and `MOV ES,AX`, then exposes the next request at `$d213`
(`AH=$49`). It does not claim that DOS allocated or freed memory. The `$d213`
return is also typed at the exact child CS and return IP `$d215`; its local
continuation loads BX `$00a6` and AH `$48`, stopping at `$d21a`. The return is
retained raw and no DOS free effect is inferred. The `$d21a` return and its
carry-dependent successor are now decoded as a bounded branch. Its result is
retained raw and AX is stored only at the source-verified `CS:$d114` word. A
set carry stops at local `$d2ac`; clear carry publishes a typed
`INT $21/AH=$3d` open request at `$d1d4` (`DS=CS`, `DX=$100d`). Exact helper
bytes now gate observed open, seek-to-EOF (`AH=$42`, `AL=$02`), and close
(`AH=$3e`) returns. The observed low-word file size is rounded up to DOS
paragraphs, then the next `AH=$48` at `$d232` is exposed. Its result is stored
at `CS:$0116`; success reaches the next request `$d23e`, while carry stops at
`$d2ac`. The following file helper call is hash-checked and names `DX=$11c2`;
after its open/seek/close sequence the next allocator request is `$d251`. The
observed `$d251` result stores AX at `CS:$0118`, then the `$d25d` and `$d26d`
requests store their AX results at `CS:$010c` and `CS:$0110`; success reaches
the ninth allocator request at `$d27d`. Each subsequent `MOV AH,$48` keeps
the observed prior AL. The file-size rounding is 16-bit, including wrap.
The `$d27d` success path stores the same AX at `CS:$1044` and `CS:$1048`,
then reaches `$d294`; success there stores AX at `CS:$105a` before `$d2a4`.
Those two results branch to `$d2ac` on carry. The `$d2a4` return has no carry
branch: its AX is stored at `$d12a`, then the allocator returns with DX zero.
The caller stores AX at `$d128` and reaches its local call boundary `$d2f5`
(`$11161`). When that final allocator return is observed, the native process
now follows the bounded `$1161 → $053a` selector with the startup-owned
`$da05` byte, preserving its selected DX name pointer and stopping at DOS
`INT $21` `$054d` (`AH=$3d`, `AL=$02`). An externally observed successful
open result is retained at `$da00`; the hash-checked helper then exposes the
next DOS boundary, read at `$0580` (`AH=$3f`, `CX=$ffff`, `BX` from `$da00`,
`DS:DX` from the allocation pointer at `$011a`). Carry on open reaches the
existing file-error boundary. A successful observed read return at `$0582`
reaches close at `$05a2` (`AH=$3e`, `BX` from `$da00`), preserving the read
return's CX/DX and low AX byte; read carry reaches `$05a7`. No file bytes or
DOS result are invented. An observed successful close return now follows the
hash-checked caller to the next external boundary, INT `$21/AH=$35/AL=$93` at
`$4260`, where the original routine discovers the existing vector before its
later set-vector request. Its observed ES:BX result is stored at `$4257/$4255`
and the hash-checked continuation exposes INT `$21/AH=$25/AL=$93` at `$4271`,
with the destination segment loaded from the previously allocated `$011c`.
The native DOS return observation now reaches the SDL-facing `RuntimeHost` API
and is admitted through the title-handoff gate; the open return's `$da00` store
and vector-query stores are committed to owned child memory. The DOS vector-set
return at `$4273` is accepted only at that exact INT `$21/AH=$25` boundary and
retained as an external observation. Its checked local epilogue now leads to a
runtime-byte boundary at `$d2fd` for `CS:$0082`; the API verifies the byte against
owned child memory, stores `$AL-$30` at `CS:$0122`, and reaches the next local
call boundary at `$d305`. No byte is synthesized, and the next `$d07a`/INT `$95`
services are not yet executed. Every DOS result remains externally observed.

The focused real-media tests, including the new SIBM startup-profile test, pass.
Full CTest has 28 passing tests and two
corpus checks failing because the canonical release-count and Millennium Amiga
and Atari ST corpora are not available in this checkout. The Python preservation
suite passes 543 tests (six skipped); the repository artifact verifier,
gitleaks, and `git diff --check` pass. This is startup progress, not a playable
game or parity claim.

The DOS startup open at `$054d` selects among `VGATXT.BIN`, `EG3TXT.BIN`,
`EG6TXT.BIN`, and `TDYTXT.BIN` according to `$da05`; the read-only local
installation and English source archive contain only VGA and EG6. Do not
substitute those files for the two absent selector outcomes. The accepted
An in-memory locator scan found `EG3TXT.BIN` and `TDYTXT.BIN` only inside the
separate Spanish pre-installed archive (hashes and scope in
`PRESERVATION.md`); those Spanish bytes remain outside active runtime and do
not fill the English gaps. The accepted `AH=$25` return is linked statically
to caller `$d2f5` and continuation `$d2f8`.
The runtime requires an observed `CS:$0082` byte at `$d2fd`. The source stores
its `$30` subtraction to `DS:$0122`; the admitted selector success path restores
DS from CS at `$1197` and `$119e`, so Eon maps that write to the owned child
segment. It follows `$d305 → $d07a`, selects function-zero input `$0380` only
for mode byte 1 and `$0220` otherwise, and executes the hash-locked first-call
prefix through DOS `AH=$35/AL=$08` at `$0233`. Ordered raw return paths now
carry function zero through its `$0180/$0182` stores to the `AH=$25/AL=$08`
request at `$0246`, then stop at `$0248` after an ordered raw return. The
remaining `$d07a` code would dispatch function 0 through `$cf98`; it touches
speaker/PIT ports and installs a DOS timer vector. It then dispatches function `$0c` with size `$000f`, then
calls function `$0b` fourteen times through `$cfb9`, using the argument table at
`$d05c`. Function `$0c` makes four DOS allocations, initializes one buffer, and
returns failure without rolling back earlier allocations. An isolated native
translation exists for function `$0c`'s four paragraph allocations and the
fourteen exact function-`$0b`/`$08b3` VOC loads; it writes Eon-owned segmented
memory with original headers, block headers, decoded PCM, pointer tables and
loaded flags. Direct-media tests verify all voices and the function-`$0c`
AX=3 partial-allocation path. The active native startup accepts ordered raw
AH=$35 and AH=$25 returns through `$0248`, then stops before the guest stack
epilogue. Function `$0c` remains unreachable through the active process.
A source-locked native function-`$0e` PCM replacement now accepts the complete
startup checkpoint and all fourteen source-ordered voices. Its direct-media
test verifies start, replacement, invalid-index silence, and completion. The
player is still isolated from live function-call dispatch. Function `$05` is
the timer-stream interpreter and its event bytes dispatch PIT/speaker
callbacks. A playable native route still needs an admitted `$d0c9` caller's
registers and gate preimage, the lower-level voice loader after successful
initialization, and a host audio path. SDL PCM consumes the original decoded
VOC bytes but does not reproduce the PIT-driven waveform. Do not advance to `$d30d` until the
entire sequence has native behavior and tests; never substitute an invented
DOS-vector result.

The exact `$034d..$0423` SIBM timer-handler session models software countdown
and accumulator state after explicit native callback activation. The
generation-bound coordinator operation activates that service at the
`AH=$35` `$0233` boundary and loads all fourteen hash-verified voices into
Eon-owned storage. It publishes a separate native-service checkpoint. The DOS
process can advance to `$0246` and then `$0248` only after ordered raw return
observations; it does not install a guest vector or execute the stack epilogue.
`RuntimeHost` exposes
the observation entry point, but SDL does not synthesize or produce that byte
observation itself. A new owned-memory execution API can derive it directly
from the authenticated child `CS:$0082` cell at the exact `$d2fd` boundary and
assign the next sequence from coordinator state; its focused test verifies the
source byte, derived selector, missing-cell rejection, and wrong-boundary
rejection. No DOS scheduler tick currently reaches that boundary, so this API is
automatically chained only after the external `$4273` DOS return is accepted
and the hash-checked continuation publishes that exact local boundary. The
existing coordinator test does not reach `$4273` without adding external DOS
results, so that transition is verified at helper level and remains untested
end to end. SIBM function `$05` is the timer-stream initializer at `$05dc`,
but the static `2200AD.EXE` caller inventory has no function-`$05` caller.
The game does contain a distinct source-backed event route: 24 callers reach
`$d0c9`; its `$0121` byte gates a function-`$13` call, then nonzero caller AL
selects SIBM function `$0e` and zero selects `$10`. Although both installed
video drivers' function-`$13` leaf preserves BX, `$d0c9` clears BX after that
leaf, copies caller AH into BL, and clears AH.
The `$cfc7` wrapper pushes BX then AX before the function number, while the
driver's `[BP+4]` reads AX; therefore the `$0e` voice index is zero-extended
caller AL and caller BH is discarded. No admitted runtime state supplies the
entry registers, `$0121` preimage, and owned SIBM voice tables together at a
live `$d0c9` boundary. The new hash-bound
`evaluate_millennium_dos_game_sound_dispatch` helper derives that exact route
from explicit boundary observations and validates `$d0c9`, `$cfc7`, `$cfd4`,
and `$cf8f`; direct-media tests cover AL indices, gate/return ordering and
source mutation. `MillenniumDosSibmAudioPlayer::start_voice_from_dispatch`
now accepts only a function-`$0e` result and routes it to the matching original
PCM voice; direct-media coverage checks voice 3's first sample and proves a
function-`$10` route does not replace it. These fixture observations do not
invoke INT `$95`, provide a live DOS caller, or reach SDL. The next step is
connecting authenticated `$d0c9` state to owned SIBM memory and the native audio
clock; do not synthesize a voice from SDL input. Host clock chaining and timer
callback behavior remain open, so this is not playable gameplay or a
release/parity claim.

### Native parity blockers confirmed 2026-10-24

The post-overlay DOS loop now has a bounded Eon console queue for physical
F1–F10 input. It emits the documented extended-key two-poll byte sequence
only when the exact `$d3db -> $0f05` caller return is admitted; the following
hash-bound AL test and `$3b..$44` dispatch choose the matching table index.
This removes the missing host-byte adapter for that narrow boundary, but no
live native process currently advances the loop from SDL. The original DOS
empty-queue return remains unspecified; SDL input does not infer it. The next
playability work is to own the deterministic loop calls and DOS poll in the
native process, then recover the game handler's service and display effects.

Millennium's current native SIBM service loads the fourteen verified voices
and models a separate Eon timer callback. The guest process accepts ordered
raw returns through SIBM `$0248`, but lacks the saved caller ES and outer INT
`$95` stack frame for the guest epilogue. Do not publish `$d30d` from that
service: the intervening caller and DOS effects are not modeled. `$d30d` is
DOS `AH=$48` with `BX=$fa00`; the
carry, AX, and ES:BX outcome must be observed and generation-bound before the
following `$49` free and native continuation can be implemented. Clock
chaining and the timer callback remain open. The game has a source-backed
event route through `$d0c9` and SIBM function `$0e`; although function `$13`
preserves BX, `$d0c9` then clears BX, so the voice index is zero-extended
caller AL. The live caller registers and voice index are still missing, so
native dispatch cannot yet be connected. Millennium is not yet playable.

`MillenniumDosSibmFunctionZeroSession` consumes the sequenced AH=$35/AL=$08
return at SIBM `$0233`, records raw AX/BX/ES, and derives the decoded
`$0180/$0182` stores. The active coordinator accepts it only from the loaded
SIBM code segment and generation-bound native service, applies the stores to
owned driver memory after source-preimage checks, and publishes the next AH=$25
request at `$0246`. It also admits an ordered raw AH=$25 return from that same
segment and stops at `$0248` before `POP DS`. It does not install a guest DOS
vector or claim a successful vector result. Capture-backed reachability and
the later caller continuation remain open.

The SIBM service takes its code segment from the authenticated `MILL.COM`
load and requires the exact IBM-speaker leaf at Eon's compatibility INT `$95`
vector. Linear disassembly shows `2200AD`'s child `CS:$011c` is used by a
separate INT `$93` path, so it must not identify SIBM. The service reserves
the exact voice-table paragraph span in that same compatibility arena, then
suballocates and commits its source-validated driver, table and voice bytes
into Eon's native DOS memory map. The process can advance through `$0248` only
with ordered raw DOS return observations; it still lacks a caller continuation
for `$d07a`. The previous timer handler must be represented by Eon's clock
service rather than guessed as an original DOS address. `$d30d` also depends on the earlier DOS calls and their
source-ordered effects, not just the service's successful voice load.
The hash-bound caller tail then stores returned BX at `CS:$d130`, sets ES from
AH=$48's AX, frees that ES segment at `$d318`, reloads DS:DX and DS:SI from
`CS:$1042`, and calls `$6bf2` at `$d32f` with AX=`$0a00`. Those caller-side
instructions are known; both DOS results and the data reached through
`CS:$1042` remain unmodeled. Preserve the `$0233` boundary until their inputs,
memory ownership, and DOS return flags/registers have an admitted native
source. Keep `$0248` as the first unexecuted SIBM instruction until the saved
caller ES and outer interrupt frame are owned or explicitly modeled.

For Deuteros, the follow-up static dataflow audit found no `$4077e` destination
in the populated `$226b2` A1 dispatch records; the `$222a2` pointer starts at
zero and takes its `$36a8c` fallback. Runtime PC and pointer provenance at the
indirect dispatch remain needed before the pending SDL response can be consumed
by the original response loop. Do not connect the loop from SDL input alone.
`DeuterosAmigaTitleComputedDispatchSession` now validates the hash-identified
`$223c4..$223de` caller sequence, reads the pointer from owned memory, and
reports the zero fallback or a source-backed in-stage dynamic target. The
observation API is available through the runtime coordinator, native session
controller, and runtime host, after the title-stage session is active. It does
not advance the caller or synthesize the observation; the native scheduler
still has no step that reaches this indirect call. The real-media tests prove
dispatch arithmetic and lifecycle rejection, including a fixture overlay of
`$4077e`. Next derive this observation from the real native path and advance
only a target whose entry is source-verified.

The Deuteros native outer-input fixture now isolates held left-mouse/fire
input from right-mouse input. At the hash-bound CIAA read `$2185e`, the active-
low fire sample changes the owned latch `$21720` from zero to one; the fixture
then advances the original scheduler and frame-buffer/view path. This is a
source-backed state change candidate, not a user-facing action or proven
input-caused pixel change. The coordinator assertion is compiled in
`eon-native-tests`, but its enclosing launch fixture runs only after the full
canonical six-release archive scan. `~/.projecteon` does contain English
Millennium Amiga ADF candidates and the two Electric Dreams Atari STX disks;
however, it does not contain the exact canonical outer archives required by
that fixture. The 29 passing direct-media/unit CTests do not include this
enclosing scenario. Next verify a no-input comparison through the same native
route and establish whether the resulting frame visibly differs before
claiming playability.

Static execution of `$2185e..$21892` sharpens that comparison: with fire held,
the active-low read falls through `$21866` and sets `$21720`; with fire
released, `BNE.B $21870` leaves the latch unchanged. Both paths then test
`$2171e`, and mode `$14` runs its record only when both latch bytes are
nonzero. This confirms a source-level input branch, not its visible result. A
paired native regression now replays this exact secondary-input span from one
identical owned-memory preimage for fire released and held. It proves the latch
and opposite `$21866` branch outcomes while reaching the same scheduler
address for an odd counter. With the fixture's even counter and nonzero
`$2126a` mode word, the held-fire branch proceeds through `$21982` and reaches
`$222ac` (fade service boundary), while release returns to `$2181c`; this
reproduces the original `$21704` selector normalization and fade gate. That
is a source-derived control-flow difference, not proof that the fade service
returned or changed pixels. The SDL frame loop currently
supplies the `$21822`, `$2185e`, `$218be`, and `$217e4` input boundaries, but not
the resulting `$222ac/$dff01f` bit-5 read. The source records this as a raw
hardware observation. The Amiga Hardware Reference Manual identifies this as
the `INTREQR.VERTB` request at vertical-blank start, but the native runtime does
not yet model interrupt-request delivery or the original monitor standard.
Although SDL VSync is available, it is a presentation preference and the main
loop explicitly keeps recovered game timing independent of host presentation;
it is not evidence that the original request was pending or acknowledged. Do
not synthesize a set bit from input or host frame cadence. The next gate is to
establish a source-supported native VERTB producer and interrupt-clear model,
then admit the fade return and compare cloned states under matched display
inputs.

The title path stops earlier at `$204f4` before an `AddIntServer` call using
node `$204c0` and handler pointer `$202ca`; its separate typed observation still
requires ExecBase and the returned D0/SR values. The title-handoff image loaded
at `$13000` overlays the main-stage bytes at this address. The corrected
hash-bound span is ADF `+$7b2ca` to runtime `$202ca` (698 bytes, SHA-256
`e49fd6cf6d44759bdf59448b163e2d6903beaffdd5da4a8609842cd1d242bd3d`).
Control-flow decoding confirms its wrapper saves all data/address registers,
calls `$202dc`, restores them, sets D0=0, and returns. The body branches to
`$20584`, which overlaps the linear decode at `$20580`, and calls several
game routines. Bounded slices show `$20584` is a conditional early return,
`$79e86` lands on an RTS stub, `$41a68/$41b24` copy three four-byte groups
with destination stride `$1f40` across 16 iterations using stage globals and
a lookup table, and `$3fdac` updates Paula
channel registers from channel records. `$40358` advances event state and has
a conditional `$3fbf8` call. Their live branch conditions, global pointers,
buffer ownership, and device-service effects remain unknown. These findings
are recorded with source offsets and slice hashes in `docs/PRESERVATION.md`;
they do not justify invoking the handler or scheduling it from host frame
timing. It is distinct from the main-stage `$2086a/$207ec` server. The earlier `$5aca` report was based on the wrong
overlapping load and is retracted for handler semantics. Corrected external
report: `~/.cache/project-eon-analysis/deuteros-amiga-addintserver-20261024/title-overlay-handler-202ca.md`
(SHA-256 `17ad9fef850b62cef982f47bd840651bb059495262f860995e434708dc8f4705`,
174 lines).

The distinct recurring-loop path also stops at `$208b4` for its own
`AddIntServer` node `$2086a` and handler `$207ec`. Its compatibility API and
VERTB entry API are tested through the runtime facade, but the SDL launch loop
calls neither. Neither registration may be auto-completed from the other's
profile. Next model the corrected `$202ca` handler's source-backed state and
library-service boundaries, then derive native registration and interrupt
dispatch for each verified server; do not treat an invented service return or
cadence as a fix.

The same linear candidate disassembly resolves the continuation after that
gate. At `$21982`, `CMP.B #2,D0` sends values below two through the
`$21704 := 1` normalization and into `$218cc`; value two enters `$218cc`
unchanged; values above two return to `$2181c`. The native route's `$21380`
continuation is its scheduler entry for that return. If `$218cc` proceeds,
`$218e2` waits for the timer cell to change, stores that result at `$12fe4`,
then `$21904` increments `$21704`: selector two reaches `$21a4c`, selector
three reaches `$219f8`, and the ordinary resource path starts at `$21926`.
These exact static edges help target the next native boundary, but do not prove
that live fire input reaches the path or changes the rendered frame. The
listing remains external at
`~/.cache/project-eon-analysis/deuteros-next-playability-20261024/deuteros-amiga-clean-loaded-spans.md`
(source-bound report SHA-256
`6c3390c40d8c5127fe5644845d6de2f50245b36c50c4de95abbd5e1f61f3bf9e`,
classified linear candidate).

The raw-mouse callback's local clamp commits `$1ffc8/$1ffce` before its
`MoveSprite` stop at `$201b6`. The typed MoveSprite validator now compares
delta/bounds with the callback inputs and checks the computed clamped
postimage in owned memory, so it no longer treats valid native writes as a
stale preimage or applies deltas twice. This closes a call-validation gap,
not the external graphics.library effects or input-to-visible-cursor route.
The observation also has to match the owned library base loaded at `$12fec`.
Next recover the required ViewPort/SimpleSprite and library state before
implementing or scheduling `MoveSprite`; keep the service return explicit.

The Millennium DOS post-overlay loop now commits the exact `$d3be` XOR effect
on guest `CS:$07f9` into owned DOS-segmented runtime memory. This closes a
state-propagation gap between the hash-bound loop model and subsequent native
continuations; it does not prove public launch reachability or visible
gameplay. The full CTest suite passes in direct-media mode (29/29) after
leaving the incomplete canonical archive corpus unset. The canonical-data
configuration fails because the complete canonical release corpus is absent.
The English Millennium Amiga Defjam direct container
(`ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd`) and
Atari ST Equinox direct container
(`0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69`) are
present and recognized from `~/.projecteon`; each currently reaches only its
hash-bound bootstrap profile, with gameplay unrecovered. No release is
substituted for another. The action-`$0b` helper entry now validates and carries the same
owned `$07f9` byte into its hash-bound game prefix. Next finish that native
action's bounded helper/return effects, then establish public launch
reachability and verify rendered output. The `$0666` helper's literal
`CS:$05c8 := 0` now reaches the child DOS memory image after the `$0669`
segment read is observed and before the table/helper call path continues. This
preserves source order and is not a gameplay-state claim. Deuteros still needs
a source-backed input-to-action edge and a visible state comparison. The
`$0678` far table word is now retained in its exact observed DOS segment and
offset, so the selected `SI` pointer is available in owned memory. The
caller-connected `$05f7` F7 program is now decompiled from all five NUL-terminated
fragments and its parent’s `$da**` numeric formatting path. The parser
`parse_millennium_dos_f7_presentation_program` admits only the exact English
`2200AD.EXE` and `2200AD4.BIN`, verifies the caller and interpreter code-span
hashes plus every fragment, and returns typed operations without executing
private INT `$93` calls or changing original bytes. Build succeeded and
`native-data-detection` passed with its new mutation-rejection and operation
shape assertions. Next connect these typed operations to the native
presentation/runtime path, prove the INT `$93` results, and supply the live
dynamic values from owned memory. This parser alone does not display the
program; every glyph run still needs its stable localized game-text key before
it is presented. Keep all other opcodes and records as explicit boundary
cases until source-connected.

The English DOS startup's `INT $21/AH=$3f` at `$0580` now commits the complete
selected text leaf into its exact allocated `DS:DX` buffer after a successful
source-ordered open/read request. It resolves only the original VGA and EG6
leaves by their admitted SHA-256 values, requires the observed byte count to
match the file, checks that the destination segment equals the owned `$011c`
allocator result, and rejects any conflicting pre-existing bytes. The bytes
come from `VerifiedReleaseMedia::borrow`; no file is unpacked or written to
disk. The admitted direct set lacks the EG3 and TDY leaves, so those selectors
remain explicit boundaries without mode substitution. This supplies native
text memory for the following `$93` setup but does not establish that dispatch,
rendering, or gameplay succeeds. The loader now installs the hash-verified
selected driver at INT `$93`, but no ordered post-install handler call has been
observed and there is no native VGA-aperture renderer. Do not synthesize an
INT `$93` request from the parsed text program. The next required evidence is
the first actual handler call with its installed segment, AH/AL/DX, cursor,
page, and color state; then model only the observed bounded writes.

The action-`$0b` continuation at `$0669` reads the data segment from the
hash-bound child cell `CS:$0116`. Before accepting a supplied read value, the
runtime now checks it against the native DOS-segmented word when that word is
already owned. A partial owned word or a disagreement is rejected; an entirely
unowned cell remains an explicit observation boundary. This protects the
following `$0678` table lookup from bypassing existing process state. The
English `$101a` loader now has a separate source-ordered typed path through the
shared DOS `$0580` read return, committing only hash-verified `2200AD4.BIN`
bytes to the already-owned segment from `$0116`. The table word at `$0254`
therefore exists in owned memory for the later `$0678` lookup when these
observations occur in order. Static code and tests establish the path, but no
actual runtime observation of `$0678` is recorded. Next capture source-ordered
runtime evidence there, then connect the typed presentation operations to
native rendering and public input. Private INT `$93` results and gameplay
remain open boundaries.

### 2026-10-24 Deuteros `MoveSprite` helper disassembly

`tools/analyze_deuteros_movesprite_rom.py` now reproduces a bounded
Kickstart 1.3 r34.005 disassembly on trv2. It reads the pinned ROM member in
memory, writes its raw listing only beneath the external analysis cache, and
records span hashes and offsets in `docs/PRESERVATION.md`. The helper follows
ViewPort and SimpleSprite fields plus indexed tables rooted at graphics
library offsets `$26` and `$d0`; this confirms why the current callback cannot
be replaced with a coordinate-only write. The callback still stops at
`$201b6`. Next establish those library tables and all touched objects in owned
memory from source-ordered initialization before implementing the external
call. The disassembly does not establish live pointers, hardware effects, or a
playable action.

### 2026-10-24 Deuteros VERTB source boundary

The clean Disk 1 handler `$207ec` contains no INTREQ-clear write. Its
registration at `$208b4` and actual interrupt dispatch remain unobserved; the
main stage polls INTREQR bit 5 at `$216f2` after `LoadView`. The ADF and
`ViewPort.Modes=$4000` do not establish PAL versus NTSC or a native periodic
request producer. The exact source pass and hashes are in `docs/PRESERVATION.md`;
the external listing stays under trv2's analysis cache. Next require an
admitted server-registration/dispatch observation and a source-backed monitor
selection plus request-clear model before connecting this to the SDL frame
loop. Do not infer VERTB from host VSync or assume 50/60 Hz.

`evaluate_deuteros_amiga_vertb_wait_scenarios` now checks the hash-bound
`$216f2` poll over a private sparse clone and reports only labelled synthetic
bit-clear/bit-set branches. It does not alter owned state or produce runtime
evidence. The remaining real facts are the ordered `$208b4->$208b8`
registration return, registered `$207ec` entry with `$2079e` counter
preimage/result, `$216ee/-$de->$216f2` `LoadView` call/return with owned A1/A6,
and actual ordered `$dff01f` bit-5 samples.

### 2026-10-24 Deuteros callback-pointer store values

The nine previously unresolved dynamic stores to `$222a2` are now source-
resolved from bounded contexts in the canonical loaded Disk 1 stage. Eight
write D0 immediately after `MOVEQ #0,D0`; the ninth writes D1 immediately
after `MOVEQ #0,D1`. Their spans, ADF offsets, and individual hashes are
recorded in `PRESERVATION.md`; the external context report is
`~/.cache/project-eon-analysis/deuteros-response-loop-20261024/callback-pointer-store-contexts-20261024.txt`.
Thus those sites do not install `$4077e`, which narrows the missing
`$4077e` entry to another caller or an unlisted pointer writer. Next continue
from the owned runtime callback dispatch and identify its actual selected
target; do not consume the title response queue based only on SDL input.

The same direct-reference check was run read-only against the complete
canonical English Disk 1 and Disk 2 ADFs on trv2. Neither image contains an
absolute-long JSR, JMP, LEA, or unaligned pointer literal to `$4077e`. This
closes only those byte-pattern candidates; it does not prove that Disk 2 code
is loaded at title time or rule out computed/system-region transfers. Continue
from owned runtime PC and pointer provenance rather than broadening static
guesses.

### 2026-10-24 Millennium native display boundary

The first EGA640/MCGA `INT $10/AH=$0f` query (`$1d6`/`$1f4`) only stores the
returned `AL`; the later verification query (`$1e2`/`$0200`) uses `AL` for
mode selection and `CX` for the count path. The IBM BIOS reference does not
define portable `CX`, `DX`, or FLAGS outputs. Do not feed the original BIOS
observation session from SDL's canvas state. The next admitted step requires
the exact ordered BIOS return values, especially verification `AL/CX` and
their raw register/FLAGS provenance; a separate Eon-owned display service
must remain independent of that boundary.

### 2026-10-24 Millennium action-$0b owned helper operands

The source-locked `$0666` shared helper now has a native continuation for its
two reads when the required words already exist in `NativeRuntimeMemory`:
CS:`$0116` selects the data segment and the caller's AX selects the far table
word at DS:`AX*2`. The continuation checks both bytes of each word, executes
the proven CS:`$05c8 := 0` write as an atomic memory transaction, and stops at
the unresolved `$067b -> $05f7` call. Its transaction identity includes the
memory revision so repeated action-$0b entries in one process generation do
not collide. Direct-media tests cover the exact table offset, repeated entry,
missing and partial words, and unchanged state after rejection. The coordinator
now invokes this continuation at the exact action-$0b helper entry when its
operands are complete in owned memory; missing operands retain the explicit
observation boundary. Public SDL input still does not reach this entry, the
`$05f7` helper remains unresolved, and no rendered gameplay is established.
Next route a verified public action to this boundary, then continue only
through `$05f7` effects whose code, stack, and destination bytes are admitted.

### 2026-10-24 Kickstart VERTB registration source

Read-only analysis of the exact Kickstart 1.3 r34.005 ROM on trv2 resolves
Exec LVO `-$a8` to `$fc1210` (64-byte routine hash
`7690322fd581dfbee86bc9ea64b5f36a3e6caf9c68c5163e131d8d4d27c4f757`). It
inserts the supplied node by priority, enables its requested INTENA source,
and returns no registration status. Exec initialization creates the five
server-chain headers including VERTB bit 5. The ROM contains a timer.device
registration call for bit 5, but that does not prove timer.device started or
the game registered its own `$207ec` handler. ROM graphics reads of VPOSR and
INTREQR do not resolve the selected monitor or establish interrupt cadence.
The archive/member identities, vector-table/init hashes, report hashes, and
uncertainty are recorded in `PRESERVATION.md`; both generated reports remain
outside the repository in trv2's scoped analysis cache. This refines the OS
source boundary but does not admit runtime registration or VERTB edges. Keep
the game loop observation-driven until those runtime values are available.

### 2026-10-24 Deuteros save-disk media search

Read-only inspection of both supplied archives labeled `Deuteros ... [save
disk]` found that their sole 901,120-byte `.adf` members begin with `MSCF`, not
an Amiga `DOS\0` or custom `DEU\0` header. In memory, the CAB header declares
one folder and 25 Windows Internet Connection Wizard files, including
`ICWCONN1.exe` and `ICWDIAL.dll`; nothing was extracted. Both archive and member
hashes plus this preservation boundary are recorded in `PRESERVATION.md`.
These are mislabeled catalogue false positives, not real game saves or a
route around the missing native title-stage services. Continue from the clean
hash-identified game media and require source-proven service state.

### 2026-10-24 Deuteros title-handoff search probe

On trv2, the hash-identified English release was run through the bounded
native CLI diagnostic with synthetic `opening_input_held=1` solely to find the
next real source boundary. The probe reached the title handoff after 82 of the
requested 1024 ticks (4 palette events, 6 sound events, one alternate resource)
and the native continuation advanced two steps to the external
`OpenLibrary("graphics.library", 0)` call at `$1ed8c`/vector `-$228`. This
confirms the current executable route and its next stop, but it is not a
captured original input or a gameplay/parity result. The required real library
return and subsequent graphics base remain external runtime data; do not feed
the synthetic probe value into the game session.

### 2026-10-24 Millennium Atari save-disk follow-up

The supplied Electric Dreams `Disk 2 of 2 [a savegames]` archive contains
four structurally valid FAT12 `2200SAVE.*` chains. `MILL22B.INF` from the
Equinox executable names all four slots and its bounded native reader consumes
exactly 7,313 bytes, checking marker `$81` at offset `$155`; this establishes
the file interface, not cross-release pairing or meaningful state validity.
The hash-bound `$1c392..$1c4ad` original consumer reads exactly 7,313 bytes
and its sole pre-copy data check is marker `$81` at offset `$155`. Alternate
saves I–III meet both conditions and the same disk's `MILL22E.INF` and
`MILL22F.INF` match Equinox hashes exactly. A new read-only adapter now admits
only those exact disk, executable, consumer, slot, and payload hashes and
produces the original 7,313 ordered memory writes; the real-media test passes
all three slots and rejects IV (7,168 bytes). Its direct test independently
checks all 7,313 source-byte-to-destination effects, order, width, and byte
value for each admitted slot, including exactly-once payload coverage. This
verifies the effect plan, not execution by the live game. The adapter is not
yet connected to a caller in the native game session. A new exact caller map
identifies four slot callbacks at `$1c50c/$1c512/$1c51a/$1c522`; they dispatch
through `$1c526` to `$1c342` when `$165e8` selects the save reader, then return to
`$1c546`. The native runtime does not yet reach these code addresses or own
the live `$165e8` value. Next admit that actual caller state, apply the save
batch at the selected slot boundary, and continue to a verified state/frame.
No playability claim is made. Hashes and offsets are in `PRESERVATION.md`; no
media was extracted or changed.

### 2026-10-24 Deuteros callback target prefixes

The canonical Disk 1 `$222a2` reference candidates were decoded from the
original ADF in memory. This refines the callback candidates from their store
sites to seven exact target entries. Their initial code clears `$222a2` before
continuing into source-backed local state routines; none directly selects
`$4077e`. This still does not produce an owned `$223dc` runtime instruction or
show where the response consumer is called. Next trace the actual source-ordered
caller path from the loaded title scheduler into the selected callback, using
owned PC, stack, pointer, and response-queue bytes. Do not schedule a callback
or dequeue input from SDL alone. Prefix spans and uncertainty are recorded in
`PRESERVATION.md`.

The three candidate targets in the fallback title hitbox table have now been
decoded to their local returns: `$228f8` reads a state-selected byte and sets
bit 31; `$2294e` and `$229da` select distinct record tables, update `$22296`,
and return bit-30-tagged pointers. This narrows the next capture payload to the
mask, selected index, exact record, `$22296` before/after, and returned D0 at
the ordered `$226b2` call. It does not establish that the candidate table is
active or that these handlers produce visible actions.

### 2026-10-24 Millennium Atari STX loader entry

The supplied English Electric Dreams Disk 1 STX now yields its original
`EXEC.TOS` PRG only when both divergent FAT copies resolve the same complete
chain and bytes. Its relocation table marks the copy-source pointer and both
filenames, so the `$77000` stub reads from the relocated image base plus `$1c`.
The stub then opens `MILL22A.INF`, requests up to `$20000` bytes into buffer
`$2a500`, and calls `$2a500`; the file hash matches the already admitted
Equinox payload. The following source path opens `MILL22B.INF`. The 512-byte sector
has a consistent BPB but fails the TOS boot checksum, so this is not a
bootable-sector entry. The stub copies 514 bytes from TEXT offset `$1c`, so
only 158 bytes are inside initialized TEXT and the next read is at load-base +
`$ba`; the remaining 356 requested bytes are outside the segment. The
original cluster's post-EOF zeros do not establish TOS process memory. A
reusable hash-bound analyzer
and synthetic transport tests now preserve the physical image and write
listings only to the external analysis cache. Next identify the STX-specific
launch route and admitted process-memory preimage at the first tail read,
then join its loaded `MILL22A.INF` entry to the already recovered native
Equinox path. No runtime handoff or playable state is yet established; hashes,
offsets, and uncertainty are in `PRESERVATION.md`.

The matching Disk 2 is now independently identified in place. Its BPB
geometry agrees with the 80-track/720-sector physical STX layout, and both
FAT copies agree on the complete `MILL22E.INF` and `MILL22F.INF` chains. The
ordered two-container candidate identity is
`c895b150d2102f235cac08fd10c59b47f43bac4ab55d98da7253b7a47b244279`; it is
not registered or connected to runtime. Next resolve the STX-specific first
launch and disk-switch/file-open path, then admit only source- and
runtime-supported handoffs into the native Millennium Atari session. File
presence and a set digest do not prove the second disk is requested or
gameplay is reachable.

The exact Disk 1 `EXEC.TOS` is a 221-byte PRG (SHA-256
`e8ecdb43275ecf1a69d159a4c7a170b2462dd4a430fb09804f7bd0e9823627a5`), whose
bounded static sequence copies 257 words to `$77000`, opens and reads
`MILL22A.INF` at `$2a500`, calls that buffer, then opens and reads
`MILL22B.INF` at `$11e00`. Disk 1 resources A-D and Disk 2 resources E-F
hash-match the already analyzed Equinox resources, so the remaining difference
is the physical two-disk launch/service path, not those six resource payloads.
`AtariStStxPairedFileSource` now retains both original physical images and
resolves files through the selected root's per-file FAT-consensus read. A
real-media CTest opens the two hash-pinned archives independently and verifies
the launcher plus one resource from each disk without flattening either image.
It also parses the hash-pinned `EXEC.TOS` with the bounded Atari PRG parser and
checks the TEXT/DATA/BSS bound and relocation count. Next connect that source
to a runtime-owned read-only GEMDOS provider and admit the exact `EXEC.TOS`
entry only when its post-text process-memory bytes and ordered GEMDOS results
are sourced; matching resource hashes alone do not prove those execution
inputs or establish playability. The listing is external at
`~/.cache/project-eon-analysis/millennium-atari-stx-exec-20261025/exec-tos.md`.

The first `EXEC.TOS` operation is now source-bounded without assuming a TOS
stack or basepage. A typed session admits the exact Disk 1 PRG at Eon's native
load base `$10000`, applies only its three hash-verified relocations, and
executes the original local copy loop through its 79 initialized words. It
stops before reading `$100ba`, the first byte beyond the `$ba`-byte initialized
TEXT image; 356 further requested bytes remain unsupported. The real-media
STX test applied the image and exact 158-byte prefix into owned native memory
and confirmed the next source byte and destination tail remain absent. This
does not enter GEMDOS or the game module. Next continue from this typed
frontier only when the post-text process-memory preimage and stack are sourced,
then connect the paired read-only GEMDOS service. No TOS values are inferred.

The canonical Disk 1 source also yields one concrete fallback-table candidate
at `$21550`: three hit regions map to transform selectors `$10..$12`, whose
hash-identified records call `$228f8/$2294e/$229da`. This is a source-derived
candidate map, not proof the fallback table is live. A hash-locked title-stage
profile records the ordinary nine-row table at `$21f28` and its source pointer
store at `$210fa`. The canonical English ADF bytes are
`23fc00021f28000222b2`, SHA-256
`c96d8c22592d3ed8f572f68c2c94e446eb54a19d65d7e7cfed7f724ed157d1a5`; the
previously recorded digest was wrong. The source path does not prove startup
ran or the pointer was live at the loop. Next establish the owned
`$222b2==$21f28` at `$225ce`, consume owned cursor words `$1ffc8/$1ffce`, and
only then route a click to a handler. The parser now exposes the exact nine
half-open hitbox rows and `$54` action words in a typed profile array, covered
against the clean English Disk 1 bytes by a real-media test. This is source
data, not a click result; the existing SDL path commits the callback's owned
mouse-coordinate/button writes but still does not run `$225ce` after input.
The literal `$210d4` at ADF `$7c2d0` is dispatch index 8 from base `$212b0`,
but the complete table extent and live selector are unproven. The ordinary
hit-test action `$54` indexes cell `$21400`, which holds `$20e08`; that
hash-identified local prefix reads the selected row index `$222a0`, calls
`$20e18`, then reaches `JSR $1fb9a` at `$20e6a` with `D0=$f9` before later
jumping to `$36a8c` (details in `PRESERVATION.md`). This is the source-level
handler route for a table hit; the title-stage parser exposes its action,
cell, 98-byte local prefix hash, and first service boundary as a typed profile,
covered by the real-media native test. It is not evidence that native SDL
reaches it or that the service return is known.
The captured `$210d4` opcode `$51c8` is resolved to the earlier
main-stage load (`ADF $68d4`, runtime `$210d4`, `DBRA D0`); the later
title-stage load overlays that address with `JSR $36a8c` from ADF `$7c0d4`.
That title-stage path continues through `$33d2e` before storing `$21f28` at
`$222b2`, but its ordered service returns are not owned yet. Establish those
returns and the active selector before executing either route.

The exact `$33d2e` entry is now resolved from the canonical ADF bytes at
`+$8ed2e`: `LEA.L $33d26,A6; JMP $201d2` (12 bytes, SHA-256
`a594d712557e7b8ed8d42c1fb70de7717f3230b0a66076901f7f22a4e131aa38`). This
replaces the earlier linear decode that started at `$33d26` mid-stream. The
96-byte title caller prefix at ADF `+$7c0d4` hashes to
`664bd394bf8618da304f54ed056b575cf6af9f3aedcc3fbb8606f398995b7b25`; it
calls `$36a8c`, `$41a10`, `$33d2e`, then `$41170` with D0=2, followed by
`$41bb4`, before storing `$21f28` at `$222b2`. The `$41170` wrapper loads one
six-byte palette record and calls graphics.library `LoadRGB4` with A0=`$12e12`,
A1=`$1ed24`, D0=8. Its caller ignores the return value, but the ViewPort,
graphics base and LoadRGB4 effects are not owned at this runtime boundary; the
ordered title-init execution and the later `$225ce` scheduler remain unproven.
This sharpens the static path without authorizing synthesized graphics state.

The corresponding mask writers are now identified in the real Disk 1
overlay: `$2fd44` sets the `$13024` mode mask, `$2fbb0` creates a 28-byte
`$1aca6` record and sets the `$1303a` mask, and `$2fc58` creates a 34-byte
`$1bc66` record and sets the `$13050` mask. Their exact source spans and
hashes are in `PRESERVATION.md`; listings remain in the external analysis
cache. The next trace should link one such ordered state write to the active
hitbox test, then follow its tagged return through `$22484` to the already
modeled command/parser boundaries. The static writers do not prove an actual
title click or accepted action.

The exact `$22668..$226c9` continuation confirms that a changed hitbox result
returns through `$226b2`, updates `$22292`, and branches to `$22484`. That
dispatcher sends bit-31 results to `$1ff08`, bit-30 results through
`$1fa00/$1fb9a/$1ff08`, and bit-29 results through `$1fb9a/$1fe56`. These
command/display paths have existing source-bound sessions, but their command
session is currently admitted at startup call `$404f8`; it does not yet admit
this repeated `$22486` call. Next add a generation-safe re-entry for the
hash-bound loop call, and test it with explicit ordered scenario inputs. This
will make the existing parser available at the title boundary without
pretending that a real click or command stream has been captured.

The real English Disk 1 closes one source-data uncertainty for that work:
table base `$1c482` entry 1 resolves to candidate stream `$1c7c3` (ADF
`+$777c3`), whose bounded hashes are recorded in `PRESERVATION.md`. Both
startup `$404f6` and loop `$22486` pass D0=`1` to `$1fb9a`, selecting that same
candidate. Bind the next parser generation to this hash-addressed source
window while still requiring runtime-owned `$22292` and the loop's exact
`$2248c` return; the static mapping does not prove either was executed.
The native post-load profile now verifies these source windows when present
and exposes the candidate without rejecting other title-stage media whose
table bytes differ. The runtime still requires ordered pointer, word and
command-byte observations. The session compares observed opcode bytes in this
candidate's first 32-byte window with the original ADF before advancing. The
coordinator also rejects any dispatch-base, table-word or opcode value that
conflicts with corresponding owned memory, and previews/applies the parser-mode
byte write before committing the table-word observation. Missing owned bytes
are still an observation boundary, not proof of a completed source copy. Next
establish source-to-owned-byte correspondence and give each exact loop call a
fresh parser generation while requiring owned `$22292` and the `$2248c` return.

The current command object also shares pending operand, repeat, high-table,
two-operand and call-return state across that one startup generation. Opcode
zero completes specifically at `$404fe`. Do not implement re-entry by clearing
only the halt flag or reusing the startup return. Give each loop invocation its
own parser-generation state, require the owned `$22292` value and exact
`$22486->$1fb9a`/`$2248c` continuation, and preserve the startup contract.

### Deuteros Amiga Disk 2 briefing data

The hash-admitted Disk 2 contains 21 German `Forschungsleiter.` briefings in
resource bytes beginning at ADF `+$13400`; the bounded source window and
hashes are recorded in `PRESERVATION.md`. The source Disk 1 lacks those phrases.
Do not expose them as localized game strings until the Disk 2 read, record
selection, visible-text caller, and language-selection relationship are
source-connected. Next trace the caller that selects the Disk 2 resource and
identify the exact record bytes and destination buffer at its first text
consumer. The current locator establishes real source bytes and candidate
offsets; it does not make a briefing visible or prove that English mode uses
these German records.

### Read-only original-media search

`tools/locate_real_game_media.py` now accepts directory roots and recursively
scans ZIP archives in place, so `Downloads/` and `~/.projecteon` can be searched
without manually naming each archive. A run over both roots found 35 archives
with bounded disk-image members (17 ADF candidates, 16 Atari ST images, and 2
STX images); the JSONL report remains under the external analysis cache. The
Atari results include the two already identified English Millennium STX disks
and several alternative Deuteros ST disk images. The locator now resolves
standard Atari ST FAT12 roots in memory and reports hash-addressed files only
when both FAT copies agree on the complete chain. It found the same 221-byte
startup PRG and `MILL22A.INF` on the Robocop and 4AFC Millennium disks, but
each has a different `MILL22B.INF` from the clean Equinox release. Next
establish their release identity before any parser or runtime admission; do
not substitute these resources into the clean path. No original media was
copied or written, and locator output is not execution evidence.

The Replicants Deuteros Disk 1 variants `a49f509b…e65fc8` and
`5def9740…76714f` now pass the bounded bootstrap session: both share the
canonical track-70 and track-2 hashes, and the exact state-1 and state-5
variant hashes preserve their one-byte difference at disk offset `$93baa`.
The session's focused native test mode read each original member in memory;
no media was extracted. The release scanner and native-code parser manifest
still admit only the canonical split pair, so these variants do not yet
produce launchable release cards. The matching `[a2]` Disk 2 has 2,064 sectors
and nonstandard 12-sector geometry; the matching `[a]` Disk 2 has a different
boot hash and does not match the current KILLER_BOOT profile. Its new
hash-bound boot-sector candidate reaches three `TRAP #14` sites, one with
selector word `$0009`. Source stack decoding and TOS-HYP confirm a conditional
Protobt followed by Flopwr of sector 1/track 0/side 0/count 1 from `_dskbufp`
to the callback's floppy drive. The actual buffer contents, branch and
service return remain unobserved. Next trace the `_dskbufp` comparison and
callback drive value; any native write needs an in-memory or private overlay.
Classify `[a2]` geometry separately, then register only a complete, verified
pair with its own container-set identity and parser ranges.

### Millennium Atari game-init source correction

The native path previously substituted `MILL22B.INF` at the `$2a634` Fopen
call. The literal in the loaded A module is `MILL22D.INF`. Its exact 18,428
bytes are now authenticated and read into `$2c24a`; the file supplies header
words `$0050/$80c8` and token `$48`. Runtime prefix and token checks now use
that actual buffer. Continue the 320x200 native decode, then connect the
resulting screen memory and palette to the native presentation path.

### Deuteros response-loop caller search

The hash-locked search of the canonical loaded title stage found no direct
M68000 transfer to `$4077e`. A bytewise scan of all 11 Deuteros ADF images in
the local original ZIP collection found no absolute-long `$4077e` literal;
the same images also have no contiguous 24-bit encoding, and Disk 2 is
negative as well. The read-only archive scanner and its synthetic marker test
are reproducible. This narrows the search to computed/relocated or
system-installed control flow, without claiming that the response loop is
unreachable. Next follow the source-ordered native computed dispatch from its
owned PC and pointer; do not turn the synthetic `$4077e` fixture into a live
game callback.

### Atari STX original-media search

The read-only media locator now parses bounded STX physical sectors in memory
and reports root-file hashes only when both FAT copies agree on each file's
complete chain. A synthetic directory fixture confirmed that this search can
locate a named file; it is not runtime evidence. Running the locator on
trv2's original archives found the real Millennium Disk 1 `EXEC.TOS` and
resources A–D, and Disk 2 resources E–F. The Atari roots contain no
`2200AD.PRG`; that real absence keeps the existing one-disk GEMDOS path from
launching. The two-disk release instead enters through `EXEC.TOS`, so next
connect that exact PRG and paired read-only file source to a source-supported
native TOS startup path. No original file was extracted or copied.

### Millennium DOS post-overlay console input

The SDL launcher now enables text input while the hash-admitted
post-overlay AH=06h poll session is active and queues only one-byte printable
ASCII characters. Multi-byte UTF-8, control characters, and text outside
ASCII are rejected rather than truncated or mapped to a guessed DOS code page.
The existing F-key scan-code path remains separate, and queued bytes are
consumed only at the authenticated poll-return boundary. This closes a host
input gap once that session is active; it does not establish the still-missing
verified title-to-post-overlay handoff or playable gameplay. Full CTest passes
31 tests, Python preservation tests pass 553 with 6 skipped, repository
artifact policy and gitleaks pass, and `git diff --check` is clean. Next
connect the admitted title/overlay execution to this session from observed
state, then trace the resulting gameplay action without synthetic runtime
inputs.

### 2026-10-25 reviewed capture-recorder availability

The current hash-pinned locator was rerun against local Downloads and scoped
analysis/tool caches, and against trv2's Downloads and scoped caches. It found
zero reviewed recorder matches for both Deuteros Amiga and Millennium DOS:
1,794 local executable hashes and 131 trv2 executable hashes were checked for
each recorder kind. The locator script itself matched SHA-256
`2415004a647522bf95776c41111cd76e5e211d7689af7932be7a097de4ec7b75` on both
hosts. No capture was run. The title-initialization and title-handoff gaps
remain external evidence boundaries; do not substitute an unpinned emulator,
synthetic guest state, or scripted input. Resume when an exact reviewed
recorder or another independently authenticated capture source is available.

The two existing Deuteros v23 physical-input receipts from 2026-10-08 were
reverified with the current receipt verifier. Capture 07 has 1,445 raw-PC
records and capture 08 has 1,288. Both contain one sample each at `$40450`,
`$4046c`, `$1ed80`, `$1eda6`, and `$1edac`, and later samples at `$1fbe6` and
`$1fc22`; neither records `$33d2e`, `$201d2`, or `$225ce`. Their Copper/display
streams contain no linear-memory observation proving
`$222b2==$21f28`. These historic, previously pinned captures establish that
input-linked title/display setup ran, but do not provide the active-table
preimage or scheduler reachability needed to continue the title path. The raw
receipts remain outside the repository under
`~/.cache/project-eon-tools/existing-game-captures/`.

### 2026-10-25 captured Deuteros bitplane-base reproduction

Captures 07 and 08 were reverified and both expose Copper BPL1PT `$ab00`,
followed by `$ca40`, `$e980`, and `$108c0`. These match the hash-bound native
deinterleaver's `$12ff4` base and `$1f40` plane spacing. A native regression
now feeds the genuine, in-memory-decompressed ADF auxiliary payload through
that deinterleaver at `$ab00`, checks every byte against the source, then
decodes the 320x200 frame and verifies the already admitted index and RGBA
hashes. CTest passes 11/11, Python preservation tests 556 (6 skipped),
repository artifact policy and gitleaks pass, and `git diff --check` is clean.
This proves the source image can be reconstructed at the captured buffer
address; it does not publish that frame in the live launcher or establish the
missing Exec/graphics initialization and input-to-gameplay path. Next connect
the source-backed buffer selection and frame publication to active native
startup, then continue into a guest action and visible state change.

### 2026-10-25 local original-media locator refresh

The read-only locator was rerun over `~/Downloads` and `~/.projecteon`,
streaming archive members in memory and writing its report only to
`~/.cache/project-eon-analysis/media-locator-20261025/results.jsonl`.
Downloads contains the English Millennium DOS archive, while `~/.projecteon`
contains English Millennium Amiga ADF candidates and Millennium Atari ST
disks, including the already identified Equinox disk (`3f090651…579afe7`)
and the original two-disk STX pair. It also found multiple Deuteros Amiga
disks. These search leads do not change release admission or establish that
any candidate is the canonical English runtime set; the existing hash-bound
release identities remain authoritative. No archive member was extracted or
copied. Resume playability work from admitted media and use synthetic fixtures
only to test search logic or locate candidate source data, never as game state
or behavioral evidence.

### 2026-10-25 Millennium Amiga raw-stage variant comparison

Read-only hashing of the five local Software Studios ADF candidates at the
Defjam raw-loader source ranges found the same second-stage transfer in every
candidate. The Defjam first-stage transfer is byte-identical in the Leaders
and QTX candidates except for 20 bytes in three short ranges; the two Razor
candidates differ in 773–775 bytes over 61–62 ranges. Disk and transfer hashes
and exact difference intervals are recorded in `PRESERVATION.md`. The variants
are not admitted by runtime because their loader entry and service arguments
are not yet source-bound. Leaders/QTX's static entry route reaches the same
`$415d6` suffix that the current Defjam adapter reaches through its bus-error
boundary; all five candidates share those suffix bytes, which branch to
`$424f6`. That shared
window's first state-dependent store consumes incoming A1, the next A1 value
comes from A7, and the first Exec LVO call is at `$4251a` after the custom-chip
write and stack setup. The 40-byte source hash is in `PRESERVATION.md`. Next
bind the actual incoming A1/A7 and establish the LVO result contract before
turning this path into a native runtime continuation.

### 2026-10-25 trv2 prior-capture cache search

Using the existing SSH key, a read-only search of trv2's scoped
`~/.cache/project-eon-tools` and `~/.cache/project-eon-analysis` found no
`raw-pc.txt`, `title-display.txt`, or `run-status.txt` capture artifacts, and
no JSONL trace or CSV receipt files. The scoped caches do contain the already
known Kickstart VERTB and MoveSprite source-analysis listings, which are not
runtime captures. This search supplies no new guest state or service result;
the Deuteros scheduler/input boundary and Millennium DOS title/game handoff
remain open. No original media or cache output was copied.

### 2026-10-25 trv2 original-media locator follow-up

A filename-and-format inventory of trv2's `~/.projecteon`, `~/Downloads`,
and the two scoped Project Eon caches found no additional loose ADF, STX, ST,
or MSA images. The game archives match the media families already present
locally: Millennium Amiga candidates, the two-disk Electric Dreams Atari ST
set, and Deuteros Amiga disks. `~/Downloads` also has the English Millennium
DOS archive. This is a bounded inventory, not proof that other disks do not
exist elsewhere on trv2. Synthetic locator data remains useful for improving
search coverage; any resulting lead must still resolve to a real archive or
disk image before it can supply a runtime byte or preservation claim. Nothing
was extracted or copied.

### 2026-10-25 Deuteros title action-dispatch prefix

The hash-locked English Disk 1 ADF was read in place to follow the title
hit-test scheduler toward a caller. The 352-byte linear candidate at runtime
`$22350..$224af` (ADF `+$7d350`, SHA-256
`2d2b7b1fd1b0f5cd37324b5931af49f1a46bdf5d042aac867a77a96145d2a37c`)
contains an action dispatch at `$2237c`: it loads a longword from table
`$212b0 + 4 * D0`, where D0 came from `$2229e`; a null entry returns, and a
nonzero entry is called through A0. The first eight table longwords at
`$212b0` (ADF `+$7c2b0`, 32 bytes, SHA-256
`1e6458b695bd8f0ed58e33d7d55a1b78133026aaf4eeb9052f8e27fedf472a93`) are
`0, $23f60, $36a8c, $36e60, $358b6, $38380, $39632, $20cfe`.

After that indirect call, `$22390` tests `$222be`; the clear path runs local
cleanup calls and reaches `$223b2`, the already documented title input and
hit-test scheduler prefix. This gives a concrete static route from the
selected action index to local action code and back to the scheduler. The
linear range and table do not establish the live `$2229e` selector, the
runtime target's effects, or which action follows a physical SDL event. Next
trace a source-owned hitbox selection into this table dispatch and advance
only the target whose state and return path are owned. No input value, game
state, or service result was fabricated; original ADF bytes were not written.

### 2026-10-25 Millennium Atari game-init source correction

The exact filename at `$2a634` is `MILL22D.INF`, not `MILL22B.INF`. The
later native service now reads D's 18,428 authentic bytes (SHA-256
`de0a95d3e4659a305b3e55b3417a7648127b41866de0a0ca344a81c66979dbc0`) into
`$2c24a`, and returns that exact file length. Its header at `$2c24c` is
`$0050/$80c8`; these belong to the post-palette read, not the earlier decoder
input. Fread-prefix and token validation read the native buffer rather than
aliasing the initial `$2a500` file image. The pre-decode Fread now checks the
literal `MILL22C.INF` at `$2a640` and materializes its observed returned prefix
from its hash-verified FAT12 source into `$2c24a`. This supplies authentic
decoder input, but frame correctness and playability remain unproven. The
separate later module load of `MILL22B.INF` remains hash-bound at `$11e00`.

### 2026-10-25 Millennium Atari title decode diagnostic

A bounded replay of the recovered token paths against the exact Equinox C
file (SHA-256 `a28a49eea33a14210193bbe6e36abf95700ac6789681bf1a9eac5d09a0999055`)
consumed all 9,597 bytes and emitted 16,000 words / 32,000 planar bytes. The
derived buffer hash is
`97ca82e4296c649a853d45f3900f90b2b8490c1cf12c0580258b3c89e13b4475`; its
grayscale index view exposes the Millennium title lettering. The preview is
diagnostic output in the external cache, not runtime, capture or parity
evidence. Eon now reserves 32,000 zeroed bytes at `$30000` for a 256-byte
aligned low-resolution ST screen, after the PRG+BSS end `$2fd58` and below
hardware registers. `MILL22A.INF` also establishes the zero initial palette at
`+$f28`; replaying the original seven arithmetic passes over `+$16c` derives
the 16 final RGB3 words. `tools/analyze_atari_title_palette.py` reproduces the
palette from the exact FAT12 file hash and emits only metadata. The native
`decode_millennium_atari_title_frame` now validates source and derived hashes,
and the CLI displays the source-derived 320x200 frame with the recovered RGB3
palette. This recovers title pixels only; first unresolved Atari service,
subsequent screen state, and interactive game loop remain open.

The active Millennium Atari driver now continues through the hash-bound local
game-init return at `$2aafe` and palette-copy routine `$2b448` once the four
decoded planes finish. It reads the palette destination words from owned
runtime memory, runs the recovered arithmetic, and commits its exact output
then services each hash-bound XBIOS Setpalette call through Eon's TOS
compatibility layer without defining D0. The real-media diagnostic now reaches
`$2b4c6`, the palette routine's RTS boundary, in 9,589 steps below its 65,536
budget. The return address and caller continuation remain unresolved; title/game
state and interactive play remain unproven.

### 2026-10-25 Millennium Atari presentation capability declaration

The native bootstrap already decoded the hash-locked title frame and the SDL
launcher drew it, but both English Atari ST runtime capability records declared
`decoded_presentation=false`. The session snapshot and the two exact release
records now declare decoded presentation while retaining `bootstrap_only`, no
admitted input, and the bootstrap boundary. A fresh `--launch-check-json`
reports `runtime_admission=READY`, `decoded_presentation=true`, and
`admitted_input=false`; this corrects capability reporting only and does not
advance Atari gameplay.

### 2026-10-25 Deuteros native graphics-library handle

The native Deuteros driver now answers the exact hash-validated
`OpenLibrary("graphics.library", 0)` boundary with a generation-bound Eon
service token. Its result type is separate from
`DeuterosAmigaObservedOpenLibraryReturn`; it carries native-compatibility
provenance and is never entered as a captured trace observation. The original
local route stores that token at `$12fec` and stops at `$1eda6`, before the
unresolved display-base read. The token is not claimed to be an Amiga address
or an OS allocation result. The application build succeeds. This opens the
native service path but does not yet provide the display surface, all graphics
vectors, or a playable frame. The display surface has since been connected;
remaining graphics-vector handlers and callback/game-loop continuation still
need source-bound native semantics.

### 2026-10-25 Deuteros Eon-native bootstrap memory profile

The hash-locked English system ADF boot-block caller at `$1306c` now continues
through the exact source-ordered `$13028` and `$13034` `AvailMem` calls using
an explicitly Eon-native, generation-bound chip-like arena of 512 KiB. The
`MEMF_FAST` query returns zero because this profile has no FAST arena; the
`MEMF_CHIP` query computes free bytes from the arena's actual capacity and
currently owned native ranges. These are Eon policy values, never claims about
Kickstart allocation results. The source branch then selects `$ab00`, and the
arena reserves the source-derived 32,000-byte four-plane range there, after
checking it against admitted boot/code memory. The range ends at `$12800`,
immediately before the boot-block image; it does not overlap loaded code.

The exact source continuation calls native `InitBitMap` at `$1309e` and writes
its recovered ColorMap/ViewPort links and plane pointers into owned memory.
The focused runtime test verifies these stores and the range checks. It next
reaches `InitRastPort` at `$13154` (LVO `-$c6`, A1=`$12e6e`); that routine's
effect is not yet admitted. The following source calls are `MakeVPort` at
`$13188`, `MrgCop` at `$1319a`, and `LoadView` at `$131a6`. No visible title
frame or playable state is claimed. This is a bounded native service slice,
not Amiga OS emulation or a recovered historical memory configuration.

### 2026-10-25 Deuteros native title display initialization

The existing Eon-owned 320x200 display surface now continues through a separate
native compatibility path for the title's four exact `$dff000` setup values
(`$40/$7fff`, `$42/$7fff`, `$9a/$c000`, `$96/$87ff`). The session records this
as native display initialization, never as observed hardware writes. It then
continues through the separately typed native input-handler registration;
neither event is represented as an observed Exec return. Build succeeds; the
first title pixels and callback/game loop are still not verified or playable.

### 2026-10-25 Deuteros native input-handler registration

The same generation-bound compatibility path now accepts the title's exact
`input.device` `IND_ADDHANDLER` request after native display initialization.
This is typed native-service state, separate from the Exec return observation;
it arms the next hash-locked local setup entry at `$206d4`. The existing SDL
raw-key adapter can then feed the title callback under the native handler
contract. Build succeeds, but the later setup services and a rendered title
frame remain unverified.

The next 248-byte setup span now has its Exec vectors identified from the
official autodocs: `$206de` `FindTask(NULL)`, `$2070c` `AddPort`, `$20732`
`OpenDevice`, `$2077a` `AddPort`, and `$207a0` `OpenDevice`. The original name
is `trackdisk.device`; the code opens unit 0/IORequest `$205e4` and unit
1/IORequest `$2063e`, each with its own message port and flags zero.
`DeuterosAmigaTrackdiskCompatibilitySession` now holds generation-bound Eon
task/device tokens and source-bounded port, open, and motor state. Its focused
CTest covers the exact call sequence and the proposed Eon policy to keep device
availability independent of media attachment. That policy is explicitly
native-compatibility state, not an observed Exec D0/SR return or a guest-visible
IORequest effect. The source also shows `$ffffffff` stores at request `+$30`
and zero stores at `+$1e`; these constants do not establish that a request or
device is inactive. Its original title `$8002` read path is now connected to
`NativeRuntimeMemory`: only the next title request ordinal is accepted, the
hash-addressed Disk 1 bytes flow through the owned staging pointer, and the
source loader copy is committed to its recovered destination before advancing.
No return registers are fabricated. This covers title loading only; writes and
updates still need a separate copy-on-write save overlay and proven guest
semantics.

### 2026-10-25 Kickstart trackdisk command contract

The exact supplied Kickstart 1.3 ROM was scanned in memory by source command
values from the hash-locked Deuteros title stage. Its resident trackdisk
dispatcher confirms bit 15 is the extended request path, checks the
`IOExtTD` change count, then indexes the handler table by the low command
byte. The title's `$0009` calls at `$207cc/$207f0` are `TD_MOTOR` with lengths
1/0; the same routine writes the previous motor state to `io_Actual`. The
later `$8002/$8003/$8004` requests select extended read, write and update.
These findings replace the earlier generic “read/write request” description
with exact ROM-backed commands. Both ADFs are already hash-admitted in
`DeuterosAmigaOpening`, and `AmigaAdf::sector` is bounded and read-only. Before
adding runtime handlers, keep unit-to-image selection explicit and add a
copy-on-write save overlay for the write/update path; original disk and save
bytes must stay immutable. ROM ranges, hashes, command addresses and remaining
uncertainty are recorded in `PRESERVATION.md`. No commands were executed in
the ROM, and no guest service result or gameplay state was fabricated.

### 2026-10-25 Deuteros native title-read source chunks

The `$20934` title-reader profile now exposes the four authentic `$1600`-byte
source chunks from its admitted system Disk 1 through
`DeuterosAmigaTitleStageSession::title_disk1_read_chunk`. Each access checks
the corresponding chunk hash and rejects any ordinal outside 0–3. A genuine
media test checks the four chunks and their concatenated `$5800`-byte hash.
The request-shaped span search across all 11 supplied standard-size ADF
members found identical chunks in the clean Disk 1 and two alternate Disk 1
images; Disk 2 and save disks differ. This turns the static request extent
into a verified original-data source without deriving the drive unit from the
`$206a0` cell: the source copies `$205e4` there, and `$205e4` is the IORequest
opened on unit 0. Runtime media attachment to that unit is still not observed,
so a native handler must select its media explicitly. The source initializes
`iotd_Count` to `$ffffffff`, and the scanned Kickstart ROM accepts that
sentinel; require it instead of inventing a change count.

The hash-bound title image initializes the `$206a4` `io_Data` pointer to
`$256c0` (ADF offset `$7d6a4`, four-byte SHA-256
`c9e599467922c260811ea8cb116da548eb16f90d779754ec723d67ebaca20fee`). Its
`$1600` bytes end exactly at the loader's `$26cc0` first destination. The
direct-media inline-stream test asserts the original value, and the trackdisk
mechanics fixture uses that recovered staging address. The trackdisk service
is still not connected to the live coordinator or guest DoIO boundary.

`DeuterosAmigaTrackdiskCompatibilitySession::do_title_read` implements the
four exact unit-0 ETD_READ requests against an explicitly attached,
hash-matching ADF. It validates command, flags, the staging pointer copied
from `$206a4`, length, offset and count from owned IORequest memory, then
atomically commits the authentic bytes, `io_Actual=$1600`, and `io_Error=0`.
Its result is separately tagged native compatibility. The pinned Kickstart
1.3 handler at `$fea3b4` sets `io_Actual` to zero, increments it by `$200` per
successful sector, and stops when it reaches `io_Length`; genuine-media tests
check all four blocks and memory results.

### 2026-10-25 Deuteros native title-read runtime integration

The native title input-handler handoff now installs the generation-scoped
trackdisk compatibility service using the recovered `$206d4..$207ca` call
order and an explicit system Disk 1/unit-0 policy. Eon tokens remain host-side;
the setup does not mutate guest MsgPort or IORequest state. The deterministic
title/session driver now services each pending `$20934` ETD_READ ordinal from
owned request memory, validates its request/staging pointer cells, stages the
source-proven operands, transfers the matching hash-locked original bytes and
copies each staging chunk to `$26cc0 + ordinal*$1600` before the next request.
The native path is mutually
exclusive with the existing observed-return/status gate, so it cannot create
or replay capture evidence. The `$209f0` main-stage OpenDevice remains
independent. Focused direct-media tests cover payload hashes, request-pointer
rejection, memory atomicity and ordered native ordinals. Original media is
read-only; write/update requests remain outside the service.

The current launch-linked run now advances through native graphics-library
setup and allocates and initializes Eon's title surface. The real-media CLI
path accepts seven deterministic steps and now returns at `$404bc`, after
native input-handler setup, the bounded four Disk 1 reads, and the Eon-native
`$206d4` setup service. `$404bc` is the caller return immediately before
`$206be`, not a main-stage presentation result. The Exec graph does not
establish the original OS state or make a visible title frame. The direct-media
title-runtime test exercises the native continuation and verifies the original
media remains byte-identical. A player action reaching a visible state change
remains unverified.

### 2026-10-25 Deuteros native Exec graph continuation

Exact Kickstart 1.3 code confirms what the current host-only trackdisk tokens
cannot replace: `FindTask(NULL)` returns `ExecBase.ThisTask` (`+$114`),
`AddPort` initializes each guest `mp_MsgList` and links it through
`ExecBase.PortList` (`+$188`), and `OpenDevice` finds a device through
`ExecBase.DeviceList`, initializes IORequest fields and invokes the device
open routine. `trackdisk.device` also creates or resolves unit state and writes
`io_Unit`. ROM offsets, full routine hashes and source-level port/request
fields are recorded in `PRESERVATION.md`.

The task/port/device-object gap has been closed with a typed Eon compatibility
graph in `DeuterosAmigaExecCompatibilitySession`. It reserves `$d00000..$d00fff`
only after checking the range against owned memory; models ExecBase, current
Task, `DeviceList`/`PortList`, the two title MsgPorts, trackdisk, both units,
and the request fields consumed at the source call sites. The native title
handoff installs it before servicing Disk 1 reads. Native tests cover list
sentinels and links, task pointers, unit ordering/counts, input preimages, and
atomic rejection. Generated pointers and successful service results remain
tagged Eon policy, not captured Exec returns. The separate main-stage Exec
chain after audio setup is now serviced through the graph while preserving its
distinct call order: `FindTask(NULL)` at `$2099e`, `AddPort` at `$209ca`, then
`OpenDevice(trackdisk.device, unit 0)` at `$209f0`. The service checks the
source call registers and guest preimages, then advances atomically through
the CIA boundary and existing native resource/request loop. A separate branch
later reaches the ExecBase read at `$208b0`. Later device behavior still needs
source-bounded service work and visible input-to-state verification. No
original OS scheduler is claimed.

The launch-linked driver reports a typed Eon-native receipt for five
compatibility calls in source order: FindTask, AddPort, OpenDevice, AddPort,
OpenDevice. The graph and trackdisk objects are created at the `$206d4` call
boundary, after input-handler registration. The local success path now writes
each request's `IOExtTD.iotd_Count=$ffffffff` and `io_Flags=0`; `io_Error=0`
remains an explicit Eon service result. An Eon virtual-stack receipt starts from the
`$7fff0` supervisor-stack argument passed to `UserState`, while the restored
user A7 is `$40b62`. It records the caller return at `$40b5e`, saved FindTask
D0 at `$40b5a`, service-call low-water `$40b56`, and final A7 `$40b62` before
`$404bc`. The receipt labels balanced intervening calls as an assumption and
the stack-bank behavior as Eon compatibility policy; Kickstart V34.005's
`UserState` defect and whether SetPatch runs remain unresolved. It does not
write these words into guest memory or claim an original stack allocation. The
real-media regression verifies this Eon receipt and stops before `$206be`.
Next, validate that helper's exact branch for the Eon zero-return path, then
apply its pointer copy and the `$403e6` literal seed before continuing toward
the first controller action.

### 2026-10-25 Millennium Atari native startup continuation

The generic active-session driver now runs the typed Eon TOS services in the
Atari config consumer: SR user mode, XBIOS `Physbase`, `Logbase`, low-resolution
`Getrez`, and the Line-A `$A000` parameter workspace consumed by the exact
local continuation. The 4 KiB workspace is separately labelled Eon-owned and
does not claim original TOS addresses or returned font/routine vectors. The
real-media CLI advances through XBIOS `$15` Cursconf(0,0) and selector `$06`
Setpalette, then executes JSR `$2b55a`, BSR `$2b598`, and `CLR.B` at `$2b59e`.
The callee's `MOVEQ #0,D0` determines the indexed byte sources, which are read
from native memory alongside the table words consumed by all three finite DBF
iterations. The proved writes commit through the native memory layer. Cursconf
hides the VT52 cursor; Setpalette reads sixteen words from original guest pointer
`$2a612`. The native path carries the exact MOVEM/RTS transition after the
three finite iterations and stops at the next XBIOS `$26` call `$2aa72`. Saved
register values and absolute A7 remain opaque; the caller helper reads no
restored register before that service boundary. CTest hashes direct media
before and after. Line-A drawing, SDL palette presentation, rendered game
state, and player input to gameplay remain unresolved. The read-only media locator also found an English
two-disk Atari STX pair and multiple Millennium Amiga variants; hashes differ
from the canonical multi-archive Amiga release, so they are leads for real-data
admission, not substitute evidence.

The hash-verified Atari config analyzer now supports an alternate entry offset;
the real-media CLI check uses it to decode `$2aa3a` independently. The Supexec
routine at `$2aa42` stores Physbase and queues `$2aa3a` in the TOS VBL table.
That VBL entry calls `$2b568`, whose wrapper calls `$2b6c6`; the callee accesses
MFP/video and PSG registers. This is a concrete native-runtime gap, not an
unknown XBIOS return value: model the TOS-owned VBL queue and its cadence, then
recover the hardware/audio effects of `$2b6c6` before replacing the call with
SDL behavior. The independent entry hashes and exact register addresses are
in `PRESERVATION.md`; do not execute the routine as ordinary guest memory code
without the required hardware services.

The post-read VBL state is now located in real media: after the exact
`MILL22D.INF` Fread into `$2c24a`, the callback's PC-relative `A3` points to
D-file `+0x64`. Its first mutable bytes at D-file `+0x62a..+0x641` are
hash-addressed in `PRESERVATION.md`. The queue is installed before this Fread,
so an earlier VBL may have observed a different preimage; its first-event
state and cadence are still unknown. Trace that ordering, then translate the
callback's MFP/video/PSG reads and writes into a labelled Eon Atari hardware
service before scheduling a VBL. Do not select a video timing branch from SDL
refresh or assume the callback has already run.

The active diagnostic's default transition budget is now 4096, and the Atari
driver reads the original hash-identified `MILL22C.INF` through its owned
GEMDOS compatibility path before running bounded token transitions. The
real-media run reaches the game-init zero-copy boundary at `$2b2ea` after 4096
transitions. Earlier diagnostics reported `$2b5a6` from a stale boundary
address and omitted the current state. They now expose the consumer sequence,
local instruction count, loop state, helper-entry count, and the current
hash-bound stop address. The zero-copy boundary is not a completed frame or
playable state. Continue the recovered init stream and connect the completed
frame and palette to Original-mode presentation without replacing source
pixels. Deuteros still needs a source-backed SDL input-to-action-to-visible-
state path.

### 2026-10-25 Deuteros native post-Exec pointer seed

The active Deuteros driver now continues from the generation-bound Eon Exec
service setup through the hash-locked `$206be` helper and `$403e6` literal
seed. It verifies `$20698 == $205e4`, copies that pointer to `$206a0`, and
writes the source literal `$1c482` to `$1f97c` in one owned-memory batch. The
real-media CLI advances from seven to nine steps and reports the next stop at
`$404ce` before the unresolved `$403f4` service batch. The updated direct-media
test verifies the new boundary and Eon service sequence. Next recover the four
targets and caller effects in `$403f4`; keep both games' playability status
open until an SDL input produces a source-backed visible gameplay change.

### 2026-10-25 Native playability frontier recheck

Re-ran the current native diagnostics against the read-only `~/.projecteon`
media set. The English Millennium Atari ST release
`0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69` advances
to XBIOS selector `$26` at `$2ab24` after 9,596 native steps. The exact
`$2b4c6 -> $2ab04` caller return is now an Eon compatibility transition that
keeps the stack address unknown. The recovered `MILL22D.INF` caller reads the
authentic 18,428-byte payload into its owned buffer through a generation-bound
virtual handle; the CLI regression verifies the requested pathname and
unchanged source archives. The callback behavior after `$2ab24` remains
unresolved, and the completed planar decoder/palette is not yet a rendered
gameplay frame. The call is `MILL22A.INF+0x624`, with selector `$26`; the
10-byte caller span at `+0x61c` hashes to
`cbbed60639bf375af516e0e9d7252db6a738c20aea6d8b887e17e8f01b1b2617` and
pushes callback entry `$2ab2a`, whose first instruction branches to `$2ab9c`.
The caller restores A3=`$2a64c` and A4=`$2a66c`. The staged caller separately
writes `$361436a7` to `$2ab2c`, while the
configuration bytes at `+0x62c` are zero. Source does not establish callback
entry D0-D7/A0-A7/SR, the absolute A7 frame, or TOS vectors `$10`, `$20`, and
`$24`. The branch target saves registers, manipulates exception vectors and
executes `ILLEGAL`; it remains behind an authentic register/vector/frame
observation boundary.

The Millennium DOS English release
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` has
hash-bound MCGA startup parsing and a typed video policy. The current direct
media CLI stops at its first private INT `$91` result; the native title-session
test can advance farther only after explicit test-only results are supplied.
Those results do not prove historical INT `$91` behavior, a title frame, or a
playable state. Preserve this as the active DOS boundary until an authentic
handler result is admitted.
Millennium Amiga English
`ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd` has no
deterministic driver for the active lifecycle state. Deuteros Amiga English
remains stopped at `$404ce` before its unresolved four-call `$403f4` service
batch; Deuteros Atari ST also has no deterministic driver in its active
lifecycle. A read-only locator pass over the available Downloads and
`~/.projecteon` archives found no additional supported ROM or game payload
that changes these execution frontiers. A separate scoped read-only search
also found no `tos*.img`/`.rom`, `etos*.rom`, or `emutos*.img`/`.rom` in the
local or trv2 `.projecteon` and analysis-cache directories. The authentic
Millennium Atari ST STX pair is present locally (Disk 1 member SHA-256
`081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd`, Disk 2
`4b55a53285240541c42374dc8275ae7c73d182fc3872514727d9d1795d8ce7f4`), but
these disks do not establish the TOS vector baseline or callback entry
registers required at `$2ab24`. Their bytes were not extracted or copied.
Synthetic probes may help locate authentic state, but cannot supply that
state as runtime behavior. The game archives were streamed in memory and not
extracted or changed. Neither game is yet playable across its
supported releases; continue with the Atari `$2ab24` callback and
Millennium DOS sound-driver admission while retaining the unresolved
Deuteros graphics-library and Exec service boundaries.

### 2026-10-25 Millennium DOS virtual IVT policy

The DOS compatibility runner now owns a generation-scoped virtual IVT. AH=`$35`
Get Vector for the two hash-bound title requests returns Eon-allocated opaque
handler slots, and the stored result is tagged `eon_virtual_ivt_policy`. AH=`$25`
Set Vector updates that same virtual table and the modeled IVT at segment zero;
the game's handler at CS:`$1124` remains source-derived. No historical DOS vector
values are claimed, and the opaque previous handler cannot be invoked or chained
without a resolver. The direct-media path now advances from `$10f4` through both
installs and the existing tagged INT `$15` policy. It reads the prior INT `$1c`
target from `$0000:$0070` using an Eon-allocated opaque virtual slot, records
that policy provenance, then source-derives the game's `$11d8` handler install.
It also reads the previous INT `$09` target as an Eon-owned opaque slot and
installs the source-derived handler at CS:`$126a` in the native-session test.
With explicit test-only INT returns, that test reaches the MCGA function-four
INT `$91` result at `$0127`; its input is the authentic TITLES record byte at
CS:`$1ac5`. The end-to-end direct-media CLI still stops at the initial private
INT `$91` return. The hash-verified title call sequence sets
`SS=CS`, `SP=$da00`, and places `$1ad1`/`$1bb0` at CS:`$d9fc`/`$d9fe`. This
does not reveal the active handler stack frame or result. The old return-chain
path required the wrong word at `$d9fc` and fabricated other stack words; it
is disabled until the handler frame is derived. Tests verify both opaque IVT
provenance and the new INT `$91` boundary. This remains startup progress, not
a playable state.

### 2026-10-25 Millennium Amiga input-device boundary on the live snapshot

The active Millennium Amiga bootstrap snapshot now exposes the hash-bound
`input.device` setup and its source-defined success branch at `$41d88` (the
earlier `$41d8a` metadata value was corrected from the actual branch opcode).
It continues through `DoIO` command 1, constructs the exact Interrupt record,
and reaches `DoIO` command 9 with `io_Data=$41d1e`; the event handler, key table,
and queue consumer each have bounded source hashes. A native typed session now
applies only the handler's verified raw-key translation, duplicate-key filter,
20-byte queue, queue-pop, and event-flag effects from the exact source table.
It accepts a separate Eon-native registration token and generation; these are
not OpenDevice/DoIO observations. Focused tests verify `a`/`A` translation,
release/re-press, timer pass-through, queue shifting, flag writes, and generation
isolation. Host SDL key events are not yet translated to Amiga raw codes or
wired into this session, and dequeued bytes have no verified game-visible
action. There is still no active live service path or native title frame, so
Millennium Amiga remains unplayable. Next work is runtime injection of the
typed SDL-to-RAWKEY boundary and recovery of the display path, without claiming
that registration or a gameplay effect was observed.

### 2026-10-25 English Millennium DOS function-$19 corpus search

The genuine direct-media CLI stops after 53 native steps at TITLES `INT $91`,
function `$19`, for both startup choices: MCGA source `$e2c5:$fe9` and EGA
source `$e450:$fe9`. A read-only scan of all 31 files in the exact English
Downloads archive (`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`)
and its matching direct-media set found no references to the MCGA destination
words `$09be/$09c0` in `MILL.COM`, `TITLES.EXE`, `2200AD.EXE`, or `2200GX.EXE`.
The raw selector-byte candidates resolve to the known INT `$33` mouse calls
plus one `2200AD.EXE` store through unresolved `BX`; no candidate establishes
the loaded MCGA driver pointer. The scan excludes neither alternate encodings
nor runtime setup outside the admitted images. Keep the function-$19 boundary
and recover the authentic launcher/loader pre-title state or another matching
English source before advancing startup rendering. No synthetic driver state
or destination was supplied.

The EGA driver's function-$19 slot is independently source-verified as a
single `RET` at `$0234` (driver hash and dispatch details are in
`PRESERVATION.md`). The active runtime can now consume that return only for a
fully verified EGA image/vector and then continue at `$1c17->$1725`. The
ordinary direct-media CLI still selects Eon's MCGA default policy and stops
at `$127`. A full-image disassembly scan established that MCGA `$09c7` is
initially zero and is written only by selector `$08` at `$09e0`; the exact
TITLES path calls `$19` without a prior selector `$08`, so `$09be/$09c0` are
not consumed when a live-byte guard confirms `$09c7==0`. The next MCGA task is
to source-bind the TITLES call stack and translate the palette-conversion
prefix through the first VGA DAC write boundary `$0ade`. Do not use the
unknown destination pointer or claim a rendered screen from this prefix.

### 2026-10-25 Deuteros `InitRastPort` ROM chain resolved

Rechecked the exact Kickstart 1.3 LVO `-$c6` chain from the supplied ROM
archive. The `$13154` request is tied to the library stub, bounded
initialization helper, clear routine, GetStdFont lookup, and SetFont helper;
all spans and effects are hash-documented in `PRESERVATION.md`. The Eon-native
service now maps the three proven `topaz.font` words into a generation-bound
object at `$d10000`, initializes the exact RastPort writes, and records the
Eon allocation separately from the unknown historical AllocMem address. The
active Deuteros route advances to `MakeVPort` at `$13188`. Its generated
Copper/display effects remain the next graphics boundary. The checked Atari
CLI still reaches XBIOS `$26` at `$2ab24`; callback vector/frame state remains
unavailable, so no gameplay result is inferred.

The active Deuteros session driver now applies the verified Eon memory profile
and paired-view continuation when the opening hands off at `$13028`. It runs
the bounded InitRastPort service and reaches MakeVPort at `$13188` with the
32,000-byte plane layout and RastPort state in owned memory. This is active
native progress, not an AmigaOS result or a playable-screen claim.

### 2026-10-25 Millennium Atari second Supexec callback preimage

The hash-locked `MILL22A.INF+0x610..0x624` caller sets A3/A4, pushes callback
`$2ab2a`, and invokes XBIOS `$26` at `$2ab24`. The callback's short branch
targets `$2ab9c`; its body at file offset `+0x69c` saves the incoming D/A
register set, reads the prior illegal-instruction vector at `$10`, reads
exception vectors at `$8,$c,$10,$14,$18,$1c,$20,$24`, changes vector state
and executes `ILLEGAL`.
At callback entry only A3/A4 are initialized by this visible caller. The
callback separately saves entry A6, consumes A7 as its supervisor stack, then
overwrites A6 and advances A7 before saving the remaining registers.

Atari Corp's Supexec contract guarantees supervisor-mode execution, but does
not specify the register preimage, supervisor stack address/layout, SR or
exception-vector contents. The callback cannot be safely run from a synthetic
zero-state fixture and no Eon callback service was added. Its next dependency
is an authentic TOS/supervisor machine preimage covering those registers,
stack and vector table; absent that, retain the external boundary at
`$2ab24`. Hashes and the precise register read order are in
`PRESERVATION.md`; the generated listing remains only in the external analysis
cache.

The selected branch is now more precise: Supexec guarantees S=1 for the second
ILLEGAL frame, so the handler's `BCHG` sets Z=0 and the branch to `$7a4` is not
taken. The path at `$78a/$78c` reads `MILL22A.INF+0xe1a` (runtime `$2b31a`,
source-span SHA-256 `7c1de5f41fb0f7939681dff5ac21b37d78e76204ac839a98df317bec1cef054d`)
and writes the next source longword to the loaded address. The file preimage
alone yields destination `$2a4e5302` (24-bit bus address `$4e5302`); no
admitted RAM profile covers it, and no runtime mutation or bus-error result
has been recovered. Keep the external boundary until the exact runtime
source/vector preimage is explained. The refreshed alternate-entry reports
and their hashes are recorded in `PRESERVATION.md`; all remain outside the
repository.

### 2026-10-25 Deuteros MakeVPort color slice advances the live native route

The active native-session driver translates the admitted Kickstart `MakeVPort`
prologue and `$fcc7b0..$fcc934` color-range slice when the original bootstrap
reaches `$13188`. It validates BitMap depth 4, ViewPort mode `$4000`, and the
20-entry ColorMap; emits the first WAIT plus 20 color writes; and retains the
two `$fccff4` copies in symbolic A5-relative slots. It follows the reachable
geometry branch to `$fcd0b4`; the next instruction reads `$d6(A0)` from the
graphics library base, but the active value is an opaque Eon token. Recover
that exact library field before continuing MakeVPort, then `MrgCop` and
`LoadView`. Keep the call source-bound and do not substitute a fabricated
vector return. Visible output, the title screen, and gameplay are still not
established.
Millennium DOS remains at INT `$91` function `$19`; Millennium Atari ST
remains at the unresolved Supexec callback context at `$2ab24`.

### Deuteros MakeVPort geometry caller evidence

The caller maps D4=View, A3=ViewPort, D3=RasInfo, and A2=`A5-$64` scratch.
The hash-bound `$fcd010..$fcd0b8` reachable slice copies ViewPort width 320
and the InitVPort-cleared word 0 to symbolic slots `A5-$58` and `A5-$56`, then
follows the `$400`-clear branch through `$fcd0b4`. The runtime does not invent
an absolute A5 value. It stops before `$fcd0b8` reads `$d6(A0)` from the
opaque graphics-library token. Resolve that field from authentic library
state before modeling further Copper output, `MrgCop`, or `LoadView`.
### 2026-10-25 Deuteros MakeVPort geometry and caller prefix

The source-bound Deuteros Amiga route now takes the active Kickstart 1.3
graphics initialization profile into the `$fcd010..$fcd2a4` MakeVPort
geometry path. It requires View+$e=`$81`, the initialized ViewPort/RasInfo
preimage, and BitMap.BytesPerRow=40. MaxDisplayColumn `$1d1` and
MinDisplayColumn `$30` are tied to ROM slice `$fcaea4..$fcaeb0`, SHA-256
`d7f03feed5d8382a8bebc1c0543a72d3f8e26d816cb156e5442e62dc70183f3`; the
opaque Eon library token remains uninterpreted as RAM. The symbolic scratch
result is position (56,208), size 320×0, RasInfo offset 0, row bytes 40; the
full `$fcd010..$fcd2a4` source interval hashes to
`1c8e89a6e9edddad0f7ac00468f8dc37bd914da57f1a8ba32cf56341ab870961`. A5
locals remain symbolic. After the inner RTS, the caller appends its first
post-geometry Copper tuple `{0,$f08e,$0500}` at `$d200a0`, then writes the
source-defined first two words `{0,$2f00}` of the next tuple at `$d200a6`.
It stops at `$fcc9a2` with node count 22 and cursor `$d200a6`. The 92-byte
caller slice `$fcc946..$fcc9a2` hashes to
`ce1bfc1a50073999d629997883ac22674a869daae77aab0d310c5eab59456087`. The
still-uninitialized third tuple word depends on GraphicsBase+$a4, initialized
by a helper that polls
VPOSR through `$fc5ece` and samples `$dff006` twice, returning 0 or `$102`.
Eon has no admitted video standard or beam origin/phase; SDL presentation
pacing and the Paula audio clock do not supply that state. Keep the native
boundary at `$fcc9a2` rather than choosing a beam branch.
The exact native and real-media CLI continuation tests pass. This does not
establish the outer MakeVPort return, remaining Copper list, `MrgCop`,
`LoadView`, a rendered frame, or playable state. DOS MCGA remains at INT `$91`
function `$19`; Atari ST still needs the authentic Supexec supervisor preimage.

### 2026-10-25 recorder and alternate-media recheck

Rechecked the external recorder cache on trv2 by SHA-256 against the pinned
Millennium DOS v21 and v13, and Deuteros Amiga v10 and v33 identities. The
scoped cache and Downloads contained seven regular files; none matched a pinned
recorder. An in-place scan of 37 ZIP archives across trv2 `~/.projecteon` and
Downloads found only the already-known English DOS member set: the exact
`MILL.COM`, `TITLES.EXE`, `2200AD.EXE`, `2200GX.EXE`, `MCGA.BIN`, `EGA640.BIN`,
`TITLE.LIB`, and `GX.LIB` hashes. No alternate MCGA driver or recorder was
found, and no original media was extracted, copied, or modified. This does not
advance either game's runtime boundary; the next work remains source-bounded
execution where all machine inputs are owned, with the authentic recorder and
hardware-dependent returns left unavailable until their pinned evidence is
present.
