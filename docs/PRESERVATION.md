# Project Eon preservation record

This is Project Eon's durable evidence ledger. It makes compatibility claims
reproducible from legally obtained source media without committing or
redistributing commercial game data. Filenames are descriptive; SHA-256
content identities are authoritative.

## Principles

1. Original bytes are read-only evidence; importers never alter source media.
2. Real data always takes precedence. Synthetic assets, invented tables, and
   guessed mechanics must not replace original data that is present.
3. Unknowns remain unknown until supported by data, executable code, or a
   recorded observation of the original game.
4. Every decoded format becomes a bounds-checked parser tested against genuine
   media.
5. Original mode cannot change recovered simulation rules or saved state.
   Modern changes are allowed only when explicitly enabled and documented
   under the mode contract below.
6. This repository contains code, documentation, hashes, and newly created
   menu artwork—not commercial game assets.

### Millennium DOS eighth-function runtime boundary

The English eighth-table handler has an independently admitted continuation
for `$7306..$7319`. It can only be created from the authenticated post-overlay
`$d40a -> $76f1` boundary with scaled index 7 and exact handler `$7306`. Its
`$731a` preflight and `$09fa` helper remain explicit call/return boundaries;
the runtime does not turn separately recovered static preflight evaluators
into invented call results. See `MILLENNIUM_DOS_EIGHTH_FUNCTION.md`.

### Millennium DOS ninth-function runtime boundary

The hash-bound `$7339..$7381` continuation is admitted only from the active
`$d40a -> $76f1` dispatch at scaled index 8. It publishes exact runtime reads,
writes, calls, loop count, and the terminal `$73cc` jump as typed checkpoints.
It does not execute or infer `$73cc`; see `MILLENNIUM_DOS_NINTH_FUNCTION.md`.

The F9 jump target `$73cc..$740e` is now a separate exact-identity native
continuation. Active ownership requires the existing F9 session to expose its
`$7381 -> $73cc` handoff and a separate matching entry observation. The session
then observes calls `$7bcb`/`$a2a0`, byte `$da41`, the `$09fa` ZF/BL loop, and
either the short `$4111`/RET `$73e6` path or the guarded long path through
`$4111`, `$be28`, `$0b9d`, word `$07da`, `$0ae3`, and `$4bf7`. Only the literal
terminal writes `$da41=0` and `$da42=$80` are reconstructed. Call returns,
runtime values, flags, loop exits, and gameplay meaning are never invented.

### Millennium DOS fourth-function runtime boundary

The `$72f9 -> $ba5e` continuation requires authenticated dispatch index 3.
Its three literal writes are emitted only after their preceding exact call
returns; see `MILLENNIUM_DOS_FOURTH_FUNCTION.md`.

### Millennium DOS fifth-function boundary

The complete `$7597..$75a5` four-call chain is hash-bound and admits only
dispatch index 4. Each callee remains opaque; see
`MILLENNIUM_DOS_FIFTH_FUNCTION.md`.

### Millennium DOS third-function boundary

The hash-bound `$6faa..$6fee` continuation is admitted only for dispatch index
2. It stops before dereferencing original far pointer cell `$0112`; see
`MILLENNIUM_DOS_THIRD_FUNCTION.md`.

### Millennium DOS first-function boundary

The authenticated `$6f9a` handler and local `$771d..$7789` setup are admitted
only for scaled dispatch index 0. Original calls remain explicit return
boundaries; see `MILLENNIUM_DOS_FIRST_FUNCTION.md`.

### External replay checkpoints

Canonical frame, audio, state, and physical-input checkpoint bytes remain
outside the repository even after a capture becomes useful for recovery. The
`tools/verify_replay_fixture.py` admission tool binds one opaque external
payload to a recognised outer-release identity, an independently retained
capture hash, canonical checkpoint ordering fields, and the payload's own
size/SHA-256. It does not open original media or decode/execute the payload.
The format, safety limits, and explicit non-equivalence boundary are specified
in [`REFERENCE_TRACE_FORMAT.md`](REFERENCE_TRACE_FORMAT.md#opaque-replay-checkpoint-fixtures).

### Runtime release-identity boundary

The SDL-free `ReleaseRuntimeCoordinator` is the sole retained identity for a
live CLI or card-menu launch. It accepts a `ResolvedLaunchRequest` only when
game, platform, language, and outer SHA-256 all exactly match the attached
`ReleaseArchive`, then reopens and hashes that archive through the manifest
before retaining the value. It clears any prior identity before each acquire
attempt and on source replacement. A malformed, stale, or replaced archive
therefore leaves no active identity for a loader to consume.

### Direct-media evidence boundary

The scanner also recognises a loose physical-media leaf only when its exact
size and SHA-256 occur in the parser-profile manifest. Such an observation is
reported as **unbound direct media**, separately from recognised releases. It
does not create a `ReleaseArchive`, light a platform card, or permit a launch:
the same ADF, ST image, or DOS disk can occur in more than one container, and
one verified leaf does not prove that the complete original media set is
available. A future direct-media launch path must add a separately documented
complete-set identity (ordered members, hashes, platform, language, and
evidence) before it can reach `ReleaseRuntimeCoordinator`. No archive is
constructed, unpacked, copied, or substituted in the meantime.

### Split-container media-set boundary

Some legally supplied releases preserve each original disk in a separate ZIP
container.  A split-container release is admitted only when every declared
container occurs exactly once, its outer SHA-256 and size match, and that
specific container supplies its declared hash-addressed disk leaf at the
declared size.  The canonical ordered disk records are hashed as
`outer-sha256<TAB>outer-size<TAB>leaf-sha256<TAB>leaf-size<LF>` and bind to a
known logical release identity.  Discovery does not use filenames, does not
unpack disks, and rejects a partial, duplicated, repacked, or mixed set.

The first documented set is the English Deuteros Atari ST pair: the
Replicants disk 1 outer container
`a9318feb83ff34b79f5a5ea1e5ffcb45828e4432ac75a859f55c3de97d724c93`
contributes `aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee`,
and the clean disk 2 outer container
`7842adb599dbc4cf79827e31e912740f259af45718c124d5806e1c8860f2253d`
contributes `5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193`.
The set digest is
`0a87871cdfc6e0f11c598b86be0726c842c2cdcb1cb7d0dba651f1d43b835ffa`.
Runtime admission reopens and verifies every container again, retaining only
transient in-memory decoded leaves; it never materialises original disks.

The clean English Deuteros Amiga pair is bound the same way: disk 1 outer
`7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533`
contributes `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`,
and disk 2 outer
`b98ee3c36141773485c5e03dd8bb4aa59784eaf08a1363fa6a2951a5eb5fdc0a`
contributes `99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a`.
Its ordered set digest is
`3d5dc5cf605f5b19a1ba42038321d79f9e4d35d3e56f7e4de90d8f732d8a8c45`.

### Millennium DOS installed-directory evidence

The user-supplied `millennium-return-to-earth-2-2` directory under the default
data root was measured read-only on 2026-09-02. It contains the 31 regular
files from the recognised English DOS archive, with every filename, byte count
and SHA-256 ordered lexically and serialized as `name<TAB>size<TAB>sha256<LF>`.
The resulting complete-set SHA-256 is
`d938cd6a611a83897a745b257a371613b73a7dddffb2d336ec2167a192803783`.
Its `MILL.COM`, `TITLES.EXE`, `2200AD.EXE`, `TITLE.LIB`, `GX.LIB`, video
drivers, VOC bank, and initial save hashes match the existing English DOS
parser-profile leaves. It is now a runtime admission source: the scanner only
recognises it after every declared direct child is a non-symlink regular file
with its exact byte count and SHA-256, and rechecks that complete set before
each runtime admission. The set digest is deliberately separate from the
logical English DOS release/profile identity
`e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`; a
directory is never misrepresented as an outer archive. No ZIP is reconstructed,
and no source member is unpacked, copied, cached beyond the bounded lifetime of
the in-memory admission view, or mutated. Unrelated sibling files are not media.

The ADF reader follows the same rule: inspection code borrows bounded views of
a verified source image, while a long-lived native session takes ownership by
moving the already extracted image exactly once. It never creates a duplicate
whole-disk buffer merely to parse sectors or raw offsets.

The FAT12 reader applies the same ownership boundary to Millennium's DOS/ST
media: directory inspection and provenance parsing borrow a verified disk
image, while a session may take that image by move. A FAT cluster chain is
materialized only when a consumer explicitly requests its logically contiguous
file bytes; parsing the disk itself never duplicates the complete image.

`tools/extract_static_control_flow.py --dos-directory` applies this same
complete-set model to offline disassembly metadata. It reads the committed
direct-media-set ledger, rejects a relative or symlinked root, verifies every
declared direct child as an unchanged non-symlink regular file with its exact
size and SHA-256, verifies the lexical set digest, and only then decodes the
explicitly requested executable leaves in memory. Its sidecar labels this
provenance `verified-direct-media-member`, binds it to the logical release hash
and mandatory complete-set SHA-256, and never serializes a filesystem path,
source bytes, reconstructed archive, or
capture evidence. A partial installation, altered leaf, unknown release, or
requested non-member remains a preservation boundary.

The five current native bootstrap/opening adapters—Millennium DOS/Amiga/Atari
ST and Deuteros Amiga/Atari ST—are SDL-free engine factories behind that
boundary. They select only their named hash-verified leaf and return no
substitute if the selected release, language, or leaf is absent. The DOS
factory also retains the original `2200AD4.BIN` celestial-label table and
pointer-table topology as immutable source data for the selected English or
Spanish edition; it neither translates those bytes nor presents them as a
recovered game UI. It otherwise returns parser-only title and startup evidence;
it neither executes
an unproven handoff nor changes source bytes. Their resulting objects remain
bounded bootstrap/opening evidence; moving them out of the SDL layer does not
execute an unknown ABI or claim game parity.

Admission is transactional. After either the outer archive rehash or complete
direct-set revalidation succeeds, the coordinator constructs one bounded
read-only `VerifiedReleaseMedia` view and uses that exact verified snapshot for
every leaf requested by the selected platform adapter. It is destroyed when
admission finishes; no archive is unpacked, written, cached, or retained as a
replacement data source. The
coordinator then publishes exactly one platform-appropriate engine adapter and
its launch identity together. A failed parser/leaf admission leaves no active
identity or prior adapter. SDL owns only renderer, audio-device, and host-input
resources; it borrows the admitted adapter and never reloads a release
independently.

`NativeSessionController` reaches the coordinator only through the typed
`LauncherRuntimeController` facade. That facade returns copied snapshots or
transient audio/event buffers and offers no coordinator reference, so the
launcher cannot independently acquire an adapter, reopen source media, retain
a mutable input session, or bypass the return-to-menu revocation order.

The controller also derives one SDL-free `RuntimePresentationSnapshot` for
each live, internally consistent session. It contains only the declared
native state, presentation boundary, capabilities, input-contract identifier,
and stable labels. Menu, rejection, teardown, stale-state, and invalid
declaration combinations produce no snapshot. This gives every platform a
single UI-facing boundary contract without handing SDL original pixels, audio
samples, paths, archive handles, adapter references, or a way to create an
engine state.

The F10 developer readout reports the result as **ready**, **not selected**,
or one of three deliberately non-sensitive rejection classes: **identity**,
**archive hash**, or **adapter**. It identifies the preservation boundary that
declined a launch without exposing a local path, archive member, original
bytes, or parser exception.

The profile-card page mirrors a failed final admission using that same limited
vocabulary. This gives the user actionable startup feedback before opening F10
while preserving the same no-path/no-member/no-exception disclosure boundary.

CLI and card-menu candidates both cross `admit_runtime_launch`: it resolves
the candidate through the scanner's exact identities, rehashes the selected
outer archive through `ReleaseRuntimeCoordinator`, and constructs one typed
adapter. A missing/stale candidate resets the coordinator; no prior runtime
can survive a failed later route. Renderer pack preflight reads that one
already admitted identity rather than resolving menu fields independently.

`--launch-check` exposes this same final gate as a bounded CLI diagnostic. It
requires an explicit game/platform, emits only game/platform/language and the
safe admission result, and exits before SDL initialization. It is not an
emulator, replay, renderer, input path, audio path, or save operation.
`--launch-check-json` writes this same minimal result as
`project-eon.launch-check/v1`; it includes the exact release SHA-256 and
explicit presentation and renderer-geometry choices plus the stable,
media-safe `runtime_rejection` code. It never exposes a local path, member
name, original bytes, or parser exception.

`--runtime-diagnostics-json` is the fuller active-native-session report,
`project-eon.runtime-diagnostics/v1`. It crosses that same explicit
game/platform/hash admission gate and exits before SDL initialization. It
adds the admitted adapter/boundary/capabilities and the selected release's
declarative startup, recovery, and function-map facts. It does not expose a
path, archive member, original byte, guest state, trace contents, emulator
state, or a route for executing original code. `runtime_rejection` is `NONE`
after a successful admission; otherwise it identifies only the native gate
class (launch identity, original media, capability, adapter construction,
input contract, child session, or lifecycle transition). It is not an emulator result or gameplay
state.

Before it exposes those provenance records, the diagnostics composition
validates the compiled startup-boundary, recovery-map, and function-map
declarations as one no-I/O contract. Recovery records are one-to-one with
parser profiles; startup records are one-to-one with recognised releases; and
function records must have unique IDs plus an exact release/profile binding.
An inconsistent declaration fails closed for diagnostics rather than becoming
a cross-release fact. These maps remain read-only preservation metadata: this
diagnostic validation does not execute media and does not change native
runtime admission.

For a corpus containing more than one release for a game/platform, automation
must pass both `--release-language` and `--release-sha256`.  They are one
four-field identity with game and platform, not independent display filters:
a language/hash mismatch is rejected before SDL initialization and produces no
launch-check JSON.  The genuine-media launch test exercises this contract for
each recognised archive, including the co-installed Millennium DOS English
and Spanish releases; a default English selection is never evidence that an
explicit Spanish container was admitted.

The launcher carries the same provenance boundary before admission. Its
source identity is exactly game, platform, original-release language, and
outer SHA-256; Original/Modern/Custom presentation selection is not a source
change. Any change to one of those four source fields clears the separately
installed Modern-pack admission and invokes the full runtime reset before a
new card route can launch. Keyboard, gamepad, pointer, and touch all use that
same route. Its native data-source picker has separate explicit choices for a
directory and for one archive because the host API exposes those shapes through
different dialogs. The callback transfers only the selected path plus that
declared shape; the shared original-data-source classifier used by both GUI and
CLI accepts only a non-symlink directory or regular file and passes the
unchanged path to `ReleaseScanner`. It does not inspect, open, extract, cache,
or infer a release from the picker result. The scanner remains the sole
bounded, hash-addressed recogniser, so a selected regular file is only a
candidate until normal archive-size and SHA-256 verification succeeds.
Directory traversal applies the same non-symlink rule to every encountered
entry. Rejected links are counted only in the aggregate scan diagnostics; a
link target, its name, and any bytes outside the selected collection are never
opened or reported. `ReleaseScanSnapshot` is the one SDL-free diagnostic
projection shared by the card-menu `D` panel and `--inspect-json`: source
shape, scan phase/progress, aggregate rejection counters, and verified-release
count may be shown; media paths, filenames, archive members, and bytes may not.
If an incremental scan changes a previously unique platform into a
multi-release platform, Project Eon revokes its automatic release selection
and returns to the generated Original-release cards. Only a release identity
the user explicitly chose may remain selected across that discovery event; no
new container, language, or platform is substituted.

The generated macOS app bundle is smoke-tested before archiving with an empty,
isolated home directory. Its `--inspect` result must report the absent
`~/.projecteon` path without creating it; this tests the finished bundled
executable and dylib closure without requiring, copying, or packaging game
media.

Runtime diagnostics are also manifest-bound before composing recovery or
function-map rows. This identity check reads no path or media bytes; it rejects
a forged game/platform/language/hash DTO rather than letting it borrow another
release's diagnostic map.

When the recovered Deuteros Amiga opening reaches its exact `$0f` title-stage
handoff, the live runtime snapshot changes from **Deuteros Amiga Opening** to
**Deuteros Amiga Title Stage**. The latter is a no-input bootstrap boundary:
it retains only the already validated title-stage provenance and refuses later
opening ticks or host input, rather than presenting a fabricated title screen
or crossing the unresolved Exec ABI.

Millennium DOS likewise publishes a terminal runtime snapshot when its two
recovered startup observations reach an opaque original boundary. Selecting an
admitted English sound driver becomes **Millennium DOS Sound Driver Boundary**;
a nonzero `TITLES.EXE` console-poll result becomes **Millennium DOS Title
Handoff Boundary**. Both snapshots reject further host input and expose no
successor screen, audio, simulation, or save state until the original ABI and
return chain are evidenced.

Every runtime snapshot also carries an immutable input-contract identifier.
`millennium-dos-startup-observation` means only the recovered literal startup
observations may be forwarded (the English chooser byte or the verified DOS
console-poll result); it does not name a game action. `deuteros-amiga-opening-
held-signal` means only the recovered physical held signal exists during the
finite opening. All bootstrap and terminal snapshots use `none`. Admission
rejects a release capability record whose `admitted_input` flag disagrees with
that contract, and the native coordinator applies it as a typed fail-closed
gate before it reaches a release-specific input session. This makes the CLI
and F10 diagnostics useful for future Modern remapping work without inventing
original controls.

## Original and Modern mode contract

### Localized game-text presentation

Original preserves game behavior, media identity, and source bytes; it does
not require presenting English source bytes when the player selected another
UI language. Every recovered player-visible string in either mode is resolved
by `src/game_text_localization.*` from its exact game, platform, and source
text to a stable key and canonical catalog message. Menus, item names,
messages, help, and labels share this rule. Original and Modern call the same
resolver, so Modern cannot acquire a different translation or fallback path.
The Deuteros Amiga opening and title-stage snapshots preserve the admitted
prompt tokens across their lifecycle transition without retaining ADF bytes.
Presentation resolves those tokens by stable ID; merely admitting a prompt
does not prove that the current recovered state should display it.

Each registry row additionally records the original leaf name, complete leaf
SHA-256, byte offset, and byte length. `verify_game_text_source` rehashes the
whole supplied leaf and compares that exact range before a row can be treated
as preservation evidence. The first admitted set is the English DOS
`MILL.COM` sound-selection text at offsets 775, 799, 832, 868, 885, 904 and
933, plus the three displayed driver-table names at 1322, 1349 and 1358. Its
complete leaf SHA-256 is
`4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e`.
The genuine-media test reopens that installed leaf, verifies every range, and
proves that a one-byte alteration revokes admission.

The registry also covers all 41 bounded celestial labels already parsed from
both DOS static-data editions. English `2200AD4.BIN` SHA-256
`1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d`
uses offsets 978 through 1299; Spanish `2200AD4.BIN` SHA-256
`8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31`
uses offsets 987 through 1306. Each edition retains its exact spelling and
padding (for example `Earth ` versus `Tierra `) while both resolve to the same
stable semantic key and selected presentation catalog. Thus the Spanish
release is not translated by substituting English source data. Together with
the ten launcher rows, the map has 92 source-bound definitions and 51 unique
catalog messages.

The source byte sequence remains owned by the hash-admitted parser or runtime
snapshot and is returned in localization diagnostics unchanged. English uses
the canonical English presentation. Every other selected language must contain
a nonempty PO entry; an unknown source string or missing entry is rejected
rather than silently displaying English or text from another release. The PO
catalogs therefore alter presentation only: they never write game media,
runtime state, original saves, or reconstructed pixel assets.

Newly recovered player-visible tables use the source-range localization API.
It rehashes the complete immutable leaf and requires the declared leaf name,
game, platform, exact offset, and size before returning text. Its batch form
resolves all declared ranges in source order and fails atomically on a wrong
leaf, invalid or overlapping range, or missing catalog entry. This is the
shared contract for future object names, messages, status text, and game
vocabulary in Original and Modern; a plausible string alone is not evidence.
Runtime acquisition now converts the complete English or Spanish Millennium
celestial table into 41 copy-only provenance tokens while the verified
`2200AD4.BIN` leaf is available. Each token is revalidated against the compiled
source map when localized, so a stale or forged snapshot cannot change an ID,
range, source language, or canonical message. The runtime retains no complete
commercial source leaf for this purpose.
The English DOS runtime likewise converts all ten declared `MILL.COM` prompt
and driver-name ranges into copy-only tokens at admission time. SDL resolves
the multiline sound prompt and the selected driver name from those tokens;
it no longer accepts a bare hash/string pair from the presentation snapshot.
Original and Modern share this exact path. A missing or forged token aborts
presentation instead of leaking untranslated original text or guessing a
semantic identity. Other platforms receive the same treatment as their
player-visible tables are recovered; an empty token set grants no fallback.

Project Eon deliberately distinguishes a preservation result from an
enhancement. **Original** is the preservation contract: a hash-admitted,
recovered baseline is rendered and played without host-side feature
substitutions. Its compatibility claims must remain reproducible through the
corpus identities, bounded parsers, recorded observations, and reference
captures in this ledger.

**Modern** is an explicit opt-in improvement mode. It may add graphics,
resolution and aspect handling, scalable UI, contemporary input,
accessibility, runtime diagnostics, and evidence-documented gameplay or
quality-of-life changes. Every non-renderer improvement must declare its
enablement, affected state, save compatibility, evidence basis (if it claims
to reproduce an original behaviour), and test coverage. A Modern feature must
never silently become active in Original.

Modern graphics may go beyond post-processing filters. They may perform
asset-aware upscaling, regenerate sprite or scene presentation, or select a
separately installed, lawful high-resolution art layer. The active rendering
path and its inputs must be visible in the UI and diagnosable. Any
media-derived output is transient renderer state unless a separately
versioned, user-controlled cache contract is added; it must never be written
into, beside, or in place of supplied game media. Project Eon does not commit,
package, or redistribute original pixels or unlicensed derivatives.

Delivery verification rejects both recognised release containers and common
physical-media/archive variants (`.hfe`, `.ipf`, `.scp`, `.ctr`, `.lha`,
`.lzh`, `.lzx`) from desktop packages and iPadOS IPAs. This is a packaging
firewall, not a scanner admission rule: it prevents a future platform importer
or archival convenience file from causing redistribution before it has any
runtime meaning.

### External Modern asset-pack admission

`modern_asset_pack` implements a read-only, opt-in admission boundary for a
future separately installed high-resolution art layer. It discovers only
direct children of a user-selected root and validates `pack.eonmodern` without
creating a directory, following a symlink, traversing recursively, unpacking
an archive, or selecting any asset for rendering. The manifest binds one
declared game/platform to one exact compiled release SHA-256 and binds every
external asset to a safe relative path, byte count, and SHA-256. Thus an
unrelated file, a path traversal, a changed file, or an asset for another
edition cannot become a Modern substitute merely by name.

The two permitted provenance declarations are `independently-created` and
`licensed-derivative`; they record the provider's legal claim, not a legal
finding by Eon. No packs are shipped or accepted as proof of original
behaviour. V1 has two deliberately narrow renderer mappings: an explicitly
selected pack may provide `millennium.dos.title.png-640x400` or the preferred
4× `millennium.dos.title.png-1280x800` for the English Millennium DOS P00
title, or all 82 PNGs of the English Deuteros Amiga held-input opening route
at one complete 2×/4× tier. Each is a bounded (at most 8 MiB), exact 8-bit
RGBA PNG and remains Modern-only. The Deuteros sequence advances only from
the existing original VM tick and admits tick 82 only after the verified
original handoff; it does not add an opening clock, input mapping, audio
track, or title-stage simulation. Eon revalidates the manifest, release
binding and file hash immediately before decoding; it validates PNG chunks and
inflates the fixed-size RGBA IDAT scanlines before SDL_image receives the
bytes, then uploads only a transient texture. No pack can affect saves, input,
simulation, Original rendering, or original media. The precise syntax and
integration requirements are recorded in
[`MODERN_ASSET_PACK_FORMAT.md`](MODERN_ASSET_PACK_FORMAT.md).

Custom's native chooser applies the same validation as a pre-launch
admission gate against the currently selected release card. Its session-local
state is **unselected**, **ready**, or **rejected**; only **ready** retains a
manifest path for a future Modern loader. Selecting another game, platform,
release identity, or original-data source clears that state rather than
carrying an art candidate across preservation identities. The loader still
performs its final revalidation immediately before decode, so the UI result is
never a time-of-check substitute. Original mode clears an optional CLI pack
before any pack access. Modern's F10 developer readout may show only the
preflight admission state, compact pack ID/provenance, and declared renderer
targets. It never shows the local manifest path or validation error, opens no
pack, and labels targets as declared rather than uploaded or active; final
hash verification remains exclusively at decode.
path before it is read and never opens an external pack.

The explicit `--inspect --modern-packs <root>` diagnostic performs that
admission report only after the filtered original-release reports have been
rehashed. A valid pack is reported **eligible** only when its complete release
identity also occurs among those reverified reports; otherwise it is reported
as rejected for this invocation. The root must already be a non-symlink
directory and is never defaulted, created, scanned recursively, or selected
as a renderer input. `--modern-packs` without `--inspect` is a command-line
error, which prevents a pack scan from masquerading as a game launch. The
separate `--modern-pack <pack.eonmodern>` launch option requires explicit
`--game`, `--platform`, and `--presentation modern`; it never has a default
path. It can render only the documented release-bound Millennium DOS title or
Deuteros Amiga held-input sequence, and falls back to Eon's normal Modern
Scale2x surface if its selected external mapping is rejected or cannot be
decoded.

The pre-launch Custom dialog is a second explicit candidate route, not a pack
discovery mechanism. It requests a single manifest without a default location,
does not retain the directory or selection across sessions, and ignores a
cancelled dialog. Its asynchronous native callback only transfers a UTF-8 path
to the launcher; all filesystem access and the same manifest/release/hash/PNG
validation remain on Eon's main launch path. It cannot change a running
session, supply Original presentation, or make a filename or dialog filter a
trust decision.

### Transient Scale2x pixel reconstruction (Modern)

The current Modern path provides a deterministic, edge-aware RGBA **Scale2x**
reconstruction for Eon's already decoded, verified Millennium DOS title and
Deuteros Amiga opening surfaces. This differs from SDL nearest or linear
sampling: every output pixel is selected from the original pixel and its four
direct neighbours, so hard pixel-art edges can be continued without inventing
an interpolated colour. It is an explicit Modern graphics-popup setting;
Original always presents the original decoded texture path.

`reconstruct_rgba_scale2x` accepts a bounded, tightly packed RGBA span and
returns a separate in-memory buffer. It has no archive, file, cache, encoder,
or save API. The source span is const, the renderer discards the derived SDL
texture on exit, and native tests cover input immutability and malformed
surface rejection. This is a reversible presentation enhancement—not a new
game asset, a change to supplied media, or evidence about original rendering.

Both modes read supplied media in place. Neither mode may modify, replace,
unpack, redistribute, or use an original asset as a writable runtime cache.
Modern must not alter an original save file; any future Eon-native save or
extension data must be separate, versioned, and documented. Modern behaviour
is not evidence for Original compatibility, and Original-mode evidence never
authorises unlabelled Modern changes.

## Corpus identity

The initial supplied catalogue corpus has six outer ZIP archives and 67 leaf
assets. Native recognition uses complete archive SHA-256 and size, never
filenames.

### Direct-container recognition

The supplied catalogue archives also contain two exact single-disk ZIP
containers that are meaningful user-facing inputs in their own right. Eon
recognises the 425,912-byte Millennium Amiga Defjam container
`ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd` and
the 299,516-byte Millennium Atari ST Equinox container
`0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69`.
They respectively contain the same hash-addressed 901,120-byte ADF and
819,200-byte ST image already recovered from the catalogue wrappers, so the
existing bounded Amiga loader and Atari FAT12/STX readers apply without a
byte-level substitution. They are accepted only by their complete container
hashes; sibling crack, alternate, save-disk, multi-disk and filename-similar
containers remain distinct, unrecognised preservation evidence until their
own complete media set and runtime path are recovered.

### Scanner admission and duplicate accounting

The direct-media scanner discovers one filesystem entry at a time, then sorts
the complete regular-file candidate set lexically before hashing only files
whose byte length occurs in either the outer-release manifest or the
hash-addressed parser-leaf manifest. Discovery and hashing
share the launcher work budget, so a large data directory cannot delay its
first SDL frame. It never opens a ZIP, extracts a leaf, or selects a game by
name during recognition. A digest match is one *verified occurrence*. Equal
digest matches at multiple locations (including a user-created link) are
deduplicated to one release card and one CLI launch target; the first lexical
path is retained as the deterministic in-place source and later occurrences
are counted, not silently treated as separate editions.

`--inspect` prints aggregate `SCAN SUMMARY` counters: candidates, files rejected
by a size absent from both manifests (which are not hashed), outer
manifest-size matches, direct-media leaf-size matches, hashed
candidates, manifest-sized hash rejections, verified and duplicate occurrences,
unique releases, verified complete direct-directory sets, unique unbound
direct-media leaves, and unreadable candidates. A direct-set occurrence is
reported separately from an archive occurrence so the scanner never labels a
directory as a ZIP container.
The matching `--inspect-json` scan object carries the same aggregate totals
without paths, filenames, member names, or media bytes. The report deliberately
does not print unrecognised filenames or infer their platform: it makes admission and
scanner failures auditable while preserving the strict content-addressed boundary.
The launcher's `D` scanner panel consumes the same immutable aggregate snapshot,
including unique verified-release and duplicate-occurrence counts. It is a
presentation of the existing admission result, not a second scanner or a path
to launch an unrecognised candidate.

### Runtime session envelope

`ReleaseRuntimeCoordinator` publishes a `RuntimeSessionSnapshot` only after the
selected outer release is rehashed and exactly one typed platform adapter is
constructed. Before adapter construction, admission also reopens every unique
leaf named by that release's compiled parser profiles and checks its exact hash,
declared size and every profile interval bound. This is a fail-closed media
provenance check; it neither executes original bytes nor promotes a profile to
a playable path. The snapshot holds release identity, the adapter kind, narrowly
declared presentation/audio observation capabilities, and the next hard
boundary. It contains no filesystem path, archive member, original bytes, SDL
object, inferred input mapping, save state, or generic game model. The
Millennium DOS title snapshot alone declares `admitted_input = true`, and its
coordinator accepts only a literal ASCII sound-choice byte or the separately
observed nonzero DOS console-availability result. The Deuteros Amiga opening
snapshot likewise permits only its recovered held input signal and advances
one 20 ms opening tick through the coordinator; it does not name a generic
keybinding or authorize title/gameplay input. Every other input requires its
own caller-connected evidence. Resetting or rejecting a launch clears the
snapshot atomically with the adapter. This is shared runtime plumbing, not a
claim that a session is playable or frame-parity complete.
The F10 developer diagnostics page reads this same snapshot; its adapter and
boundary values are provenance codes, while the visible row labels remain
translated launcher chrome.

`NativeSessionController` is the SDL-free lifecycle state machine around that
coordinator. Its complete vocabulary is `MENU`, `ADMISSION REJECTED`, each
published Millennium/Deuteros session kind, the explicit DOS and Deuteros
title-stage boundaries, and `RETURNING TO MENU`. Launch, admitted input,
Deuteros opening ticks, rejection, and revocation synchronize through this
one vocabulary. SDL may render a menu, F10 modal, texture, audio queue or
diagnostic panel, but none of those can advance native session state. Opening
pixels and title-stage facts cross this boundary only as immutable snapshots;
Paula event submission and mixing remain coordinator-owned, and SDL receives
only transient float buffers. Before an SDL caller clears coordinator-owned
state it enters `RETURNING TO MENU`,
where launch, input and opening ticks fail closed; it then finishes the reset
at `MENU`. Stale source-derived resources therefore cannot be presented as a
new or active original release.

For an unfiltered inspection, `PLATFORM ADMISSION` additionally reports each
rehashed game/platform card state. `READY` has exactly one verified original
language, or has English among its verified languages and therefore uses the
documented English default. `RELEASE SELECTION REQUIRED` has several languages
but no English and cannot launch until a specific identity is selected.
Filtered reports omit this aggregate deliberately: their scope cannot
establish whether an unprinted sibling language is present.
The report is derived from scanner identities only and does not execute Atari
ST GEMDOS/XBIOS services, callbacks, or guest code.

The adjacent coverage field is independent of admission and comes from one
explicit game/platform table: `RECOVERED STARTUP` for Millennium DOS,
`RECOVERED OPENING` for Deuteros Amiga, and `BOOTSTRAP ONLY` for Millennium
Amiga and both Atari ST releases. It makes a successful card visible but does
not claim full native runtime parity. Each label identifies only the current
bounded startup/opening evidence; it does not mean a full game loop, all
assets, input, audio, state, or save parity.

`platform_coverage` is an SDL-free compiled contract consumed by the platform
cards, F10 diagnostics, `--inspect-json`, and `--launch-check-json`. This
keeps every user-facing diagnostic on the same release/platform recovery fact;
it has no media path, archive-member, or guest-execution capability.

Every card-menu launch first revokes the prior host-side session resources
(textures, queued audio, text input and source-bound state) before the shared
runtime gate rehashes and acquires the new exact release. This lifecycle rule
does not touch user media and prevents old release-derived presentation data
from crossing a launcher relaunch boundary.

Each rehashed Atari report includes `ATARI LAUNCH BOUNDARY`, matching the card
label to a release-specific limitation: Millennium stops before the GEMDOS
`TRAP #1`/`Fopen` result, input, and later launcher flow; Deuteros stops before
protected XBIOS/callback behavior, state selection, title, and gameplay. It
is a concise preservation diagnostic only, emitted after hash verification and
without invoking guest code.

### Scan-to-use identity binding

Recognition alone is not authority to parse a later version of a path. Before
each inspection report, title/bootstrap load, or reference-trace report,
Project Eon reopens the selected outer archive, verifies that exact in-memory
byte stream against the `ReleaseArchive` SHA-256, and only then walks its ZIP
directory or extracts a hash-addressed leaf. The release's game, platform and
language must also still form one exact compiled manifest record. A renamed,
replaced, truncated, or metadata-forged path is rejected rather than inheriting
an earlier scanner result. The original archive is still read in place; this
does not create a cache, unpacked copy, or mutation.

The same binding applies to external trace admission. A valid trace manifest
cannot lend provenance to an archive that changed after scanning: Project Eon
re-verifies the trace's selected outer archive before emitting its report.

### ZIP structural boundary

After an outer archive has passed that byte-identity check, its classic ZIP
directory is still treated as untrusted structure. Project Eon accepts only
unique, relative `/`-separated entry names (no NUL, backslash, absolute,
`.` or `..` component), and verifies every local header name, flags, sizes,
CRC and payload range against the central record before any leaf is exposed.
Local payloads and optional classic data descriptors must end before the
central directory; descriptors must repeat the central CRC and sizes. This
prevents a malformed archive from presenting ambiguous inventory paths or
from treating directory metadata as disk data. ZIP64, multi-disk, encrypted
and oversized records remain explicit unsupported boundaries.

| Game | Platform | Lang. | Bytes | Outer archive SHA-256 |
| --- | --- | --- | ---: | --- |
| Deuteros | Amiga | en | 4,066,771 | `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` |
| Deuteros | Atari ST | en | 3,021,682 | `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653` |
| Millennium 2.2 | Amiga | en | 2,558,009 | `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400` |
| Millennium 2.2 | Atari ST | en | 1,524,836 | `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01` |
| Millennium 2.2 | DOS | en | 328,383 | `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` |
| Millennium 2.2 | DOS | es | 330,050 | `b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4` |
| Millennium 2.2 | Amiga | en | 425,912 | `ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd` |
| Millennium 2.2 | Atari ST | en | 299,516 | `0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69` |

Leaf counts are 17 Amiga ADFs, 18 Atari ST disks, one DOS floppy, three
DOS flat executables, one DOS COM program, 14 audio files, 12 game resources,
and one unknown item. Alternate/cracked dumps are comparative evidence; clean
dumps are preferred as semantic baselines.

## Machine-readable release/profile manifest

[`release-manifest.json`](release-manifest.json) is the canonical interchange
manifest for the currently verified corpus. Each parser profile names an outer
release SHA-256, a leaf SHA-256 and size, plus the exact byte span used as its
present evidence. The compiled `release_manifest` table uses the same records
for native recognition and tests prove every declared profile against the
user-supplied archive. A profile is not transferable to a matching filename,
a different language, an alternate/cracked dump, or another platform. The
manifest contains no game bytes and does not ask the runtime to extract, copy,
or mutate media.

The manifest has a source-parity test: the compiled profile table must exactly
reproduce each JSON record (ID, outer and leaf identities, size and span). This
prevents preservation tooling and the runtime from silently diverging. It now
includes every English DOS leaf consumed by a recovered parser (game flow,
static data, overlay, both video drivers, last screen, title library, launcher
and first decoded voice), alongside narrower instruction/resource windows where
a parser intentionally has a smaller evidence boundary.

It also records whole-image evidence for parser families whose inputs are
recovered from an original FAT12 image rather than exposed as an archive leaf:
the Millennium Atari ST PRG, configuration and auxiliary-resource chains, and
the separate physical control-text dump. Spanish DOS title, static-text and
launch-manual parsing is likewise anchored to its original floppy image. These
full-image spans are explicit preservation boundaries; a future sector-accurate
span may replace one only with a measured mapping and a regression test.

## Declarative recovery map

[`recovery-map.json`](recovery-map.json) is the companion index for recovered
code paths. It borrows the auditability of a declarative function map without
adopting a recompilation hook model: each entry names an exact outer-release
SHA-256, one existing bounded `parser_profile_id`, CPU family, observed source
address, evidence level, and preservation-document anchor. The profile ID
must resolve through `release-manifest.json`; an entry cannot be reported for
a sibling dump, matching filename, related language, or another platform.

The compiled `recovery_map` table and JSON are source-parity tested. During
`--inspect`, after the archive has been rehashed, Project Eon prints only the
rows admitted for that release as `RECOVERY MAP` diagnostics. This is a
read-only provenance report. The map contains no guest-to-host hooks, patch
targets, replacement byte sequences, emulation directives, or inferred code
flow; it never executes source instructions and never changes original media.

The companion declarative function map is now admitted through the same
runtime media boundary. Before a native adapter is constructed, Project Eon
re-hashes the parser-profile span named by every function-map row for the
selected release and requires it to equal that row's `source_span_sha256`.
This prevents the CLI/F10 diagnostic map from describing a detached leaf or a
changed profile interval. It remains provenance only: a matching span does not
classify bytes as code, establish reachability or ABI semantics, or authorize
their execution.

The separate compiled `startup_boundary` table contains one exact release
identity, parser profile, first observed source address, and an explicit
unresolved boundary for every inventory release. It is accepted only when the
same release hash still admits that parser profile. F10's read-only Modern
diagnostics may display this navigation marker as `STARTUP BOUNDARY`; it is
not a dispatch address, emulator hook, or claim that execution beyond the
recorded boundary has been recovered.

The SDL-free `runtime_diagnostics` composition is the single reader for the
CLI JSON inspector and the F10 readout. For every selected, rehashed release
it rechecks the game, platform, language, release hash, and parser-profile
binding of every startup, recovery-map, and function-map record before that
record is displayed. A mismatch throws and suppresses the report rather than
mixing provenance from related editions. The composition reports
`trace_admission=not-loaded` for media inspection: scanning or choosing a
release never implies that a reference trace, ABI, renderer, or game session
has been admitted.

`--inspect-json` exports every safe field from those admitted declarations:
the recovery-boundary CPU and documentation anchor, and the function-map
documentation anchor and `address_space`, in addition to their identities,
source/runtime addresses, evidence level, status and uncertainty. A function
map address defaults to `runtime`; the explicit
`image-relative-unrelocated` value means the original image offset is known
but GEMDOS relocation/load base is not. The latter is never displayed or
treated as a guest program counter. It deliberately omits
local source paths, captured events and original bytes.

Every bounded parser profile in the release manifest has exactly one map row.
This includes data and format readers as well as recovered control-flow
boundaries: a map address may therefore be a verified leaf/file offset rather
than a runtime entry point. The address is provenance, not a reachability or
execution claim.

The map is evidence-neutral between Project Eon's modes. Original mode uses it
only to identify proven source boundaries. Opt-in Modern presentation may
display the same diagnostics, but gains no authority to alter original media,
saves, or recovered game logic from a map entry.

### English Millennium DOS reference-trace adapter

The generic external reference-trace validator remains v1 provenance and
ordering evidence. Its optional v2 `millennium-dos-en-startup-v1` adapter is
the first deliberately narrow semantic schema: it accepts only the clean
English DOS outer archive and only declared observations of the already
byte-locked `MILL.COM`/`TITLES.EXE`/`2200AD.EXE` startup sites. The accepted set is the
`MILL.COM` raw `INT 21h` vector request whose setup site is `$0209` (the
`INT` opcode is `$020c`), title wrapper `INT 91h` at
`$0127`, the `2200AD.EXE` entry's first private-wrapper `INT 91h` at `$0124`
with its literal `AX=$001f`, `ES=CS`, and `BX=$d19e` setup, title input poll
and exit sites, the `$02cf` video-driver-load
boundary, and `$0337` DOS EXEC request for `TITLES.EXE` or `2200AD.EXE`.

The adapter validates literal image names, loaded addresses, operands and
declared target names only after the normal external event-file hash and full
outer-release rehash. It reports counts as diagnostics. It never invokes an
interrupt, opens/loads a driver, selects a driver, performs EXEC, supplies a
result/carry flag/register, replays a trace, or changes original media, saves,
or runtime state. This keeps recorded observation useful for preservation
without promoting an external capture into an emulator or synthetic game path.

On 2026-08-30, a private, explicitly configured DOSBox-X recorder capture
from this exact outer archive was assembled and accepted by the CLI's v2
validator with two ordered records: the `$02cf` `mcga.bin` driver-load request
and the `$0209` setup-site/`$020c` `INT 21h` vector request. Its event-stream
SHA-256 is `dc4a67ce61ed6bcd32767c2bf354444f525176ba81308cb9374d7718d1b7aa9a`.
The event stream and all provenance preimages remain outside the repository.
This validates only that narrow external diagnostic record; no return, driver
outcome, EXEC, child execution, input result, frame, audio checkpoint, or
game state is admitted. See [the capture ledger](MILLENNIUM_DOS_CAPTURE.md).

The first subsequent raw result reconnaissance is separately bounded. The
source bytes continue from `$020c` to `$020e`, and an external normal-core
probe observed `AX=$2591` at that first post-interrupt instruction. The later
`CALL $0511` begins at `$0210`; its caller continuation is `$0213`, before
`AND AL,AL`. The input-free timed probe did not reach `$0213`. This is a
recorded absence within one bounded run, not proof of a non-return or a reason
to assign a DOS/private-driver ABI. It remains outside the v2 event grammar
and Project Eon runtime.

One later private capture made the literal original guest DOS command tail
`mill.com 0` explicit and injected no device input. Its external v2 stream
(`cb55a2ad7935da29ff2b698be0abb162a1d61532a750d4cb585b1cd4a366c89f`) was
assembled and CLI-accepted as three ordered observations: the same two
`MILL.COM` records and the existing hash-locked title-wrapper request at
`TITLES.EXE:$0127` with `AX=$0000`, `ES=CS`, and `BX=$1ac4`. The separate raw
observer also reached `$0213` with `AX=$0000`. The command tail, result, and
all preimages remain external. This proves neither a generic input model nor
any result, private-wrapper return, title frame, audio, EXEC, or game state;
it merely removes the earlier input-free-run limitation for that one declared
command-tail condition. See [the capture ledger](MILLENNIUM_DOS_CAPTURE.md)
for exact timestamps, executable/configuration hashes, and the unaltered
archive proof.

A third external normal-core run retained that same command-tail condition and
observed two raw returns at `TITLES.EXE:$0129`, immediately after the declared
private `INT $91` request: `AX=$0101`, then `AX=$0000`. The observations are
ordered, hash-recorded, and intentionally raw only. They establish neither
the private-vector ABI, flags, record writes, branch choices, title pixels,
input result, nor game state. Eon does not consume either word as a runtime
result. The separately assembled v2 candidate remains the same three
diagnostic request events, with all raw evidence external; see [the capture
ledger](MILLENNIUM_DOS_CAPTURE.md#title-private-vector-return-reconnaissance-not-a-v2-event).

The same ordered five records are now retained by the strict
`millennium-dos-en-title-init-v2` adapter. Its validator requires the exact
driver-load and request prefix, then exactly `AX=$0101` and `AX=$0000` at
`TITLES.EXE:$0129`, and reports the result only as diagnostics. This captures
the real observations without allowing a trace to supply a private-vector
result to runtime code. Exact manifest, recorder, and preimage hashes are in
[the capture ledger](MILLENNIUM_DOS_CAPTURE.md#first-strict-title-init-v2-trace-diagnostics-only).

### English Millennium DOS startup prefix

The clean English `2200AD.EXE` has a caller-connected, hash-locked startup
prefix at `$d2b0`. Project Eon applies its local `SS=CS`/`SP=$da00` setup and
then stops at the original private `INT 91h` wrapper (`$0124`, interrupt at
`$0129`). A continuation requires an explicitly recorded returned `AX` word:
the original bytes store that word at `$d128`, copy its `AH` to `$4368` and
`$da05`, snapshot `$da00` at `$d12c`, and select `$d1a1` only when that
observed byte is one. A second observed private-interrupt return permits the
equal route's literal `$da05=1` store and RET. Other observed selector values
reach the first `INT 10h` palette request at `$0476`; selector two additionally
performs its encoded `$0107=$b800` word store. Eon reports that first request
as register zero/value zero and does not invoke BIOS, infer further loop
iterations, fabricate either interrupt result, or modify executable/save
media. This is a recovery boundary, not a claim that the original private ABI
has been reimplemented.

### Stable evidence anchors

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Deuteros Amiga clean system ADF | 901,120 | `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38` |
| Deuteros Amiga clean data ADF | 901,120 | `99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a` |
| Millennium Spanish `2200AD.EXE` | 54,566 | `9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6` |
| Millennium Spanish `GX.LIB` | 311,420 | `e27d1c697da677994e2f864a776f4fc900c7feb4ec4b85500b2bfea3bc834767` |
| Millennium Atari ST Equinox disk | 819,200 | `3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7` |
| Millennium Atari ST `DATA12.BIN` | 932 | `6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda` |
| Millennium Atari ST `MILL22B.INF` | 84,720 | `e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9` |
| Millennium Atari ST `MILENIUM.TOS` | 49,269 | `4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686` |

## Verified format knowledge

- Nested ZIP parsing validates ranges, Deflate completion, output size, CRC-32,
  and SHA-256 before classification.
- ZIP parsing additionally rejects multi-disk/ZIP64 archives, encrypted
  entries, inconsistent central-directory extent, and local-header method,
  flag, filename, size, or CRC disagreement. For data-descriptor entries,
  central-directory CRC and sizes are authoritative because local fields may
  be placeholders; non-descriptor local values must match. The classic
  central directory must end exactly at the EOCD, and the EOCD comment length
  must reach the physical end of the supplied stream; a marker-looking value
  inside a comment cannot hide the actual directory. These checks are applied
  before recursively reading any DOS, Amiga, or Atari ST leaf bytes.
- DOS and Atari ST media use a native read-only FAT12 reader with validated
  geometry, bounded cluster chains, loop detection, and directory parsing.
- Standard Amiga ADF geometry is 80 cylinders × 2 sides × 11 sectors × 512
  bytes. Both clean Deuteros boot blocks pass the carry-around checksum.
- Deuteros identifiers are `DOS\0` (system) and `DEU\0` (custom data). Logical
  block 880 is game code/data rather than a normal AmigaDOS root directory.

### Amiga and Atari ST corpus boundary census

The platform labels in outer and inner archive names are catalogue metadata,
not release identity.  The following census is anchored in the complete leaf
image hashes and container bytes, rather than treating a `cr`, `a`, or `save`
name as a semantic property of the original program.

| Corpus | Supplied leaf media | Container fact | Admitted entry evidence | Documentation/control status |
| --- | --- | --- | --- | --- |
| Millennium Amiga | Six ADFs: five 901,120-byte images and one 698,368-byte image | `DOS\0` is present, but the usable program path is raw-sector data; the valid Razor filesystem has no game files. | The Defjam bootstrap requests ADF `+$6e000`/`0x24200` to `$41000`, then `+$2c000`/`0x16400` to `$68000`; the first transfer is materialized natively and stops at its `$41000` entry. | There is no live standalone manual recognised by the bounded filesystem readers. Visible function-key trainer text occurs only in altered variants and is not original control evidence. |
| Deuteros Amiga | Clean system/data ADFs plus comparative alternate, save, and modified images | The clean system disk is `DOS\0`; clean data disk is `DEU\0`, whose logical block 880 is custom raw data rather than an AmigaDOS root. | The clean system boot path loads `$5800` to `$20000` and has entry `$21734`; title-stage transfer remains separately bounded. | A genuine on-disk text block contains load/save prompts, but no caller-connected input binding is yet recovered. |
| Millennium Atari ST | Two physical-dump `.stx` images, one save image, and four one-disk `.st` variants | `.stx` is retained as a physical-media container and is not silently converted to a flat FAT image.  Five of the seven supplied images have a valid FAT12 volume. | The hash-identified Equinox FAT12 image admits a fully relocated native `MILENIUM.TOS` image and an exact read-only `MILL22A.inf` compatibility load. Native control follows `JSR $2a500`; an explicit typed SR observation selects either exact branch at `$2aa88`. A typed XBIOS selector-2 result then advances both paths from `$2a520` through the exact result store to selector 3 at `$2a52e`. The original Disk 1 STX has a bounded sector index, but no executable handoff has yet been linked to its physical loader bytes. | Original physical-dump bytes contain visible mouse/keyboard and prompt text, but no code-to-input map has been recovered. |
| Deuteros Atari ST | Eleven 737,280/1,056,768-byte `.st` images | The 737,280-byte game-media candidates have a BPB-shaped boot sector, but their apparent root records are not a live FAT12 namespace: entries carry impossible cluster/size combinations.  The raw protected boot chain is authoritative. | The hash-identified raw chain reaches the first and second stages through explicit nine-sector reads; its XBIOS callback and state selection remain boundaries. | The supplied game-media variants contain no standalone manual.  Embedded prompts are preserved as raw text only; a separate 1,056,768-byte development/tools disk is excluded from game-control evidence. |

This protects two easy-to-make mistakes: a structurally plausible BPB does not
prove a usable FAT filesystem, and a printable string does not prove an input
binding.  A future decoder must preserve the original container selected by
its hash, read it in place, and reject rather than substitute a different
platform's filesystem or executable.

The clean Deuteros Amiga system ADF has a directly observed embedded text
region at ADF `$78bc0..$78d0f` (336 bytes, SHA-256
`66b312b5e7b148bdfe0e43af4d6cc6f4b451ed05f83be8edd7a1e11f17264680`).
It includes the original byte sequences `LOAD`, `Press 'L'`, `SAVE`, and
`Press 'S'`, followed by the original data-disk prompt.  The raw text is
retained unchanged, but neither its record framing nor a code reference from
the input dispatcher to this block has been demonstrated.  Project Eon
therefore does not expose `L`/`S` as reconstructed gameplay controls.

Six unambiguous prompt literals within that block now have stable presentation
keys: the hardware-failure message and the five-part data-disk confirmation
prompt at ADF `$78c71`, `$78c93`, `$78caa`, `$78ccd`, `$78ce7`, and `$78cf3`.
Admission rehashes the complete clean system ADF
`6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`
and verifies every exact range before retaining copy-only tokens in the native
Deuteros Amiga session. Original and Modern resolve those same tokens through
all shipped PO catalogues. This does not establish record framing, expose a
disk-swap or confirmation control, or assign semantics to the adjacent
`LOAD`/`SAVE` fragments.

The Millennium Atari ST Disk 1 physical dump has a separate raw text
span at `$12420..$1258f` (368 bytes, SHA-256
`6330b762858bb4b1fb0bc17f4f577eca3b1e8de4c078fd3fc01192bcd05a89f7`).  It
contains `SAVE GAME`, `LOAD GAME`, `press left button to continue...`,
`MOUSE MODE`, and `KEYBOARD MODE`.  This is direct content evidence from the
SHA-256 `081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd`
physical-dump leaf, not an STX filesystem claim, executable entry point, or
SDL mapping.  It remains inspection-only until physical-track decoding and a
caller-connected control trace establish its use.

That physical-dump evidence belongs only to the recognised multi-member Atari
release that actually contains the STX leaf. The standalone Equinox archive
contains only the `$3f0906...` flat ST image, so its parser/recovery ledger does
not claim the `$081d8b...` STX as an available member. This is also an
admission invariant: selecting the standalone archive must start its native
Equinox bootstrap without borrowing unrelated corpus media.

`MillenniumAtariPhysicalControlTextEvidence` accepts only that full
physical-dump SHA-256 and the exact 368-byte span. It locates the five
printable literals at raw offsets `$12425` (`SAVE GAME`), `$12436` (`LOAD
GAME`), `$12445` (`press left button to continue...`), `$1255d` (`MOUSE
MODE`), and `$12572` (`KEYBOARD MODE`). This is a bounded preservation parser,
not an STX decoder, a menu model, or a control map: it neither assigns a key
or mouse action nor attempts to execute the protected physical media.

The same Disk 1 container now has a read-only, hash-bound STX sector index.
Its 80 track records expose 800 identified sectors without producing a flat
disk image. Its direct BPB is internally consistent with that physical view:
512-byte sectors, two sectors per cluster, one head, ten sectors per track,
800 sectors, two five-sector FAT copies, and a 112-entry root. The root is
read directly from logical sectors 11 through 17 through the STX CHS map;
six live 8.3 records are retained (`EXEC.TOS`, `MILL22A.INF` through
`MILL22D.INF`, and `DESKTOP.INF`). The two original FAT copies are **not**
identical: their exact five-sector SHA-256 values are
`2421cedef5612bca7bbc90168a7338d904f82ea1fdc09214c684424b428d9417` and
`22c2c826ed3de246e506187e16aea375dc2fee09a03abbc9140ebdd251640879`.
`AtariStStxFat12Root` reports both and deliberately declines to certify any
cluster chain or select either copy when they differ. It neither flattens the
STX container nor extracts a file or claims a boot or program handoff. Direct
original spans are T0/H0/S1 at container `$c0` (512 bytes,
SHA-256 `d0601ec6e1bbea0d5f4d5ba37130148e6670225b6337d001f4d4e6b8fc45fd08`)
and T1/H0/S9 at `$1570` (512 bytes, SHA-256
`096869a11a3f601c587bb915c6c93d7985f8eb2185dc2d0f2839286df9905dad`).
The latter contains the literal `MILL22B.inf` at sector-relative `$be`.
These are physical-container provenance facts only: Project Eon performs no
STX flattening, file-payload extraction, boot interpretation, or executable
handoff from them. The sector-backed root/FAT scan is read-only metadata, not
a claim that the container has been turned into a flat filesystem image. This
is read-only metadata traversal of physical sectors, never a flattened-media
substitute.

### Millennium Atari ST relocation evidence

The Equinox FAT12 `MILL22B.INF` chain is separately hash-identified (84,720
bytes, SHA-256 `e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9`).
At file `+$11600` it has the isolated NUL-terminated literal `MILL22E.INF`;
the immediately preceding 14 bytes end in `RTS` at `+$115fe` and hash-lock the
literal's local provenance. This is not evidence that any routine opens
`MILL22E.INF`, chooses one of its records, or decodes graphics. Project Eon
records the name only through its original FAT chain and does not render its
packed contents until a loader ABI, codec bounds, palette association, and
planar layout are independently recovered.

### Millennium DOS `LAST.LIB` screen evidence

The English DOS release contains `LAST.LIB` (18,117 bytes) with one literal
directory entry, `last`, at offset `0x6`.  The entry is a complete codec-2
indexed bitmap: flags `0x07`, 318 × 197 pixels, palette indices 0–15, and a
native 256-entry VGA RGB6 DAC plus its 16-entry logical-to-DAC translation.
Project Eon decodes that resource directly from the supplied archive in memory
and retains its original indexed pixels and palette before presenting RGBA.
The indexed-pixel SHA-256 is
`b13d52cab4ee715be28bca56997157fa102eaf86f53b0771c6b072dc0b701136`; the
derived RGBA SHA-256 is
`c0a556f3e618585967b9ed3d6c0606f958434c94def1afd0940658786a88dd17`.
The name `LAST.LIB` alone is not treated as proof of a narrative or gameplay
transition; no selection point is inferred until executable control-flow or
an original observation supports one.

The verified Equinox `MILENIUM.TOS` PRG has 227 compact GEMDOS relocation
sites. Project Eon retains each site and the original unrelocated big-endian
longword, without choosing a load base or producing a relocated copy. The
first site is offset `0x6`, value `0x0000115e`; the last is `0x1150`, value
`0x000139c8`. These values are read straight from the TEXT+DATA bytes in the
SHA-identified disk file and are native test anchors for future execution
research.

The exact PRG's loadable TEXT+DATA range (file `+0x1c`, 49,010 bytes) now also
has a byte-complete linear candidate report. Its retained external report
SHA-256 is `8c4acf574f52890a407f881e44bf41f4bb51ae5ccc7afd6ad240018bb30cc548`
(17,519 lines). Every address in that report is an unrelocated PRG
image-relative offset, not a GEMDOS runtime address. The report therefore
does not establish a load base, applied relocation, reachability, code/data
classification, TOS/XBIOS return, or executable behaviour.

The earliest literal TEXT path is independently anchored too. Entry offset
`0x0` is `BRA.W 0x24`; that bootstrap loads `A0 = 0x115e`, `A1 = 0x1232`, and
`A2 = 0x1d636`, then post-increment copies longwords while `A0 <= A1`. It
therefore transfers exactly `0xd8` bytes (inclusive source range
`0x115e..0x1232`) from original DATA into BSS at `0x1d636` and makes an
absolute `JMP 0x1d636`. Project Eon validates and reports that path without
choosing a GEMDOS base, creating a relocated image, or executing the
as-yet-unanalysed transferred bytes.

The copied bytes start with a second strict stub: `MOVEA.L #0x77000,A1`,
`MOVEA.L #0x1d652,A0`, `MOVE.W #0x100,D0`, `MOVE.W (A0)+,(A1)+`,
`DBF D0,-4`, then `JMP 0x77000`. Thus it requests 257 original 16-bit words
from the literal address `0x1d652` into `0x77000` before the next transfer.
The source's provenance is now fully accounted for from the PRG layout and
that bootstrap. `0x1d636 - (TEXT + DATA)` establishes the observed load base
`0x116c4`, so `0x1d636` is the first BSS byte. The requested source begins
`0x1c` bytes into the transferred bootstrap: its first `0xbc` bytes are the
original DATA range `0x117a..0x1235`; its remaining `0x146` bytes lie in the
declared BSS and are therefore the loader-zeroed tail. Project Eon can form
that exact 514-byte source only in memory, retaining the original DATA bytes
and explicitly zeroing only the PRG's BSS portion. It neither unpacks nor
writes media, applies no relocation, and still does not execute the jump.

The second copy is now materialized only as that exact in-memory 514-byte
target at `0x77000`. Its verified first instructions are `MOVE.W #0x2,-(A7)`,
`MOVE.L #0x1d6e4,-(A7)`, and `MOVE.W #0x3d,-(A7)` (followed by the original
`TRAP #1`). These bytes are validated against the source transfer and reported
for preservation; Project Eon does not invoke the trap, emulate a GEMDOS call,
or infer anything about the target's gameplay meaning.

The strict next boundary is now accounted for without crossing it. The literal
pointer `0x1d6e4` lies `0x92` bytes into that reconstructed source and names
the original NUL-terminated `MILL22A.inf` string. The target pushes access mode
`0x0002`, that pointer, and selector `0x003d`, then executes `TRAP #1`; this is
the documented GEMDOS `Fopen` interface. It then pushes the returned `D0` word
and selector `0x003e` (the GEMDOS `Fclose` selector), but no second `TRAP #1`
exists in this proven range. The following `TST.L D0; BMI.S -2` tests the
`Fopen` return and branches back to its own branch opcode on a negative OS
return. Project Eon records those exact offsets (`+0x0e`, the prepared
`Fclose` selector at `+0x12`, and the `+0x18` self-loop) and validates the
filename bytes. It does not issue either service, model GEMDOS return values,
turn the loop into host behaviour, or infer the later successful control path
as gameplay.

The requested file is genuinely supplied rather than synthesized. On the
SHA-identified Equinox disk, the FAT12 root entry for `MILL22A.inf` starts at
cluster 3 and is 7,506 bytes. Its exact file-chain SHA-256 is
`74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6`; its
first recovered words are `0x4ef9 0x0002aa88`. Those are retained strictly as
file and machine-word facts, not interpreted as a configuration schema or
executed. `tools/analyze_atari_st_config.py` can produce an external,
byte-complete M68000 candidate report for this exact file. Its offsets are
file-image-relative and its entry is explicitly unproven: the recovered Fread
setup has competing candidate load locations, so the report neither chooses a
runtime base nor claims reachability. A read-only inventory scan covers all seven supplied Millennium ST
images: five expose valid FAT12 volumes and four contain a regular
`MILL22A.inf` entry. The other two are raw/protected media and therefore have
no FAT12 pathname namespace to substitute. Project Eon reads a present entry
only through its original cluster chain in memory, and never creates, changes,
or falls back to a synthetic `.inf` file.

The release-wide FAT12 diagnostic now distinguishes a matching filename from
the exact launcher pair. Of seven supplied Atari leaves, five are readable
FAT12 volumes and four contain a regular `MILL22A.inf`; all four have the
same verified configuration hash
`74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6`.
Only one volume contains the exact `MILENIUM.TOS` hash
`4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686` used
by the bounded Equinox bootstrap. A shared configuration filename or hash is
not permission to pair it with another executable, flatten an STX image, or
use another variant as a launch fallback.

The Equinox bootstrap session additionally verifies both the complete original
819,200-byte FAT12 image (`3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7`)
and its supplied `MILENIUM.TOS` chain before materializing any local loader
state. This keeps an otherwise valid FAT12 filesystem or matching configuration
file from being joined to unrelated program bytes.

The same exact Equinox FAT12 root is now retained as a 13-file,
cluster-addressed inventory. `DESKTOP.INF` is cluster 2, 555 bytes, SHA-256
`ce2aa85b442be281f25c22456c0d081d01b51108e96716bba9f867b7e791ab19`;
`MILL22A.INF` through `MILL22F.INF` occupy their original root records; the
four 7,313-byte `2200SAVE.*` files, `DATA12.BIN` at cluster 442 (932 bytes,
SHA-256 `6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda`),
and `MILENIUM.TOS` at cluster 540 (49,269 bytes, SHA-256
`4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686`)
complete it. `MILL22E.INF` remains an opaque 302,892-byte original cluster
chain (cluster 122, SHA-256
`9aeb6aafceab228521725ffe687cd3d95406d7f272bca77f855ebb600664b2af`).
The inventory verifies each original FAT chain and digest before exposing it
to the bounded Atari bootstrap session. It establishes neither load order nor
file semantics: no `.INF`, save, data, or desktop file is opened, decoded,
written, or substituted because it appears in this evidence table.

| Original FAT12 entry | First cluster | Bytes | SHA-256 |
| --- | ---: | ---: | --- |
| `DESKTOP.INF` | 2 | 555 | `ce2aa85b442be281f25c22456c0d081d01b51108e96716bba9f867b7e791ab19` |
| `MILL22A.INF` | 3 | 7,506 | `74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6` |
| `MILL22B.INF` | 11 | 84,720 | `e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9` |
| `MILL22C.INF` | 94 | 9,597 | `a28a49eea33a14210193bbe6e36abf95700ac6789681bf1a9eac5d09a0999055` |
| `MILL22D.INF` | 104 | 18,428 | `de0a95d3e4659a305b3e55b3417a7648127b41866de0a0ca344a81c66979dbc0` |
| `MILL22E.INF` | 122 | 302,892 | `9aeb6aafceab228521725ffe687cd3d95406d7f272bca77f855ebb600664b2af` |
| `MILL22F.INF` | 418 | 22,123 | `26ef995a9c6a43647e7905477168980159d1426d90f901d4f4c32f7cf13e455e` |
| `2200SAVE.I` | 440 | 7,313 | `b0b91572a7cc8ca0b7b112a8ce09bcf0c6645c6b32df836ae8c2eb27d86c333a` |
| `2200SAVE.II` | 448 | 7,313 | `fa11ee72b3ca009d8a5d6cece8ff3f95b01b29ed53106e2d3730c9a545400065` |
| `2200SAVE.III` | 456 | 7,313 | `54519e0eebfe3f3a38b04e4b372caf67476148c135dafbfe8d0a4bcae601eae2` |
| `2200SAVE.IV` | 464 | 7,313 | `8c1709bb7aba3adc2e6538867383229c4d6a285d29a78fb431970d0d926ffbd2` |
| `DATA12.BIN` | 442 | 932 | `6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda` |
| `MILENIUM.TOS` | 540 | 49,269 | `4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686` |

### Millennium DOS/Atari ST save-artifact comparison

The verified English DOS archive contains a 9,538-byte `2200SAVE.I` with
SHA-256 `a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7`.
The verified Equinox Atari ST disk contains the four 7,313-byte
`2200SAVE.I`–`2200SAVE.IV` files listed above. `MillenniumAuthenticatedSave`
admits only those five platform-plus-original-filename-plus-size-plus-digest
tuples and retains each supplied byte stream in memory only. It has no save
writer, serializer, import path, or compatibility fallback.

`MillenniumSaveByteComparison` is deliberately narrower than a format parser:
it reports shared physical length, equal/different byte positions, common
prefix/suffix length, and each side's unmatched tail. It does not name fields,
infer slot state, assert cross-platform compatibility, or treat matching bytes
as shared gameplay data. Against the supplied original media, DOS
`2200SAVE.I` and Atari ST `2200SAVE.I` share 7,313 positional bytes, of which
6,030 are equal; their common prefix and suffix are both zero and the DOS
artifact has a further 2,225 bytes. Atari `I`/`II` share 6,719 of 7,313 byte
positions (common prefix/suffix 22/6); Atari `III`/`IV` share 6,607 (4/8).
These are reproducible byte facts, not an inferred save schema. Any altered,
wrong-sized, wrong-named, or unrecognised save is rejected before comparison.

`project-eon --inspect-save <path>` is a separate, read-only diagnostic for a
user-supplied Millennium DOS `2200SAVE.I` file or the verified English DOS
archive which contains the supplied reference save. A direct file accepts only
the recovered 9,538-byte `$0056` envelope. An archive is admitted only after
its complete hash selects the English DOS release, then the expected save hash
is read directly in memory; it is never unpacked or copied to disk. Both
routes report the save SHA-256 plus all 38 recovered columnar records. A hash
matching the supplied English DOS initial save is explicitly labelled as that
reference artifact. Any other structure-valid direct input remains
**unverified provenance** and is reported only as a structure observation: it
is not selected as a release, persisted, converted, loaded into a runtime, or
written anywhere. This makes later
user-save preservation work auditable without confusing file-format validity
with an assertion of original-media identity or save compatibility.

The nonnegative fall-through after the self-loop is byte-verified. At
reconstructed target `+$1a`, 26 original bytes have
SHA-256 `663d5f1418326aa9c0efde064ad95bda21c84d7f23241ce3505f21f1f07474d0`.
They push literal buffer `$2a500`, count `$20000`, the OS-owned `D0` handle,
and selector `$003f`, then issue `TRAP #1`; `ADDA.L #12,A7` immediately
cleans the prepared arguments. `$003f` is the documented GEMDOS `Fread`
interface. Eon's exact-media compatibility path owns a private read-only
handle, reads all 7,506 bytes of the matching FAT entry into `$2a500`, and
never exposes create/write semantics even though the original access mode is
write-capable. This native result is not claimed as an observed TOS D0 value.

The immediate 14-byte suffix after that static Fread boundary is now
hash-locked as the loader-to-configuration-buffer transfer boundary. At
reconstructed target `+$34` (address `$77034`) its SHA-256 is
`845d677c7c17d2152f0e89e0a396b6bbfb1ed6a75479a325b39310bbf0d99e58`.
The original words are `TRAP #1`, `ADDA.L #12,SP`, and `JSR $2a500`; `$2a500`
is exactly the Fread destination previously prepared in the same immutable
target. The production runtime applies the exact configuration as one atomic
memory batch and follows this JSR plus the payload's leading absolute JMP.

The exact first six bytes of the supplied `MILL22A.INF` are
`JMP $2aa88` (`4ef90002aa88`, SHA-256
`5c2fb1d412ca66ba8928a77c22eb0351ab5d3d6fd9c04cff1b037f25a94c7829`).
If file byte zero occupied the `$2a500` Fread/JSR destination, this jump would
name file `+$588`. Native Fread establishes exactly that mapping. The
independently hash-validated candidate entry is file `+$5aa`, 34 bytes later;
the intervening bytes are retained as executable original code, not discarded
as a load-address disagreement. Native control reaches `$2aa88`; only an
explicit generation-owned SR/privilege observation may select its branch.
Both exact branches converge at `JSR $2a51c` and stop before XBIOS `TRAP #14`
selector 2 at `$2a520`.

The live Millennium Atari bootstrap session now executes a deliberately tiny
local interpreter for only the two proven in-memory copy loops from
`MILENIUM.TOS`: 54 original longword copies to the BSS bootstrap, followed by
257 original word copies that materialize the 514-byte target at `$77000`.
The execution record pins entry PC `$0`, branch PC `$24`, BSS entry `$1d636`,
and first trap address `$7700e`. Production acquisition additionally owns the
exact PRG relocation image and narrow read-only config service described
above. It reaches `$2aa88` autonomously and `$2a520` after a typed SR
observation. There is no general
68000 decoder, host stack, or GEMDOS implementation, and no Atari display
state is fabricated.

The static evidence records below are the contracts used to validate that
narrow service. Statements that a parser does not select a result describe
the parser in isolation; production selects only the exact, hash-bound
read-only compatibility result documented above.

The caller-connected local path also executes the target's complete 14-byte
Fopen prefix before that boundary. Its three original pre-decrement writes—
access mode `$0002`, pathname pointer `$0001d6e4`, and selector `$003d`—are
recorded as eight relative stack bytes in final memory order: `$003d`,
`$0001d6e4`, `$0002`. The record has stack delta `-8` but no chosen initial
A7 address: Project Eon does not allocate a host stack, invoke GEMDOS, choose
a file handle, or follow either result path. Execution stops at the original
`TRAP #1` at `$7700e`; this adds only byte-proven local 68000 effects between
the BSS jump and the existing operating-system boundary.

The immediate post-`Fopen` result gate is separately symbolically executed,
without supplying a GEMDOS result. Its ten original bytes at target `+$10`
(SHA-256 `d124b586e52a783689925186d8cc93366870526fd894567b7c55761a617807c7`)
encode `MOVE.W D0,-(A7)`, `MOVE.W #$003e,-(A7)`, `TST.L D0`, and `BMI.S -2`.
The first saved word remains an opaque D0 dependency; Eon records only the
known additional stack delta of `-4` and both original successors: the
negative self-loop at `+$18` and the nonnegative static Fread-preparation
entry at `+$1a`. It does not choose D0's sign, materialize a handle, invoke
Fclose/Fread, or treat either successor as dynamically taken.

The gate's nonnegative **static** successor now has its next local execution
prefix recorded too, but is not selected as a runtime outcome. At target
`+$1a`, 18 original bytes push literal Fread buffer `$2a500`, count `$20000`,
the opaque GEMDOS-owned `D0` handle word, and selector `$003f`. The resulting
relative frame has delta `-12`; its handle slot is explicitly reserved at
frame bytes `+$2..+$3` rather than populated by Eon. The prefix stops at the
original Fread `TRAP #1` at target `+$2c`. This is a caller-linked symbolic
continuation from one encoded successor, not a claim that Fopen succeeded or
that Fread is reached, called, or supplied a result.

The SDL launcher creates this bounded session only for the exact identified
Equinox image when the Atari ST Millennium card or CLI target is selected; it
does not reuse the DOS title flow for that platform.

The Equinox payload's first JMP is now traced through its first proven control
block. Its absolute references establish an observed load base of `0x2a4de`,
which resolves the target `0x2aa88` to file offset `0x5aa`. At that offset the
original sequence first invokes `TRAP #14` with selector `0x15` and a zero
longword argument, then `TRAP #14` with selector `0x06` and longword
`0x2a612`. It contains literal JSR destinations `0x2b55a`, `0x2aa68`,
`0x2aa0c`, `0x2b2be`, `0x2b448`, and `0x2aa0c`, before `PEA 0x2ab0a`, a final
`TRAP #14` selector `0x26`, and `RTS`. Project Eon verifies this exact byte
sequence in the original FAT chain and reports the control facts. It does not
name or emulate trap effects, execute the JSRs, synthesize a configuration,
or write any disk data.

The preceding loader bytes independently name an Fread destination of `$2a500`
and then encode `JSR $2a500`; the exact read-only compatibility service now
supplies that buffer. The supplied leading `JMP $2aa88` maps to file
`+$588`, rather than the separately bounded `+$5aa` candidate block. The
intervening 34-byte original prefix `$2aa88..$2aaa9` (file `+$588..+$5a9`,
SHA-256 `dede20eddbd8015da1d1a4f2f5e53424c2bc2195bff238d830ea24c9f522ea59`)
has an SR-dependent branch to `$2aaa4`; a typed SR observation selects the
original branch. The supervisor path applies only the directly encoded
`MOVEP.W`, `MOVE.B`, and SR effects; the user path bypasses them. Both execute
`JSR $2a51c` and stop before its first XBIOS trap at `$2a520`. A typed observed
selector-2 D0 result admits the exact `ADDQ.L #2,A7`, big-endian store to
`$2a50a`, and selector-3 stack prefix, stopping at the next trap at `$2a52e`.
The 20-byte continuation hash is
`751915c217471e4763ebeef2928dc4cca68bc481dae3113adabb441c2446ee2f`.
The following typed selector-3 result admits three more exact instructions:
stack cleanup, an atomic big-endian D0 store at `$2a50e`, and the selector-4
stack prefix. The bytes `$2a52e..$2a53d` hash to
`f4a7b019591ccff43e4478ac1549e262387ebfb22c16ded18457fe2aca6bbcc2`.
Execution stops before selector 4 at `$2a53c`; its result remains external.
A typed selector-4 D0 result admits only the exact low-word store at `$2a512`.
The enclosing 12 bytes hash to
`42c6d7ede7609ced9c859e6222d678edf861018b86ee80be2cfe6f8a23010e44`;
execution now stops before Line-A initialization opcode `$a000` at `$2a546`.
No Line-A registers, pointers, display state, or firmware effects are inferred.
A typed observation may provide returned A0 plus the exact values read at
`8(A0)` and `12(A0)`. The local 24-byte block hashes to
`1705523f57debe7644c3a874cd76e42464f1f34f227c9ee1247026afdb2f3539`;
the values are committed together at `$2a514/$2a518`. RTS reaches `$2aaaa`,
whose eight deterministic bytes hash to
`37f9fb95e45dc6c4807821ac79189a2d764fffe6bbbef6196ee17f3ad1a18684`,
then stop before XBIOS selector `$15` at `$2aab0`. No Line-A firmware state or
selector-$15 result is invented.
A typed selector-`$15` observation records the unused D0 return. The 16-byte
continuation hashes to
`de3f0996c3b76c20c1e83a686f9a97f7a5ad8f9575a03d8f01b7f4cadf45a233`;
it cleans six stack bytes, pushes pointer `$2a612` and selector 6, and stops
before `TRAP #14` at `$2aabe`. Selector 6 remains the external boundary.
A typed selector-6 observation records its unused D0. The 10-byte continuation
hash is `ba614a28f861921a263225ef85209b20dc2673ea3444cb556b88ca29b2b23163`;
after six-byte stack cleanup, execution stops before `JSR $2b55a` at `$2aac2`.
The separately inventoried file-`+$107c` candidate maps to `$2b57c` and remains
rejected. Direct inspection of the admitted memory at `$2b55a` instead yields
`48e7fffe61000038`, hash
`b1b4328c9f54737553994259dac4dfb0247bf422414ed05a1c5c6166ec37ba62`.
The native path executes its MOVEM save and BSR control transfer, stopping at
the newly exact external callee boundary `$2b59a`.
The first 16 genuine callee bytes hash to
`967cb0022c8e29e0bef0dae618b95750fff3afa255094f9356210f1c89686fa3`.
Native execution preserves the BSR return `$2b562`, derives A3 `$2b0e8`, and
atomically clears `$2b6b8`; it stops before the D0-indexed instruction at
`$2b5a6`. No source index or register value is invented. A generation- and
sequence-owned observation supplies D0 and the byte at derived address
`$2bdfd + sign_extend(D0.W)`. Production verifies it against owned memory.
The exact 12-byte instruction pair hashes to
`e87859079e18a266cc359d7e0be47667c5cfe79dbffa05daad80ee951fa777d7`;
both writes to `$2b6b0/$2b6b1` commit atomically. Contradictory, stale, and
revoked observations change neither checkpoint nor memory.
From `$2b5b2`, 48 hash-bound bytes
(`4345389397550c90280802d10a3f03b3e181745bcb98f8c693a2c0980722a1ef`)
perform the deterministic A1/A0/D7 setup and seven atomic initialization
writes. Execution stops before the next D0-indexed word read at `$2b5de`.
That read uses a typed D0/source-word observation at
`$2bdfe + sign_extend(D0.W)`, checked against owned memory. The 18-byte block
hashes to `6fae36f2f65050ca3ff99c8cb73f43a8c130dd4d252d4b7d38d0be9118eeba78`;
two direct word stores commit atomically before the next indexed read at `$2b5ec`.
That second read resolves through A3 plus the word just loaded into D0. The
first pass reaches `$2be08`; subsequent D0.W values and indexed addresses are
derived from the preceding observed word plus the exact `ADDQ.W #2,D0`.
Production rejects a fresh or contradictory D0 on later iterations and checks
each typed source word against owned memory. The exact 20-byte tail hashes to
`82379ace33d5464b74e03aa0669f8a1097498fd21ce3639c180ab5e21cac810b`;
it derives A0 `$56eee4`, atomically stores it at `$2b620`, increments D0.W,
decrements D7 to 1, and records the taken DBF target `$2b5b8`. Contradictions
and revocation commit no checkpoint or memory changes. The taken DBF edge
resumes at `$2b5b8`; its 42-byte loop-setup span hashes to
`9efa7511411f3ca6698746d8bac484420a14e67e35467be2909f3647b0612034`.
With A1 advanced to `$2b64e`, seven initialization writes commit atomically,
then execution returns to the D0-indexed word boundary `$2b5de`.
The remaining two iterations reuse the same typed source contracts; their
second indexed reads both resolve to `$2be0c` from carried D0.W state. A1 advances through
`$2b64e` and `$2b67e`; D7 becomes zero and then `$ffff`, so the final DBF
falls through to `$2b600`. Its 28-byte epilogue hashes to
`51ea54e46ad38380435c7a367889825fce566b4f33036fb5dd38846dafdf4ab7`.
Six direct initialization effects commit atomically, RTS returns to `$2b562`,
and execution stops before the saved `MOVEM.L (A7)+` restore frame.
That frame is one typed observation containing all 15 longwords in mask
`$7fff` order, its A7 address, and the required caller return `$2aac8`.
The exact MOVEM/RTS bytes hash to
`7f09b538ef863cae65b4a16e1301251bde1fed37c1dba591dd4ec9f4b34106b1`.
Restoration and RTS commit as one checkpoint transition; A7 advances by 64
bytes including the return longword. The caller's six-byte absolute JSR hashes
to `fd41f7c5a0cdb684768c3da230cb9ca56bac136abd2254b55090d6b1cf58da78`.
Execution stops before its target `$2aa68`.
The admitted bytes at `$2aa68` are not the older 22-byte-shifted candidate.
Their exact 12-byte prefix is `2f3c0002aa423f3c00264e4e`, SHA-256
`fd6e1ace58bbc4108fcc0b8a7f75103c04337c41d24b2c9de5907f9538aaf439`.
It pushes pointer `$2aa42` and XBIOS selector `$26`, then stops before
`TRAP #14` at `$2aa72`; no firmware result is inferred.
A typed selector-`$26` D0 return advances through exact cleanup and RTS bytes
(SHA-256 `59f7345ed980fd79117e7ad10db1a93c3872cafca3000afb7ef3f7eda5603adc`).
The caller then sets D7 to `$2a640` and reaches absolute `JSR $2aa0c`; its
12-byte span hashes to
`7218804023c2ec3e694e19b581efeb17703f7bfe78d77b6da330354cc23a18f2`.
The exact absolute JSR at `$2aa0c` (`4eb90002a5aa`) hashes to
`25939d2a8a98420749b181f742081cc576f302cffd0bea5b8008765af3b5d9f0`.
Its target `$2a5aa` pushes open mode 2, the proven D7 filename pointer
`$2a640`, and GEMDOS selector `$3d`. That 12-byte prefix through `TRAP #1`
(`3f3c00022f073f3c003d4e41`) hashes to
`bdfb77219a19903ee730f3361af0958841aae3570ef3ed0d2ea60c3b56a3491e`.
Execution stops at the trap instruction `$2a5b4`: no handle, filesystem
access, return value, or post-trap control flow is inferred.
A generation-owned typed observation may now supply the raw signed D0 return.
The exact 12-byte cleanup/store/test/RTS continuation hashes to
`dfe4c3bc4466d6d8772f3633cb125f64ea7a9114d3d0be45aca5be3daf28b30b`;
it atomically stores D0.W at `$2a5fa` without treating it as a valid host
handle. At `$2aa12`, nonnegative D0 takes BPL to `$2aa1c`, loads literal D0
`$7d42` and D1 `$2c24a`, then stops before `JSR $2a5c2` at `$2aa28`. This
22-byte branch path hashes to
`e3c9dfa674089f687e0042be07645d2d57bf321a76d53b0276f86ba8316f06f4`.
Negative D0 executes the original JMP to `$2a632`, whose `BRA.S $2a632`
is an exact failure spin; the 12-byte branch/JMP/spin path hashes to
`3a06cb0af877cc363d5ad25b670d680c77b4abcd00955b260c2139270b57426c`.
The positive `$2aa28->$2a5c2` call now executes five exact argument pushes.
The 16-byte prefix `2f012f003f390002a5fa3f3c003f4e41` hashes to
`6d2ddd7da4866769c78162433427fb37fe2f885926f429c098fca3062e282921`.
It passes count `$7d42`, buffer `$2c24a`, the word previously stored at
`$2a5fa`, and selector `$3f`, then stops at `TRAP #1` `$2a5d0`. Eon neither
reads a host file nor claims the buffer has been populated.
A typed selector-`$3f` observation supplies only raw signed D0; buffer bytes
are deliberately not required because the following original `TST.L D0`
does not branch. The 10-byte cleanup/test/RTS body hashes to
`9f590fdbc6197d898da37312cddcb27a0411bf687877778f77320cb5c61f8ed3`.
The caller's absolute JMP to `$2a5dc` hashes to
`a3bf89946746662879548e7a74f8f77c8d107c234cae2908c9b94abe94b19f89`.
There the exact 12-byte handle/selector-`$3e` prefix hashes to
`e815352850ca1cb7dffb7fa6d7e46d7775e82146695009e790e525daac17a2e9`
and stops at Fclose `TRAP #1` `$2a5e6`. No file bytes or host I/O are inferred.
The typed raw Fclose return advances through its six-byte cleanup/test/RTS
body (SHA-256
`1653b046f59ffdf7cdcdae81914ab08b45f9fd09915e21b1c27ea8c6021e0b2f`).
The caller consumes only two Fread-buffer words before its next call. A
bounded typed observation supplies exactly those four bytes at `$2c24c` and
`$2c24e`; Eon commits them atomically, loads D6/D7, and reuses the already
owned selector-3 pointer as A5. The 16-byte consumer/call span hashes to
`06aca8d014e4064f17c8dba3c9b19ed705214dcb63cc41b0b3d9f8da7a2cd782`.
Execution stops before `JSR $2b2be` at `$2aaec`; no remaining buffer is
invented or admitted.
The loaded `$2b2be` target begins at file `+0xdbe`, not the older candidate at
`+0xde0`. Its 32-byte deterministic setup hashes to
`73fd6f3a91efb666e74c8022cd546f20457e599c36268691b0a27f43b22dc2bd`.
It derives A3 `$2b2ba`, transforms D6/D7, stores both words atomically, copies
the owned A5 into A6/A0, sets D5 to 4 and clears D2. Execution stops before
`MOVE.B (A4)+,D0` at `$2b2de`, source `$2c250`.
A generation-owned observation admits exactly that byte. The production
facade rejects it unless it equals the byte in the hash-admitted
`MILL22A.INF` Fread image; the same check covers every later token payload,
so typed input cannot manufacture decoded pixels. Rejection leaves both the
consumer checkpoint and native memory unchanged. The 12-byte
read/copy/mask/branch span hashes to
`948e269d0e24d6ec05013d07ffe3d3ba66400189b98a30d676b44e5b39683fe6`.
Bytes with no `$c0` bits reach the first pair copy at `$2b2ea`; a bit-6-clear
value reaches `$2b3b8`, a bit-7-set value reaches `$2b376`, and bit 6 alone
reaches the next source read at `$2b338`. The exact nonzero bit-gate span
hashes to `4b98ca43cbf9af758b5d56087a8d113f23fedf107e1320a2a6ee137d6cfe92c3`.
On that zero-bit path a second typed observation supplies exactly the two
bytes at `$2c251..$2c252`. The exact eight-byte pair-copy/A5-advance/D6-
decrement prefix at `$2b2ea..$2b2f1` hashes to
`8b97786735b1f1be41f931a62098f2f1080b5067b2db2a9835125619ad3b7623`.
Eon atomically retains those source bytes and copies them to the owned A5
destination, advances A4 by two and A5 by eight, and decrements D6. The exact
48-byte counter continuation at file `+0xdf2`, runtime
`$2b2f2..$2b321`, hashes to
`9b3476f5d2ecb028149eec6ee575cd79c7c9f94589a7e7398d794ecd176f04ef`.
Eon now executes its normal-path D6 run dispatch, D7 row dispatch, four-plane
D5 dispatch, A0/A5/A6 address updates and D2 run countdown. It returns to the
typed pair boundary, the next typed token boundary, or the proven RTS at
`$2b3c6` according to those owned counters. No new source byte is admitted by
that deterministic transition.

The other three token forms are now native under typed source observations.
Bit 6 alone uses the low six bits as a run length, reads one byte and repeats
it into both bytes of every destination word. Its exact 68-byte path at file
`+0xe32`, runtime `$2b332..$2b375`, hashes to
`6429d7b0634cff176ec01486b3f4e05bd648e3de11a67edd151f8345724b6701`.
Bit 7 plus bit 6 uses the low six bits as the run length and writes the two
typed source bytes in reversed order; its exact 66-byte path at file
`+0xe76`, runtime `$2b376..$2b3b7`, hashes to
`dbf80460ade3c9cc5fba8b4a62937920cc9e131052d3a48bfc8b0981e150a9b9`.
Bit 6 clear with bit 7 set forms a 14-bit run length from the token's low six
bits and one typed count byte, then joins that same reversed-pair path. Its
exact 14-byte prefix at file `+0xeb8`, runtime `$2b3b8..$2b3c5`, hashes to
`72fa63385edc5122cd3fe1c4031d0a0089a187d498c04ff1f8be912f4462b0c5`.
Each admission atomically retains only its consumed source bytes and the
statically derived destination words. The common counter engine bounds output
at the next token or routine RTS; no display or renderer interpretation is
assigned to the decoded words.

After the decoder reaches its verified RTS at `$2b3c6`, the exact 18-byte
caller continuation at file `+0xd36`, runtime `$2aaf2..$2ab03`, hashes to
`155575e295ad1e7831c0eef9809316db6f68321beb0661c03b7c14bb141f793e`.
It loads A3 `$2a64c`, A4 `$2a66c`, and calls `$2b448`. The callee's exact
62-byte deterministic prefix hashes to
`748d9b2df05839b68583069e29ff34954477ce7a367b0a88ef9e9bad7abfa0ca`.
Eon clears eight longwords at the decoder's final A5, then copies the 24
original longwords at `$2a66c..$2a6cb` to `$2b3c8..$2b427` in one atomic
batch. Those 96 immutable source bytes hash to
`a2263d35c251e787a9a5705a5277bcf641321817f825e7689081280fbd157dfe`.
The existing 16 destination words at `$2b428..$2b447` are admitted as one
ordered typed observation. The exact 40-byte span at file `+0xf86`, runtime
`$2b486..$2b4ad`, hashes to
`0866601f1a271ee74b399dd544b5b1ced15693e600c30034531a094dbc41d746`.
For each of 16 groups it adds the three adjacent byte pairs in the copied
source, stores each low sum byte back at the first byte of its pair, and adds
carry weights `$0100`, `$0010`, and `$0001` to that group's destination word.
Eon commits the 48 byte writes and 16 word writes atomically. Execution then
stops at `TRAP #14` `$2b4ac`, with selector 6 and pointer `$2b428` retained;
the XBIOS result is accepted only as a generation-owned typed D0 observation
and is not assigned palette, display, or timing semantics. After that return,
the exact 16 bytes at file `+0xfae`, loaded runtime `$2b4ae..$2b4bd`, hash to
`9e3fd4aeca606c5560b204d12a20a77de12552ded7fa64a0677cca56c4676bf1`.
They clean six stack bytes, overwrite D0 with `$00004e20`, execute exactly
20,000 `SUBQ.L`/`BNE.S` delay iterations, then decrement D7 from 6 to 5 and
take the first outer `DBF` edge to the corrected loaded address `$2b46e`.
This transition changes only
typed register/control state, so it has no runtime-memory effect batch to
commit. Each of the six recurrent passes now admits the current 96 source
bytes and 16 destination words as one typed observation, checked against
native memory by the production facade. The exact recurrent setup, arithmetic,
and trap span `$2b46e..$2b4ad` hashes to
`a50d1864336da9b76c9594f94b2eb736108d738d0aefc6443a91c8e8fdd7088b`;
each pass atomically commits its 48 byte and 16 word effects, then requires a
new typed selector-6 return before its delay and D7 transition. After pass
seven, D7 becomes `$ffff` and falls through to the terminal selector-6 trap
at `$2b4c2`. Its exact ten-byte path at file `+0xfbe` hashes to
`876ea72e7f61e2604ffa34d0fae7a6c1b3f880aa43e88006af18e1f67677c967`.
A final typed raw return owns the six-byte cleanup and local RTS at `$2b4c6`.
The RTS destination is now a generation-owned typed stack observation. Only
the exact return `$2ab04` is admitted; the stack address is retained without
reading or fabricating unrelated stack contents. The 12-byte caller span at
file `+0x604`, runtime `$2ab04..$2ab0f`, hashes to
`ae672762da7616abc67d0a1e5a5aaf3ab540b96b94b9689b31f8a11a8de256d7`.
It loads D7 with the corrected second configuration filename pointer `$2a634`
and calls the already bounded `$2aa0c` helper. That helper deterministically
reaches the existing GEMDOS selector `$3d` boundary with open mode 2 and D7
as its filename pointer. A dedicated second-config observation accepts the
raw signed D0 return only in that caller context and atomically stores its low
word at `$2a5fa`. The exact shared helper then dispatches a negative return to
the proven `$2a632` failure spin. A nonnegative return loads D0 `$7d42` and D1
`$2c24a` and reaches the existing `$2a5c2` Fread helper boundary. Reusing that
path is justified by the identical `$2aa0c` call target; no open operation,
file content, handle validity, or filesystem effect is inferred.
On the success path, dedicated typed Fread and Fclose observations retain only
their raw signed D0 results. The shared Fread helper uses the already proven
handle slot, buffer `$2c24a`, and count `$7d42`; its exact cleanup then reaches
the selector-`$3e` Fclose boundary. The six post-Fclose bytes hash to
`1653b046f59ffdf7cdcdae81914ab08b45f9fd09915e21b1c27ea8c6021e0b2f`
and reach RTS `$2a5ec`. A separate typed stack observation admits only its
second-caller return `$2ab10`. The exact 22-byte caller span hashes to
`eea2683953b1fe18e3e7b88e1744fa10a9684444fe183d283efee9f54302c1a0`;
it restores A3 `$2a64c`, A4 `$2a66c`, pushes the PC-relative pointer `$2ab2c`
and selector `$26`, and stops at XBIOS trap `$2ab24`. The selector result and
all referenced-data meaning remain external until supplied as a typed raw D0
observation. The exact four-byte cleanup/RTS suffix hashes to
`2b1d33a613d225ccb932ee7c7ad5efb29dcdd736ba28ad3c4b75162694bc09ed`
and reaches RTS `$2ab28`. Its typed stack destination is fixed to the original
staged PRG caller `$77042`. The exact 22-byte caller continuation, SHA-256
`dc2a50400e22fdbe4870f790d4f70c7446caa379dc68281a0445db4ee027fe4d`,
retains the literal stack longword `$11e00`, pushes open mode 2 and filename
pointer `$1d6d8`, and stops at GEMDOS selector `$3d` trap `$77056`. No meaning
is assigned to the preserved longword, filename, or service result. A typed
signed raw result admits the exact handle-word push, pending selector `$3e`,
test and branch. Negative results reach the `$77060` self-loop through the
10-byte span SHA-256
`d124b586e52a783689925186d8cc93366870526fd894567b7c55761a617807c7`.
Nonnegative results additionally push buffer `$11e00`, count `$20000`, the
handle word and selector `$3f`, reaching trap `$77074`; the complete 30-byte
span has SHA-256
`2ceb9e3c6a8c2882f13708d64367b0a9f8bf18ee7456ea396a3e600734825476`.
No open, read, close, filename, or buffer semantics are inferred.
Typed Fread results of either sign follow the same eight executed bytes,
SHA-256
`368338a18784d37b5867fa551121703b2fb0ab613db51cbc5b2c08e14f474558`,
which remove 12 stack bytes and reach the pending selector `$3e` trap at
`$7707c`. A typed Fclose result similarly has no inferred meaning. Its exact
cleanup and branch reach `$770a2`, write original constant `$361436a7` to
`$2ab2c` and `$11dfc` atomically, and stop at local RTS `$770ba`. The
concatenated executed span hashes to
`aa177208872c4125af13601feb4566003e5fb01c851c44f8b7f4904fb5f52b52`;
the skipped embedded strings are not executed. The bootstrap entered this
staged target with `JMP $77000`, not a call, so no return destination exists in
the admitted instruction stream. Eon accepts the terminal RTS destination only
as a typed even 24-bit address, retains it, and stops without inventing a
caller continuation.
No selector-3 return, display, input, or other firmware effect is inferred.

The second literal `TRAP #14` argument is not a palette and no service meaning
is assigned to it. At runtime address `0x2a612` (file `+0x134`) the exact 24
original bytes have SHA-256
`815bea3862908e01557486cae7d42132853c94348b49b920f9d3e88e14956c51` and form
two NUL-terminated strings: `MILL22D.INF` and `MILL22C.INF`. Project Eon
validates and reports those bytes as a bounded preservation fact only; it does
not open either name, invoke the trap, or presume the following JSR is
reachable because both preceding XBIOS return values are unrecovered.

The first of those literal JSRs is also bounded against the genuine file
chain. Address `0x2b55a` maps to file offset `0x107c`; its eight verified
bytes are `03 5a 4c df 7f ff 4e 75`. Project Eon retains the first word
`0x035a` only as an original dynamic-bit-operation boundary because its effect
depends on register state supplied by the caller. The complete following
local sequence is `MOVEM.L (A7)+,D0-D7/A0-A6` (`0x4cdf`, mask `0x7fff`) and
`RTS` (`0x4e75`). This does not execute the JSR, model the caller's registers,
or claim a routine-level meaning beyond the directly verified bytes.

The entry's second literal JSR target, `0x2aa68` (file `+0x58a`), is bounded
separately. It begins `0x0880 0x000d 0x6714`: an immediate-bit instruction and
its original conditional short branch. The branch target is `0x2aa82`; it
skips the 20-byte middle path and joins it at `0x2aa82`, a literal JSR to
`0x2a51c`. After that call returns, control falls through to the entry block
at `0x2aa88`, whose first original JSR at `0x2aaa0` targets `0x2b55a`.
Project Eon reports this converging control shape and
validates every byte through the following call, but does not invent D0,
evaluate the branch, execute either call, or assign a platform effect to the
intervening instructions.

The shared call target `0x2a51c` is now independently bounded at file
`+0x3e`. Its complete 32-byte local body begins `0x548f`, stores the literal
`D0` word through opcode `0x33c0` to `0x2a512`, contains original Line-A word
`0xa000`, stores longwords to `0x2a514` and `0x2a518`, then returns with
`0x4e75`. The Line-A instruction is deliberately opaque: Project Eon does not
choose a firmware implementation, invent register or RAM contents, execute
the helper, or treat those slots as a host-side configuration model.

The repeated entry-block JSR target `0x2aa0c` is a separately verified
forwarding boundary at file `+0x52e`: `JMP 0x2a5dc`. The 12-byte destination
at file `+0xfe` begins `0x3f01`, pushes literal selector `0x0019`, executes
`TRAP #14` (`0x4e4e`), performs original stack cleanup `0x504f`, and returns
with `0x4e75`. These are exact original machine-code facts only. Project Eon
does not invoke the trap, infer a selector meaning, choose a Line-A/XBIOS or
firmware implementation, or synthesize a result or configuration state.

The older static candidate formerly mapped to `0x2b2be` is bounded at file `+0xde0`.
Its initial original words are `0x1400 0x0200 0x00c0 0x6600 0x003a`; the
conditional branch's exact destination is `0x2b300` (file `+0xe22`), where
the original bytes begin `0x0802 0x0006 0x6700 0x0090`. Project Eon preserves
the two D0-dependent gates and their literal branch shape only. It does not
choose a D0 value, execute either path, infer a consequence, or admit this
34-byte-shifted candidate as the loaded runtime target.

That candidate's complete local routine is hash-locked through its `RTS`:
`$2b2be..$2b3a5`, file `+0xde0..+0xec7`, 232 bytes, SHA-256
`85c58759b0cb2f067734fb006aa543fc74926422187506914c823ceaaf9c6cd8`.
Every path remains local original code, but its branches/copies depend on
caller-owned D0, A3, A4, A5, A6 and loop registers; it crosses no native
service boundary and provides no recoverable input or display state. Project
Eon validates this immutable span and does not execute it or use it as a
replacement configuration result.

The direct target `0x2b448` is preserved through its complete local setup
prefix at file `+0xf6a`: it loads `D7=0x0006`, `A5=0x2b428`,
`A4=0x2b3c8`, `D6=0x000f`, `D5=0x0002`, and `D4=0x0100`. This is only direct
instruction/dataflow evidence. Project Eon stops before the ensuing loop body
and does not dereference the pointers, execute its loops or traps, or infer a
meaning for those registers and constants.

The immediately preceding 34 bytes are independently hash-locked as a static
adjacency anchor, not a newly claimed call path. At `0x2b426` (file `+0xf48`)
their SHA-256 is
`6f135d6e68a1b6c48826ae484223166f4e6061cd4b6b5cbc2d0dfcc2bc8fb550`.
They set literal `D0=0` and `D1=7`, contain `DBF D1,-4` back to `0x2b430`,
push `A3`, set `A5=0x2b3c8`, set word `D0=0x17`, and contain `DBF D0,-4`
back to `0x2b442`, before falling through to `0x2b448`. This is an immutable
68000 byte/control-flow fact only. No original callsite to `0x2b426`, loop
entry, pointer contents, native-service effect, or game-state result is
asserted or emulated.

The first local loop after that setup is fully bounded as bytes at `0x2b464`
(file `+0xf86`): a 22-byte original block ending in `DBF` opcode `0x51cd`
with displacement `-20`. The taken backedge returns to `0x2b464` itself; the
adjacent setup had supplied literal `D5=0x0002`. Project Eon validates this
exact backedge but does not run any iteration, read the loop's pointed-to
data, derive an iteration outcome, or translate it into replacement game
state.

The immediate fall-through after that inner `DBF` is also bounded. At
`0x2b47a` (file `+0xf9c`), the original six-byte path is `0x548d 0x51ce
0xffde`: the first word advances `A5`, and the second is an outer `DBF` with
displacement `-34`. Its taken target is `0x2b45c` (file `+0xf7e`), whose
verified prefix is `0x3a3c 0x0002`, the local D5 setup. Project Eon records
the backedge and literal setup only; it runs neither loop, accesses no loop
data, and invokes no native service.

The target's full local prefix is now also linked: `0x2b45c` contains exactly
`0x3a3c 0x0002` (`D5`) and `0x383c 0x0100` (`D4`), then falls through at
`0x2b464` to the already verified 22-byte inner-loop body. This is a strict
control/dataflow continuation, not an execution model: Project Eon does not
run an outer or inner iteration, read the referenced data, or derive state.

The only strict fall-through fact after the outer `DBF` is now documented up
to its first native-service boundary. At `0x2b480` (file `+0xfa2`) the
original pushes longword `0x2b428`, pushes selector `0x0006`, and reaches
`TRAP #14` (`0x4e4e`). Project Eon validates the exact 12 bytes and stops at
that opcode: it does not invoke or emulate the trap, infer its service, read
the argument's data, or manufacture a return value.

The 26 original bytes immediately after that opcode are separately retained as
a hash-addressed preservation anchor: address `0x2b48c`, file `+0xfae`, SHA-256
`34d497b9c4408944ea24d4eede21838f691c43d5a0d772db922187bed0e87fc8`.
This does **not** establish that the suffix executes: reaching it requires a
native `TRAP #14` return that Project Eon does not emulate. Its instruction
words are now also decoded literally: `ADDQ.L #6,SP`; `MOVE.L #0x4e20,D0`;
`SUBQ.L #1,D0` and `BNE.S -4` back to `0x2b494`; then `DBF D7,-78` to
`0x2b44c` (DBF is relative to its extension word); selector `0x0006` and another `TRAP #14`; followed by the same
stack cleanup and `RTS`. This records bytes, operands, and PC-relative
targets only. Loop effects, native service calls, return values, and any
resulting game state remain unrecovered rather than inferred.

The tail's corrected `DBF` target is also linked to its complete 24-byte setup
prefix at `0x2b44c` (file `+0xf6e`, SHA-256
`85f6e69ef8d058c021e0c70fe51375ef2f09a2c67c798c73f066ffdb6f14a187`). That
prefix is the literal A5/A4/D6/D5/D4 setup and falls through to the separately
validated loop body at `0x2b464`. This establishes a static byte/control-flow
relationship only; it does not make the native trap return, recurrence, or
loop effects runnable.

For the next disassembly phase, Project Eon now keeps a fail-closed whole-file
inventory of all 19 original `0x4eb9` absolute-JSR encodings. The first is at
file `+0x50c` to `0x2a5aa`; the last is at `+0xdb2` to `0x2aa78`. This is
explicitly a byte inventory, not a reachability claim: only the six encodings
in the independently verified entry block are established callsites. The
other patterns remain preservation anchors until their surrounding control
paths are proven from original bytes.

One of those inventory-only targets is now retained as a complete bounded
body, without promoting its caller to a live path. The original encoding at
file `+0xdac` names target `$2b576` (file `+0x1098`). The contiguous span
through `RTS` at `$2b5f8` is 132 bytes and has SHA-256
`07e36fd52b00af1557c0da08efc7388d9d7cf6567e9c24102267db80b34adcd8`.
It starts with original words `0x7000 0x47fa` and ends `0x4e75`. Project Eon
records this only as immutable disassembly evidence: the inventory establishes
an encoding, not reachability, register inputs, a calling convention, routine
meaning, native-service behaviour, or a game-state effect. The bytes are not
executed or translated into replacement logic.

### Millennium AmigaDOS filesystem evidence

The Millennium archive contains six independently cracked images. The two
Razor images are standard 880 KiB `DOS\0` ADFs with an intact root block at
block 880. The verified Razor image has SHA-256
`fe83c10119ef9bf2953b6fcd9a13d07f2c276215aaa64e2e541402a527a616f2` and
root volume label `Millennium (Crack Razor)`. Its 72 root hash slots contain
no file entries: game content is loaded from raw sectors (not fabricated as
filesystem files). Four other supplied Millennium variants have game code at
the boot-declared root-block location, so they are correctly rejected as
non-standard AmigaDOS volumes.

`AmigaOfs` is therefore a strict, read-only OFS/FFS reader for future standard
images: it validates root, directory and hash-chain block types; detects
cycles; bounds every block reference; and refuses incomplete file chains. It
does not infer missing files or mutate image data.

### Millennium Amiga raw-loader evidence

The hash-identified Defjam ADF has SHA-256
`8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c`.
The relocated bootstrap's common reader at `$661da` copies `D7` to `D2`,
chunks `D0`, and calls `$66216`. That routine writes `D0` to
`IORequest+$24` (io_Length), `D1` to `+$28` (io_Data), and `D2` to
`+$2c` (io_Offset). Earlier documentation inverted length and offset; all
raw-resident mappings derived from that inversion have been revoked.

The authoritative caller-connected transfers are:

| Disk offset | Length | Destination | SHA-256 |
| ---: | ---: | ---: | --- |
| `0x6e000` | `0x24200` | `$41000` | `df97c7f6cd622b16b9ffb57bc562906e349c18c56ed8abeb564c6f411e64891c` |
| `0x2c000` | `0x16400` | `$68000` | `3337a21984346f06f295c9cfbb89d2a0c0d622853dd2e11cf14a5c5cc29a276f` |

This correction is also structurally proved: the first range ends at
`$65200`, before the live relocated loader at `$66032`. The reversed
interpretation would overwrite the reader while it was executing.

After a typed successful trackdisk return, the native runtime atomically maps
the exact `0x24200` source bytes to `$41000..$651ff`. The stage begins
`BRA.W $410bc`. Its first `0x1f4` bytes hash to
`644bab0527fe05e91695e2996768a4b6c1203ebd3fafe35dffa9728c93875f84`
and statically establish the register-save/vector setup through `ILLEGAL` at
`$410de`, which uses exception vector address `$10`. A typed register/vector
observation now advances the native session through the exact `BRA.W`, saved
`A6`, `MOVEM.L D0-A7`, saved-register patch, vector read, `PEA`, and vector
installation. The final register table, transient stack cell, and vector
write are committed atomically.

The next admission is frame-complete rather than an emulated 68000
exception. It requires the observed handler PC `$410e0`, the six-byte format-0
frame (saved SR plus saved PC `$410de`), its stack address, and all eight
vector-table longs read from `$8..$27`. The third long must equal the old
vector `$10` already retained from entry. The handler restores that vector,
snapshots the eight longs at `$4108a`, installs `$41172` at vector `$10`, and
reaches the second `ILLEGAL` at `$410fc`. Frame, snapshot, and vector update
are one fail-closed memory batch.

The second complete format-0 observation admits only the clear-bit route at
`$41172`: saved PC must be `$410fc`, the frame address must be even, and SR
high-byte bit 7 must be clear. The handler installs vectors 8 and 9, changes
the saved PC to `$410fe`, updates the saved SR, retains its temporary D0/A0/A1
stack image, and performs the exact media-derived transform
`D503FFE1 XOR B503FFEF = 6000000E`. `RTE` executes that new branch with the
trace bit set; the resulting vector-9 exception saves resume PC `$41110` before
the encrypted `$ff89` word is executed. Frame, stack, vectors, cursor,
ciphertext save, and code transform are one atomic batch.

The first trace-handler pass is also native behind a separate complete format-0
observation. It requires vector address `$24`, handler `$411ac`, saved PC `$41110`, an even bounded
frame address, and the handler's live SR separately from the saved SR; Eon does
not synthesize the processor exception transition. The exact handler masks the
live SR, preserves D0/A0/A1, restores `$d503ffe1` at `$410fe`, advances the
cursor to `$41110`, saves the genuine `$ff896076` ciphertext, and derives the
key from the genuine preceding long `$4accd533`. The resulting atomic transform
is `$ff896076 XOR $2accb533 = $d545d545` at `$41110`.

Eon executes that exact `ADDX.W D5,D2`, including X/N/Z/V/C, then consumes ten
complete vector-9 frames at `$41112,$4112e,$41166,$4115e,$41132,$41162,
$41152,$41106,$4116a,$41142`. Each handler restores the preceding ciphertext,
saves and transforms the current long, and exposes one unconditional `BRA.W`.
The final branch reaches `$411d8`. All ten frames, temporary stacks, cursor,
ciphertext, and transformed instruction writes form one atomic batch. The
The next eight trace frames decrypt and execute three LEAs, two MOVEQs, the
genuine table word `$059a`, and `ADD.L A2,D1`, leaving D0=`$7`, D1=`$41656`,
A0=`$411fa`, A1=`$8`, and A2=`$410bc`. The final pass decrypts `$411ee` to
`MOVE.B D4,$a183ec32`; that external destination is the next stateful boundary
and is not written by Eon.

The bootstrap relocation itself remains bounded by its final source-byte
observation at `$70400`; the exact caller then performs `JSR (A3)` at
`$662e4` with `A3=$41000`. No emulator or synthetic bytes are used by
this native transfer.
### Deuteros Atari ST protected-media boot chain

The supplied Atari ST collection consists of protected/cracked raw `.st`
images; it does not include a pristine, ordinary GEMDOS release.  Although the
boot sector retains a 720 KiB BPB (512-byte sectors, two heads, nine sectors
per track), its FAT root area is overwritten by loader/data bytes.  Project
Eon therefore must not present this medium as a valid FAT12 filesystem.

The release-wide scanner classifies every supplied Atari leaf before selecting
the single hash-bound recovery chain. The present archive has 11 Atari leaves:
10 are 720 KiB protected-media candidates; nine have the exact checksum/BPB
boot envelope; three contain the Replicants first-stage shape; two contain a
`KILLER_BOOT` marker; and one is nonstandard geometry. Of the ten 720 KiB
candidates, one fails the first 68000 boot-branch envelope; none fail the
BPB or word-checksum gate. These are census facts, not editions to merge: Eon
never borrows a first stage, marker, boot branch, or raw resource from one
variant when launching another. The scanner does not
interpret a FAT namespace, execute a branch, invoke XBIOS, or load a sector.

Both verified evidence disks retain the Atari boot-sector word checksum of
`0x1234`.  Replicants Disk 1
(`aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee`)
branches to `$1e`; its literal XBIOS `Floprd` argument setup at boot offset
`$50` reads track 70, side 0, sectors 1 through 9 (4,608 bytes).  The SHA-256
of that direct sector interval is
`dad3594c53375bd8285ef33e2d685bd38a5b38d930f2ea1305d117d63667f168`.
This is a raw first stage, not a resource archive and is only read in memory.
Its word branch enters at stage offset `$9c4`; there it validates bytes
`$0006..$0440` (exactly `$43b` iterations) using seed `$22225555`,
`ADD.B (A0)+,D1` / `ROL.L #8,D1` and
expected value `$7ae26af7`.  Only on that validation path does the recovered
code request the next raw interval: track 2, side 0, sectors 1 through 9 to
RAM `$70000`.  Its callback chain pushes `$70000` at `+$a74`, then after the
read pops that preserved value at `+$ac8` and copies 4,608 bytes to `$1e00`.
These are control-flow facts, not claims that the next interval is a title
screen; the latter remains unclassified. `ADD.B` updates only the low byte of
`D1`, so its carry never propagates into the upper 24 bits before the longword
rotate; Project Eon models that operand width explicitly.

That track-2 interval has SHA-256
`2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7`.
The complete-disassembly inventory now records it as a separate executable
image: disk offset `$4800`, length `$1200`, load address and entry `$70000`.
The `$70000` value is therefore a runtime destination, not a disk offset. A
fresh external byte-complete linear listing has 1,547 lines and SHA-256
`59bbbb22f803004b69d62d23ce5a5a455dcd9b781c9551882d28a43712be6555`.
This listing is candidate classification only; it does not prove that every
byte is reachable code or provide any XBIOS result.
The SDL Atari ST launch path now creates a bounded session for this exact
Replicants Disk 1: it reads those two original raw ranges in memory, verifies
both hashes and the first-stage checksum, then records the second-stage
dispatcher. It stops before `Floprd`, callback/XBIOS behavior, state
selection, or a display is invented; other protected disks are detected but
never substituted for this profile.
The release-neutral protected-media preflight borrows the already verified
disk span only long enough to derive its boot profile; it no longer creates a
second full-disk buffer for that inspection. A later bootstrap session retains
an owning read-only image only when it needs its documented sector ranges.
The first-stage copy is now retained separately as a hash-gated, isolated RAM
record: its literal byte loop copies the complete 4,608-byte track-2 interval
from `$70000` to `$1e00`, preserving the same SHA-256 at both locations. Its
direct-entry source offset `+$c4` therefore maps to relocated `$1ec4`.
This proves only the bytes a local copy would produce if its preceding service
returned and the loop were reached. It neither asserts either condition nor
executes the copied dispatcher, reads its runtime state, selects a vector, or
calls XBIOS.
At its loaded address `$70000`, it is executable code rather than a resource.
Its SR-dependent entry paths rejoin at track-2 `+$18`; Eon executes that
common, hash-locked 12-byte local suffix (SHA-256
`b40da514f09891a46ce07d1def675f82f77b7752f8153beb7638bdf5aea973ee`):
`LEA $2478,SP` followed by `JMP $1ec4`. It deliberately does not choose an SR
branch or manufacture an incoming status word. Because the preceding copy has
now been proven to source `$70000`, that jump maps exactly to track-2 byte
offset `+$c4`. Execution stops before that copied dispatcher: its first
instruction consumes runtime RAM at `$25fc`, and its connected path later
reaches the undocumented supervisor callback/XBIOS boundary. The dispatcher
stores a runtime word at `$1eaa`, indexes a vector table at `$1eac`, calls the
selected address, then forwards returned `D1`/`D2` values to raw reader
`$70030`. The state word and selected handler are runtime-dependent: Project
Eon does not assign a title/game meaning, load a guessed sector, or manufacture
state. Its local raw-reader routine at `+$60` caps each XBIOS request at nine
sectors and maps linear tracks from `$50` onward to side 1.

The first six static table slots are `$1f1a`, `$1f2e`, `$1f50`, `$1f1a`,
`$1f1a`, and `$1f52`; they are all code addresses within the copied track-2
interval. The `$1f1a` vector returns raw-loader arguments: destination
`$13200`, byte count `$4800`, linear sector `$4`. `$1f2e` returns destination
`$b000`, byte count `$5e400`, linear sector `$4c` after an observed GEMDOS
call. These returned values flow to `$70030` through the proven dispatcher,
but state selection is still not emulated and no sector is read by this parser.
Slot 2's `$1f50` is a literal branch to `$1f1a`; slots 3 and 4 point directly
to `$1f1a`. Thus all three aliases share only the already-proven state-0 raw
arguments, without a new state interpretation.

Within the third distinct table body (`+$152`, hash
`eaee587850078d67a72dcf0da4b45e672c89a1352b040db580bedc0ba3b20e97`), the
28-byte interval at `+$170` is independently hash-locked to
`92cb6cf8a41c55df8459a9608c9626ff7cc831cceb69dd2b5531ac766b111552`.
Its literal-pointer and loop encoding loads `$57a00` and `$b006`, sets the
word counter to `$9392`, and contains `MOVE.B (A0)+,(A1)+` followed by a
`DBF` backedge from `+$184` to `+$182`. This is an instruction-layout record
only. It does not establish a table index, table call, loop execution, return,
destination contents, disk operation, or game-state meaning.

`build_deuteros_atari_state0_raw_load_plan` now models that one wholly static
dispatch result without selecting it at runtime: destination `$13200`, length
`$4800`, linear sector 4, represented as four original nine-sector reads from
Disk 1 offsets `+$4800`, `+$5a00`, `+$6c00`, and `+$7e00`. Their independent
SHA-256 values are respectively
`2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7`,
`c5cef5d02d47d09a758487e873ce1e86a9905b0e62241fc3bff7a8bf9114718a`,
`2515d3507aa37eaf5bbc0dd12f72a8dcc44712e4773a1e9e3f57517f8a21777c`, and
`510e1793d5d08ef18d5bc5039f5843aa403024c63abaad000078c61f65011e34`.
The concatenated raw span is hash-locked to
`88afae4bd5182d916183b01bf688ab524d739749e84a092eda1435e386b57b58`.
No consumer, format, state-selection source, or title/game semantics are
inferred for these bytes.

The state-0 span's first `0x1200` bytes are byte-identical to the already
validated track-2 loader (`248925…81d7`): at state-0 base `$13200` they contain
the same stack setup, absolute jump `$1ec4`, copied dispatcher at `+$c4`, and
raw-loader argument vector at `+$11a`. `parse_deuteros_atari_state0_duplicate_stage_prefix`
asserts this identity and the duplicated parser boundaries. It does not claim
that `$13200` or `$132c4` is entered: the proven original path instead copies
the track-2 bytes to `$1e00` and enters `$1ec4`; no loader-return continuation
selecting the duplicate has been recovered. The remaining state-0 chunks stay
unclassified raw data.

`DeuterosAtariBootstrapSession` validates this same state-0 prefix once from
the exact four raw requests and retains only its length, entry/dispatcher
offsets, and stage hash in the active diagnostic checkpoint. It immediately
discards the materialized raw span and never selects state 0, enters its
duplicate, or assigns it a title/game meaning.

`build_deuteros_atari_state1_raw_load_plan` records the second vector's
equally static read as 84 original requests: 83 complete nine-sector side
spans followed by one seven-sector span. It starts at Disk 1 `+$55800`, loads
`$5e400` bytes to `$b000`, and has SHA-256
`0d5ccb3a337fcbd4d34d34b3ad24f20c3bb2edca7e7b734b8abb14f6c0a30f47`.
It has no unique GEM, DEGAS, or other image-resource header paired with a
proved consumer, dimensions, and palette. Header-like words inside this raw
protected-media interval are therefore not promoted to artwork. Project Eon
does not render the interval or choose state 1 merely because its physical
span can be calculated.

The second direct table body at track-2 `+$12e..+$14f` gives that state-1
plan a caller-connected boundary. Its complete 34 original bytes are
SHA-256 `0bc76b22089d008e4ce90d63216c75acbe0786b0a06127fbd66ef0dc252949ac`.
They push literal longword `$00002630`, then word `$0026`, and reach `TRAP
#14`; `ADDQ.L #6,A7` follows the trap. The remaining literal instructions
load `$b000`, `$5e400`, and `$4c` into the same raw-loader argument registers
recorded by the state-1 plan, then `RTS`. The byte count and six-byte cleanup
are instruction-layout facts, not a claim about the XBIOS selector, its
return, state selection, game semantics, or a successful read. The live
bounded Atari session and `--inspect` retain this hash-validated provenance
record without invoking the trap or materializing its state-1 media.

Within that unselected state-1 span, Disk 1 `+$9d800` (state-1 `+$48000`)
contains the exact branch encoding `60 00 09 c2` (`BRA.W` with literal
displacement `$09c2`). A byte-bounded printable block begins at Disk 1
`+$9d80a` and spans `$438` bytes; it has SHA-256
`8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e` and 18
printable runs. This is crack-era raw-media metadata, not recovered original
game presentation: Project Eon neither displays, translates, parses, nor
assigns a consumer or control-flow target to it.

The classification is now itself byte-addressed rather than being inferred
from a printable-byte count. The first 55-byte run at state-1 `+$4800a`
hashes to `785ebbc9d234032ee38c1cb5444ac1b5d46db21151ffad08d7b1898d6e6ce52a`;
the two following 55-byte game-name marker runs at `+$48046` and `+$48082`
share hash `f0eb99896cde59d36a075e624092cbf02de3ce0d201ca3c5050c13f9c65720dc`.
`DeuterosAtariState1SkippedAsciiBlock` retains only their offsets, lengths and
hashes. It does not expose their strings to the launcher or treat those names
as title assets: the surrounding unconditional branch remains the evidence
that this is a skipped presentation block in the supplied cracked release.

The sixth vector (`$1f52`) makes two further static calls to `$70030`.
`build_deuteros_atari_state5_raw_load_plan` records its first as ten complete
nine-sector reads from Disk 1 `+$55800` (length `$b400`) to `$b000`, SHA-256
`9659b21315e5c0528020be0b41eb75d57428f41b3b632fabfebe16d34038d298`.
It then copies `$9393` bytes from RAM `$57a00` to `$b006` before the second
static raw plan: 68 complete nine-sector reads from Disk 1 `+$60c00` (length
`$4c800`) to `$16400`, SHA-256
`6b3e27702649ac201c4ecf92ad54f40656fd4d8633fadf5790014da34ce03ac6`.
These are preserved instruction/dataflow facts only; they do not authorize
Project Eon to select vector 5, perform its runtime callbacks, or infer title
or game semantics for the loaded bytes.

The two vector-5 reads are physically contiguous (`+$55800..+$60c00` then
`+$60c00..+$ad400`) and together form the first `$57c00` bytes of the separate
state-1 interval. `validate_deuteros_atari_state5_state1_prefix` compares the
two in-memory spans directly and locks their shared prefix to SHA-256
`ed55ad2a893a87af9f11d269faa6358420c47ed6beb1fee7a177e9beaed1e77c`, while
also retaining state 1's full independent hash. This is a media-geometry and
byte-identity fact only: overlapping physical reads do not prove vector
selection, load success, ownership, resource type, or any title/game meaning.
The active native bootstrap session verifies the same two direct read spans
against its temporary state-1 interval and retains only the shared prefix's
source offset, byte count, and SHA-256 in diagnostics before both temporary
raw vectors are discarded.

The live `DeuterosAtariBootstrapSession` retains these three static plans,
the vector-5 return profile, and the XBIOS callback-byte boundary after
validating the same original second stage. Thus the SDL launch request and
`--inspect` share one hash-validated provenance record; retaining it does not
substitute a title or game runtime. The session also validates the complete
Replicants Disk 1 leaf before it reads either boot stage, so matching stage
prefixes from another protected-media variant cannot enter this runtime path.
read a selected state, invoke XBIOS, or materialize a title/game surface.

The native diagnostics path additionally publishes a narrow
`DeuterosAtariBootstrapCheckpoint` only while that exact hash-gated session is
active. It contains the two stage hashes, statically recovered entry/dispatcher
addresses, the observed XBIOS selector, and raw-plan counts/source offsets.
It contains no emulator registers, RAM, source path, disk bytes, selected
vector, or service result. This makes static disassembly facts available to
the launcher/runtime diagnostic layer without upgrading them into dynamic
trace evidence or gameplay parity.

For native-engine development, a later ordinary Hatari/Capstone cross-check
located the observed display transition's original bytes at Disk 1 offset
`$9d800`, inside the separately verified state-1 raw interval beginning at
`+$55800`. The bytes begin `60 00 09 c2`, disassembling as `BRA.W $b256` when
observed at relocated `$a892`; the following local instructions prepare
`Setscreen(-1, -1, 0)` through `TRAP #14`. This identifies a concrete original
code/data edge to recover as a renderer-side native transition. The observed
relocation itself remains an implementation target, not a guessed memory map,
and no Hatari machine state is retained by Eon.

The native `DeuterosAtariState1DisplayServiceBoundary` now locks that edge to
the unselected state-1 source interval rather than the observed emulator PC:
the branch is state-1 `+$48000`, SHA-256
`6321ea5a7fcf59fb3f07d02b6bd333a62b9c897be5a67b233a83b3c935a38bf6`, and
targets `+$489c6`. Its 18-byte local setup has SHA-256
`a07c7766104d5bf581862d24de4e594b60414625824e8360b1677cf92e88c6f3`; it
encodes one literal `-1` longword push, a second stack-copy longword push,
XBIOS selector `$0005`, `TRAP #14`, and 12-byte stack cleanup. The source
bytes alone do not encode the third service argument seen by Hatari, prove a
raw-read result or relocation, or show that the call returns. The session
also retains the already parser-validated skipped-ASCII profile as offsets,
run count, and one block hash. It never retains the raw state-1 bytes, calls
XBIOS, exposes the ASCII strings, or exposes a display.
`--runtime-diagnostics-json` publishes this only as the optional
`atari_bootstrap_checkpoint.state1_display_service` object for the active
Replicants session; non-Atari sessions emit `null` rather than a fabricated
cross-platform checkpoint.

For reproducibility, an ordinary Hatari 2.6.1 diagnostic boot was also run
against the read-only mounted Replicants Disk 1 and a user-supplied TOS 1.62
image. The respective SHA-256 values were
`aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee` and
`220fc9b35fd99908db9f9075fb3d850bf196d25741405ac6fa062facbbbd1583`.
Hatari selected STE mode, loaded TOS at `$e00000`, and confirmed that TOS
patches were disabled. An initial non-benchmark run did not reach its bounded
300-VBL exit before the host safety timeout. A later explicit-STE, write-
protected, no-input benchmark run did complete 300 VBLs and reported
`XBIOS 0x08 Floprd(..., 0, 1, 70, 0, 9)` at PC `$18c4`, independently matching
the static track-70/side-0/sector-1/nine-sector request shape. Neither run
establishes a guest frame, XBIOS ABI result, boot/game parity, or a recovered
runtime transition: ordinary Hatari trace output is not an admitted recorder
receipt. The logs remain external diagnostic evidence; recorder admission is
still governed by the separately pinned recorder policy.

The vector's immediate static continuation is now bounded too. Track-2
`+$1a2` (Disk 1 `+$49a2`, copied RAM `$1fa2`) is exactly `60 00 ff 70`,
SHA-256 `4d11113ca2040c3c0d8e9fe7fc7ef2b65175cc580b8a4b81466908ae7c537896`.
Its `BRA.W` displacement is relative to the extension word, so it resolves to
track-2 `+$114` / copied RAM `$1f14`. The target is exactly six bytes,
`30 38 1e aa 4e 75`, SHA-256
`506215d03a2272be5f938a8926864075fc50a79d8c2fc23f22955d290fe0c98f`:
literal `MOVE.W $1eaa,D0; RTS`. This links the post-read branch to the original
dispatch-word return only. It does not select vector 5, give the word game
meaning, perform raw reads, or emulate XBIOS.

A separate copied-dispatcher route reaches an explicit supervisor-callback ABI
boundary. At track-2 `+$d2` it pushes literal callback `$1fa6`, then selector
`$0026`; those 10 bytes have SHA-256
`11b26d5900e614547617a9c95611515e8238184756a0a18c7ff18b1ec372657b` and
are followed by `TRAP #14`. The callback at track-2 `+$1a6` is 12 bytes,
SHA-256 `1f8bdb0e61454fef9acb0dc3abcf7bfed2621828937380b415ab85d4f57ef143`:
`MOVE.L (A7),D0; LEA $7b000,A7; MOVE.L D0,-(A7); RTS`. Project Eon records
these literal operations but does not emulate XBIOS, supply a callback frame,
or infer what frame or return path that ABI would establish.

The 20 original bytes immediately after that `TRAP #14`, at track-2 `+$de`,
are SHA-256 `ed326a1d22a28ce5646b242c947c5120cb0855d6d05080e35ce398d48d459f56`.
They read longword `$25f4`, compare it with literal `$00071100`, then use
`BEQ.S +8` to join at `+$f2`; the not-equal route contains `BSR.W +$714`
and `BSR.W +$1032`, resolving from their extension words to `+$800` and
`+$1122`. This is deliberately recorded as a post-service control gate, not
as an XBIOS or callback return value: the read is a distinct RAM location and
neither its provenance nor either subroutine's effect has been recovered.

The not-equal branch's two local BSR targets are now individually
hash-validated, but remain behind that runtime-dependent comparison. The first
target at track-2 `+$800` is 48 bytes, SHA-256
`bb662ff9f02861d2bc40c9d3d2ca97a662abc494ec20a4037807a81b22ca95a6`.
It loads `$00071100`, stores it at `$25f4`, prepares literal stack words
including selector `$0005`, then reaches `TRAP #14` at `+$824`. The following
`ADDA.L #12,A7` and `BRA.W +$08e8` (to `+$1116`, calculated from the branch
extension word) are recorded only as post-trap byte layout; Project Eon does
not claim that the trap returns, invoke it, assign a service meaning, or
execute the branch.

The second target at `+$1122` has a 22-byte prefix, SHA-256
`c74fb6b1e03cf6a123698e0356f3c9dbc45e637d9ce2a9479fef37eec6cbfd8c`.
It loads literal words `$7e00`, `$20000`, and `$9000` into the original
register setup, then its `BSR.W -$1106` resolves from its extension word to
the local range wrapper at `+$30`; that wrapper reaches the already bounded
XBIOS-facing raw-reader at `+$60`. Those values are only caller-side
machine-code facts. Project Eon does not select the comparison path, perform
the raw read, infer a disk result, or follow code after that raw-reader/XBIOS
boundary.

That shared range wrapper is now independently bounded as 48 original
track-2 bytes at `+$30`, SHA-256
`132ce2473e3764453bba01308e1f5044dc748bbea8b01975b67a259aa57cea7e`.
Its `DIVS.W #$1200,D7`, saved-register sequence, and `BSR.W +$24` statically
target the raw reader at `+$60`. Later literal branches target `+$2a`,
`+$5e`, and its save/call loop at `+$34`; `+$5e` itself branches to the
six-byte `MOVE.W $1e28,D7; RTS` helper at `+$2a`. This is a verified control
layout, not an interpretation of `$1e28`: Project Eon neither supplies a RAM
value, infers an XBIOS/raw-reader result, nor claims that any caller reaches
or returns through the wrapper.

The wrapper's direct `BSR.W` target is also separately bound as the complete
74-byte raw-reader call layout at track-2 `+$60..+$a9`, SHA-256
`a5bec9d04daa8ce600add594f6325030acd2ad8535910dee62497da90d572c90`.
Its literal setup begins with `MOVEQ #9,D2`, compares against `$1200`, and
has byte branches to `+$72` and `+$82`; the later fixed ABI encoding has
selector `$0008`, opcode `TRAP #14` at `+$9c`, literal cleanup `$14`, then
the bytes `MOVE.W D0,$1e28; RTS`. These are hash-validated machine-code and
branch-layout facts only. Project Eon neither executes the ABI call, asserts
that it returns, treats `$1e28` as a particular status/result, nor performs a
disk operation from this routine.

The copied dispatch table's three distinct direct target bodies are now bound
separately from the runtime-dependent `JSR (A1)`. Table slots 0, 1, and 5
refer respectively to track-2 `+$11a..+$12d` (20 bytes, SHA-256
`04c8eba86a6259f8d0b175fa18792cc64263863db51e76f9de839eec5c79ce0f`),
`+$12e..+$14f` (34 bytes, SHA-256
`0bc76b22089d008e4ce90d63216c75acbe0786b0a06127fbd66ef0dc252949ac`),
and `+$152..+$1a5` (84 bytes, SHA-256
`eaee587850078d67a72dcf0da4b45e672c89a1352b040db580bedc0ba3b20e97`).
The 2-byte `BRA.B -$38` at `+$150` is likewise bound to the first body's
start. These are direct table/linkage and byte-span facts only: Project Eon
does not provide a table index, claim that an indirect call reaches or returns
from any body, execute a raw read or ABI call, or assign a state/game meaning
to the literal code.

The 26-byte suffix after the third body's independently bounded transfer loop,
track-2 `+$18c..+$1a5`, is SHA-256
`45ac9d176b63fa93e16475543939d2f16b4e98cc839b44d2ce2ba9358e978083`.
It contains two literal immediate-adjust encodings (`$b400`), a literal
`MOVE.L #$4c800,D0`, then `BSR.W -$170` to the already bounded local range
wrapper at `+$30`, and `BRA.W -$90` to the separately bounded dispatch-word
return at `+$114`. Both 68000 word-branch targets are calculated from their
extension-word address. This connects static byte boundaries only:
Project Eon neither selects this vector, executes its transfer/call/branch,
assigns a meaning to the registers or literals, nor performs raw-media I/O.

The copied dispatcher also contains the first byte-proven state-selection
mechanics, separated around the existing XBIOS boundary. Before that boundary,
track-2 `+$c4..+$cf` is 12 bytes with SHA-256
`03cf620d981a775fd1adabe55deea940e08760e3e49c62cd0643c22b5aa08082`:
`MOVE.L $25fc,D0; MOVE.W D0,$1eaa; LEA $2478,A7`. This records only that the
low word loaded from RAM `$25fc` is written to the dispatch word at `$1eaa`.
It does not supply or identify the value at `$25fc`, or attach a game meaning
to either address. A separate 22-byte table-lookup layout at track-2
`+$f2..+$107` has SHA-256
`8e8551a51a7b989e6d2b7d1535819dea658a4e3e64562737755125c13c8f0d3c`:
it restores `A7`, loads table `$1eac`, reads `$1eaa`, shifts the word left by
two, loads `0(A1,D0.W)` into `A1`, and encodes `JSR (A1)`. That block lies
after the unmodelled supervisor-service boundary, so Project Eon does not
assert that it is reached, that its JSR executes, that the index is bounded,
or that any table vector is selected. The two independently hash-locked
layouts make the original input-and-lookup relationship inspectable without
inventing XBIOS, callback, or boot-state semantics.

The 18 literal bytes immediately after that indirect call, track-2
`+$108..+$119`, are independently hash-locked to
`e9ae4bd51bb06c6cb57ac7f26e81497995f7639f99a12e2a149194a39589e16c`.
They encode `MOVE.L D1,-(A7); ADDA.L #$1200,D4; MOVE.L D2,D7; BSR.W -$e2`
to the local range wrapper at `+$30`, then `MOVE.W $1eaa,D0; RTS`. This is a
post-indirect-call register and branch layout, not proof that any table
handler returns, that D1/D2 carry a particular game meaning, or that the
wrapper/raw reader is reached. Project Eon records the original `$1200`
increment and branch target without executing or supplying their inputs.

#### Deuteros Atari ST dynamic-trace acquisition boundary

There is no further caller-connected static parse that can establish a
runtime state after this point. The supplied Replicants Disk 1 bytes prove
the code *requests* the supervisor callback and later indexes the six-entry
table, but they do not contain the callback frame, the service result, or the
RAM values that determine the branch and indirect call. A next recovery step
therefore requires an external trace made from this exact outer archive
(`c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653`), the
hash-identified Replicants Disk 1
(`aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee`), and
the second-stage interval
`2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7`.

The trace must retain the emulator/ROM/configuration identity and input
timeline already required by the reference-trace format, plus these raw
observations in execution order:

- the `TRAP #14` at copied-stage `+$dc` (RAM `$1edc`), including the incoming
  A7/SR, selector `$0026`, callback argument `$00001fa6`, and the service
  return PC/A7/SR/D0;
- callback entry at RAM `$1fa6` and its return, including the incoming stack
  longword read by `MOVE.L (A7),D0`, the outgoing A7 after it is set to
  `$0007b000`, and the returned PC/A7/SR/D0;
- the values and provenance of RAM longwords `$25f4` and `$25fc` at the
  comparison/input-capture sites, the resulting branch PC at copied-stage
  `+$ee`, and the word written to `$1eaa` by the hash-locked
  `+$c4..+$cf` block;
- when the table lookup is reached, the table base, shifted index, resolved
  `A1` target, indirect-call entry/return PCs, and that callee's returned
  `D1` and `D2`; and
- the first subsequent raw-reader call/return at copied-stage `+$60`, with
  its complete XBIOS input frame and result, before any claimed load,
  resource type, palette, or display state.

The physical stage offsets above map to copied RAM by adding `$1e00`; the
original source remains the hash-locked track-2 interval, not an emulator
snapshot. A trace missing any one of these observations may still be kept as
preservation evidence, but cannot choose a dispatch vector or justify a
runtime implementation. No current parser, test, or launcher path supplies
these values, calls XBIOS, or synthesizes a fallback screen.

`deuteros-atari-st-boot-v1` is now a reference-trace v2 diagnostics adapter
for exactly this acquisition contract. It pins the outer archive, Replicants
Disk 1, and copied second-stage interval hashes, then accepts only the raw
`trap`, `callback`, `state`, `table`, `frame`, and `raw-reader` observations
specified in `REFERENCE_TRACE_FORMAT.md`. Admission reports counts only; it
does not replay values, invoke XBIOS, install a callback, choose a dispatch
target, or treat a captured frame/result as a runtime input.

The byte-proven continuation after that wrapper boundary is retained without
asserting that the raw reader returns. At track-2 `+$1138`, the next 38 bytes
have SHA-256 `5b1480495df8defe3e1264dd083ec1c91134c01e56d3d94e060c583ee9b54a89`.
They place RAM `$20000` and literal selector `$0006` before `TRAP #14`, then
lay out `ADDA.L #6,A7`; a copy loop from `$20020` through the longword pointer
at `$25f4`; and `RTS`. The loop begins with `D7 = $1f3f`; its `DBF -4` targets
the preceding `MOVE.L (A0)+,(A1)+` at `+$1156`, giving a literal 8,000-longword
layout if reached. Neither selector `$6`, the pointer's provenance, service
return, copy outcome, nor the enclosing raw-reader return is inferred or
executed by Project Eon.

The other post-service layout is bounded independently. The first callee's
literal `BRA.W +$08e8` at track-2 `+$82c` resolves from its extension word to
track-2 `+$1116`. The 12 target bytes have SHA-256
`8778c08ae16a5f66009dda8d60a0dacba267cca4d29211a11fd2e30c40a7796b`:
`MOVE.L #$0000b000,D0; MOVE.L D0,$25f0; RTS`. This records a target which is
encoded after the selector-5 `TRAP #14` boundary; it does not assert that the
service returns, that the branch is taken, that the target executes, or that
either RAM address/value has a game meaning.

The supplied unlabelled Disk 2
(`5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193`)
branches to `$22` and carries the literal `KILLER_BOOT\0` marker.  Its
complete 512-byte boot sector is now a separate static-disassembly image at
the established boot address `$1000`; its first instruction branches to
`$1022`, and that location branches to `$1030`. The exact sector SHA-256 is
`169991a2e9f6210b3285f8bf0afcdcccf9b652f87bce52ac44a57b0490a1329f`.
Its byte-complete external listing has 191 lines and SHA-256
`3ed535b76b5f73b66c9a097813a50aef732fc38f2a571ef2246046ffbc29c2be`.
This mapping does not make the reset vector at RAM `$4` known.
Its
post-BPB setup copies ten longwords from boot offset `$f0` (the `LEA
$000e(PC)` at `$de` resolves from its extension word at `$e2`) to RAM `$8`,
then jumps to relocated address `$12`. The 40 copied bytes have SHA-256
`21a5d61e2289fe2f2141d3710fad31faf42e96f59c5fba768819380e8f595a8d`.
There, the relocated continuation clears eight longwords at a time beginning
at `$32`, advances by `$20`, and loops without a counter or return. Project
Eon executes one bounded, local-only prefix of this exact protection path in
an isolated sparse-write record: it copies the ten original longwords to
emulated `$8..$2c`, enters the direct relocated `$12` continuation, records
the first eight zero longword writes at `$32..$4e`, and stops at the proven
backedge to `$30` with the next address `$52`. It never reads the separate
reset-vector cell at `$4`, follows `JMP (A0)`, invokes an ABI, wipes host
memory, or infers any resource/title semantics. The runtime reports these
boundaries rather than inventing a GEMDOS title path or unpacking media.

`DeuterosAtariKillerBootHandoff` now binds the complete caller-connected
relocation edge rather than only its resulting clear-loop shape. The 24-byte
setup at boot `+$d8` hashes to
`1ce81773d11374cac65ce69742a475e0731cbc8798f7c7bd374c04a2d2a7d150`; it
contains `DBF D7,-4` after `D7=$0009`, so the literal ten-longword copy is
encoded in the original boot sector, followed by `JMP $0012.w`. The destination
is within the relocated source span: `$12 - $8 = +$a`, whose first word is
`LEA +$1c(PC),A0` (`$41fa`). Separately, relocated offset `+$8` is the literal
`JMP (A0)` (`$4ed0`) after `MOVEA.L $0004.w,A0`; this is a vector-cell boundary,
not evidence that Project Eon may read or follow RAM `$4`. The parser validates
both hashes, the direct relocation relationship, the relocated continuation,
and the distinct indirect-jump layout. `execute_deuteros_atari_killer_boot_prefix`
adds the bounded direct `$12` branch described above; it neither chooses a
reset vector nor executes the indirect branch or unbounded loop, and it
attaches no game semantics to the protection code.

There is a separate static boot-entry route on the same supplied Disk 2. The
eight bytes at boot `+$6c` (`LEA $1156(PC),A0; BSR.W $10c6`) hash to
`5e21bb3b7a3bc300d36f330a3112efbc5388515eb0441f23d9205bcc26df3d95`.
The 18-byte callee at `+$c6` hashes to
`218908b4c5751ffa0b5b19aaebd278df41e29a8f70cd6285a0e05ee9e07f5c04`:
it saves A0, applies `EORI.B #$b9,(A0)+` until the transformed byte is zero,
then pushes GEMDOS selector `$0009` and executes `TRAP #1`. The first
caller-connected encoded span is boot `+$156..+$189` (52 bytes), hash
`56ca6d45903d6cd36809ebbba04adcf398197a84e1e41e1bf0e1e3d53de9e7f2`.
`decode_deuteros_atari_killer_boot_message` evaluates only that bytewise XOR
in a new host vector; its 52 resulting bytes, including the zero terminator,
hash to `9dfdd91bcc5c6b21d7d0751be79a527449045168d77f2f12240598384f898485`.
It neither modifies the supplied boot bytes nor invokes GEMDOS, displays the
decoded protection text, emulates a condition selecting this caller, or
assigns any title/game meaning. The `TRAP #1` service/frame and all later
boot-entry control remain explicit preservation boundaries.

Static inspection finds no next disk-stage load in the Killer Boot vector
route. The route copies only boot-sector bytes `$f0..$117` to RAM `$8..$2f`;
the direct `$12` continuation enters the proven local clear loop, while the
separate relocated `$10` path reads RAM `$4` and jumps indirectly. Neither
path contains a caller-connected disk read before those respective boundaries.

### Deuteros Amiga execution chain

Opcode-validated 68000 decoding proves:

```text
boot block
  decoded read: disk 0x2c00, length 0x1600 -> memory 0x12800
  entry 0x12a4e
    profile D0=0
      decoded read: disk 0x5800, length 0x4200 -> memory 0x20000
      JMP 0x21734
```

The main loader at `$21932` computes an unchecked `D0 × 4` address from
`$21708`. Caller-connected recovered paths prove only selectors zero and one,
which name these verified bundles:

| Disk offset | Length | Objects | Mode |
| ---: | ---: | ---: | ---: |
| `0x1b800` | `0x2f3f4` | 4 | 0 |
| `0x4ba00` | `0x215f0` | 6 | 1 |

Each 60-byte header has a big-endian length, object count, seven relative
channel pointers, six relative auxiliary pointers, and a mode word. The native
importer rejects an out-of-range bundle or non-null pointer. See the
[annotated disassembly](generated/deuteros-amiga-boot.md).

The three following longwords (`0x37000`, `0x59600`, and `0x6e000`) are raw
main-stage bytes adjacent to the two proven entries. Their locations begin
with values that cannot bound a source range within the supplied system ADF.
Eon therefore records neither as a resource-table entry, never probes it as
media, and requires a caller-connected selection trace before extending this
two-entry contract.

The live Amiga session also binds the separate clean Disk 2 image
`99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a` and
requires its `DEU\0` custom-media header. The opening presently reads only
the caller-proved Disk 1 ranges, but it cannot silently omit, replace, or
reinterpret Disk 2 while later game paths are still unrecovered.

`inspect_deuteros_amiga_data_disk_header` independently binds the genuine
Disk 2's 1,760-sector geometry, valid boot checksum, root block 880, and its
opaque first `0xc8` bytes (SHA-256
`3494ee5dc34793d7f09fdf2d8141be2ce5a0f07c78d6be5ccc12397bca7d9c06`). It
records the eleven literal `DEUTEROSDATA` markers in that prefix but does not
treat them as a directory, decode a data file, or attach semantics to the
remaining custom-media sectors.

After the verified opening handoff, `DeuterosAmigaTitleStageSession` now
executes the complete local profile-one prefix exactly once: its two sparse
title-RAM stores followed by `A7 = $40b62`. It records that result in a
one-way local-prefix advance and stops before the original `MOVEA.L $4.W,A6`
Exec-base read at `$40456`; no Amiga address space, Exec base, vector call,
or title display is fabricated.

The caller-connected bootstrap transfer immediately before that prefix is now
an explicit native state machine as well. Starting from the exact `$12ffc = 1`
return produced by the recovered `$0f` opening command, it validates profile
table `$12a36`, profile-one routine `$12b30`, its original destination/length/
track constants, the complete stage SHA-256, and the stage's absolute `JMP
$40426`. These synchronous advances record a read-only transfer checkpoint;
they do not allocate Amiga RAM, perform host disk I/O, add timing, or execute
an unknown service. Only after the entry dispatch is validated does the live
opening construct the existing title-stage session, which still stops before
the unresolved Exec-base read.

The first auxiliary pointer is a palette bank. The interpreter's command 4
multiplies its operand by 32 and copies 16 words from this bank to each active
display list. The words are standard 12-bit Amiga RGB4. Bundle 0, palette 1 is
anchored by `000 886 664 442 220 a60 840 620 080 ff0 004 008 02f 0cf fff e40`;
the native decoder expands every nibble exactly to 8-bit (`n × 17`).

### Deuteros Amiga opening audio

Channel opcode `$0b` passes its two words to `$22ab8`: the first is a sound
index and the second is a four-bit Paula-channel mask. The initialization
routine at `$212ca` installs bundle auxiliary pointer 3 in `$22aa6`; pointer 4
is subsequently consumed as the following resource, giving a strict boundary
for this table. In bundle 0 this is `0x121b4..0x122de`: 21 exact 14-byte
records plus four raw tail bytes (`0001ce8e`) which the 14-byte stride cannot
reach. They are retained as a bundle-relative offset, length and SHA-256
(`3f82cccd0194a3cda5510304a0696c3a9436c38e798c73441c1d9d9d6868ce0d`),
rather than treated as padding or a guessed twenty-second sound record. The
runtime does not make a second copy; the original ADF remains their byte
source.

The complete 298-byte table at ADF `0x2d9b4..0x2dace` is SHA-256
`04491b3f24bc635cfc7be4cfdad4536dc83fa8c3056848092aecb662594b68a4`.
The native source view carries that table hash and, for each record, its
bundle-relative descriptor offset, descriptor hash and DMA-byte hash. This
lets a renderer or audio sink cite the precise original descriptor it observes
without retaining a path, extracting media, or assigning timing to the
control words.

Every record contains a bundle-relative DMA address (longword), DMA length in
words, Paula period, volume, and two raw control/parameter words. The
hardware routine at `$22bea` copies the first ten bytes directly to
`AUDxLCH`, `AUDxLEN`, `AUDxPER`, and `AUDxVOL`; the remaining two words are
retained verbatim because `$22c08` onward uses their individual flags for
runtime modulation and looping. The native reader validates the boundary,
nonzero period/length, `volume <= 64`, and that the complete `length × 2` DMA
range remains inside the original bundle. Each descriptor retains a
non-owning span into the already owned, verified ADF rather than duplicating
its PCM range; the sound bank cannot outlive that ADF. It neither converts,
unpacks, copies, nor writes the game media.

The opening's second scheduler tick proves live use of entries 1 and 2:
`$0b,$0001,$0001` then `$0b,$0002,$0002`. They share source offset `0x2a8b`
and length `0x40bc` words (`0x8178` bytes; SHA-256
`f23fcd05f543be31726271b08ebfe7d907acfe31d1780aaf286fd2db701ae5d5`), while
their original periods are respectively `0x01c0` and `0x01c2`. SDL playback is
now enabled for the recoverable first DMA pass. The native Paula mixer takes
only an emitted `$0b` event with a nonzero bundle-table index, applies its low
four mask bits to AUD0..AUD3 exactly as the four `lsr.b` tests at `$22ad6`
through `$22b62`, and replaces each selected channel's DMA state. It uses the
original signed 8-bit sample bytes, original `AUDxPER`, and original
`AUDxVOL`; nothing is unpacked, filtered, looped, clipped, or replaced with a
generated waveform. The PAL sample clock is `3,546,895 / AUDxPER` Hz, carried
through the host's 48 kHz renderer as an integer phase accumulator so host
rounding cannot change the sample boundaries. Amiga's physical output routing
places AUD0/AUD3 on left and AUD1/AUD2 on right.

This is intentionally narrower than a guessed general sound driver. `$22bea`
first copies the descriptor to the four AUD register blocks, then executes the
raw words at offsets 10 and 12 through the modulation/loop branches at
`$22c08..$2301a`; their service cadence relative to the title scheduler has
not yet been proved. Project Eon therefore plays the authentic initial DMA
span and stops at its original `AUDxLEN × 2` byte boundary. Sound index zero
is also rejected for playback because `$22abc` selects the private `$22aaa`
descriptor rather than source PCM from the bundle. SDL receives at most one
20 ms host queue of this verified output. The mixer returns a short final
buffer at an original DMA boundary rather than padding a host callback with
made-up silence. The unresolved control-word service timing remains a preservation
research item, rather than a reason to invent looping or modulation.

#### Deeper first-DMA driver boundary

The two descriptors which the genuine opening emits on scheduler tick two are
not plain one-shot records: entries 1 and 2 both carry control word `$0202`,
with parameter words `$01c4` and `$0554` respectively. This is direct
on-media evidence that the subsequent service cannot safely be replaced by a
generic SDL looping rule. The initial descriptor copier and its following
control path are independently located at `$22bea..$2301a` (system ADF
`+0x83ea`, 1,072 bytes, SHA-256
`204033c2290a8457ed1b7c84191ed1794219d24278753be58c7173269e67a7a8`). The
control-only portion begins at `$22c08` (ADF `+0x8408`, 1,042 bytes, SHA-256
`0db998f5fa68023e02c6e1010d55618877da84c07723f3622e6ee835d5bf38c9`).

Its four repeated channel lanes read the descriptor's offset-10 control word,
branch on individual bits 8, 9, 11, 12, and 13, and consume mutable original
state including `$22a20`, `$22a16`, `$22a30`, and the per-channel AUD block.
The opening VM proves neither the writer/cadence for those state cells nor the
DMA-completion / register observation that selects each branch. In particular,
the two nonzero initial parameter words are not sufficient evidence for a host
loop length, modulation interval, period slide, or volume envelope. Project
Eon therefore keeps the raw fields in `DeuterosAmigaSoundRecord`, hash-locks
the descriptor table, records the observed code hashes above, and stops playback at the original initial DMA
length. Advancing that behavior requires a caller-connected trace which
captures those state values and service timing; no synthetic silence, loop,
or envelope is introduced meanwhile.

### Deuteros channel programs

Each non-null channel pointer begins with ten bytes copied verbatim into the
interpreter's 24-byte runtime state: two longwords and one word. The command
stream follows immediately and is word-opcoded. Routine `$214aa` recognizes
the complete range `$00`–`$14`; Project Eon now decodes the exact operand
shapes (zero, one, or two big-endian words/longwords) and rejects unknown or
truncated instructions without assigning guessed higher-level meaning.

Bundle 0 has four channels, all headed `00ff0000 00000003 0001`; their first
opcodes are `$13`, `$04`, `$03`, and `$03`. Bundle 1 has six channels headed
`00ff0009 00c60003 0001`; the first starts with command `$04`, operand `$0010`,
while the remaining five start with `$05`. These values are asserted directly
against the clean system ADF.

Auxiliary pointers 4 and 5 delimit a big-endian longword index and its payload
blob. Bundle 0 reserves 160 index slots, with 143 populated boundaries for 142
records in a `0x1ce96`-byte blob. Bundle 1 reserves 128 slots with 75 boundaries
for 74 records in a `0xb95e`-byte blob. In both, record 0 starts at offset zero, subsequent used
offsets strictly increase, and the unused table tail is zero-filled. The parser
validates these invariants but does not yet label the record contents as
graphics until the consuming routine is fully traced.

Routine `$20c8c` proves that this bank contains four-bitplane bitmap records.
For the normal path, each record begins with total planar words per row and a
height. The RLE control byte's top two bits select literal words, a repeated
byte-pair, a short repeated word, or a 14-bit-length repeated word. Decoded
words cycle through planes 0–3 for each 16-pixel group; bit 15 is the leftmost
pixel. All 74 records in bundle 1 use this normal path. Bundle 0 mixes 72
normal records with 70 bit-15 records; `$20eb2` proves those use the same RLE
classes but store each complete plane sequentially. Both paths now decode
natively, covering all 216 records in the two verified bundles.

`inspect_deuteros_amiga_bitmap_catalog` is the bounded preservation inventory
for that complete set. It records each source-relative record boundary, raw
record SHA-256, dimensions, and decoded-index SHA-256 while immediately
discarding decoded pixels. `--inspect` reports each bundle's record and pixel
totals, so diagnostics can identify the exact original-backed graphic evidence
without exporting, caching, or relabelling the commercial assets.

As a stable decoded-output anchor, bundle 0 record 1 is 48×17 pixels, has 311
nonzero pixels, and its 816 palette indices have SHA-256
`fca175276cfe376b85e936f455aa9e89d1a0d4c89a61d2b6ce317fa6aa58a6a3`.

### Channel runtime

The native, SDL-independent channel VM mirrors the 24-byte state consumed by
`$21380` and the opcode effects at `$214aa`. Implemented state transitions
include bitmap selection, signed coordinates, palette selection, timer waits,
audio-position waits, stepped vertical motion, relative jumps, one-level
calls/returns, sound events, alternate-resource selection, input gates, and
transition requests. Random opcodes require an explicit original-compatible
random source; absence fails closed instead of inventing a sequence. The
implemented `$2016a` source indexes the current bundle at
`(seed + vblank_counter) & $3ffe`, reads one big-endian word, adds 14 modulo
16 bits, then adds that result to the 16-bit seed. VBL interrupt `$207fe`
advances the 32-bit counter by four between scheduler calls. With the verified
zero start phase, the first opening random command occurs on tick 145 at
counter `$240` and returns/seeds `$0011`. A VBL can race the very first
scheduler call on original hardware, so an alternate startup phase remains an
explicit runtime parameter rather than being erased from the evidence model.

Command `$10` has a deliberately narrower meaning than its former
`transition_requested` event name suggested. The original dispatcher at
`$2162a` writes `$ffff` to `$210f4`; after `$21380` returns, the main loop
tests the byte at `$21856` and its nonzero branch at `$2185c` continues at
`$21892`. This is a verified main-stage request edge, not evidence for a
title, menu, or gameplay destination. Exhaustive control-flow walks of the
four bundle-0 and six bundle-1 channel streams (including their valid relative
jumps, calls/returns, and every `$11` branch displacement) contain no
reachable `$10`. The recovered opening input route reaches `$0f`, then `$05`
and `$00`, rather than `$10`. Consequently Project Eon does not synthesize a
channel state or expose a user input that would request this unproven later
continuation.

That continuation is separately preserved as raw static evidence. Main stage
`$21892..$218cb` maps to ADF `+$7092`, is 58 bytes, and hashes to
`120fba90e0b4fa9e96d8a6cf95fbac512d67d7daa42c3776ce0d3066b3f02ee9` on the
clean system disk. Its exact control-flow encodings are: zero branch `$21898
→ $218a2`; two local BSRs `$2189a → $2229c` and `$2189e → $224a2`; JSR
`$218a2 → $22a5a`; an equality loop `$218b6 → $218ae` after reads from
`$2079e`; JSR `$218b8 → $208ba`; bit-6 test at `$218be` with zero loop
`$218c6 → $218be`; and final branch `$218c8 → $217f6`.
`DeuterosAmigaChannelRequestContinuation` reports these byte-addressed facts
only. It does not select a condition, invoke a service, simulate the input
port, or assign names to cells and targets.

The first direct BSR target is independently retained as `$2229c..$2232f`,
ADF `+$7a9c`, 148 bytes, SHA-256
`d1a162af50f92b60d03b1da4ab186a547e46d145b0599cfbbeff7fb5af324ac1`.
It encodes a bit-5 test at `$222ac` with zero branch `$222b4 → $2232c`, a
literal counter `$000f` and DBRA `$222e0 → $222be`, two `-$c0(A6)` ABI calls
at `$222fc` and `$22312` using A6 from `$12fec`, a subtract-eight test at
`$2231c`, two `$21698` calls, and `RTS $2232a`. The complete range is
hash-locked by `DeuterosAmigaChannelRequestFirstCallee`; its custom-register
poll, state writes, vector calls, service calls, and return-dependent paths
are never performed or named by the runtime.

The second direct BSR target is separately hash-locked at `$224a2..$224cb`
(ADF `+$7ca2`): 42 bytes, SHA-256
`d4e9a1ee0065537a627cdd9ee8827f11d5fa28e0f860aacb21bbdc7e11784bd1`.
It encodes a longword transfer `$224e6 → $006c`, four literal word clears at
`$dff0a8/$dff0b8/$dff0c8/$dff0d8`, a literal `$000f` at `$dff096`, and RTS
`$224ca`. `DeuterosAmigaChannelRequestSecondCallee` retains only those raw
encodings; it neither reads `$224e6` nor applies low-memory/custom-register
writes, names hardware effects, or executes the return.

The following JSR target `$22a5a..$22b89` maps to ADF `+$825a`, is 304 bytes,
and hashes to `d5fdbdacd004d2cf377ea0dbaefb9d8b308ba23b568cfb3785456622bde49d19`.
It initializes literal zero at `$22a30`, starts with mask `$000f`, branches
over embedded bytes at `$22a6a` to `$22ab8`, and ends at RTS `$22b88`.
Its static descriptor facts include base `$22a6e`, stride `$000e`, source
record `$22aaa`, payload addend `$32a24`, flag cell `$22a6c`, and encoded flag
values `1/2/4/8`. `DeuterosAmigaChannelRequestFollowingService` reports those
facts only: no embedded descriptor, flag, or runtime-cell write is applied.
The adjacent entry `$22b8a` is deliberately outside this hash range because
it begins a separate caller-state-dependent path.

That adjacent entry `$22b8a..$22be9` maps to ADF `+$838a`, is 96 bytes, and
hashes to `10ed8be15c107dbb56ca98eb8d17ffd2bce3910dd169d67ba058447c9031b1ff`.
It tests `$22a30`, branches `$22b90 → $22b94` or returns at `$22b92`, then
encodes four conditional copies at `$22bb2/$22bc2/$22bd2/$22be2` and final
RTS `$22be8`. Its multiplication literal `$000e`, pointer cell `$22aa6`,
descriptor base `$22a6e`, field offset `$000a`, and stride `$000e` are raw
facts only. No caller register, pointer, branch, read, or write is supplied,
dereferenced, or executed by Project Eon.

The opening program provides tick anchors from genuine data. Tick 1 only
decrements initial waits. Tick 2 selects palette 1, enables the input gate, and
emits sound `(1,1)` then `(2,2)` and immediately consumes the newly yielded
timer once, as the original scheduler does. Tick 3 selects bitmap 1 at word
coordinate `x=8`, pixel coordinate `y=183`; its blank-backed 320×200 frame has
SHA-256 `d841fd0e6e01c09f7dc8ce6cd2bda1828a0eb62c5f198750403aa996cd7d48d4`.
Tick 4 enters stepped mode 6, moves to `y=181`, and leaves timer 38.

While this recovered opening is active, the launcher exposes a compact
machine-notation overlay: scheduler tick, original VBL counter, current raw
palette index, active channel count, and the `$2171e` gate bit. These values
are queried directly from the opening VM and never drive host input or title
logic. They make the rendered opening's provenance inspectable without
interpreting a channel as gameplay or drawing any synthetic status screen.

### Deuteros Amiga title input and bootstrap handoff

This is a control-flow fact, not a reconstructed game-menu interpretation.
Channel 3 of bundle 0 begins with `$03,$0050` and then `$14,$0001`. The
scheduler at `$2140c` resumes `$14` only after both the global gate at
`$2171e` is set and the previously-polled input word at `$21720` is nonzero.
The opening's `$13` sets that gate on tick 2. With continuously asserted prior
input, the real channel reaches `$0f,$00000b38` on scheduler tick 82; `$0f`
adds it to the verified main-resource base `$32a24`, stores `$3355c` at state
offset `$0c`, and replaces the selector with `$fe`. The VM separately retains
the raw bundle-relative `$0b38` operand for its alternate-resource event,
without giving either value an invented gameplay name.

The native real-media conformance test hashes the decoded RGBA surface after
every one of the 82 held-input scheduler ticks. Its 82 expected SHA-256 values
were derived from the hash-recognised English Amiga ADF at test-recording time;
the test contains hashes only, never source pixels. This makes a changed timing,
palette, blit, or handoff frame a deterministic preservation failure while
keeping the original media external and immutable.

The launcher keeps that held signal scoped to an active opening session. It
clears it before a new Deuteros session and when the Modern F10 renderer dialog
opens, so a host-modal transition cannot leak a stale Space/Enter/South hold
into the next recovered `$14` poll. Closing the dialog requires a fresh host
press; it does not manufacture an original CIA sample, title command, or input
past the Exec/graphics boundary.

Separately, the main loop polls active-low CIA-A port-A bit 6 at `$bfe001`
after `$21380`. Once the gate and recorded input are both set, the first-buffer
path branches to `$21982`. For the opening's initial index zero, `$21982`
writes one to `$21704` and calls `$218cc`; its confirmed post-display path
increments that value to two and branches to `$21a4c`. That routine writes one
to `$219f4`, copies it to bootstrap return slot `$12ffc`, and returns.
Bootstrap table entry one at `$12a3a` is routine `$12b30`: it requests raw
decoded track data from disk offset `0x6e000`, length `0x6ca00`, into memory
`0x13000`. Project Eon retains these load constants as `title_handoff_profile`;
the first word of that real stage is verified as `JMP $00040426`. The target
is range-checked against the loaded interval and retained as `title_stage`;
the runtime still reads this source ADF range in place and does not unpack it.

When—and only when—the live opening VM reaches that exact `$0f` handoff with
raw operand `$0b38`, `DeuterosAmigaTitleStageSession` now opens the same ADF
interval read-only. The session validates the existing title-stage opcode
profile and binds the caller-proven bootstrap profile one to its exact local
entry-prefix result, exposing only disk provenance (`+0x6e000`, length `0x6ca00`,
destination `$13000`, entry `$40426`) and hash-validates whole-stage SHA-256
`48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03` for the
verified clean system ADF; altered stage bytes fail closed. It never creates a title bitmap, inferred registers,
global work memory, or replacement menu: its next execution requirements still
cross unrecovered Exec and graphics-library vector boundaries.
The root title-stage parser enforces that same complete stage identity before
it accepts any opcode window, so a matching entry or palette prefix cannot
substitute altered bytes elsewhere in the original `ADF +0x6e000`,
`0x6ca00`-byte stage.
The entry begins by preserving the bootstrap's `A1` value at `$206a0`, storing
the passed mode word at `$4040e`, and comparing its low byte with five. The
meaning of those mode values and the later gameplay dispatch remain unknown.
For the live profile-one handoff, `DeuterosAmigaTitleStageSession` now exposes
the two direct pre-Exec stores as an immutable sparse state record:
`$4040e.w = $0001` and `$19d52.b = $01`. This is a direct result of the
hash-validated entry instructions, not a title-stage RAM image: the incoming
controller longword is still unavailable, no other title RAM is initialized,
and neither the source ADF nor a host-side Amiga memory map is changed.
After this handoff, the launcher also presents those two numeric writes and
the one local A7 setup value as a compact provenance row beside the raw palette
strip. It obtains all three values from the session's hash-validated sparse
records; the row is machine notation rather than a title screen, title text,
or a guest-memory allocation.
The same admission also verifies the immediately following hash-locked local
graphics-setup and display-clear profiles.  This binds the twenty-word RGB4
source at `$1ed24` (destination operand `$12ecc`) and the `$1f40` four-byte
clear-loop descriptor to the live, read-only title-stage session.  The session
does not execute either helper, resolve the externally supplied `$12ff4` base,
or clear host memory; it rejects a stage whose entry prefix is intact but whose
later setup bytes no longer match the verified clean stage.
`DeuterosAmigaTitleHandoffRoute` additionally binds that live event to the
six original command bytes at ADF `+$1c28a` (`00 0f 00 00 0b 38`, SHA-256
`9f3880bf72d32f0fc119b941527dfe6004e18ad7e0fdfc40fe87eb6a13fe9c41`) and
to the verified main-stage return cell `$12ffc` with profile value one. Its
resulting original pointer is `$32a24 + $0b38 = $3355c`. This is a narrow
caller-to-session admission check: the VM retains the `$0f` command's exact
bundle stream offset, absolute ADF offset, and channel index. The session
requires the original command at ADF `+$1c28a` (bundle `+0x0a8a`, channel 3)
as well as its `$0b38` operand, so another `$0f` event, an equal operand in
another bundle or channel, or altered command bytes cannot open the title
stage.
For mode five it copies the byte to `$3717e` and writes `$0101` to `$38092`;
every other path writes byte one to `$19d52`. The shared prefix is now
opcode-validated through `$40574`: it installs stack `$40b62`, loads Exec base
from `$4`, calls vectors `-$96` and `-$9c` with literal `D0=$7fff0`, calls the
original internal setup sequence, copies `$1f168` to `$1f974` and `$410d8`,
and prepares custom base `$dff000` with words `$7fff/$7fff/$c000/$87ff` at
offsets `$40/$42/$9a/$96`. It then distinguishes a mode-five call to `$36a8c`
from the normal call to `$1fb9a`. `DeuterosAmigaTitleStageProfile` records all
of these as initialization requirements only: Project Eon does not call Exec,
write the custom chip, allocate memory, or infer the calls' higher-level
effects. After that shared setup, the recovered
recurring loop starts at `$40574`, calls `$222c0` then `$23e4e`; a mode/input
change clears `$40410`, and the loop compares it with `$0000ea60` before the
original `$4069a` dispatch, subject to another original-state check. The strict
parser validates these operands directly and does not claim their gameplay
semantics.

The first common internal setup callee is independently hash-locked as a
caller-connected ABI boundary. `$1ed80..$1edf5` (ADF `+0x79d80`, 118 bytes,
SHA-256 `42c96aa502e36711ed274b9ddf4d2d1de53abfebb4ebdf88fa99346d2b03e30b`)
passes the literal NUL-terminated `graphics.library` at `$1ed02`, zero in D0,
and Exec base `$4` to vector `-$228(A6)`. A zero D0 result takes the original
self-loop at `$1edf6`; a nonzero result is stored at `$12fec`, the same raw
cell later supplied to graphics-library vectors. This does not establish the
vector ABI, the result value, or whether the original call returns.

The next direct setup call at `$1f172` enters local helper `$1eda6` and clears
word `$1f16c` after it returns. `$1eda6..$1edf5` (ADF `+0x79da6`, 80 bytes,
SHA-256 `d6b37bc6431a1fe9145ae9403a5165028ccfd856a6529d1752f824b166807223`)
copies the externally established longword at `$12ff4` to `$1f168` and
`$1f164`, copies exactly twenty original RGB4 words from `$1ed24` to
`$12ecc`, then adds `$7d00` to `$1f168` and stores the result at `$1f16e`.
The 40 source bytes hash to
`5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55`.
`DeuterosAmigaTitleGraphicsSetupProfile` reports this provenance only; it does
not resolve `$12ff4`, write title-stage memory, open a library, or turn those
palette words into an SDL title screen.

The engine-owned `DeuterosAmigaTitleOpenLibraryBoundarySession` now makes the
preceding transition explicit without implementing Amiga Exec. It admits only
an observation newer than both exact Exec returns and matching entry `$1ed80`,
name `$1ed02`, Exec-base source `$4`, call `$1ed8c`, vector `-$228`, and return
`$1ed90`. D0 and SR are retained verbatim as value-only evidence. Observed
`D0=0` stops at the original `$1edf6` self-loop; an observed nonzero D0 retains
only the proven sparse-store destination `$12fec` and stops at caller return
`$40472`, before `$1f172` consumes unresolved graphics/display state. No host
library is opened and neither outcome is a title-display or parity claim.

For a nonzero admitted return, the same session can now advance through the
remaining caller-connected local instructions. It retains the observed D0
store at `$12fec`, records the original one-word increment operation at
`$1ed70` without inventing its prior or resulting value, returns to `$40472`,
and follows the exact direct calls `$40472 -> $1f172 -> $1eda6`. Execution then
stops before `$1eda6` reads the unresolved external display-base cell `$12ff4`.
No palette copy, derived framebuffer pointer, display clear, or graphics ABI
call is performed by this continuation.

The following boundary accepts a strictly later, explicitly addressed read
observation only at instruction `$1eda6` from cell `$12ff4`. With that raw
value, the engine can describe the hash-proven local effects: copies to
`$1f168/$1f164`, the twenty original RGB4 words at `$12ecc`, the checked
`+$7d00` pointer at `$1f16e`, zero at `$1f16c`, caller copies to
`$1f974/$410d8`, and the `$1f182` clear plan of `$7d00` bytes in four-byte
writes. These remain sparse values and a bounded write plan; the host does not
allocate or clear that address range. The session stops at `$40498` before the
first custom-chip write and therefore claims neither display output nor a
graphics/hardware ABI implementation.

The next native boundary admits the four custom-chip writes only as an exact,
strictly ordered observation sequence: `$40498/$40/$7fff`,
`$4049e/$42/$7fff`, `$404a4/$9a/$c000`, and `$404aa/$96/$87ff`, all relative
to `$dff000`. Project Eon performs none of these hardware writes. Once all four
are observed, the hash-locked caller continues through `$404b0 -> $1ef74` and
retains the local descriptor/request plan (`$1ef48`, callback `$1f056`, request
`$1eefa`, command 9). It stops at `$1f04a`, before reading Exec base `$4` and
calling vector `-$1ce` at `$1f04e`. The descriptor plan does not establish the
service, callback ABI, device, or a callback invocation.

The callback-registration Exec boundary now accepts a return only when its
sequence follows all four admitted custom-chip observations and its identity
matches base source `$4`, call `$1f04e`, vector `-$1ce`, and return `$1f052`.
D0 and SR are retained verbatim without interpreting the service result. The
hash-proven `$1f052` RTS then returns to `$404b6`, where the session stops
before the next unresolved direct call to `$206d4`. This does not assert that
a callback was installed, invoked, or associated with a host input/device.

At `$206d4`, a separately hash-locked 14-byte prefix (SHA-256
`a5c916b3959fe074f18e12a12d0488a38b2c8b638079fb05d1ad3a0739848001`)
clears A1 and calls Exec vector `-$126` from base `$4` at `$206de`. The native
session accepts only an exact, later return observation at `$206e2`, retaining
D0/SR verbatim. The following 38 local bytes (SHA-256
`b1cc2be3a282d4a49fdc161f1d6b8c74a03be4a7aa5b13b8f2300f179dbb8cde`)
save observed D0 and describe the literal `$2061c` descriptor fields, including
pointer `$206ac`. Execution stops at `$20708`, before another Exec-base read
and vector `-$162` call at `$2070c`; neither Exec service is identified or
implemented.

The next eight-byte Exec boundary at `$20708` hashes to
`913043cfe14c05c8e74c79915e6922eb2ccd071169a85e1ab0b47f85925ff795`.
Only an exact later return from call `$2070c`, vector `-$162`, to `$20710` is
admitted, with D0/SR retained as uninterpreted values. The following 30 local
bytes hash to `f4312fcc6e66dd97c124f167ac5634d69bd32071fada15f48194324bb1b29dd7`:
they establish the literal `$205e4` pointer at `$20698`, link `$2061c` at
offset `$0e`, and clear local D0/D1. The session stops at `$2072e` before the
next Exec-base read and vector `-$1bc` call at `$20732`.

The `-$1bc` boundary bytes at `$2072e` hash to
`0a982fb16e92100a04d3528d727297363de61d99ac61f8a193c4ee6c55ac4888`.
An exact later return at `$20736` retains D0/SR and follows the original test:
nonzero stops at the `$2073a` self-loop. Zero admits the next 64 local bytes
(SHA-256 `e5f6841f53d99f63a4c4de84abc98d334f2a376cea4acbf279f5534c4e79b063`),
which mark the first `$205e4` descriptor inactive, restore the genuinely
observed first `-$126` D0 value, and place it in the literal `$20676`
descriptor. The session stops at `$20776` before another `-$162` Exec call at
`$2077a`; it does not infer why either branch occurs.

On the admitted zero path, the repeated eight-byte `-$162` boundary at
`$20776` has SHA-256
`913043cfe14c05c8e74c79915e6922eb2ccd071169a85e1ab0b47f85925ff795`.
An exact later return at `$2077e` retains D0/SR without interpretation. The
following 30 local bytes hash to
`ee1c0c590b6037a7e59608bb83dae26c4ffc510fd53e3a8b05ccff78fab8c2c0`;
they establish `$2063e` at `$2069c`, link descriptor `$20676`, and set literal
D0/D1 to 1/0. Execution stops at `$2079c` before the next `-$1bc` call at
`$207a0`.

The final eight-byte Exec boundary in `$206d4` starts at `$2079c` and hashes
to `0a982fb16e92100a04d3528d727297363de61d99ac61f8a193c4ee6c55ac4888`.
An exact later `-$1bc` return at `$207a4` selects one of two paths in the next
40 bytes (SHA-256
`cdf3332e5b071d102231d45ecf0b05a87728df49a7f08ff27ffc2e92f055e416`):
nonzero copies the first pointer cell `$20698` to `$2069c`; zero marks the
second descriptor reached through `$2069c` inactive at offsets `$30/$1e`.
Both join at RTS `$207ca`, return to `$404bc`, and stop before the unresolved
call to `$206be`. No pointer target is dereferenced by the host model.

After either admitted final `$206d4` outcome, `$20698` still contains the
literal first descriptor pointer `$205e4`. The complete local `$206be..$206d3`
helper hashes to
`cdcda125af5c05d4d88e7d486f15f50bd87c4641a38e9c8a4e29a9394152317a`;
its caller-connected path copies that value to controller cell `$206a0` and
returns at `$206d2`. The caller then sets D1 to `$13000` and invokes the
separately hash-locked `$404c2 -> $403e6` pointer seed, which records literal
`$1c482` at `$1f97c`. The native session stops at `$404ce` before the opaque
`$403f4` service batch. These are sparse local effects only; no pointer target
is dereferenced and no service return is assumed.

The first `$403f4` batch target `$403c8..$403e5` is independently hash-locked
(SHA-256 `3f9cf2302a4078faddd0796fc05268386d46c4be64f294b8082ba085b9609f5f`).
It supplies `$1ed24`, `$12e12`, and D0 `$14` to graphics vector `-$c0` at
`$403e0`. Admission requires the exact observed library base previously
returned by OpenLibrary at cell `$12fec`, an ordered return at `$403e4`, and
retains D0/SR verbatim. The batch then reaches `$20510`; its complete 38 bytes
hash to `60ee2fcb4a18f62cd2066aba2429e760a64f14cd3f07f3cfe8467972030008bc`.
Only the three literal writes before its runtime read are exposed. Execution
stops at `$2052a` before reading unresolved word `$20276`; no graphics copy or
runtime word value is fabricated.

The `$2052a` boundary now admits only a strictly later 16-bit observation from
exact source `$20276`. That value is retained unchanged for the original
`$2027c` destination; the hash-proven routine then reaches RTS `$20534`.
The enclosing batch advances through direct call `$40400 -> $1f37a` and stops
at `$1f37a` before its first unresolved nested call to `$20094`. The observed
word is not assigned a timer, device, or gameplay meaning, and `$20094` is not
entered or presumed to return.

The existing hash-locked `$20094..$200f9` profile now has an active first ABI
boundary. It admits only a return newer than the `$20276` observation and
matching library-base cell `$12fec`, the exact previously observed OpenLibrary
value, call `$2009c`, vector `-$19e`, and return `$200a0`. D0/SR remain raw
evidence. The following local instructions set D0 to `$ffffffff`, load
descriptor A0 `$1ffda`, and reload the same library-base cell. Execution stops
at `$200b0` before vector `-$198`; no graphics service effect is inferred.

The second active `$20094` boundary requires the same `$12fec` library-base
identity, a strictly later observation of call `$200b0`, vector `-$198`, and
return `$200b4`. D0/SR remain raw; only D0's instruction-defined low byte is
retained for sparse destination `$20092`. The hash-proven local continuation
records literal pointer `$1ffe6` at `$2008e`, descriptor `$1ffda` words
`10/10/12` at offsets `6/8/4`, and the exact A0/A1/A2 inputs for the next call.
It stops at `$200f4` before graphics vector `-$1a4`, without applying any
graphics effect or dereferencing the destination pointer.

After the verified opening handoff, the launcher may show the first sixteen
raw RGB4 words at the independently hash-validated `$1ed24` source as a small
palette-evidence strip. `DeuterosAmigaTitleStageSession` decodes only those
literal 12-bit colours from the original in-place stage after range checking;
it does not invoke graphics.library, allocate a display, use the palette to
colour a reconstructed bitmap, or present the strip as an original title
screen. The launcher uses all twenty words copied by the graphics-setup
routine, while the separately modeled transition only consumes its first
sixteen. The surrounding launcher text explicitly preserves that boundary.

The immediately following caller-connected local callee is now bounded too.
`$1f182..$1f195` (ADF `+$7a182`, 20 bytes, SHA-256
`9b02afb723e201cacb93d18d87613dee0f56369707867989209a41d9430ec5f3`) loads
its destination only from the externally established `$1f168` cell, clears D1,
and uses original `DBRA D0,-4` with initial D0 `$1f3f`.  It therefore encodes
exactly `$1f40` sequential four-byte zero writes followed by RTS `$1f194`.
`DeuterosAmigaTitleDisplayClearProfile` preserves that loop and its source
hash, but does not resolve `$1f168`, allocate or clear host memory, name the
target a screen, or treat the preceding graphics-library call as having
returned.

The next contiguous routine `$1f196..$1f22f` (ADF `+$7a196`, 154 bytes,
SHA-256 `31fc346d9d2647001899a2e939482aa97bd8bc94221ae81384787997928bb42b`)
is separately hash-locked as `DeuterosAmigaTitleFourPassByteCombineProfile`.
It returns unless unsigned D2 is in `$0040..$0137` and D3 is in
`$0024..$006f`; accepted values have `$40` and `$24` subtracted, respectively.
The low nibble of D0 selects an eight-byte record from `$1f8ec`; the routine
uses a D3 stride of `$28`, then reads its base from the same unresolved
`$1f168` cell. It derives one bit and performs four identical byte-combine
passes separated by `$1f40`, finally restoring A0/A1 and returning at
`$1f22e`. This establishes exact local gates, table/pointer operands, and
four-pass shape only. Project Eon supplies no D0/D2/D3 values, does not read
the table or unresolved base, performs no byte writes, and does not call the
routine a renderer or infer a UI/control effect.

When that counter reaches the verified threshold, the call at `$405b6` enters
`$4069a`. This transition sets byte `$202c6`, saves and clears word `$202b8`,
then copies exactly sixteen RGB4 words from `$1ed24` to `$40678`. Each copied
word is ANDed with `$0eee` and shifted right once before being written. It then
uses the original graphics-library base from `$12fec` for vectors `-$c0` and
`-$1a4`; both vector calls and all operands are opcode-validated. Project Eon
records this as a timed title display transition, not as a guessed description
of a menu or gameplay screen.

The caller's immediate control-flow conditions are also retained. It enters
`$4069a` only when `$40410 >= $0000ea60` and word `$22d34` is not `$0011`.
The fully local gate at `$4059e` (ADF `+0x9b59e`, title-stage `+0x2d59e`) is
40 bytes with SHA-256
`47c56a2ad892d973cc967bca2a8c3b34338ffbdbff3b1b57ecef63cc6d8d7200`.
Project Eon evaluates its unsigned threshold and inhibit comparison directly
from hash-locked original bytes. Both skip routes converge at `$405c6`; only a
dispatched transition whose original call returns would reset `$40410`. The
evaluator does not mutate a counter or invoke `$4069a`.

Its prefix before the first graphics.library vector is independently executable
as a read-only in-memory preservation model. At `$4069a` (ADF `+0x9b69a`,
title-stage `+0x2d69a`), the exact 72 bytes hash to
`fda01edebbc2e99372cb22a858269202343f98d31bee1e473f751048666759ca`.
They set `$202c6` to one; read sixteen RGB4 words at `$1ed24`; apply
`AND.W #$0eee` then `LSR.W #1`; write the results at `$40678`; save and clear
`$202b8`; and prepare A0=`$12e12`, A1=`$40678`, D0=16 and A6 from `$12fec`
for `JSR -$c0(A6)`. The source is ADF `+0x79d24` / title-stage `+0xbd24`, 32
bytes SHA-256 `6920018538a18ca186ef36431678de4fc8f7bc68ac6b481e82086dbda54ff1e1`.
The recovered transformed sixteen-word result serializes to SHA-256
`e8f4bdf6b52bc849b626145464ccbc2701c6869cc97e62ef9dcfecb660a01aa8`.
Project Eon models no graphics vector, custom hardware, or title screen here:
callers must establish the original gate before this prefix can ever be part of
a live session.
Immediately after the routine returns, it writes long zero to `$40410` before
resuming the normal loop. This is a verified reset/gate relationship only:
Project Eon does not assign gameplay meaning to `$22d34` or synthesize either
state value at run time.

Both timer skips converge at `$405c6` (ADF `+0x9b5c6`), whose ten bytes hash
to `68ccbd8edf32800e43fe55c47356e162896b8500b01d2e9fd461191ba1760736` and
test byte `$1bf36` before branching to `$40638`. The clean original state word
at ADF `+0x76f36` is zero (SHA-256
`96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7`). The
zero branch's 60-byte response loop at `$40638` / ADF `+0x9b638` hashes to
`b47192ea229873ef1ae47f841d044393bfd3e7e1a7fc0ca92308a555c2eb84d0`.
It calls `$1f238`, treating its low-byte return from the locally recovered
but runtime-dependent helper as an explicit input; a
non-`$43` response returns to `$40574`. A `$43` response XORs `$1bf36` with
`$0101`, and the verified clean-state route emits literal `$0f00` writes to
`$dff180` before further helper responses. Project Eon records this as a
hash-locked machine-write trace only: it neither calls the helper nor writes
the custom chip, and it leaves the unrecovered nonzero-state route unmodeled.

The same `$4069a` routine has a bounded, verified two-call return phase. Its
first `-$1a4` setup supplies `$12e12`, `$1ffda`, and `$20056`, then stores the
third address in `$2008e`. On return it snapshots words `$1ffc8`, `$1ffce`,
and `$1ffd4`; while all three still compare equal, the original tight branch
loops at `$4071a`. The bytes do not identify a concurrent writer or permit
Project Eon to provide one. When a comparison differs, the second `-$1a4`
setup instead supplies `$12e12`, `$1ffda`, and `$1ffe6` and again stores its
third address in `$2008e`. The routine subsequently clears `$202c6`, invokes
`-$c0` with `$12e12`, `$1ed24`, and count 16, restores the saved `$202b8` word
from the stack, and returns at `$4077c`. The parser opcode-validates every
fact here; the addresses are preserved as raw machine-state boundaries rather
than named as a presumed menu, fade, or gameplay subsystem.

### Deuteros Amiga post-transition control loop

Immediately after the previous transition return at `$4077c`, original code
at `$4077e` clears word `$407e6`, then invokes `$3f7a8`, `$1f9a4`, `$1fe7a`,
`$3fbf8`, and `$1f238` in its original order while preserving that word on the
stack. Its feedback tail at `$407ba..$407e5` (ADF `+$9b7ba`, 44 bytes SHA-256
`b4212844a9f0fb4008caad00950e613b70581a5552cacabc253ea0966ed16df3`)
compares the helper's **low byte** against `$1b`, `$20`, `$2e`, and `$2c`.
`$1b` returns; `$20` and unmatched bytes loop without a local write; `$2e`
increments the low byte at `$407e6`; and `$2c` has the net low-byte effect
`-1`. Project Eon can evaluate that exact local feedback trace with explicitly
supplied helper response bytes, but neither calls the helper nor writes title
state. It does not assign names such as “selection”, “menu”, or “start game”
to the control word, helpers, or literal response values before the original
subroutines are independently recovered.

The title response helper is now recovered as far as its own original runtime
data boundary. `$1f230..$1f259` (ADF `+$7a230`, 42 bytes, SHA-256
`ed2794b7bb16f17ca9690b367c9465c75ff52838356bf6b46d9744cb16da1054`) first
loops on word `$1eed6` while it is zero. It then reads that word again; the
second zero branch reaches RTS `$1f258`. On the nonzero route, it reads one
byte from `$1eec0`, copies twenty subsequent bytes downward by one address
with `DBRA` initial value `$13`, decrements `$1eed6`, and returns. The direct
call sites include the known `$40638` and `$407b4` title response paths.
`DeuterosAmigaTitleResponseQueueProfile` hash-locks this exact wait/shift
shape. It does not supply the pending-word value, read or return a byte, model
concurrency, or make any writes to the original in-stage byte region.

For a nonempty, explicitly supplied original-memory snapshot,
`evaluate_deuteros_amiga_title_response_queue_once` now models the local half
of that path: it returns byte zero, copies original snapshot bytes 1 through
20 down to positions 0 through 19, and reports `pending - 1` as a new trace
value. It rejects zero pending count because the original then loops while an
unrecovered concurrent writer may change the word. The helper does not call a
callback, decide how a frame was produced, write title memory, or bind either
result to host presentation/input.

Static backtracking identifies a producer for that same byte region but also
the next hard input boundary. During title setup, `$1ef74` places callback
address `$1f056` in the original descriptor at `$1ef48 + $12`, then reaches
Exec vector `-$1ce(A6)`; neither registration result nor callback invocation
is available in the supplied ADF. The candidate callback itself is wholly
present at `$1f056..$1f14f` (ADF `+$7a056`, 250 bytes, SHA-256
`ff4b055b2d5128465c891debcad00ff4e53cbf661de47b9ee3d6278f33d5e5f8`). Its
local byte-one path reads caller-owned A0 offsets `$8` and `$6`, rejects a
repeat, values at or above `$50`, and a pending count at or above `$14`, then
copies one byte from table `$1ee20` to `$1eec0 + [$1eed6]` and increments
`$1eed6`. That connects the static callback path to the recovered response
queue, but does not identify A0's ABI or values, table semantics, the
registration service, or a real input device. Project Eon consequently leaves
title input and response production unimplemented rather than inventing a
keyboard, mouse, or host event mapping.

`DeuterosAmigaTitleCallbackRegistrationProfile` now hash-locks both the
registration body (`$1ef74..$1f051`, ADF `+$79f74`, 222 bytes, SHA-256
`f571a8e5e48c29fe3d6f493e503e2a3a0b3328ac4cafb425808eff48804c4f27`) and
the callback. Direct instructions establish request `$1eefa + $1c = 9`,
`$1eefa + $28 = $1ef48`, descriptor `$1ef48 + $12 = $1f056`, and the call to
Exec base `$4`, vector `-$1ce`. The immediately following original instruction
at `$1f052` is `RTS`, so the local routine does not inspect a service result
before returning; this still does not identify the service ABI or establish
that it returns. The callback's producer route begins only for
the byte-one comparison at caller-owned `A0 + $4`; its direct bounds are word
`A0 + $6 < $50` and pending word `$1eed6 < $14`, after which it indexes
the exact 160-byte original interval `$1ee20..$1eebf` (SHA-256
`2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265`) and
writes `$1eec0 + [$1eed6]`. The `$50` bound plus the original conditional
`+$50` adjustment proves this is the complete locally addressable source
interval; it does not prove the meaning of any byte. These are an ABI-shaped
data path,
not proof of the request type, callback event names, input device, or any
runtime value supplied by Exec.

The same hash-locked callback has two additional locally bounded routes. Before
its branch checks it mirrors caller byte `A0+$4` into `$1ef2e`. The byte-two
route first rejects nonzero `$1ee16`; with word `A0+$6 = $00ff`, it copies
words `A0+$a` and `A0+$c` to `$1ee10` and `$1ee12` then reaches unresolved
service `$20118`. Otherwise it masks `A0+$6` with `$007f`, accepts only `$68`
or `$69`, derives a two-bit value from word `A0+$8`, and stores it at `$1ffd4`.
The byte-one producer stores word `A0+$8` at `$1ee0e`; its low three bits select
whether the bounded source index from `A0+$6` addresses the first `$50` bytes
of `$1ee20..$1eebf` or the second `$50`. Project Eon records these exact
operands and bounds only. It neither supplies an A0 frame nor calls `$20118`,
Exec, or a device, and does not give the bytes, words, or destinations a
presumed control/input meaning.

For preservation experiments, the accepted byte-one producer arm is also
available as a deliberately detached, controlled trace. After validating the
same callback and table hashes, `evaluate_deuteros_amiga_title_callback_producer`
accepts only caller-supplied `A0+$6 < $50` and pending count `< $14`; it
records the original mirror to `$1ef2e`, word copy to `$1ee0e`, the table index
(`A0+$6`, plus `$50` when low three bits of `A0+$8` are nonzero), and the one
original byte that would be written at `$1eec0 + pending`. It returns the
incremented count as a trace value only. It does not invoke a callback, map
host input, write the queue, or make the accepted arm evidence of an original
input device or title-menu action.

The callback's independently bounded byte-two arm is exposed on the same
terms. `evaluate_deuteros_amiga_title_callback_second_event` takes an explicit
value for the original `$1ee16` gate plus caller-frame words; it returns before
the arm when that gate is nonzero. With word `A0+$6 = $00ff`, it reports only
the two local word copies to `$1ee10`/`$1ee12` and stops before service
`$20118`. Otherwise it masks that word to `$007f`; only `$68` or `$69` reach
the local two-bit carry trace from `A0+$8` and the intended `$1ffd4` store.
The evaluator does not invoke the service, register or invoke a callback, map
host events, or write title-stage state. These remain raw control/data-flow
facts, not a recovered input ABI or menu action.

Read-only `--inspect` diagnostics now report the callback registration, its
Exec-vector boundary, byte-one original-table/queue operands, and byte-two
gate/service operands. The report is intentionally static provenance; it does
not display a decoded event, evaluate either controlled trace, or imply that
the original callback registration or input path ran.

The third helper's concrete next boundary is also recovered. At `$1fe7a`, the
raw title image masks `D0` to `$0000ffff`, performs original unsigned divides
by `$0064` and `$000a` (with the two intervening original subroutine calls),
adds `$0030`, clears byte `$1fe54`, and then executes an absolute `JMP
$1fbe6`. Both `$1fe7a` and `$1fbe6` are range-checked against the same
title-stage load interval (`$13000` plus the profile's original length). This
proves the direct recovered route after the transition remains in title-stage
code; it is explicitly not evidence of a handoff to the separately loaded
main/game stage. Project Eon preserves the arithmetic, write, and destination
without inventing menu or gameplay labels.

The two word-displacement `BSR` instructions in that helper are separately
resolved from their extension-word bases: `$1fe84` (`+$0022`) and `$1fe92`
(`+$0014`) both target `$1fea8`, with return PCs `$1fe88` and `$1fe96`.
The v3 bridge grammar pins this distinction, so an external capture cannot
mistake a caller return location for the original local callee. It is still a
control-flow fact only; neither call is asserted to occur or return without a
genuine trace.

The selector destination is now also bounded. `$1fbe6` tests signed byte
`$1f98c`: zero enters `$1fc22` (which immediately tests `$1f98e`), while the
positive `BPL.W` target is `$1fc9c`: this is the sibling route's `tst.b
$1f98e`, immediately after the prior route's `RTS`; it does not target the
middle of an instruction or return directly. Its clear/set variants begin at
`$1fca6`/`$1fd7a`, use the same `$1f99c` pattern-table and `$1f974` destination
pointer cells, combine bytes across eight rows by four planes, and increment
`$1f974`. Clear uses literal `$28`/`$1f40` strides; set loads stride cells
`$1f994`/`$1f998`. This is a concrete original byte-combine/pointer effect,
not an inferred resource or UI label.
For zero, a clear `$1f98e` enters `$1fc2c`; a set value enters `$1fd0a`. Both
preserve registers and traverse eight rows by four planes using pattern-table
pointer cell `$1f99c` and destination-pointer cell `$1f974`. The clear route
combines source cells `$1f970` and `$1f96c`, uses literal row/plane advances
`$28`/`$1f40`, then advances `$1f974` by the long in `$1f9a0`. The set route
uses long stride cells `$1f994`/`$1f998`, source cell `$1f96c`, and increments
`$1f974`. These are raw byte-combine and pointer effects, not names for
resources or UI. The negative fall-through calls `$1fc24`. On that negative
path, the original preserves D0/D5, suppresses a service call if D0 is `$0020`,
otherwise supplies literal D0/D1 values `$0013`/`$000c` to `$3fbf8`, then runs
a `$00004e20` decrement loop before restoring registers and returning. These
are opcode-validated control-flow, call, and timing facts from the raw title
stage; Project Eon does not name the state bytes, service, or output, and does
not synthesize their unknown data.

The non-suppressed call's ABI boundary is now bounded too: it pushes A0 and A1
after saving D0/D5, calls `$3fbf8`, then pops A1/A0 before the delay. Both the
suppressed and service paths converge on `move.l (a7)+,D5`, `move.l
(a7)+,D0`, `RTS` at `$1fc20`. Thus this routine returns its incoming D0/D5;
the service's internal output and purpose remain intentionally unknown.

### Deuteros Amiga title-stage exits

The title entry's locally verified low-byte-five arm is also retained without
assigning it a gameplay meaning. At `$40438` (ADF `+$9b438`) its 16 bytes hash
to `c4f5b0fa571dc0c932e9bb3df9f48e4c4336840d49ae2368e69fffa8c05c87a7`.
Only when an explicitly supplied incoming word has low byte `$05` does it
record the low-byte store to `$3717e`, literal word `$0101` at `$38092`, and
branch to the pre-existing hard Exec boundary `$40450`. No controller value,
write to original media, or Exec vector call is performed.

The raw title-stage has three independently validated tails that leave its
loaded interval. They begin at `$37f56`, `$38038`, and `$38068`. Their prior
render/control work is intentionally not named, but each tail copies the
incoming controller pointer from `$206a0` to bootstrap cell `$12ff8`, writes
respectively long profile values `2`, `4`, or `3` to `$12ffc`, and performs
`JMP $12800`.

The first of those exits has one additional bounded local transfer before its
already verified profile-two tail. At `$37f56` (ADF `+$92f56`), the exact
40-byte prefix hashes to
`51b8d6875ea6d0c35557c358d4fe22e4cac6cff79ead9df604d213cab1adfe1c`.
Conditional on its two original calls to `$3880a` and `$204fa` returning, it
copies exactly `$9392` bytes from loaded title-stage address `$13006` to the
original runtime address `$66000`, then reaches the still-unexecuted BSR at
`$37f7a`. The source maps directly to ADF `+$6e006`, remains inside the same
title-stage interval through `$1c397`, and hashes to
`2951d0ae6dd01f84c1fb9b6cbb766c15378af1abb9a91fa5ded748d70b3e90eb`.
Project Eon exposes only a read-only copy trace and those genuine source bytes:
it does not call either helper or the BSR, write `$66000`, infer a title choice,
or create replacement data.

The BSR's return continuation is a separate explicit ABI boundary. Only when a
caller supplies that `$37f7a` returned does Project Eon inspect its following
28-byte tail at `$37f7e` (ADF `+$92f7e`), SHA-256
`bacc75771f84068878d031ad87b0708c08911e85b605436c29d8d4c1faa2884c`.
The original instructions name controller source `$206a0`, bootstrap cell
`$12ff8`, profile-two cell `$12ffc`, and `JMP $12800`. The evaluator exposes
only those instruction destinations and the literal profile: it does not read
or materialize the controller longword and does not execute the jump.

The BSR target itself is retained as static provenance, without treating its
calls as executable behavior. `$37f9a..$38031` (ADF `+$92f9a`, 152 bytes,
SHA-256 `b076611efd33354e311dc9f64b57454e31cddd69c0749a05034f0d828a5b36c1`)
loads literal D1/D7/D0 values `$12800`/`$2c00`/`$0600`, then calls `$208c0`.
It writes word `$000a` and long `$1ef48` at offsets `$1c`/`$28` from `$1eefa`,
and makes raw Exec-vector calls with A1 equal to `$1eefa`, `$1eefa`, `$1eed8`,
`$2063e`, and `$20676` at vectors `-$1ce`, `-$1c2`, `-$168`, `-$1c2`, and
`-$168`. A longword comparison `$20698` versus `$2069c` selects the unequal
path at `$38014`; all paths return at `$38030`. The parser records only these
instruction operands and branch addresses. It neither calls `$208c0` or Exec,
reads the compared state, assigns a purpose to any work area, nor asserts that
the BSR returns.

The analogous second exit is independently bounded after its four preceding
calls. Only when a caller explicitly reports returns from `$3880a`, `$204fa`,
`$37efa`, and `$37f9a` does Project Eon inspect the following 28 bytes at
`$38046` (ADF `+$93046`), SHA-256
`cf80103d5a580dc1e59f1090169c769a66a5d34c1112f14456e00713f1d078da`.
Those instructions name controller source `$206a0`, bootstrap cell `$12ff8`,
literal profile-four value at `$12ffc`, and `JMP $12800`. The evaluator does
not invoke any predecessor, read controller data, manufacture a return, or
execute the jump.

The third exit has its own 48-byte predecessor chain at `$38062..$38091`
(ADF `+$93062`, SHA-256
`e3b5d4b2448f33178f748a9a235c270c45e2c83e2b0ba9f4bd8e41ab3ee2fb80`). Its
four calls are `$3880a`, `$204fa`, `$37efa`, and `$37f9a`; their return is
again an explicit caller-provided boundary. Only then does the evaluator
inspect the distinct 28-byte tail `$38076..$38091` (ADF `+$93076`, SHA-256
`25c2f6bf241a863d0b16359553dfae9a82953dfbc25035db71634a0b369df217`). It
records the same raw controller source/destination operands, literal profile
three at `$12ffc`, and `JMP $12800`, without executing calls, reading the
controller longword, or taking the jump.

`$12800` resets the original stack/Exec state and jumps to bootstrap dispatcher
`$12a4e`. The original six-entry table at `$12a36` resolves profiles 3 and 4
directly to `$12b1c`; profile 2 selects `$12b44`, whose sole instruction is
`BRA.B $12b1c`. `$12b1c` is the already verified profile-zero loader: it
returns destination `$20000`, length `$4200`, and track `$4`, whose raw stage
entry is `$21734`. Therefore these three original title exits demonstrably
re-enter the raw main-stage load path. They are not yet interpreted as named
choices, game modes, or completed gameplay transitions. Project Eon records
only this opcode-validated profile and load-chain evidence; it performs no
media extraction, generated state, or guessed post-handoff simulation.

The full six-entry bootstrap table is also preserved: `$12b1c`, `$12b30`,
`$12b44`, `$12b1c`, `$12b1c`, `$12b46`. The recovered title exits do not
select entry five. Its first instruction is nevertheless independently
verified as `BSR.W $12932` at `$12b46`; no return value or continuation is
assumed, so this remains a hard bootstrap boundary rather than a fabricated
title or loading path.

The direct callee has one further independent, straight-line boundary before
its own unknown library call. `$12932` loads the controller pointer from
`$12822`, writes long `$00000001` at `+$24`, word `$0009` at `+$1c`, and byte
zero at `+$1e`, then loads `A6` from `$0004` and calls vector `-$1c8(A6)`.
Project Eon validates these raw writes and stops at that call; it neither names
the fields nor assumes the vector returns.

### Deuteros Amiga re-entered main stage

After any of those title exits, the original raw track is loaded again at
`$20000` and enters `$21734`. The first instructions save incoming A1 and D0
verbatim to longword `$20976` and word `$21704`, install stack `$22296`, then
request the literal memory ceiling `$7fff0`, then call raw addresses `$20068`
and `$2013a`.

The profile-two runtime now owns that first entry prefix. It accepts only a
typed D0 observation at exact entry `$21734`; A1 is not caller-supplied again,
but comes from the controller longword already admitted at `$206a0`, copied
through `$12ff8`, and retained by the same title session. Before committing,
the coordinator reconstructs all `$4200` resident bytes at `$20000` and
requires their SHA-256 to remain
`a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6`.
It then atomically writes A1 to `$20976` and D0.W to `$21704`; stack `$22296`
is retained as a register result rather than a memory write. The first Exec
call at `$2174a`, vector `-$96`, return `$2174e`, now requires an exact typed
return. Its D0 result is retained without meaning. The following original
instruction replaces it with literal `$0007fff0`, reloads the Exec base from
`$0004`, and calls `$21758`, vector `-$9c`, return `$2175c`. That second Exec
return and the following `$20068` and `$2013a` local returns are exact, ordered
typed boundaries. The `$2013a` observation supplies the longword read from
`$20128`; Eon clears `$2012c`, copies that value to `$20510` and `$20c20`, and
continues through exact typed returns from `$2177c->$22a5a` and two consecutive
`$22bea` hardware-service calls. The caller then writes `$7fff` to custom
registers `$dff040/$dff042`, `$c000` to `$dff09a`, and `$87ff` to `$dff096`.
Its two nested pointer chains rooted at `$12e00/$12f00` remain typed runtime
observations; their final pointers are copied to `$2197a/$2197e`. Execution
then follows the caller-connected `$217d8->$20994` edge. The exact 14-byte
prefix at ADF offset `0x6194` has SHA-256
`a5c916b3959fe074f18e12a12d0488a38b2c8b638079fb05d1ad3a0739848001`:
it clears A1, reloads the Exec base from longword `$0004`, and reaches the
external call at `$2099e`, vector `-$126`, whose return address is `$209a2`.
Eon exposes that call as a typed boundary and does not infer its return or
effects. Its exact typed return is now caller-connected to the next 44 bytes
(SHA-256 `2dd0b05e3fef1b0fdfb3ab2b9b1324e371f1335247b9424e212b8f50e5c2c5e7`).
Those instructions write longword `$20982` at `$2095e`, bytes `$7f`, `$04`,
and `$01` at `$2095d`, `$2095c`, and `$20963`, then preserve the observed D0
verbatim as a longword at `$20964`. The writes commit as one big-endian batch.
Execution stops at the next external call `$209ca`, vector `-$162`, return
`$209ce`; its result and effects remain unknown. No memory batch is emitted
for the register-only entry prefix. A wrong entry,
missing/replaced resident stage, replay, or revoked owner produces no partial
write. All observed results and service effects remain deliberately uninterpreted.

The main-stage parser opcode-validates that straight-line path and the first
recurring loop at `$217f6`. The loop calls `$22a5a`, clears words `$21720` and
`$2171e`, sets `$210f2` to one, calls `$21276` then `$21380`, and probes bit
10 at `$dff016` plus bit 6 at `$bfe001`. These are deliberately raw addresses, values, and bit tests:
their gameplay/UI semantics are not claimed. The parser reads only the
already supplied ADF in memory, rejects mismatching bytes, and neither
unpacks nor writes game data.

The next input-originated branch is also verified at `$21982`: it reads word
`$21704`, compares it unsigned with two, and writes one back to `$21704` when
the value is below two. Both the less-than and equal-to-two paths enter
`$218cc`; values greater than two instead branch to `$2181c`, the scheduler
call already present in the loop. This is retained solely as an opcode-level
clamp and branch map. `$21704`, `$218cc`, and `$2181c` are not assigned guessed
mode, screen, or gameplay meanings.

The greater-than-two route is now bounded further without executing guessed
main-game logic. It branches to `$2181c`, whose first instruction calls the
original scheduler at `$21380`. That routine starts at state base `$210f8`,
loads its channel count from `$21248`, and advances 24 bytes per slot. It
tests the active program longword at `+16` and a selector word at `+6`; the
raw comparisons accept selectors `$03`, `$05`, `$06`, and `$14`. The original
uses the word at `+8` in these paths before resuming its opcode dispatcher.
After the channel walk, it probes bit 5 at `$dff01f` and conditionally calls
`$21698`. These are opcode-validated scheduling and timing/service facts only:
they do not name the channel fields, emulate a hardware interrupt, or invent
any gameplay state. The bounded native VM already uses this original 24-byte
layout for its verified opening commands; parsing this continuation keeps the
post-input route anchored to the same supplied ADF bytes.

The full immediate control-flow consequences are now opcode-validated as well.
Both paths at or below two reach `$218cc`, whose shared tail reads `$21704`,
increments the register value, but does not write that increment back. Result
two branches to `$21a4c`; it writes longword one to `$219f4`, then the common
return tail copies incoming controller `$20976` to `$12ff8` and `$219f4` to
`$12ffc` before `RTS`. Thus an initial value below two clamps to one, produces
result two, and requests bootstrap profile one without asserting a menu or
gameplay interpretation. An initial value equal to two instead produces result
three and branches to `$219f8`. That path writes five to the same `$219f4`
cell, calls `$20b42`, and compares D0 with literal `$4452f018`: equality jumps
to the shared `$21a56` return tail, while non-equality continues its original
polling loop. Values above two bypass `$218cc` entirely and resume `$2181c`
(the scheduler path), leaving this post-service state unchanged. Project Eon
reports these original branch facts only; it does not synthesize a main screen,
interpret the sentinel, or mutate supplied media.

The re-entered stage's raw resource loader at `$21932` is independently
validated. It shifts its incoming D0 index by two, reads the selected longword
from `$21708`, and uses that as a physical ADF offset. It clears `$2ad24`,
transfers exactly four bytes from that offset there, restores the original
offset, and uses the resulting big-endian longword as a second transfer length
to `$32a24` from that same offset. The shared transfer routine chunks requests
at `$1600`; after the transfer it tests bit 10 at `$dff016` and retries from
`$2196e` while clear. This is a verified original resource-to-memory effect,
not a claim about the resource's format, any destination cell's role, or a
request to unpack/copy media. Project Eon exposes the fixed data-flow facts,
and has a read-only model for a successful nonzero pass: it retains the exact
source table index, source ADF offset, probe/payload destinations, length word,
and a bounded non-owning view of the original bytes (including that length
word). Its source ADF must outlive the transfer. It rejects a length outside
the physical ADF and returns no payload for the original zero retry condition.
It never writes, extracts, unpacks, or duplicates game data.

`inspect_deuteros_amiga_main_resource_catalog` reports the two
caller-proved, complete source spans with SHA-256 while retaining no source
bytes. It rejects index two or above, because `$21932` alone proves no maximum
selector and adjacent raw longwords are not a preservation-safe catalogue.

The first direct consumer of that transferred memory is now bounded too.
Routine `$2016a` saves A4, loads it with the exact transfer destination
`$32a24`, combines the word at `$20168` with the longword at `$2079e`, masks
the low word with `$3ffe`, reads one big-endian word at that A4-relative
offset, adds `$000e`, adds the result back to `$20168`, restores A4, and
returns. The main-stage command dispatcher reaches this routine from two
separate validated compare/call arms: command words `$000a` at `$2159c` and
`$0011` at `$2163a`. This proves a resource-to-control data path after the
loader without naming the resource, treating it as an extracted file, or
assigning gameplay semantics to the state cells.

The consumer's layout and index behavior are now directly observable too. It
is not a separate table after the loader's four-byte probe: the second transfer
begins at the same ADF offset, so `$32a24+0` remains the resource's big-endian
length longword. `$2016a` forms `(seed + low_word(counter)) & $3ffe`; this is
an even byte offset in the first 16 KiB of the exact raw transfer, from which
it reads one big-endian word. It adds `$000e` modulo 16 bits and adds that
result to the seed. For genuine bundle 0 with zero seed/counter, the observed
word at `$0000` is `$0002`, the intermediate result is `$0010`, and the
resulting seed is `$0010`; with that seed and counter `$00000004`, it reads
`$0a78` at `$0014` and produces `$0a96`. Bundle 1 has the same first observed
`$0002` word. Project Eon exposes this only in verification output and an
in-memory model that rejects malformed length, destination, or range facts.
It neither labels these values nor changes source media or save state.

The next consumer is the already opcode-validated channel interpreter at
`$214aa`, not a resource decoder or renderer. Its `$000a` arm calls `$2016a`,
ANDs the returned word with the following original stream word, stores the
masked result as state `+8`, writes selector `$0003` at state `+6`, and
returns to the scheduler. Its `$0011` arm calls the same routine, masks the
result to four bits, reduces it by the first following stream word when that
value is not smaller, multiplies by the second following stream word, and
adds the product to the current command-stream address. Thus the proven
effects are a timed control yield and a bounded original command-stream
choice. Neither arm uses the word-plus-14 value as a file/resource selector,
bitmap index, palette index, or a rendering address. The live opening now
uses the completed `$21932` transfer held in memory as its `$2016a` source;
it verifies the transfer's leading length and destination through the same
strict sampler before every word read. This replaces no game data, unpacks
nothing, and preserves the previous raw-ADF reader solely for independent
parser tests.

The first renderer use of the input path's `$0f` result is bounded at
`$21448`. The render pass walks the same 24-byte channel states, ignores
selector `$ff`, then compares selector `$fe` before its ordinary bitmap route.
For `$fe` it loads A4 from state offset `+$0c`—the exact `$32a24+$0b38`
pointer installed by `$21610` for the first accepted opening input—and calls
`$20580`; non-`$fe` live selectors call the indexed-bitmap compositor at
`$20c8c`. `$20580` is a real byte-stream interpreter, but it writes through
global original video pointers (`$20510`, `$20508`, `$2050c` and related
state) rather than the channel's X/Y fields. Project Eon records and tests
this exact A4/call boundary, then deliberately leaves the `$fe` pixel effect
unrendered until the setup and all stream control classes are fully recovered.
It does not reinterpret those bytes as an indexed sprite, create a synthetic
frame, or write/unpack media.

The renderer-only evidence now includes the local main-stage video link at
`$21768` / ADF `+0x6f68`: its 20 bytes hash to
`66c68dea1896b857f9cda825ef5511b34254ceed8db8a1b1481c3e3477514194` and
copy `$20128` to `$20510` and `$20c20`. It follows external initialization
calls, whose ABI is still not emulated; the subsequent local copy is recorded
only as raw provenance. The original `$2069c` position helper is also locked
as 48 bytes at ADF `+0x5e9c`, SHA-256
`b167cbda0c4e419b50e8dea16172b80a3db31e52385fe606efd146a54ce4d772`.
It bounds the `$20128`-relative `$20510` calculation used by the observed
`$20580` stream without materializing the unknown runtime value of `$20128`.
For the verified selectors one/zero, the exact original masks at `$20490` and
`$20488` are required. Altering the raw video-link bytes fails closed before
any host-frame pixel is written. The accepted `$20580` glyph writes update the
same persistent four-plane compositor surface as the other original Amiga
channels; a later channel pass retains those pixels. This is restricted to the
one hash-locked stream and does not infer the unresolved external init ABI.

The same first accepted input channel has a bounded post-renderer tail in the
real bundle: immediately after `$0f,$00000b38` are `$05,$0008,$0044,$00`.
The scheduler's selector-five arm at `$213be` resumes only if the low word of
`$22a20 - 1` equals state word `+8` (`$0008`) and state word `+10` (`$0044`)
is strictly below `$22a16`. It then dispatches `$00`: `$214ae` clears selector
`+6` only. On the following scheduler visit, selector zero reaches `$2142a`
and clears the program longword `+16`. Project Eon models those two original
in-memory effects across two scheduler calls; it neither manufactures an
audio clock nor substitutes a completion screen.

The first fully observed `$20580` stream is now executed as a strict in-memory
trace. The opening input path supplies `$32a24+$0b38`; its original bytes are
`$16,$0f,$30,$10,$01,$11,$00`, then the eleven bit-7-clear glyph bytes
`please wait`, followed by `$00`. `$20580` therefore calls `$2069c`, which
sets `$20510` to `$20128 + $1e0f`; `$10` and `$11` select `$20488 + 8` and
`$20488` for `$20508` and `$2050c`; each glyph calls `$206e6`. The latter
writes through original global font/video pointers. This first stream now has
a fully verified pixel path: the raw main stage initializes `$20538` to its
embedded `$201b0` 8-bytes-per-glyph font and `$2053c` to one byte of horizontal
advance. `$206e6` reads each glyph row, then writes each of the four Amiga
planes using `(~glyph & secondary-mask) | (glyph & primary-mask)`. The exact
selector tables are raw bytes at `$20488`; for the observed selectors 1/0,
the result is plane 0 for set glyph bits and no plane bits for clear glyph
bits. The verified `$1e0f` display offset is byte 15 of scanline 192, so the eleven
8x8 original glyphs fit through scanline 199. Project Eon applies precisely
those writes to its existing in-memory four-plane-equivalent frame; it does
not create a font, artwork, or source-media output. Any other command class,
global layout, or out-of-plane write is rejected as a preservation boundary.
At the caller-connected tick-82 title handoff, the resulting original-derived
320×200 RGBA compositor surface hashes to
`61eed88676355d0a136c943ffaa37396ba5220b7ea751b8cab6d0b125b3dd4c9`.
The native test obtains that value from the supplied hash-verified clean ADF;
it covers the final frame that SDL preserves when title-stage execution stops
at the unresolved Exec ABI boundary. It is a regression identity for the
recovered opening render route, not evidence of a reconstructed title screen
or menu.

The host scheduler treats that handoff as terminal as well: after the one
caller-connected handoff tick, it performs no later opening-VM catch-up calls.
The coordinator retires its Paula mixer before it publishes the title-stage
boundary, while SDL clears any already queued host PCM. SDL may
continue to present the hash-regressed final opening frame, but it never
advances an invented title-stage clock. In **Original**, that frame is sampled
with nearest-neighbour scaling. In **Modern**, only the opt-in, memory-only
renderer transforms (such as Scale2x, smoothing, scanlines, frame, output
resolution, and aspect ratio) may affect that same recovered frame; they
cannot alter the ADF bytes, its SHA-256 identity, the title-stage palette
provenance strip, or resume execution beyond the unrecovered boundary. The
palette strip represents raw RGB4 setup words, not a generated title image.

The compositor draws channels in ascending order into a persistent four-plane
display. X is measured in 16-pixel words and Y in scanlines. Bit 15 alone
selects `$20fb2` masked drawing where palette index 0 is transparent; an
unflagged selector overwrites the complete rectangle. Original code clips
vertically but trusts horizontal coordinates, so native code validates the
horizontal range instead of silently changing it.

The stateful paths are now also recovered from the shipped `$20c8c` code.
Bit 13 branches directly to `$21092`, which restores rows from the single
global scratch buffer at `$23024` at the channel's current Y coordinate.
Selectors with bits 15+14 (`$c000`) first clear both flags, take opaque
`$20d8e` decoding, then `$21034` copies each affected complete 320-pixel row
from all four planes into `$23024` and writes `$ffff` back to that channel's
selector. Thus the later bit-13 route restores exactly that sole buffer; it is
not per-sprite storage and is not inferred as a conventional background erase.
Project Eon follows this order with one persistent compositor buffer, rejects
a restore before a genuine save, and validates the `$ffff` state transition.

### Millennium DOS execution model

#### Title-to-game hand-off

The clean English DOS `TITLES.EXE` (7,022 bytes, SHA-256
`3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6`)
is a separate flat binary, entered at loaded address `$1b80`. Its code at
`$1c14` loads title resource index zero through its resource routine. The
direct near-call there targets `$1725`. Its first 13 original bytes end in
literal `JLE +$01` at `$172f` to `$1732`; the sequential byte is `RET` at
`$1731`. This is a caller/callee byte boundary only: the comparison condition,
callee return, and any resource effect remain unmodelled.
The `$1732` JLE target's first direct near-call is at `$173d` to `$1390`,
anchored by its first 16 raw bytes; no call or data effect is inferred.
The `$1390` callee is anchored through its first direct near-call at `$13bb`
to `$013c`; this is a static address edge only.
The `$013c` target is anchored for 16 bytes through its terminal `RET` at
`$014b`; no operation or return effect is inferred.
The English title-flow parser requires the complete `TITLES.EXE` and
`MILL.COM` leaf hashes before it exposes those anchors, so a same-size image
with matching local bytes cannot be substituted for either original program.
transition routine at `$1941` starts with `CX=$25` and `DX=$0170`, so the
verified title transition contains 37 steps with that original stride.

The title/resource chain is now byte-locked through that transition. Main
`$1c14..$1c1f` (file `+$1b14`, SHA-256
`056142489c8d70d88640c5dd0dea385fd4a3d561efe95cb57625773840ca1327`) sets
index zero, calls `$1725`, then `$1004` and `$1941`. `$1725..$1767` (67 bytes,
SHA-256 `37ec3f970f81219815bf524c6db54a12a0961d7e7452c3e2527fe422401339d9`)
reaches the 333-byte codec-2 reader `$1390..$14dc` (SHA-256
`c4e0a56ba09831d80112d3f630e070886a66b70a62251ae90e89f98281a94b52`) for
P00, then private `INT $91` wrapper `$0122`. The 39-byte `$1941..$1967`
transition (SHA-256
`f22c9595e6b1c590877b354721e6102d8107c5d6c7336b2d3491cfcaf3f8a627`)
advances original runtime offsets by 368 and requests P01 through P25. Those
37 records occupy `TITLE.LIB` `$2941..$4813` (7,890 bytes, SHA-256
`f0ecbfd374b1c6122b407b29a6fe4a872a45a0a21e9ef6584e74829e06b5514d`), but
write only to runtime buffers. Project Eon decodes them as a read-only patch
sequence: P01 through P25 are codec-2 16x23 (368-index) records, matching the
`$0170` stride. P01's decoded index hash is
`330db310a838487f4afea0011c1ba5f381e4ed7ad97d95e4745e7be2d2d8aaa1` and
P25's is `d7e44c796aed167010cdef9ab7ccef38b3b260854b51b2fba818972f30dd35dd`.
The preservation parser additionally records each unmodified source span,
requires P01 through P25 to be contiguous, and exposes the bank identity in
inspection diagnostics. That bank SHA-256 is computed directly over the
immutable original `TITLE.LIB` range, without a second source-byte copy. It
also requires the full English `TITLE.LIB` leaf
hash before admitting that transition, so a valid LIB directory with matching
P01–P25 records cannot silently stand in for the original resource bank. For
the supplied release, P01 is `+$2941`, 213 bytes,
SHA-256 `ed4cf68627d93c10545d741facfa43701774e0bb8fa28c14292877dc81b556b2`;
P25 is `+$473e`, 213 bytes, SHA-256
`b523a32da572fe7e5e93ad5f8b51675c04d85053934e051d03223a9fa1e19ba1`.
This improves provenance of genuine transition resources without treating
their decoded 16×23 regions as independently displayable frames.
The static mode-2 path bounds each post-stream XLAT range (P02: `00 cc 00`),
but does not establish a selected display mode. Segment setup, private-driver
effects, composition, color handling, cadence and host-visible transfer remain
unrecovered; Project Eon does not draw an inferred transition or claim extra
title frames.

The main title loop's byte-locked call at `$1c28` targets the unique helper at
`$0d0a`, which polls DOS `INT 21h`, `AH=$06`, `DL=$ff`; after that call, the
local path branches out of the loop only after the returned `AL` is nonzero. These caller/callee
addresses are exposed as hash-bound parser provenance for a future reviewed
recorder configuration only; they do not establish a character, key name, or
return ABI. Cleanup writes zero to the process status byte at `$1a0e`, and the
common exit stub at `$1a12` executes `INT 21h/AH=$4c`. Thus the verified title
program itself does not execute the game binary: it exits with status zero.

The complete caller range `$1c28..$1c69` (file `+$1b28`, 66 bytes, SHA-256
`d08916b10f92fe78e643a3335680c341f2347c361cded2419954422c0c37e6dd`) makes
the input boundary precise. It only performs `AND AL,AL` and sends every
nonzero result to `$1c54`; it neither stores nor compares a scan code, so no
key name or control binding is proven. Eon's host bridge therefore enables
SDL text input only while the verified title boundary is active and sends an
availability observation only for non-empty UTF-8 text input—never for a raw
physical key event. It does not decode, preserve, or assign meaning to that
text, and stops text input immediately after the one original title hand-off.
Leaving an interactive launcher title visit also stops host text input and
discards that one-shot session. A later visit constructs a new boundary from
the same verified title profile; no observed hand-off, host text state, or
original media state is carried across visits.

#### Required dynamic trace contract for the next playable DOS increment

No supplied archive contains this trace. A future recorder may extend the
English DOS adapter only after retaining the existing outer-release and event
file hashes and adding ordered, exact observations of all of the following:

1. The `$0d0a` `INT 21h/AH=$06,DL=$ff` return at `$1c28`, including the
   returned `AL` and flags, followed by the actual `$1c54` branch decision.
   The input character itself must not be promoted to a host control name.
2. Every return from the five `$0122` private `INT $91` calls reached by
   `$1968/$1931`, with full AX, flags, ES:BX input record identity and the
   visible destination/write ranges produced by each call.
3. The return/flags from `$12c0`, `$0916`, and DOS `INT 21h/AH=$4c`, then
   the parent `MILL.COM` child-status observation that determines whether its
   next EXEC request is `2200ad.exe`.
4. For `2200AD.EXE`, the exact first `$0124` private-wrapper return (AX and
   flags), the resulting `$d128/$da05` bytes, and each selected driver/file
   operation through the first presentation write. Any INT 91 return must
   identify the already hash-bound EGA640 or MCGA driver and its complete
   record read/write ranges.

The capture must preserve loaded image identity, program-counter addresses,
ordered ticks, register inputs/outputs, flags, and memory ranges as hashes or
bounded byte observations. It must not replace unknown results with zero,
derive a frame from P01–P25, or record host-generated pixels as original
writes. Only such a trace can authorize a new bounded runtime evaluator; until
then, Project Eon deliberately stops at the title availability boundary.
The static exit chain reaches `$1968`
and embedded bytes at `$1884` spelling `    LOADING    2`. `$1968` loads AX=5
and calls `$1931`; that local loop runs five times, loading AX=`$0013`, calling
the private `INT $91` wrapper `$0122`, then helper `$1917`. This establishes a
caller-connected private-driver boundary only: ABI, helper effect, destination,
composition and BIOS-visible output remain unknown. No post-key loading frame,
transition, resource effect, process exit, launcher return, or game startup is
executed or drawn by Project Eon.

`MillenniumDosTitleExitClosure` now preserves the narrower local tail as its
own hash-locked profile. `$1c54..$1c69` (file `+$1b54`, 22 bytes, SHA-256
`d0981a03e0f8fdc9449e080668b7808952a48d0d3de4beb3a528ba5fc0f05951`) first
calls `$1968`, then `$12c0`. Only if both native calls return do the following
original instructions clear `$1a0e`, restore SP from `$1aa0`, and call `$0916`;
the direct `JMP` at `$1c67` targets `$1a0f`. That 11-byte tail (file
`+$190f`, SHA-256
`b8160617c570a0dafcfea4e57187b7dd9182ced8da1153f6f77c63d5e7fe6a88`) calls
`$112e` before the existing `INT 21h/AH=$4c` bytes at `$1a12`. The local-call
returns, the process termination effect, and whether the program ever returns
to `MILL.COM` remain explicit boundaries; the runtime does not perform any of
them.

`MillenniumDosTitleToGameSession` now makes that caller-connected closure a
typed manual-recomp path while preserving every external result. Construction
requires complete original `TITLES.EXE` SHA-256
`3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6`
and `MILL.COM` SHA-256
`4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e`.
It requires exact returns `$1c54->$1c57` and `$1c57->$1c5a`, records only the
instruction-defined `$1a0e := 0` write, explicitly observes the stack word at
`$1c60` from `$1aa0`, then requires `$1c64->$1c67` and `$1a0f->$1a12`.
The DOS termination observation must be exactly `$1a18`, `AX=$4c00`.

The parent half is equally strict. The original title EXEC at `$0337` must
return with carry clear; the child-status request at `$0348` must be explicitly
observed with carry clear and `AL=0`. Only that instruction-selected path
reaches the immutable game request `$024c -> $031c`, `DX=$069a`, whose bytes
name `2200ad.exe`. Carry set or nonzero status is rejected rather than routed
to the game. This terminal boundary proves an EXEC request, not DOS success,
child loading, driver-vector survival, the `2200AD.EXE` entry, GX loading, or
a navigable frame. The deeper `$1968` and `$12c0` callees remain explicit
call-return observations because their private-driver effects require the
already documented ABI evidence; no result is synthesized.

The release runtime owns this continuation only after its existing
`MillenniumDosTitleSession` has actually reported `handed_off`. It then
reopens the verified release read-only and admits the exact MILL.COM and
TITLES.EXE code-image descriptors. Monotonically sequenced typed observations
cover the exact returns and runtime values; the copy-only checkpoint retains
only the current boundary, generated byte effect, restored stack word, and
child status. Reset and host revocation hide and destroy it. Its final
`$024c -> $031c`, DX `$069a` EXEC boundary does not admit or infer the child.
The current English startup still stops at the separately documented sound
driver boundary, so this route remains fail-closed until that predecessor
return is recovered; Spanish media cannot borrow the English continuation.
When the second exact call return produces the instruction-defined
`$1a0e := 0` byte effect, the coordinator applies it to `NativeRuntimeMemory`
as one generation-qualified, fully admitted batch. Session and memory copies
commit together, so a wrong site, stale/duplicate sequence, duplicate batch,
or memory rejection changes neither side. Reset/revocation discard the batch
with the owning generation. The private admission helper deliberately requires
an already handed-off owned title session; it is the integration point for a
future proven sound-driver-to-title chain, not a public ABI bypass.

The local helper `$1917` is now independently byte-locked. Each of its five
calls starts a fixed 15-iteration selector loop. Selector `$18f9` adds the
unknown word at `$1181` to accumulator `$18f7`, masks the result with `$03ff`,
loads one original byte at that resulting offset, and reduces values at or
above `$24` by `$18`. Its caller then adds one and calls resource loader
`$1712`. This establishes only a bounded potential resource index path; the
state word, accumulator, loaded byte, destination buffers and all resulting
rendering remain unknown.

`$1712` is a local index-to-buffer setup routine, not a recovered renderer. It
zero-extends the selected byte, multiplies it by `$0170`, and stores the word
at `$1341`. It then consumes one 4-byte entry from the fixed 15-entry original
table `$1768..$17a3` (SHA-256
`9c40c1fa63248237383703aa0aaf6659630e8d4fb48bc6ddd1c633ed4d26846f`), copies
its two words to local cells, and calls private INT 91 wrapper `$0122` with
AX=`$0006`. This proves original offset setup and table identity only—not a
resource name, visual coordinates, blit ABI, or visible pixel result.

The caller fixes only part of the AX=`$0006` ES:BX record. `$1712` sets
ES=CS and BX=`$1349`; `$174a` writes the two words of the current `$1768`
table entry to `$134f` (record `+6`) and `$1351` (record `+8`). The related
local segment cell is `$134b` (record `+2`), while codec-2 setup stores decoded
height and width at `$1357`/`$1359` (record `+$0e`/`+$10`). The remaining
record words, pointer provenance, interpretation of the table words, and all
driver writes are deliberately unrecovered.

The complete direct-reference audit separates this record from nearby buffer
state. `$135e` loads one of the original far pointers at `$010c`/`$0110` and
writes its offset/segment to `$1341/$1343`; `$1712` later replaces only the
offset word. `$135e` also writes CS to `$134b`. In contrast, no direct local
writer of `$1349` occurs in the hash-identified `TITLES.EXE`.

Both independently hash-identified AX=`$0006` drivers make that unknown
word an explicit ABI boundary: EGA640 executes `LDS DI, ES:[BX]` at `$08d9`;
MCGA executes `LDS SI, ES:[BX]` at `$072e`. Thus record `+$00` is a far-source
pointer's offset word and record `+$02` is its segment word. This proves the
driver's direct input field, not who owns the offset, the pointed-to byte
format, or any drawing result. Project Eon records neither a reconstructed
pointer nor a host-side transfer for it.

The next original instructions give a stricter but still format-neutral
boundary on that pointer. EGA640 reads words at source `+$04` (`$08dc`),
`+$02` (`$08df`), and `+$00` (`$08eb`). MCGA reads the word at source `+$02`
(`$0731`) and executes `LDS SI,[SI+$04]` at `$0736`, making source `+$04` a
second far-pointer operand on that path. These verified operand accesses do
not establish that the two driver paths share a header format, identify any
pointer owner, or authorize a host-side copy, decode, or draw.

The supplied driver images add one ABI-independent local fact for that exact
function number. In both hash-identified `EGA640.BIN` (dispatch target `$0d37`)
and `MCGA.BIN` (target `$0905`), AX=`$0013` loads DX=`$03da`, repeatedly reads
the VGA status port until bit `$08` is clear, then repeatedly reads until bit
`$08` is set, and returns. Project Eon records this read-only vertical-retrace
wait but does not perform host port I/O or infer that it produces a frame.

The same supplied driver dispatch tables connect `$1712`'s AX=`$0006` request
to EGA640 `$08a6` and MCGA `$0705`. Both begin by clamping the ES:BX record's
word at `+$10` against `320 - word[+$08]`, then return if the resulting height
is non-positive. The ES:BX record, source/destination pointers, branch
outcomes and all writes remain unmodelled, so this is a verified entry-side
clipping boundary, not a host blit.

The accompanying clean `MILL.COM` (1,445 bytes, SHA-256
`4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e`)
has a caller-side sequence at loaded `$023d`: it loads `DX=$068f` (the
NUL-terminated `TITLES.EXE` string at file `$58f`) and near-calls `$031c`
from `$0240`; after the explicit `AND AL,AL` / conditional branch bytes, it
loads `DX=$069a` (the adjacent `2200ad.exe` string at file `$59a`) and makes
the same direct near call from `$024c`. This is the exact DOS title-to-game
hand-off used by the native parser. It establishes only literal dataflow and
control edges: Project Eon does not assign a DOS/EXEC meaning to `$031c`, the
post-call `AL` tests, or either callee return.

The directly called local bytes at `$031c..$034d` are separately anchored by
the parser. Their first local branch is the literal `JC +$05` at `$0345`:
the taken target begins at `$034c`, while the sequential bytes are
`B4 4D CD 21 C3` and end in `RET` at `$034b`. The branch target's next 14
bytes are `BA 70 03 89 D2 B4 09 CD 21 B8 0A 4C CD 21`; the first following
byte, at `$035a`, is the static text-data boundary. This is a static byte/control
fact only; in particular, Project Eon does not interpret the interrupt bytes,
the carry condition, or the return as DOS behavior.

Immediately before the title-string setup, `$0210` starts with a direct near
call to `$0511`, followed by `AND AX,AX` and literal `JE +$03` at `$0215` to
`$021a`; the unbranched bytes `$0217..$0219` are `05 02 00`. The rejoined raw
sequence reaches another direct near-call at `$0231` to `$02cf`, then the
bytes through `$023c` precede `DX=$068f` at `$023d`. These are preserved as
caller-side static dataflow and control edges only. Neither call, the `AX`
condition, nor the interrupt bytes in this range is given an execution or DOS
meaning.

The `$0231` direct call's local target is `$02cf`. Its hash-anchored first 19
bytes are `B8 00 3D CD 21 73 0C 0E 1F 8B 16 D5 05 B4 09 CD 21 EB 87`.
They contain the first local split, literal `JNC +$0c` at `$02d4` to `$02e2`.
The sequential bytes end in literal `JMP -$79` at `$02e0` to `$0269`.
This records only direct byte control edges: no carry condition, interrupt,
callee result, or higher-level behavior is inferred.

The `$02d4` JNC target at `$02e2` is anchored for its first 14 bytes:
`50 93 33 D2 33 C9 B8 02 42 CD 21 72 E7 50`. Its next direct control edge is
literal `JC -$19` at `$02ed` to `$02d6`. This remains raw static control/data
evidence only; Project Eon infers no carry, interrupt, result, or DOS effect.

The `$02ed` JC target at `$02d6` has its own 12-byte anchor:
`0E 1F 8B 16 D5 05 B4 09 CD 21 EB 87`. Its sequential control byte is the
same literal `JMP -$79` at `$02e0` to `$0269`; this is an observed byte-level
join, not evidence about either condition, interrupt, callee result, or DOS.

At the joined `$0269` path, the parser anchors the opening 16 bytes and the
later `B8 08 25 CD 21 58 22 C0 74 14` branch-tail at `$02aa`. The first next
direct control edge in that tail is literal `JE +$14` at `$02b2` to `$02c8`.
This remains a raw static address/byte fact only, with no interpretation of
the intervening interrupt bytes, condition, result, or DOS behavior.

The `$02b2` JE target has the exact seven-byte prefix `B4 4C CD 21 32 C0 CF`
at `$02c8..$02ce`. `$02ce` is the first terminal control-transfer opcode
boundary; the parser stops there and does not infer an interrupt or transfer
effect from any of these bytes.

MILL.COM's first private `INT 91h` boundary now has a proven source ABI without
inventing a DOS segment. Before its `$0204` call to `$02cf`, selector `$01`
keeps `DX=$0617` (`ega640.bin`) and selector `$02` selects `DX=$03ae`
(`mcga.bin`). `$02cf` opens that original file, seeks to the end, rounds its
length to paragraphs, allocates a DOS segment, rewinds, and reads the original
file length to `DS:$0000`, then closes it. There is no embedded blob, copy
loop, or decompression stage in this loader. Its result segment remains
unknown: the only byte-proven relation is `MOV DS,AX` after the allocation and
before the read.

After that loader returns, `$0207` clears `DX`, `$0209` loads `AX=$2591`, and
`$020c` executes `INT 21h`. Thus the raw vector request has interrupt number `$21`
and the exact source ABI `DS:$0000`, whose code bytes originate in the selected
original external file. Project Eon does not assign a numeric DOS segment,
assume any DOS call succeeds, or infer handler execution/return behavior.

The loaded `ega640.bin` (4,632 bytes, SHA-256
`ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a`) and
`mcga.bin` (4,366 bytes, SHA-256
`bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228`) both
start an `INT 91h` dispatcher that doubles `AX` into their local word table.
The English driver parser verifies those complete leaf hashes before accepting
any local dispatch or instruction anchor; a same-size altered or look-alike
driver is rejected even if its inspected prefix still matches.

The original `MILL.COM` driver choice is now separately byte-locked. Its
45-byte command-tail scan at loaded `$019d..$01c9` (file `+$009d`, SHA-256
`157c83c6cdef55dfb7531bceee1759884f68237f445437553d36c12f167d6eba`) reads
PSP `$0080`: `e`/`E` sets AL `$01`, `m`/`M` sets AL `$02`, and an unrecognised
token loops. An empty tail calls `$05a1`; its 66-byte detector
`$0593..$05d4` (file `+$0493`, SHA-256
`9228c64e003a093dcdebda600fe969e4112079e454834133559c0b52f4cf351c`) makes
hardware/physical-memory observations and returns `$00`, `$01`, or `$02`.
The subsequent 48-byte map `$01de..$020d` (file `+$00de`, SHA-256
`57d768c01d59a98d7a5cc452a871be2fa2c7e20c272e88da82e644635ac57be3`) maps
AL `$01` to `ega640.bin` at `$0617` and every other value to `mcga.bin` at
`$05f9`, before the existing load-to-`DS:$0000` / vector-install boundary.
This proves original input-to-request control flow, not a host policy: Project
Eon does not read a host command tail as original hardware detection, select a
driver, or assume the loader/DOS/vector calls succeed.

The same hash-identified `MILL.COM` contains a distinct sound-effect choice
routine at loaded `$0511..$0574` (file `+$0411`, 100 bytes, SHA-256
`f9e63fc4c7c590fc57abef4a0154a2399f714951c787f98d2f7d64eee86a7434`). Its
literal prompt admits only input values `$00`, `$01`, and `$02`; after a
nonzero value it adds `$02` before indexing the original word table at `$066e`.
Thus the source table slots are `$00` `sibm.drv`, `$03` `ssbl.drv`, and `$04`
`scvx.drv`. The six-name byte table `$062a..$065f` (54 bytes, SHA-256
`a5a3260fdf7a7018df0f34b0e9ba6f74a03e157f6d97cfb8f2f70407d8791185`) and
the seven-word table `$066e..$067b` (14 bytes, SHA-256
`c49071bf0db7a712437ca74d2e9effe9222665f2ab154db1f5d748f540e10ef8`) are
both parser-validated. `srol.drv` remains the literal table slot `$02`, but
is absent from this supplied archive; it is recorded as missing rather than
being aliased to another driver.

Only the supplied `SSBL.DRV` (9,194 bytes, SHA-256
`be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72`) and
`SCVX.DRV` (4,053 bytes, SHA-256
`99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4`) are
admitted as external sound-driver leaves, by full content identity only. This
admission does not parse or execute their private ABI, perform sound playback,
invent hardware routing, or admit an `SROL.DRV`/matching-name substitute.

Project Eon has a corresponding narrow, test-covered sound-selection session.
It accepts only character bytes `0`, `1`, and `2`, exactly once, and exposes
only the proven original filename/table-slot pairs `sibm.drv`/`0`,
`ssbl.drv`/`3`, and `scvx.drv`/`4`. The runtime carries a tiny immutable
descriptor (original filename, byte count, SHA-256, and admitted kind) for the
two independently hash-identified supplied leaves. It discards their bytes
immediately; no supplied driver is loaded, executed, emulated, cached, or
written. The IBM-speaker table entry has no admitted external leaf and remains
an explicit startup boundary. This is deliberately not an audio-device
selector: no host hardware is inspected, and every selection stops at the
still-unobserved driver-initialisation return boundary. That makes the first
`MILL.COM` menu semantics recoverable without presenting a fabricated sound
result or a false continuation into `TITLES.EXE`.

The selected external-driver continuation is now a separate hash-admitted
native recomp session. `MillenniumDosSoundDriverLoadSession` requires the
complete English `MILL.COM` identity and either exact `SSBL.DRV` or
`SCVX.DRV` identity; character `1` may bind only SSBL and character `2` only
SCVX. It records the caller's instruction-defined byte write at `$068a`
(`3` or `4`) and derives the original filename address `$0645` or `$064e`.
It then admits only this ordered DOS loader path: open result at `$02d2`,
seek-to-end at `$02eb` with the same observed handle and exact leaf length,
paragraph allocation at `$02fa`, rewind at `$0309`, complete read at `$0313`
with the same handle, and close at `$0319`. Carry set, a changed handle,
nonzero rewind position, a short read, or a length different from the
hash-identified leaf is rejected. In production,
`MillenniumDosCompatibilityRunner` services only the deterministic file
operations against that already authenticated immutable leaf. Its private
handle is native engine state, not captured DOS evidence. Its bounded native
paragraph arena allocates the exact rounded-up leaf size without overlap and
labels the resulting segment as compatibility provenance; the segment is an
Eon address-space key, never a claimed DOS capture. One tick performs open,
exact-length discovery, arena allocation, rewind, complete read, atomic byte
commit, and close, then stops at the INT 95h vector boundary. Arena exhaustion,
zero/overflowing requests, and detached sequences fail without partial state.
Vector state, parent stack data, EXEC outcome, and child entry remain
observations because they depend on process or DOS state that the immutable
leaf cannot prove. Reset destroys the generation-owned arena, and revocation
prevents its state or allocator from being reached.

Only after the exact read result does the session expose byte effects for the
original leaf at observed allocation segment offset zero. These are transient
runtime-memory effects, never writes to the archive or game-data directory.
The following vector-install observation is exactly `$0239`, `AX=$2595`,
`DX=0`; it records the original INT 95h vector request but does not claim DOS
accepted it or execute driver code. The common EXEC helper then requires an
explicit parent SP observation at `$032f/$05f7`, records the three original
parameter-block segment words `$067e/$0682/$0686` from explicit CS, and stops
at the literal title request `$0336`, `AX=$4b00`, `DX=$068f`, parameter block
`$067a` (`TITLES.EXE`). Reaching `title_exec_requested` is the only safe point
at which the production coordinator may construct the already hash-bound
English title session. It is not an invented EXEC return, title frame, driver
initialization callback, audio capability, or later game handoff.

The next owned production boundary is `MillenniumDosTitleExecEntrySession`.
It is constructed only after that exact `$0336` request and revalidates the
complete supplied `MILL.COM` and `TITLES.EXE` identities. Advancing it requires
an explicit child-process-entry record carrying the parent request tuple,
child `IP=$0100`, a nonzero child code segment, a monotonic sequence and one
of two visible provenance labels: an externally observed process entry or an
explicit result from Eon's narrow DOS compatibility service. Merely requesting
EXEC does not create this record and is never reported as EXEC success.

The seven hash-bound bytes at `TITLES.EXE+$0000..+$0006` are
`0e 1f 0e 07 e9 79 1a` (SHA-256
`f68952a9bbb82fa876f35aa293b010e2fb0be9f2814c77d2f8604391716ccd07`):
`PUSH CS; POP DS; PUSH CS; POP ES; JMP $1b80`. After the process-entry record,
the native engine may execute only this exact call-free prefix, recording
DS=CS, ES=CS and IP=`$1b80`. Only then does the release-owned state machine
leave the no-input sound-driver boundary and expose the already recovered
title session. Reset, release replacement and host revocation remove this
checkpoint. This proves the title entry prefix, not DOS loader internals,
driver initialization, private-interrupt results, a rendered frame or parity.

For the admitted English release, the compatibility runner can now produce
that record without an emulator after the exact EXEC request. Its shared
paragraph arena has already reserved the selected driver image. It reserves
another 455 paragraphs for offsets `$0000..$1c6f`; the original 7,022-byte
`TITLES.EXE` leaf is copied unchanged only to child offsets
`$0100..$1c6d`. The first 16 paragraphs remain uninitialised: reserving their
addresses prevents overlap but does **not** invent a PSP. The allocation's
`native_compatibility_arena` provenance, generation and allocation ID remain
in diagnostics. Allocation, image admission, compatibility child entry and
the exact seven-byte prefix are committed atomically with native runtime
memory; failure leaves the preceding EXEC-request boundary intact.

This service intentionally has no PSP fields, environment block, command
tail, inherited handles, DOS memory-control blocks, relocation behavior or
parent return result. Diagnostics expose `psp_modeled=false`,
`environment_modeled=false` and `parent_exec_return_observed=false` instead
of filling those gaps. The child segment is a deterministic Eon address-space
key, not a claimed historical DOS allocation address.

The next deterministic native continuation is documented in
[Millennium DOS native title initialization](MILLENNIUM_DOS_TITLE_INITIALIZATION.md).
It executes the exact `$1b80..$1b95` register setup and collapses the
byte-verified `$0122` preservation wrapper into its known function-$00
`INT $91` request. The owned checkpoint stops before the private interrupt
returns and explicitly leaves the original stack storage unmodelled.
An exact typed return at `$0129` can continue through the 29-byte
`$1b98..$1bb4` local path. Raw AX/FLAGS are retained; the instruction-defined
word/byte writes commit atomically to the compatibility child, and AH selects
the `$1bad->$1ac6` or `$1bb2->$1ada` call boundary. Neither callee executes
without separate evidence.
Both selected callees then have exact native prefixes: `$1ac6..$1ad0` and
`$1ada..$1ae4` load function `$0004` with record `CS:$1ac5` through the same
private wrapper. Eon automatically reaches that second `INT $91` boundary and
stops before its result; the following `$044c`/`$0487` calls remain unknown.
An exact second result can now enter those calls: `$044c` writes `$0107 := 1`
and prepares the first RGB-triplet BIOS request at `$046d`, while `$0487`
prepares the first indexed-palette BIOS request at `$0497`. Both code spans
and their original lookup tables are hash-bound. Only known register bits are
published, and execution stops before the first `INT $10` result rather than
assuming a BIOS palette effect or completing either loop.
The typed BIOS continuation now admits all 16 ordered results per selected
route. It rereads each next table element from the exact verified title image,
retains every raw AX/FLAGS pair, and commits only the final proven `$0107`
byte or mode-two `$010a := $b800` word. Both paths then join at the exact
`$1bb8 -> $1b1f` DOS allocation call. The native continuation now observes
the exact ordered `INT $21` services `$4a`, `$48`, `$49`, `$48`, and `$48`,
retaining raw AX/BX/FLAGS/carry results. It commits only the original writes
to child words `$1aa2`, `$010e`, and `$0112`. Either buffer-allocation carry
follows the proven error return and caller write before stopping at `$1c6a`;
two carry-clear results continue through DS/DX setup and stop at the next DOS
file-open request `$1af9`. The `$1b1f..$1b61` span hash is
`62bb857bf927ca3392900f9a8f26b9ab23f0780cd84c0ccf248f084e17c02ba7`.
No DOS allocator, file result, PSP, or memory-control block is inferred.
The next typed continuation verifies the original NUL-terminated `title.lib`
name at `$0e4e`, observes open `$3d00`, seek-to-end `$4202`, and close `$3e00`
results, and retains every raw AX/BX/CX/DX/FLAGS/carry value. Open or seek carry
stops at the proven `$05a3` error target. On success the exact helper computes
`(seek_AX + $0f) >> 4` with 16-bit arithmetic, ignores the close result as the
original does, and reaches the sized allocation request at `$1b64`. The helper
hash is `4fd3a9694c9ea36d7baf33607ed0b70ac764bb1f27bb6b686c3401bce5ef6b3d`.
No host file operation, file bytes, DOS handle, read call, or allocation return
is synthesized.
The sized-allocation result now connects to the exact `$1b62..$1b7f` tail.
Only its carry-clear segment is admitted as the `TITLE.LIB` buffer; the
following one-paragraph and temporary allocate/free results remain raw typed
observations because the original does not test their carry flags. The loader
then observes its own `$3d02` open, nine bounded `$3f` reads and `$3e` close.
Carry-clear reads copy only the corresponding bytes from the independently
manifest-verified 18,907-byte English `TITLE.LIB`; returned length is bounded
by request, remaining source, and observed paragraph capacity. No bytes are
invented for carry-set or zero-byte reads. After a loaded six-byte header, the
exact relocation stores count `$0026` and the normalized directory pointer
`base+$0481:$0003`. The loader/relocation code hash is
`63d5b5a645879a0a79ed0a7c880051e98ddf62b91f07616c0a72d035ee9581cf`.
Mode one additionally stores the proven `$0e59` pointer and clears
`$014c..$044b`, then stops before `$0fc6`: its requested `$4865..$4b64`
source extends beyond the verified TITLE.LIB leaf. No missing tail is
synthesized and the later BIOS palette request remains unreached.
For non-mode-1, the separately hash-bound epilogue returns to its proven
`$1bec` caller and reaches call `$1bef -> $1aac`. Eon stops before that setup
callee; it does not reuse or bypass the mode-one `$0fc6` overread boundary.
The non-mode-1 setup prefix is now hash-bound through DOS get-vector request
`AX=$3500` at `$10f4`. Its returned ES:BX and all later vector/BIOS effects
remain external typed boundaries.
The typed get-vector result now atomically preserves returned ES:BX in
`$10e6:$10e4` and reaches set-vector service `$2500` with `DS:DX=CS:$1124`
at `$1106`; no vector installation result is inferred.
That set result is now retained raw but otherwise ignored exactly; execution
continues only to the next get-vector request `AX=$3504` at `$110b`.
Its typed ES:BX result is atomically preserved at `$10ea:$10e8`; the native
path then stops at the matching `$2504` set-vector request at `$111d`.
The ignored `$2504` result now returns through the owned `$1ab0->$10ec` call
and reaches the first BIOS `$15` request with `AX=$011b`, `BL=$46`; no BIOS
result or second request is inferred.
The first BIOS return is now retained as raw AX/BX/FLAGS with explicit full
known masks on both requests. Only literal AH/AL/BL replacement is executed;
the path stops at the second BIOS request `AX=$011c` at `$1ac1`.
Its raw result is now retained and the owned call returns to `$1bf2`, stopping
before the next opaque setup callee `$11a7`; no callee effect is inferred.
The hash-bound `$11a7` callee is now local: it atomically establishes the four
cells `$118d/$1181/$1183/$1185` from proven literals and original image words,
returns to `$1bf5`, and stops before the still-opaque `$114e` call.
The `$114e` prefix now reaches a typed far-read boundary at `$115d`: two words
from `$0000:$0070` would be copied to `CS:$10dc`. No IVT contents, vector
replacement, or handler installation is invented.
After an explicit two-word observation, the exact suffix atomically preserves
those words at `$10dc/$10de`, installs `CS:$11d8` at IVT cell `$0000:$0070`,
sets `$112c`, and returns to `$1bf8`. The next `$12a0` call remains opaque.
That call now reaches its independent two-word far-read boundary at `$12ad`,
source `$0000:$0024`, destination `CS:$1266`. No vector words or replacement
are synthesized.
Once those words are explicitly observed, the exact `$12ad..$12bf` suffix
(file `TITLES.EXE+$11ad`, 19 bytes, SHA-256
`5f72f7b8f67574d774c5ba8e480cd8257accfab90651d94836d356edbe738861`)
copies them into `CS:$1266/$1268` and atomically replaces IVT cell
`$0000:$0024` with `CS:$126a`. The exact non-mode-1 caller prefix
`$1bfb..$1c04` (file `+$1afb`, SHA-256
`a111bf870ff60815e5d9f6a8c5d3a765335dcc8d77e1b0034b185b0872a3ec4d`)
then selects call `$1c02->$1ada`. The call is entered using the already
hash-bound `$1ada` contract. A fresh typed INT `$91` result and sixteen fresh
typed BIOS INT `$10` results are required; earlier observations cannot be
replayed as a second invocation. The deterministic return preserves the
mode-2 `$b800` write when applicable, takes `$1c05->$1c0a`, restores DS/ES,
and stops before `$1c0e->$135e`. That callee is the next opaque boundary.
Mode 1 remains separately stopped at its verified `$0fc6` overread boundary.
The `$135e..$1387` callee is now hash-bound as 42 bytes (file `+$125e`,
SHA-256 `c35f93db0d58443d76374684ed2c54ce78ddb7fc8e01ffa809026382450b4868`).
For the caller-connected non-mode-1 route it reads only the already-owned
second allocation cell, atomically writes the selected far pointer to
`CS:$1341/$1343` and CS to `$134b`, restores DS, and returns. The next exact
call `$1c11->$0ff3` is now entered. Its exact 16-byte prefix (file `+$0ef3`,
SHA-256 `d17cc200504c832c3062e1c6951c753a8819c0fd1255b7273c28b3fcf1f3e363`)
writes CS into request record `CS:$0fe9`, loads function `$0019`, and calls
the common private wrapper. The next exact boundary is the typed INT `$91`
result at `$0127`; neither its raw return nor service semantics are invented.
An explicit raw AX/FLAGS observation now completes both wrapper returns. It is
stored separately from the startup INT `$91` result, and the deterministic
caller loads AX zero before the next exact opaque call `$1c17->$1725`.
That call now follows its 27-byte hash-bound prefix (SHA-256
`646ada76ab8f0b370cd3e1f3001cf2e21a5105bbcf650cf6239bf801853754dd`)
into `$1390`. Its 26-byte prefix (SHA-256
`f6be40d902e1d36bd640df417e6a3b8e813b4fce0c7bbf7801a33ae44d60a897`)
uses only the verified entry count and owned relocated directory/allocation
pointers. Execution stops before the two `LODSW` source words at `$13aa`;
the precise far source is exposed as a typed boundary and no descriptor bytes
are invented.
The admitted `TITLE.LIB` resolves the `$13aa` source to file `+$4813`, words
`$0006/$0000`; contradictions fail before mutation. The 35-byte suffix
through `$13cc` hashes to
`e8b21803c3739aac65b59a9919f03c97d0d55daf7fd2a35e7567973765724921`,
atomically stores normalized pointer `$3000:$0006`, and stops before external
word read `$3000:$001e` at `$13cd`.
The typed single-word facade admits that source only when it equals genuine
`TITLE.LIB+$001e` word `$0140`; contradictory provenance leaves state
unchanged. The retained observation advances to the next exact external read
at `$13d0`, source `$3000:$001c`, whose media word is `$00c8`.
The second typed word is provenance-checked before the exact `$13d0..$13e1`
span (SHA-256
`787613791d00d3ae372e3ec9b7b02d56a0704b9e14b44e2d6874b125927befe6`)
atomically stores the dimensions and their `$fa00` product. The next boundary
is external word `$3000:$001a` at `$13e2`; no subtraction result is inferred.
The word is now admitted as genuine `$0000`. Exact bytes `$13e2..$13e8`
(SHA-256 `0653c7fb33f8d3c60d973b7c038f4c724ffd194abd7f21990762340477246ed4`)
atomically store adjusted product `$fa00` at `CS:$138a`. The new typed external
byte boundary is `$13e9`, source `$3000:$0007`; no byte value is inferred.
The new byte-observation facade provenance-checks genuine value `$23` before
the 9-byte span (SHA-256
`ed46676eb54a03e725cbb96371e4fd13852a350ba5b027e5c59dda07c78b8ecf`)
increments and atomically stores `$24` at `CS:$1389`. The next exact boundary
is external byte `$3000:$000a` at `$13f2`.
The second byte is admitted as genuine `$00`. Its 20-byte branch suffix
(SHA-256 `172d30853354efec879699618dd36f3fbda28ddd07d8ea66bc2a23ace6ee6753`)
returns locally; the 39-byte caller suffix (SHA-256
`d095399b2a968131f10112f1895b1449f6d1572052c032e48289218e5d07355b`)
atomically builds the dimension request and reaches typed private INT `$91`
function `$0006` at `$0127`. No result is inferred.
The raw result is now accepted only as a fresh typed `$0127 -> $0129`
observation and retained without interpretation. The verified wrapper
epilogue and `$1767` helper return resume the caller at `$1c1a`; its three-byte
call span hashes to
`4d867f121cb96c5445f22218aa4b145e27c1ac41838994c348133aeba4c0d925`.
The exact `$1004..$1013` callee prefix (file `+$0f04`, SHA-256
`23f2112307ea2992920c08508de31bd2e689247c3444791c443823cab3c6438e`)
atomically writes CS to request word `CS:$0fe7`, selects record `CS:$0fdf`
and function `$001a`, and reaches the common private INT `$91` at `$0127`.
The next raw result is not inferred.
That function-`$001a` result is now a strict typed input containing raw AX,
FLAGS, and all ten bytes observed at `CS:$0fdf..$0fe8`. The coordinator
applies those bytes atomically; no field or service meaning is assigned. The
hash-bound wrapper epilogue `$0129..$012e` has SHA-256
`a6e3a351304f487a18bc22e460403bfcdb5e702831b037aa0a90a56bf3cf7baf`,
and the caller's first `$1941..$1962` loop prefix has SHA-256
`8ae5339224f631de9dbf852ab43c5553849b37ef00289e0a34055e73a760357a`.
That loop atomically advances both original zero output offsets to `$0170`,
selects descriptor index one, and re-enters the already verified `$1390`
decoder prefix. The largest static continuation ends at `$13aa` before two
external words at relocated `TITLE.LIB+$000f`. This is a typed preservation
boundary, not a rendering or descriptor-value claim.
The genuine `TITLE.LIB+$000f` words are `$0503/$1f02`. The existing exact
`$13aa..$13cc` pointer suffix (SHA-256
`e8b21803c3739aac65b59a9919f03c97d0d55daf7fd2a35e7567973765724921`)
normalizes them against owned destination segment `$3000` to
`$5050:$0003`, then atomically stores the pointer at `CS:$138c/$138e`.
Execution now stops before `$13cd` reads its word at `$5050:$001b`.
Contradictory descriptor words leave both state and runtime memory unchanged;
no content, record, or graphics meaning is assigned to the next runtime word.
The `$5050:$001b` word now enters through the same ordered typed single-word
facades used by earlier record fields. Exact instruction `$13cd`, bytes
`8b 44 18`, has SHA-256
`30cefd61e3cc968dfe7b7f54ed07251f1fe9ec99fb33bad8b4ae24ce67b80704`.
The typed `$1437`, `$144a`, and `$1470` continuations now apply only their
instruction-defined shifts, run copies, lookup arithmetic, counter changes,
and atomic output writes. Ordinary escape/run paths resume at typed `$1428`
next-byte boundaries; the mode-two `$ff` extension stops at typed `$1452`;
the lookup path stops at a separate high-nibble `$1428` state for the same
source byte. No compression or pixel interpretation is asserted. The second
record's `$13d0` word at `$3c80:$0016` is also native through the exact
18-byte multiplication/store span (SHA-256
`787613791d00d3ae372e3ec9b7b02d56a0704b9e14b44e2d6874b125927befe6`)
and stops at typed `$13e2`, source `$3c80:$0014`.
High- and low-nibble `$1428` states are now distinct, so `CL`, `SI`, `DI`,
`DX`, and the next source byte remain instruction-accurate after escape, run,
and lookup paths. The mode-two extension consumes typed `$1452`, exact
six-byte SHA-256
`846a82fa183b14b5fd42d6e0c3bdf5c16cf8863e647825e8fd588d705f655756`,
and stops at typed `$1458`. The second record's typed `$13e2` word applies the
hash-bound wrapping subtraction and stops at `$13e9`, source `$3c80:$0001`.
These remain raw control-flow and arithmetic facts, not codec or pixel claims.
The typed `$1458` continuation and both high-half variants are now native
through the whole `$1437..$1487` loop, exact 81-byte SHA-256
`5fab2565b47896f17a9418a67095c43645b61c02f960f8749dbb3d5b9718a725`.
The session preserves the selected half-byte shift, constructs the two-byte
mode-two count exactly, applies wrapping `DX` subtraction and ordered output
writes atomically, then stops at `$1488` or the next typed `$1428` input.
This expands deterministic execution only; stream and output bytes still have
no claimed compression, palette, pixel, or graphics semantics.
Completion also owns the exact `$1488..$149e` mode dispatch, 23-byte SHA-256
`7967c8650f118732cc5c884ea6d332a8dbe6dc060e5736088e7b5d0f1fb081ad`.
It restores the already admitted descriptor pointer and follows mode one,
mode two, or the other-mode prefix to typed reads at `$14a9`, `$1647`, or
`$14f0`, respectively. The genuine first-record source is `$5050:$0003`;
its raw byte remains external and has no inferred header or graphics meaning.
For mode two, genuine `TITLE.LIB` supplies ordered raw values `$48`, `$00`,
and `$4000` at `$5050:$0003`, `$0004`, and `$001d`. Exact
`$1647..$16b2` SHA-256
`9ba1e245431578fbac9c3386bea9a102be68fe6700ca057ff5d9af3f819427fd`
owns their branch/address arithmetic, `CS:$14df/$14e1` pointer stores, and
atomic destination clearing. Execution stops at typed `$16b3`; neither the
three inputs nor subsequent source bytes have inferred graphics semantics.
The subsequent `$16b3..$16e8` loop is hash-bound as 54 exact bytes, SHA-256
`24a597122dd6afe0c434683295f0e97ef04e60b67722db2042178f33f2c361ed`.
Source and lookup reads remain separately typed and ordered. Between them the
runtime owns the exact half-byte shifts, `DX` and `BX` loop edges, destination
advance, atomic byte write, and final register restoration. A genuine first
lookup resolves `$4000:$0170` value `$7a` through `$5050:$409a` value zero;
no lookup-table, palette, planar, or pixel semantics are claimed.
When all requested bytes are present in owned native memory, the coordinator
can now drive this same typed loop through `$16e8` without external byte
callbacks. DOS aliases are compared by the exact 20-bit physical address;
contradictory aliases are rejected. The operation uses a caller-supplied finite
cap and is atomic across session and memory state on missing input, detached
sequence, unexpected boundary, or cap exhaustion. Decoder output effects now
carry the recovered destination segment explicitly instead of being attributed
to the child code segment. No additional executable span or semantic claim is
introduced. On the terminal loop edge, the native session follows the actual
`$16e8` RET into its first caller. Exact `$1740..$1763` bytes (file offset
`$1640`, SHA-256
`d1e04fba870ff3677e495d131b994bfdc1dc6f95af7ea9f7cc4316d48568f115`)
consume the saved zero table displacement, load the genuine embedded words
from `$170c/$170e`, copy raw `$1357/$1359` into `$133d/$133f`, and stop at
the complete `$1764->$0122` private function-six request boundary. These
caller effects commit in the same transaction as the owned-memory loop. The
private result and all presentation semantics remain explicit boundaries.
Its raw value is retained as AX without assigning width or graphics meaning,
then execution stops before `$13d0` reads `$5050:$0019`. Detached addresses
or sequences fail before state changes, and the read does not mutate runtime
memory or original media.
The next `$5050:$0019` word is also accepted only as an ordered typed runtime
observation. The exact `$13d0..$13e1` span (SHA-256
`787613791d00d3ae372e3ec9b7b02d56a0704b9e14b44e2d6874b125927befe6`)
atomically stores both observed words at `CS:$1357/$1359` and the low unsigned
product at `CS:$133b`; AX/DX retain the exact low/high product pair. The next
boundary is `$13e2`, source `$5050:$0017`. No dimension, pixel, or rendering
semantics are inferred from either input or their product.
The third runtime word at `$5050:$0017` is now accepted only as the exact
ordered `$13e2` observation. The seven-byte subtraction/store span has
SHA-256 `0653c7fb33f8d3c60d973b7c038f4c724ffd194abd7f21990762340477246ed4`.
It atomically stores the wrapping low-word subtraction result at `CS:$138a`
and stops before `$13e9` reads byte `$5050:$0004`. Neither operand, result,
nor next byte receives inferred graphics semantics.
The raw byte at `$5050:$0004` is now accepted through the ordered typed-byte
facades. Exact bytes `$13e9..$13f1` have SHA-256
`ed46676eb54a03e725cbb96371e4fd13852a350ba5b027e5c59dda07c78b8ecf`;
they apply the instruction-defined wrapping increment and atomically store it
at `CS:$1389`. The next boundary is `$13f2`, source `$5050:$0007`. No field,
mode, palette, or pixel meaning is assigned.
The ordered `$13f2` observation now admits that raw byte and atomically stores
it at `CS:$1388`. The exact 20-byte branch has SHA-256
`172d30853354efec879699618dd36f3fbda28ddd07d8ea66bc2a23ace6ee6753`.
Values one and two follow the exact `$1406..$1418` prefix (SHA-256
`a38148b66817871d8731829b2a0703e48b2e7fecb0fee51112be1e8e3b0332d0`)
and stop at the typed `$1419` payload-byte boundary, source `$5050:$001f`.
All other values return to the caller, advance the loop output offsets to
`$02e0`, select record index two, and stop at the typed `$13aa` two-word read
from relocated `TITLE.LIB+$001b`. Detached observations fail before state
changes. No record-field, encoding, rendering, or pixel semantics are claimed.
The `$1419` payload observation is now native through the exact 15-byte prefix
ending at `$1427` (SHA-256
`912d067ef688829815594e9fdf4e2ae8f03051cd3be882dc482a02dae032d39b`).
It atomically writes the raw byte to current output `CS:$0170`, preserves the
instruction-defined register changes, and decrements the owned raw word from
`CS:$138a`. A nonzero remainder stops at typed byte `$1428`, source
`$5050:$0020`; a zero remainder stops at internal dispatch `$1488`. No codec,
pixel, or graphics semantics are inferred.
The alternate branch's genuine `TITLE.LIB+$001b` words are `$c800/$4000`.
They pass the existing exact `$13aa..$13cc` pointer transformation, normalize
to `$3c80:$0000`, and are atomically stored at `CS:$138c/$138e`. Execution
stops at the typed `$13cd` runtime-word boundary, source `$3c80:$0018`.
The complete `TITLE.LIB` remains read-only and hash-addressed; contradictory
typed descriptor words fail before any state or memory effect is committed.
The next `$1428` byte is now accepted only in sequence. The exact 31-byte
dispatch span has SHA-256
`dd7abdeaa64d537ee31fb6c4dffe319a7f824226ca44bb33e0f4cb3986560be7`.
Its low nibble selects typed `$1437` word `$5050:$0020` for `$f`, typed
`$144a` word at the same address for mode two plus `$e`, or typed `$1470`
lookup byte at `$5050:($0008+nibble)`. No codec or graphics meaning is
inferred. Independently, the second record's ordered `$13cd` word at
`$3c80:$0018` is retained as raw AX and advances to typed `$13d0` word
`$3c80:$0016`; the instruction retains its recorded SHA-256
`30cefd61e3cc968dfe7b7f54ed07251f1fe9ec99fb33bad8b4ae24ce67b80704`.

The visible choice prompt is also recovered as an ephemeral, original byte
span only: loaded `$0407..$04a1` (file `+$0307`, including its DOS `$`
terminator) is 155 bytes with SHA-256
`d84297ee58abeaa4ca09d60a533fe0b05ea4b805af46629d32c031b11700cad0`.
The runtime checks the whole `MILL.COM` identity and this span before rendering
the supplied bytes. It strips only the DOS output terminator, does not put the
text in a localisation catalog, and retains no original byte buffers after
parsing. The current renderer is text-only presentation of
that original prompt—not a claim that its DOS font, mode, or surrounding
screen pixels are recovered.

The `$1f` query's low byte is also not a fixed profile constant. EGA function
`$00` writes its derived allocation count to local `$008a` at `$022f`, and
function `$1e` can write a clamped caller byte there at `$0259`; MCGA function
`$00` writes its derived allocation count to `$00ac` at `$0246`, and function
`$1e` writes a recomputed count at `$02d0`. Both supplied file bytes are zero,
but these are conditional driver-internal writes. The recovered `2200AD.EXE`
startup requests `$1f` only and has no confirmed installed driver segment or
successful state-establishing title path. `TITLES.EXE` does contain a separate
candidate: its `$1b80..$1ba7` prefix (file `+$1a80`, 40 bytes, SHA-256
`e6e014d7c03f9efbd7e9bde67686c281cf66acca809b306cc29dfb45d614b535`) prepares
AX `$0000`, ES=CS and BX `$1ac4`, then calls the shared `$0122` wrapper, whose
`$0127` is `INT $91`; the two-byte record at `$1ac4` is `01 00` (SHA-256
`47dc540c94ceb704a23875c11273e16bb0b8a87aed84de911f2133568115f254`). For
the supplied MCGA driver this can select function `$00`; that function reaches
only the `INT $10` boundaries described below, not `INT $92`. `INT $92` occurs
in other MCGA dispatch functions and is not reached by this record. BIOS mode
results, title exit, DOS return, driver lifetime and the later `2200AD` call
remain unobserved. Project Eon therefore never substitutes
the on-disk zero or any calculated value for the unknown runtime AL result.

The parent-side ordering is separately hash-locked as one startup model.
`MILL.COM` calls a common helper first for `TITLES.EXE`, then conditionally for
`2200ad.exe`; its caller `$023d..$0252` is 22 bytes with SHA-256
`829b3d096d593d1ff4f1028eb05af1ccf8ca0b8ead98a5edcb523dba4cd725cf`.
The helper `$031c..$034b` (48 bytes, SHA-256
`62cee56837e015eecc218906046c1e1c19a7ad9ba87e6580f99674eac0976b58`) fills
the three segment fields of immutable parameter block `$067a..$0687` (14 bytes,
SHA-256 `e2b2aa089d2c6a23b14055f3721c6b53836268070c2a727d2d7fa1a75461869b`),
saves parent SP at `$05f7`, invokes DOS EXEC `AX=$4b00` at `$0337`, restores
the parent context, then branches on carry. Only the noncarry route asks DOS
for child termination status at `$0348` before returning. Thus ordering, ABI
operands, and restoration are evidence; child completion, AL, title exit,
vector survival, and `2200AD` execution are deliberately unknown.
Function `$00` resolves to `$01c8` (EGA) or `$01e6` (MCGA). The supplied MCGA
function-$00 body `$01e6..$0209` (36 bytes, SHA-256
`fb21e417ebf59d096edf515db6258423a2e304ce513b125a075e15f0a23723e8`) first
reads `ES:[BX]` into CL and clears CH, but its complete verified local body
does not branch on that caller byte. It compares code-local `$00ae` with
`$ff`: only that sentinel path requests BIOS current-mode information at
`$01f5` and stores the externally returned AL back to `$00ae`; either path
then requests mode `$13` at `$01fc`, requests current-mode information again
at `$0201`, compares its external AL result with `$13`, and returns that
unmodified external register state at `$020a` on equality or a locally zeroed
AX at `$0208` on mismatch. The `JNE` cache bypass lands at `$01fa`. EGA has
the same instruction layout at `$01c8`: local `$008c`, query `$01d7`, bypass
`$01dc`, set mode `$0e` at `$01de`, verify `$01e3`, match return `$01ec`, and
mismatch return `$01ea`. These are only static BIOS boundaries: Project Eon
does not assign either BIOS result, assert that the on-disk `$ff` is the
runtime cache state, or use the caller's title record to select a mode.
Function `$04`
resolves to `$0c17` / `$0815`, reads the input byte at `ES:BX`, masks it with
`$03`, and updates only its code-local byte (`$008d` / `$00af`) before `RET`.
Function `$1f`, which the `2200AD.EXE` startup wrapper requests at `$d2c5`,
resolves to `$0235` (EGA) / `$024c` (MCGA). Its exact six-byte prefixes load
AL from driver-local `$008a` / `$00ac`, set AH to `$04` / `$01`, and return;
neither reads the caller's `ES:BX`. The supplied on-disk bytes are zero, but
their launcher/title-runtime values and the original installed-driver choice
are not established. Therefore this is a driver-specific, read-only ABI fact,
not permission to select a driver, fix `$da05`, or execute the startup/GX
path. This is a strict SDL-adapter boundary for requested video mode and a
masked driver-local option; Project Eon executes neither the driver, BIOS call,
nor any path-dependent initial presentation.

The caller sites that request private functions `$02` and `$04` leave the
pointed `ES:BX` records inside the original executable image, but the supplied
bytes do not yet establish those records' complete layout, ownership, or the
functions' return contract. In particular, the observed AX=`$04` input-mask
operation is not evidence that the caller's adjacent record is a host video
configuration. Project Eon preserves the addresses as control-flow evidence
and deliberately does not build an SDL adapter or mutable substitute for
either call until a real caller-to-driver ABI has been recovered.

The old-vector preservation chain is also raw-byte-validated. `$0167` loads
`AX=$3591`, makes its external call, then stores `BX` and `ES` at adjacent
cells `$05e7/$05e9`. On the terminal cleanup path `$0269`, `LDS DX,[$05e7]`
feeds the same pair to another literal `AX=$2591` call. This connects the
caller-side save and restore operands only; it does not assert that either
external call reads, writes, installs, or restores any particular vector.

The English DOS archive's `SFX1.VOC` is decoded directly as a Creative Voice
File: its verified SHA-256 is
`5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88`.
Its original type-1 PCM block has time constant `$9c`, unsigned 8-bit mono
rate 10,000 Hz, and 738 samples whose SHA-256 is
`811de4108fe6551e09da1865f3ff2e18a8313aad30a6916210c4d5d49b1e1c06`.
The native decoder accepts the original uncompressed sound and continuation
blocks, preserves the source PCM bytes, and rejects encodings not yet proven
by game media rather than replacing effects.

`2200AD.EXE` also contains the exact contiguous 126-byte DOS name sequence
`SFX1.VOC` through `SFXE.VOC` at loaded address `$cfdd` (file `+$cedd`,
SHA-256 `5bc252a34057b25239c81ce4ead178c294456e9af233bdd98d2d6f0f3cb4d008`).
The parser validates this bank against the whole executable identity and
reports all fourteen original names. This establishes the original resource
family, but it is not a playback trace: no direct code reference from a
startup or UI path to that table, nor a verified sound-driver call with a
selected entry, has yet been recovered. Project Eon therefore does not attach
any VOC file to a host event, title transition, or timer. The existing decoder
is retained as a source-byte parser only until a real caller and trigger are
established.

`--inspect` additionally resolves every one of those fourteen executable-named
assets through the authenticated archive inventory and decodes them in memory.
The supplied English bank contains 37,100 unsigned PCM samples across the
original 10,000 Hz and 6,024 Hz rates. This is a read-only completeness check
of the resource family, not an audio subsystem: no sound is scheduled or sent
to SDL without a recovered event-to-index and driver-ABI chain.

The admitted English native session now carries the same fourteen-entry voice
bank catalogue (original filename/hash, decoded sample rate and sample count)
after revalidating each leaf through its already verified media snapshot. PCM
bytes are discarded after this validation and the catalogue has no playback,
event-selection, mixer, SDL-device or save-write API. Spanish DOS has no such
catalogue because the matching executable/resource chain has not been
recovered for that release.

`2200AD.EXE`, `2200GX.EXE`, and `TITLES.EXE` are flat 16-bit binaries despite
their suffix. `MILL.COM` provides a private runtime through interrupts 91h,
92h, and 95h. `2200AD.EXE` jumps from file offset `0x0004` to `0xd1b0`, then
uses DOS services and loads original libraries. See the
[DOS analysis](generated/dos-millennium.md).

The English DOS `2200AD.EXE` does have a separately bounded caller-connected
overlay load for the original `2200GX.EXE` (SHA-256
`093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb`). The
literal NUL-terminated name is at `$11c2`; loader `$11ce..$11f6` (file
`+$10ce`, 41 bytes, SHA-256
`a8972b74ad9d1dfabe508c42b7fcda0fb45e0d449613449ab8a2763ca8ecff45`) reads
the original segment cell `$0118` and has static calls `$11d1 → $053a`,
`$11e4 → $0574`, and `$11ec → $0596`. The caller at `$d335` reaches `$11ce`.
The independently locked adapter `$6c52..$6c72` (file `+$6b52`, SHA-256
`b34e5abf8ecd790fce3e7a032d7a7fcacc073d03909e98fd33f9503113e3ad87`) reads
the same cell, pushes overlay offset zero, executes `RETF` at `$6c68`, and
has local continuation `$6c69`/return `$6c72`. These are raw loader and
transfer facts only: Project Eon does not choose a segment value, invoke a
DOS/private call, run overlay code, or infer a screen/resource order.

The overlay's actual entry dispatcher is separately locked at `2200GX.EXE`
`+$0000..+$0013` (20 bytes, SHA-256
`f4d657fcbdda23d7f0fdf2bbf48405d0a04e8b8149df064607f49042525fbd55`). It
clears AH, uses AL to index the original 21-word table at `+$0015..+$003e`
(SHA-256 `4d04568e05378787921012654fe9c157419ce7c07f9943b51135258f32a06df3`),
and near-returns to the `RETF` at `+$0014`. The caller-connected observed
selectors `$0e/$0f/$12/$14` map to original overlay offsets
`$0090/$009f/$0097/$00a7`. No selector policy, handler return, overlay state,
resource, or display effect is inferred or executed.

There is a caller-connected selector prefix immediately before the adapter:
`2200AD.EXE` `$d343..$d375` (file `+$d243`, 51 bytes, SHA-256
`571626e83b0787401f89c8586c12dfb4d4221c44e0a9786727d2314b09327091`) reads
the still-unmodelled byte `$da05`. Values `$03/$04/$02` select AX
`$000e/$0012/$0014`; every other value selects `$000f`. The corresponding
literal DX values are stored at `$4b6e`, then `$d373` calls the adapter at
`$6c52`. The four overlay entries converge in a 94-byte local prefix at
`+$0090` (SHA-256
`8d412472415d513482b5c70198bb1aa04fa0d25798dd5f4b40b262151c489736`) that
only copies in-overlay record words and returns. No selector value, record,
return, asset reference, video instruction, resource, or display effect is
provided, inferred, or executed.

The English `2200AD.EXE` COM entry preserves the original segment setup before
the recovered main loop: loaded `$d2b0` first establishes `DS=CS` and `ES=CS`;
the following `$d2b4` block establishes `SS=CS`, `SP=$da00`, and makes its
first direct call to `$0124` (the raw near-call arithmetic crosses `$ffff`,
then wraps in 16-bit IP). It stores the native result in original cells,
compares its `AL` byte with `$01`, and selects direct call `$d1a1` or `$d1b5`.
After another original call it tests `DX`; the nonzero static edge is `$d44b`.
Project Eon validates these bytes, addresses, and branch targets, but does not
assume any call returns or make this into a host-side startup sequence.

The wrapped `$0124` target itself saves `DS`, `SI`, `DI`, `BP`, and `ES`,
executes the launcher's private `INT $91`, restores those registers, and has
its `RET` opcode at `$0130`. This is a bounded instruction-level fact only:
the private interrupt's result and actual return behaviour are not emulated.

If that wrapper returns, its caller's next raw block is independently fixed:
at `$d2c8` it stores `AX` at `$d128`, moves `AH` to `$4368` and `$da05`, and
stores `SP` at `$d12c` before comparing `AL` with one at `$d2d9`. The parser
records these destination operands and the following static branch edges only;
it does not assume a return, assign values to the cells, or execute either
branch.

The two selected static paths are independently byte-validated as well. The
`AL == $01` call target `$d1a1` and the other target `$d1b5` each set
`AX=$0004`, `ES=CS`, and `BX=$d19f` before directly calling the same `$0124`
private-`INT $91` wrapper again, from `$d1a9` and `$d1bd` respectively. Only
if those calls return do their next direct calls reach `$044e` or `$0466`.
These are control-flow operands only: Project Eon neither assumes either
wrapper return nor interprets the register setup, follow-up calls, or their
results.

`MillenniumDosEnglishGameStartupCallees` now keeps those two English selector
targets as their own hash-addressed preservation boundary rather than relying
on the wider main-loop profile. The 20-byte equal block at `$d1a1` hashes to
`6f59df77c567324b41dd6159a6fbac7d8970626fc40e8b908f9f58746a993a3e`; its
private wrapper call is `$d1a9 → $0124`, its conditional local successor is
`$d1ac → $044e`, and only after both encoded calls does it write literal `$01`
to `$da05` and return at `$d1b4`. The 28-byte other block at `$d1b5` hashes to
`2f61098eb45bb48ea7a38ab2fcc2e065ae0d0b2ad08ea9973e3fe464943fba9b`; it
has `$d1bd → $0124` and `$d1c0 → $0466`, then reads native `$da05`, compares
it with `$02`, and conditionally encodes the `$b800` store at `$0107` before
its `$d1d0` return. These are bytes and operands, not a claim that the
private interrupt returns, that either branch is chosen, or that the native
comparison has a particular value.

The immediate local successors are independently preserved by
`MillenniumDosEnglishGameStartupFollowups`. `$044e` is the eight-byte literal
store/RET sequence (SHA-256
`38889279a8b89e0e600bb25298015ccd8aadc09ea3858a1790097b3f7ff4ea8f`).
`$0466` through `$047c`, together with its table at `$0456`, is a 23-byte
in-image BIOS palette-request prefix and a 16-byte table, SHA-256
`b17db26fa4fa8b7307fb767ff98351bd6dcca202829dd2d9348ff4991942d779` and
`ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a`.
It loads initial `CX=$0010`, reads that table, encodes `AX=$1000`, and first
reaches `INT $10` at `$0476`. The BIOS interrupt, any register effects, and
any number of loop executions are explicitly outside this recovery.

The later English startup continuation is independently hash-locked as well.
If either selected private-wrapper path returns, `$d2e5` preserves DX,
restores DS from CS, and calls `$d1fa` at `$d2e8`. The callee's first nine
bytes end at its `INT $21` site `$d201`, with literal AH `$4a`; this is an
external DOS boundary, so Project Eon neither invokes it nor supplies AX, DX,
carry, or a return. Only if that call returns do the following bytes store AX
at `$d128`, test DX at `$d2ee`, branch on zero to `$d2f5` (whose first local
call is `$d2f5 → $1161`), or jump on nonzero from `$d2f2` to `$d44b`. These
are raw static control-flow operands, not an allocation result, startup
decision, or executable path. The 20-byte continuation and 9-byte callee
prefix are both SHA-256-validated by
`MillenniumDosStartupAllocationBoundary` against the full original English
`2200AD.EXE`; mutations are rejected before the facts are exposed.

The DX-zero successor is now independently bounded as a separate static
chain. `$d2f5` calls `$1161`, which reads the native byte `$da05`; literal
comparisons `$01/$03/$02` select the in-image name addresses
`$1131/$113d/$1155`, respectively, while the default sets `$1149`. Those four
NUL-padded names are `VGATXT.BIN`, `EG3TXT.BIN`, `EG6TXT.BIN`, and
`TDYTXT.BIN`, in raw storage order `$1131/$113d/$1149/$1155` (48 bytes,
SHA-256 `153a0b62bdec1702cdd36ff6e7dc33ec4ed6673ad5d3f5f8bc07b748f7e06d76`).
The selector then has direct call `$117c → $053a`. That callee's fixed prefix
replaces DX with the in-image `A:\\2200AD\\SECURITY.HID` name at `$2f6a`
(23 bytes, SHA-256 `1a95edb6109f3db1af0c0389f1aa5d597a184f26725e095f771b6622f654ec6a`)
before first reaching `INT $21` at `$0550` with `AH=$3d`, `AL=$02`.
`MillenniumDosStartupZeroPathBoundary` locks the three-byte zero successor
(SHA-256 `798bd5318e00348848f0ca4b876d687fec5c606abe88236ff4e922a77fe08b65`),
30-byte selector (SHA-256
`fffa1b0e03e9abf90bfde3bfb86bf1125ae579ede767eea68223e098d641992f`), and
24-byte callee prefix (SHA-256
`328e11edf0653b0e0f21db3b61cf9ff95795ec9431f07c0198a700358f75ed74`) against
the full original executable. This does not choose `$da05`, infer a selected
name's use, invoke DOS, provide carry/AX, or claim the code path executes.

There is a further caller-connected static continuation after that selector,
but only if the selector and its DOS-facing loader both return. The 23 bytes
at `$d2f8..$d30e` hash to
`9c7b13c4e0b99e8529e78063b91ae92d967b9fc6de66ebeeaacec01563e4a9d9`.
They load the encoded source address `$0082` through `CS:SI`, subtract literal
`$30` from the resulting `AL`, store it at `$0122`, and call `$d305 → $d07a`.
The subsequent literal `BX=$fa00`, `AH=$48`, `INT $21` reaches its next
external DOS boundary at `$d30d`. `MillenniumDosStartupZeroContinuationBoundary`
locks that exact span and near-call arithmetic against the complete English
executable. It does not assert any byte read, local-call return, DOS result,
or allocation; this is conditional instruction provenance only.

There is one further conditional post-allocation boundary, anchored separately
without interpreting the allocation call. Only if the preceding local helper
and `INT $21/AH=$48` both return, `$d30f..$d318` encodes `CS:MOV [$d130],BX`,
`MOV ES,AX`, `AH=$49`, and `INT $21`. Its ten bytes hash to
`f583faad7bddba301c431adb94fa9d53d5b197dcba2f447b0b654df6f1b452ce`.
`MillenniumDosStartupPostAllocationBoundary` records the encoded `$d130` store,
the `$d314` register-transfer instruction, and the next DOS boundary at
`$d318`; it does not treat AX or BX as a result, infer a segment, invoke DOS,
or claim a return from this new boundary.

There is a caller-connected continuation after that second DOS boundary, but
only on the unproven condition that `INT $21/AH=$49` returns. The 30 bytes at
`$d31a..$d337` hash to
`4d94bf904471cf96a03ce6dd111c0720f396e08ebf2f4603469377db0dc669ef`.
They restore DS from CS, pop the earlier saved DX, use `LDS` twice with the
same encoded far-cell address `$1042` (first into DX, then into SI), and make
three direct near calls: `$d32f → $6bf2`, `$d332 → $101a`, and
`$d335 → $11ce`. The latter two targets are the separately preserved
`2200AD4.BIN` and `2200GX.EXE` loaders; that relationship is raw static
caller evidence only. `MillenniumDosStartupPostReleaseContinuation` validates
the entire original English executable, this exact span, and all three
16-bit wrapped call calculations. It does not assert that AH=$49 returns,
that `$1042` holds a valid pointer, that any local call returns, or that DOS
frees, loads, or transfers any host resource.

The immediate encoded successor of the GX-loader call is separately bounded
as well. Only if `$d335 → $11ce` returns, `$d338..$d342` restores ES from CS,
loads literal `BX=$d1a0` and `AX=$0022`, then calls the existing private
`INT $91` wrapper at `$d340 → $0124`. Those 11 bytes (file `+$d238`) hash to
`64e7dddae2ca6942cddaa4c564d61203b26c469fc898bb923b2ba227d93876ab`.
`MillenniumDosStartupPostGxLoaderBoundary` validates the full 54,391-byte
English `2200AD.EXE`, the raw span, and its 16-bit wrapped call calculation.
The literals are encoded operands, not a reconstructed private-runtime ABI:
Project Eon does not assert that the loader or wrapper returns, interpret the
arguments, invoke `INT $91`, or supply a result.

The call target itself is now retained separately as
`MillenniumDosPrivateInt91Wrapper`. In the same hash-identified English
`2200AD.EXE`, its 13 bytes at `$0124..$0130` hash to
`5d17daad68e9062dc6852ae76740db4afdcb81555ba9fb7d15d4e4aa8d088175`.
The complete raw instruction sequence is `PUSH DS`, `PUSH SI`, `PUSH DI`,
`PUSH BP`, `PUSH ES`, `INT $91`, `POP ES`, `POP BP`, `POP DI`, `POP SI`,
`POP DS`, `RET`. The parser also validates the three caller bytes at `$d340`
and their wrapped near-call target `$0124`; this ties the wrapper to the
post-GX route without claiming that the loader returns. It deliberately does
not infer a stack result, register preservation convention, private interrupt
ABI, interrupt effect, or return from either the interrupt or the wrapper.

The caller's immediate encoded return site is separately retained as
`MillenniumDosPostInt91CallerSelector`, without treating that return as a
runtime fact. In the same 54,391-byte English `2200AD.EXE`, the 51 bytes at
`$d343..$d375` hash to
`571626e83b0787401f89c8586c12dfb4d4221c44e0a9786727d2314b09327091`.
They load the original byte at `$da05`, compare it with literals `$03`, `$04`,
and `$02` at `$d34d`, `$d358`, and `$d363`, respectively, select among four
encoded DX/AX pairs, store DX through a CS override to `$4b6e` at `$d36e`, and
make the first direct local call `$d373 -> $6c52`. The parser validates the
complete executable, the exact span/hash, and the wrapped near-call target.
It does not assert that the private wrapper returns, read or assign a value to
`$da05`, interpret the register pairs/store, run the callee, or infer any
interrupt behavior.

`MillenniumDosPostGxStartupPrefix` connects those two byte-locked spans only
when a reference run has explicitly observed return from the `$d340` private
wrapper and the original byte read at `$da05`. It preserves that returned AX
word as provenance, then executes the selector's local literal pair and its
one CS-store: mode bytes `$03`, `$04`, `$02`, and all other values select,
respectively, AX:DX `$0012:$0050`, `$0014:$00a0`, `$000f:$0140`, and
`$000e:$0028`, before original `DX` is stored to `$4b6e`. It stops at
`$d373 -> $6c52`, the existing overlay adapter transfer. It does not invoke
INT `$91`, manufacture a return, read an on-disk byte as runtime state,
execute the adapter, load an overlay, or make the post-adapter continuation
reachable. Missing or out-of-order observations are rejected.

The encoded caller continuation after that adapter call is independently
preserved as `MillenniumDosPostOverlayAdapterContinuation`. It is explicitly
conditional: the adapter's `RETF` transfer need not return. If it does, the
39 bytes at `$d376..$d39c` (file `+$d276`) hash to
`1df4b30f14434eae3a44463402710bcd1b162200a923c0b9cc1f827faf3763ac`.
They make six direct near calls, in order, to `$d152`, `$4f08`, `$4111`,
`$40af`, `$42b2`, and `$107a`. The prefix then compares the original byte at
`$da05` with `$01`: its encoded equal branch reaches `$d394 → $d1a1`; the
other route calls `$d38f → $d1b5`, short-jumps to `$d397`, and the two paths
converge on raw `PUSH CS`/`POP DS` and two `PUSH CS`/`POP ES` pairs. The parser
validates the whole original executable, span hash, and every near-call target.
It does not claim any call returns, choose the byte value or branch, assign a
meaning to the segment setup, execute a target, or provide native state.

`MillenniumDosPostOverlayContinuationEvaluation` makes this conditional chain
available to a trace-backed runtime without widening that contract. It first
requires an explicit observation that the overlay `RETF` returned. It then
requires six separately ordered observations for the returns from `$d152`,
`$4f08`, `$4111`, `$40af`, `$42b2`, and `$107a`; without the next observation
it stops at that original CALL. Only after all six does it accept an explicitly
observed `$da05` byte. `$01` selects `$d1a1 → $d1a9 → $0124/INT $91`; every
other byte selects `$d1b5 → $d1bd → $0124/INT $91`. The evaluator stops at
the interrupt instruction `$0129`, makes no local writes, and does not claim
the six calls' effects, the private ABI, or a return from the interrupt.

The following 69-byte encoded caller span is separately preserved as
`MillenniumDosPostOverlayAdapterLoop`. It begins at `$d39d`, directly after
the prior segment-setup span, and hashes to
`1bbb4fcc18668021306de1e0014a9baab1f526af1514fa7ce9d1a61780972cf0` in the
same English 54,391-byte `2200AD.EXE`. It has fifteen direct near CALL
encodings to `$446a`, `$5b1f`, `$6178`, `$799c`, `$52f9`, `$7b7f`, `$09e4`,
`$11a4`, `$0b0c`, `$0ea4`, `$0b5b`, `$0ebb`, `$7601`, `$7bcb`, and `$0f05`.
It contains AL tests at `$d3ba` and `$d3de`; the first encoded nonzero route
goes from `$d3bc` to `$d3c6`, while the final encoded zero route goes from
`$d3e0` to `$d3d2` before the existing dispatcher at `$d3e2`. Between them,
the raw instructions load, XOR `$01`, and store the original byte at `$07f9`
at `$d3be/$d3c1/$d3c3`. The parser validates the full executable, complete
span hash, and every direct near target. It neither assumes the adapter or
any call returns, reads the runtime byte, chooses either branch, interprets
the encoded instructions, nor gives any target a host-side effect.

The loop's fall-through target is separately fixed as
`MillenniumDosPostOverlayDispatchPrefix`. The 49 bytes at `$d3e2..$d412`
(file `+$d2e2`, SHA-256
`7abec93ec23f7ca3c4b400e16b9e746da7b0b9a1dd4bec88ba891ef04b322065`) first
compare AL with `$0b` and branch to `$d40e → $11a4`; otherwise they read and
test native byte `$da3a`, compare AL with `$0c`, and encode `$d3f4 → $d570`.
The remaining route subtracts `$3b`, bounds against `$0a`, loads table base
`$2fbf`, and has direct calls `$d40a → $76f1` and `$d40e → $11a4`, each with
an encoded jump back to `$d3d2`. This parser validates the full English
executable, raw span and wrapped CALL targets. It does not supply AL or the
guard byte, select a table item, dereference native state, assume any branch
or call return, or attach an action to a host effect.

`MillenniumDosPostOverlayLoopSession` manually recompiles only these two
hash-locked spans. The active release coordinator may construct it solely
from `MILLENNIUM DOS GX STARTUP BOUNDARY`, after receiving an explicit return
observation for that boundary's second `INT 91h` at `$0129`. Its selected mode
byte is the separately recorded `$d388` observation retained by the admitted
GX trace; AX is retained only in the new return observation and is not given
host meaning. The loop then accepts address-bound call returns, AL values at
the exact encoded tests, and runtime bytes at the exact encoded loads. It
publishes a copy-only checkpoint containing its current typed boundary,
counts, observed action byte, optional table index, and deterministic local
byte effects.

This successor is the no-capability runtime state `MILLENNIUM DOS
POST-OVERLAY LOOP`. It has no SDL input mapping, renderer, audio, device ABI,
handler execution, or automatic advancement. Wrong-state, wrong-address, and
out-of-order observations fail without creating the successor. Its span is
backed by the coordinator-owned, hash-verified English executable admission;
the loop is destroyed before that owner on reset or source revocation. The
current public launch path still cannot reach the preceding GX state until
the driver/title handoff and genuine GX trace exist, so this code is not a
claim that `$d39d` is presently reachable.

The terminal scaled-dispatch boundary now has one narrower typed successor.
`MILLENNIUM DOS TENTH-FUNCTION HANDLER` can be created only while the live
loop is stopped at `$d40a → $76f1`, its recovered index is exactly `9`, and a
separate observation binds that same call address, dispatcher address and
index to handler entry `$7384`. The coordinator does not read the static
table on the observer's behalf, auto-resolve an index, or infer that the call
returned. The successor accepts only address-bound runtime-word,
runtime-byte, call-return, zero-flag and BL observations already defined by
`MillenniumDosTenthFunctionSession`; its checkpoint copies only state,
boundary, loop counts and reconstructed byte effects.

This successor also has no presentation, audio or input capability. Its
span is constructed by the same exact-English-media admission that owns the
post-overlay loop, and member/reset order destroys both borrowers before the
verified backing bytes. Runtime-host source revocation rejects every typed
observation and hides the checkpoint. Because the normal launch path still
cannot genuinely reach the GX predecessor, no test or runtime code fabricates
a positive active transition; exact-media tests independently cover the
owned `$7384` session and wrong-state/revocation gates cover the public path.

The same terminal dispatcher has an independently typed seventh-function
successor. It requires a separate observation binding `$d40a → $76f1`, table
index `6`, and handler `$7521` while the live post-overlay checkpoint already
reports that exact scaled-call boundary and index. The runtime never derives
or fills in the handler address for the observer. The coordinator constructs
`MillenniumDosSeventhFunctionSession` only through the existing exact-English
media owner and accepts only its address-bound runtime-word, runtime-byte,
call-return, and returned-BX observations. Its copy-only checkpoint contains
the typed boundary, terminal/guard facts and reconstructed word effects.

`MILLENNIUM DOS SEVENTH-FUNCTION HANDLER` is a no-capability state: no SDL
input, frame, audio, device ABI, dispatch return, or gameplay meaning is
granted. Reset order destroys the span borrower before its verified
executable owner, and host revocation rejects observations and hides the
checkpoint. The genuine active predecessor is still unavailable, so only the
owned exact-media construction and negative public gates are tested; no
positive active reachability is synthesized.

The complementary DX-nonzero successor is independently bounded before its
first mouse boundary. The static target `$d44b` loads `AL=$08` and its short
jump at `$d44d` lands at `$d41b`, deliberately skipping the adjacent `$d419`
`XOR AX,AX`. The target's 11-byte prefix stores AL through a CS override to
`$2fb2`, restores SP from `$d12c`, then calls `$d423 → $09e4`. That callee
starts with `MOV AX,$0000` and reaches `INT $33` at `$09e7`. The five-byte
four-byte nonzero entry, 11-byte continuation, and five-byte callee prefix have SHA-256
values `92252049901ece1d56c7b17fdd7450ce8ade576650b4f7b032f61dd1e4e59522`,
`7fb9d6276e557976c68a02e9900531347fd95ecbfbd6fc3fa60cd0c176ca5c5d`, and
`d84b931c90a3b7e1baf2a0a6caf2c67fc5834ed6a160750ba6991b77fdb11909`,
respectively. `MillenniumDosStartupNonzeroPathBoundary` validates all three
against the original English executable and rejects mutations. It does not
decide DX, infer AH on entry, call the local routine, supply a return, or
emulate any mouse-interrupt state or result.

The direct follow-up targets are also bounded by original bytes. `$044e`
loads literal `$01`, writes it to `$da05`, and returns. `$0466` sets `DS=CS`,
points `SI` at the verified 16-byte in-image sequence `$00..$07`, `$38..$3f`
at `$0456`,
sets `CX=$0010` and `BL=0`, then reaches `INT $10` at `$0476`; its local loop
back-edge is raw code only. The BIOS interrupt is the first external boundary
for this route. No BIOS behavior, loop iteration, or runtime state is
emulated.

The register setup at that boundary is exact: each iteration moves a byte from
the `$0456` table into `BH`, sets `AX=$1000` (`AH=$10`, `AL=$00`), and invokes
`INT $10` with incrementing `BL` from zero through fifteen. This is the
documented BIOS "set single palette register" operation: `BL` is the palette
register and `BH` its raw color value. Project Eon's narrowly typed SDL-facing
adapter payload therefore exposes the 16 verified `(register, value)` pairs,
but nothing calls it automatically: reaching this routine still depends on
the preceding private-interrupt paths returning, and no BIOS or SDL palette
effect is presumed.

#### Typed Millennium DOS native-process boundary

`MillenniumDosNativeProcess` is the first shared, manually recompiled process
boundary for the supplied English `2200AD.EXE`. It is not an x86 interpreter:
it composes only the existing hash-validated startup and GX evaluators, records
their deterministic little-endian writes in private maps, and yields a typed
boundary before every unresolved interrupt, runtime-byte read, overlay call,
or local call. Every observation names the exact instruction or interrupt
address at which it was obtained. An out-of-order observation, a mismatched
address, an altered executable, or an altered GX overlay fails closed.

The process has two deliberately independent construction points. `startup()`
begins at the verified flat-image entry `$d2b0`, stops first at `INT 91h` site
`$0129`, and can advance through the selector-specific second `INT 91h` only
when both AX returns are supplied explicitly. The selector-one path then stops
at the local return `$0455`; the other recovered path stops at BIOS `INT 10h`
site `$0476`. Only literal writes emitted by the authenticated evaluator become
private runtime bytes.

`post_gx_loader()` separately begins at the already documented post-loader
`$0129` boundary. It can accept the explicit private-INT return, `$da05` read at
`$d349`, GX adapter return `$d373 -> $d376`, six individually ordered near-call
returns at `$d376..$d385`, and the second `$da05` read at `$d388`. It then stops
again at `$0129`. The call-free GX writes are retained only in a private overlay
map. This second factory is a recovery entry, not a transition from `startup()`:
no title handoff, DOS EXEC result, GX load result, call return, input, frame,
audio, or gameplay reachability is inferred.

The process itself holds non-owning read-only spans. The registered
`MillenniumDosNativeProcessAdmission` owner closes that lifetime boundary by
independently hashing the exact English release leaves, retaining private
in-memory backing, destroying the process before that backing, and exposing
only a copy-only static recovery checkpoint plus typed observations. Move and
reset lifetimes and altered GX bytes are tested against genuine supplied
media. The release coordinator now prepares only the `startup()` owner while
admitting the exact English DOS archive and exposes its initial `$0129`
private-INT checkpoint through copy-only runtime diagnostics. No host API can
advance it, and reset/source revocation destroys it before the verified media
owner. This diagnostic preparation does not change the live title session,
broaden its capability manifest, or claim the missing title/driver transition.

#### Main-loop action dispatch

The supplied English `2200AD.EXE` (54,391 bytes, SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`) has
its flat-image entry at loaded `$d2b0`. After the startup calls, the verified
loop at `$d3d2` reaches the wrapped `$0f05` action poll at `$d3db` and tests its returned `AL`: zero repeats the
loop; `$0b` and `$0c` branch to separate native paths; otherwise it subtracts
`$3b`, rejects values `>= $0a`, and passes a zero-based index through an
eight-byte table at `$2fbf` to `$76f0`. This proves an actionable ten-entry
range `$3b..$44` (the PC F1–F10 scan-code range), but does **not** prove what
the handlers mean or how they alter state.

`MillenniumDosGameFlow` validates the exact entry and loop bytes before
exposing those values to preservation tests and inspection. The SDL launcher
does not map F1–F10 into this loop: the title input boundary proves neither the
DOS return nor `2200AD.EXE` startup. No host action invokes a handler, mutates
`2200SAVE.I`, or claims menu/action names. The special `$0b`/`$0c` paths remain
documented but are not host-bound until their input production and state
prerequisites are recovered.

The declarative function map separately names the ten table handler entries
(`$6f9a`, `$71ca`, `$6faa`, `$72f9`, `$7597`, `$7415`, `$7521`, `$7306`,
`$7339`, `$7384`) with the same authenticated `2200AD.EXE` span. These are
verified-static diagnostic records, not function labels, gameplay actions,
hooks, or a claim that any entry is reachable. F8 retains its narrower
trace-gated prefix record alongside its handler identity because only that
local prefix has an evidence-limited consumption model.
Before either CLI or F10 can expose the static dispatch DTO, the native
coordinator resolves all ten IDs through the compiled map and compares their
runtime coordinates to the independently parsed `MillenniumDosGameFlow`
addresses. Missing IDs, malformed rows, image-relative coordinates, or any
address drift suppress the DTO. The comparison still does not establish
reachability or authorize a call; it prevents two preservation descriptions
of the same authenticated bytes from silently diverging.

Project Eon's **host** F10 is explicitly consumed before the title availability
poll and is not an original F10 action. In Original it exposes only output
resolution and aspect ratio; these centre and scale the same recovered frame
without changing source pixels, game logic, runtime state, timing, or saves.
Modern additionally exposes smooth scaling, scanline, renderer-frame and other
explicit renderer-only toggles. Neither panel maps F10 into the original
action-dispatch range.

For the live, hash-admitted English DOS title session only, the Modern F10
diagnostics readout and `--runtime-diagnostics-json` may additionally report
the immutable F1–F10 table address, stride, dispatcher address and handler
entry addresses. The JSON object is named `millennium_dos_static_dispatch`;
its ordered `handlers` records join each raw `$3b..$44` action to the stable
declarative function-map ID and verified handler address while retaining the
flat `handler_addresses` list for schema-v1 consumers. The object declares
`static_only: true`. It is coordinator-owned, revoked with the
title adapter, and has no input observer, handler invocation, runtime-cell,
save, media-byte, or guest-execution surface. It is a preservation navigation
aid for the verified code table—not a control map or evidence that the title
has handed control to `2200AD.EXE`.

`MillenniumDosGameSession` can now retain a non-owning view of the authenticated
English `2200AD.EXE` specifically for offline/runtime-trace observation of the
two special actions. It does not make this path reachable from SDL: the title
return, action poll and native prerequisites are still unrecovered. Every
action observation is bound to the recovered action poll `$0f05`, and every
special-byte observation is address-bound (`$07f9` for raw `$0b`, `$da3a` for raw `$0c`),
so a byte recorded from one native cell cannot be reused as the other action's
prerequisite. Given an explicitly observed native byte, raw `$0b` is revalidated against the exact
dispatcher and `$11a4` handler before recording the one unconditional prefix
write `$07f9 := observed XOR $01`; it stops at its first native helper `$0666`.
The supplied observation, rather than a prior Project Eon trace, is retained as
the pre-write value. Raw `$0c` similarly revalidates its dispatcher and `$d570`
handler, reports whether explicitly observed `$da3a` blocks it or reaches
helper `$6c52`, and records no write. The session owns no game bytes, copies no
media, performs no native call, and never maps these raw action values to SDL
keys. Any altered executable or session without the original byte view is
rejected before an action trace is exposed.

The first table record (raw F1 / `$3b`) is now traced further without assigning
it a game-menu name. Its eight original bytes are
`00 06 09 1b 30 00 9a 6f`, so its handler entry is `$6f9a`. That handler clears
`AX`, calls the common display selector at `$d0c9`, and only then calls
`$771d`. The latter's byte-validated prefix writes runtime selector
`$da1f = 0`, retains `$12cc` at `$da20`, obtains word zero from the embedded
pointer table at `$27c4`, and therefore selects original in-image record
`$12cc`; the observed word is indeed `$12cc`. It selects mode `$07` at
`$75a8`, descriptor `$300f` at `$75a6`, and reaches its first further native
call at `$5b1f`. The selected record begins
`03 00 11 00 00 00 00 00` (notably byte `+02 = $11` and byte `+24 = $00`).
All these F1 setup stores occur after the native `$d0c9` call, whose return and
side effects are not yet emulated. `MillenniumDosGameSession` therefore keeps
an observed `$3b` poll separate from the later setup profile: it exposes the
profile only after an explicit trace observation that this exact `$d0c9` call
returned, and revalidates the complete executable then. Project Eon still
neither creates a host overlay for F1 nor names the handler nor writes any
original executable, archive, or save byte.

The second table record (raw F2 / `$3c`) is `06 0c 09 1b 31 01 ca 71`, with
handler entry `$71ca`. This handler reads its runtime byte at `$da26` and
compares it with `$02`. If the byte is lower, it repeatedly calls `$09fa` and
returns; Project Eon neither fabricates that runtime byte nor pretends the
gate was admitted. Its admitted path at `$71de` writes callback `$7221` to
`$6f98`, records the one-byte selector `$01` at `$6e98`, and makes a word list
at `$6e99`: it begins at original in-image `$1384` and advances by `$00c0` for
each unit calculated from `$da26 - 1`. The code then writes `$08` to `$da1e`
and calls `$0b76`. These are strictly address/value observations, not inferred
names for a menu, records, or action. Project Eon surfaces this gate after F2
in the SDL evidence panel while leaving original media and its unknown runtime
state immutable.

The third table record (raw F3 / `$3d`) is `0c 12 09 1b 32 02 aa 6f`, with
handler entry `$6faa`. It returns if runtime word `$a19e` is nonzero. Only
with that word zero does it inspect runtime word `$da27`; while that word is
zero it calls `$09fa` in the original wait loop. Its admitted setup at `$6fc6`
installs callback `$712a` in `$6f98`, writes mode `$00` to `$6e98`, and begins
its list at `$6e99` from the original far pointer stored at `$0112`. Project
Eon presents these two gates and setup addresses after F3, but does not invent
the runtime values, dereference a host-side replacement list, or assign the
handler a game meaning.

The fourth table record (raw F4 / `$3e`) is `12 18 09 1b 33 03 f9 72`, with
handler entry `$72f9`. It first reads runtime word `$a19e`; a nonzero value
returns immediately. Only when that word is zero does it place `$02` in `AL`
and transfer to `$ba5e`. The recovered common bytes first load `AX=$0005` and
call `$4d2c`, then at `$ba64` write `$07` to `$da13`, call `$9dd5`, at `$ba6c`
write `$09` to `$da1e`, clear `$75a9`, and return at `$ba76`. There is no
pre-call literal write. The first write is reached only when `$4d2c` returns;
the final two are reached only when `$9dd5` returns. These are code-validated
addresses and literal writes only: the calls' effects, their return behavior
for live runtime state, and the cells' meaning are not inferred. Consequently
F4 contributes no private-overlay effect, unlike F8's pre-call store. The
guard is not save-backed: original code at `$a557` contains
`mov cx,[$a19e]; mov word [$a19e],$0000`, but the preceding path branches on
native runtime values. This proves the guard's real producer without proving
that a fresh key event passes it. Project Eon exposes the clear site and the
conditional common-path writes as evidence, but never supplies the guard,
applies F4's writes to its overlay, invokes native code, or mutates
executable/archive/save media.

The fifth table record (raw F5 / `$3f`) is `18 1e 09 1b 34 04 97 75`, with
handler entry `$7597`. It loads `AL=$02`, has **no memory store**, then makes
16-bit near calls to `$be28`, `$0b9d`, `$4bf7`, and `$0b76` before returning.
The original handler's first call target begins `call $52f9`; therefore no
F5-owned pre-call state change exists and the first safe post-call boundary is
the return of an unexecuted native call chain. `$0b9d` begins by comparing
native byte `$07f9` to `$01` and can enter a native input/hardware wait;
`$4bf7` itself immediately calls `$0bd7`; `$0b76` begins with the same
`[$07f9] == $01` comparison. These operand and control-flow facts are not
gameplay semantics and do not justify a host-side overlay effect. Project Eon
displays this immutable evidence after F5 but never executes native calls,
supplies state, or writes the original executable, archive, or save media.

The sixth table record (raw F6 / `$40`) is `1e 24 09 1b 35 05 15 74`, with
handler entry `$7415`. It first returns when runtime word `$a19e` is nonzero.
On the admitted path it clears `AX`, calls `$d0c9`, then calls `$4d2c` with
`AX=$0022` and `$c980`. Only after those native calls return, the handler
snapshots bytes `$75a8` and `$75ae` into its own `$7412` and `$740f` scratch
cells and snapshots word `$75ac` into `$7410`; it then writes literal `$0c`
to `$75a8`, `$00` to `$75ae`, and `$3207` to `$75a6` before calling `$09fa`.
The immediately following `SHR BL,1`/carry branch can repeat that poll.

The immediately adjacent but separately entered `$7455` routine proves the
paired cleanup sequence: it copies `$740f` back to `$75ae`, `$7410` back to
`$75ac`, and `$7412` back to `$75a8`, then makes its first native call to
`$0b0c`. The actual dispatcher/callback edge that reaches `$7455` is not yet
proved; in particular `$3207` is recorded only as a literal word written at
`$75a6`, not dereferenced as a host callback. Therefore neither the temporary
stores nor the cleanup are a safe private-overlay effect. Project Eon exposes
this byte-validated, strict no-overlay boundary and never supplies the
guard/carry, invokes native code, applies the writes, or alters original
executable, archive, or save media.

The seventh table record (raw F7 / `$41`) is `24 2a 09 1b 36 06 21 75`, with
handler entry `$7521`. It returns when runtime word `$a19e` is nonzero. On its
admitted path it loads `AL=$1d`, calls `$4d2c`, calls `$073c` with
`AX=$0612`, then calls `$0666` with `AX=$012a`. The recovered bytes read native
runtime words `$da17`, `$da18`, `$da27`, `$da26`, `$da35`, and `$da37`, route
them through repeated helpers `$06dc` and `$05ce`, use helper `$077e` with
literal `AL=$2e` and later `AL=$25`, and end with calls `$0b9d` and `$4bf7`.
These are code-verified operands and control-flow targets, not assigned game
semantics. Project Eon surfaces the immutable F7 gate in its SDL evidence
panel; it never supplies the guard/runtime words, executes the helpers, or
writes original game media or saves.

The English DOS main-loop's first special action is separately bounded. Its
action `$0b` comparison/dispatch slice at `$d3e2` (43 bytes at file
`+$d2e2`, SHA-256
`1e4e43aad1a2507aa7f85189022063db0f0cb481d267ef79789a447c3e184d62`)
branches to `CALL $11a4` at `$d40e`. The handler prefix `$11a4..$11b9` (22
bytes at `+$10a4`, SHA-256
`2cd76e49776b940065ecb01418394984a9e03a6d6a6fc161c218f450faac1ed5`)
reads explicitly observed native byte `$07f9`, chooses AX `$018f` only when
that byte is zero (otherwise `$018e`), XORs the byte with one, and reaches
opaque `CALL $0666`. The evaluator reports that prefix only: it does not
invent an initial byte, invoke `$0666`, assume its return, or persist the
toggle to original executable/save media. The supplied Spanish executable
shares the handler prefix but has no proven matching action dispatch, so this
English-only route deliberately rejects it.

The shared English helper at `$0666..$0681` is separately bounded at file
`+$0566`: 28 bytes, `1e 56 50 2e 8e 1e 16 01 2e c6 06 c8 05 00 d1 e0 8b f0
ad 8b f0 e8 79 ff 58 5e 1f c3`, SHA-256
`8dc7586f3809a14f3ed6acd601cd42486841adb9d9cb09d3e9b1ed727329e485`.
It preserves `DS`, `SI`, and `AX`; loads `DS` through native `CS:$0116`;
writes literal zero to `CS:$05c8`; doubles caller-provided `AX`; reaches
`LODSW` at `$0678`; and stops at `CALL $05f7` at `$067b`.
`MillenniumDosSharedHelperPrefix` accepts only the supplied English executable
(full SHA-256 `427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`).
It does not invent the segment, dereference the selected table word, invoke
`$05f7`, assume a return, or commit any instruction's provenance to host or
original game state.

`MillenniumDosGameSession` can advance this helper only after its exact
observed `$0b` prefix, using that handler's proven selected AX value. The
advance is one-shot, rehashes the executable through the evaluator, and stops
at the same `$05f7` boundary. It cannot be entered before `$0b`, repeated, or
reached from an SDL key; it supplies neither the original segment/table word
nor any helper result or game-state effect.

An additional typed native session now owns the exact `$0666` helper itself
without weakening that game-session gate. It begins with the caller-supplied
`AX`, records literal `CS:$05c8 := 0`, explicitly observes the segment word at
`$0669` / `CS:$0116`, and requires a far word observation at `$0678` using
that exact segment and the instruction-defined offset `(AX << 1) & $ffff`.
The selected word is retained as the resulting `SI`; it is never sourced from
host or synthetic memory. The session then requires the exact `$067b -> $05f7`
call-return pair (`$067e`) before reaching RET `$0681`. Effects of `$05f7`,
the table contents, and all gameplay meaning remain external observations.

Active runtime ownership is deliberately narrower than the helper's many
static callers. It admits the helper only while the owned F7 session is at its
exact `$7537 -> $0666` call boundary with known `AX=$012a`, and requires a
nonzero ordered entry observation. Runtime word, far-word, and internal
`$067b -> $05f7` return observations then advance the typed child session.
Only after RET `$0681` may a later explicit return to the statically proven
F7 destination `$753a` complete that parent call. The child checkpoint retains
entry, selected offset, effects, and optional return by value. Reset and source
revocation destroy or hide both child ownership and checkpoint. The separate
`$11b7` special-action caller remains static evidence until its parent action
has an active runtime session; it is not silently substituted for F7.

That parent is now admitted only from the active post-overlay loop's exact
raw-`$0b` dispatch boundary `$d40e -> $11a4`. A single typed entry observation
supplies the original `$07f9` byte, explicit caller `AX`, exact helper edge
`$11b7 -> $0666`, and a nonzero sequence. The coordinator replays the existing
hash-bound game-session prefix and rejects `AX` unless it equals the prefix's
instruction-selected `$018f` or `$018e`. The helper RET must explicitly name
`$0681 -> $11ba`; the following call-free decrement loop is retained only as
the proven parent RET `$11c1`, whose later explicit return must be `$d411`.
Only then does the post-overlay owner resume its four-call poll tail. Reset and
source revocation discard the parent, child, sequences, and checkpoints.

The next English-only special action `$0c` is likewise bounded. Its admission
prefix `$d3e8..$d3f6` (15 bytes at `+$d2e8`, SHA-256
`e59faad9b95521837b340ff56ef032cb140327bfabb0b39be32d01bb9c05bda3`)
reads explicitly observed byte `$da3a`: nonzero returns to `$d3d2`; zero
matches action `$0c` and calls `$d570`. The seven bytes at `$d570` (file
`+$d470`, SHA-256
`f266d52e554a2e85147994b34eb69e7678cd9339fda1b99206c18fc05361232b`)
load `AX=$000d` and stop at `CALL $6c52`. No native byte is written before
that call; the evaluator neither invokes it nor assumes its return.

The action-`$0c` route can now own the independently authenticated GX adapter
without manufacturing the missing runtime values. Admission requires the live
post-overlay dispatch boundary `$d3f4 -> $d570`, an explicit zero observation
at `$da3a`, the exact `$d573 -> $6c52` call, explicit `AX=$000d`, an explicit
nonzero code segment, and monotonically increasing observation sequence. The
typed adapter then observes the word read at `$6c60` from `$0118`, the exact
far transfer at `$6c68` to that observed segment and offset zero, and the
overlay's actual `RETF` at offset `$0014` back to `CS:$6c69`. It finally
requires `$6c72 -> $d576` and the handler's `$d576 -> $d3f7` return before the
post-overlay poll owner resumes. The adapter span remains the already recorded
33 bytes at file `+$6b52`, SHA-256
`b34e5abf8ecd790fce3e7a032d7a7fcacc073d03909e98fd33f9503113e3ad87`;
construction is additionally gated by the complete English `2200AD.EXE`
identity. Checkpoints contain only addresses, observed scalar values, state,
and sequence records. They contain no original bytes or inferred overlay
result. Reset, source revocation, and replacement admission destroy the parent
and adapter sessions together. The runtime still cannot enter this route from
SDL until the existing title-to-post-overlay evidence boundary is closed.

The ninth table record (raw F9 / `$43`) is `30 36 09 1b 38 08 39 73`, with
handler entry `$7339`. It returns when native runtime word `$a19e` is nonzero.
Its admitted path clears AX and calls `$d0c9`, clears `$da30`, loads `AL=$02`,
sets code-local byte `$6e2f` to `$01`, and clears `$dad7`. If `$da39` is
nonzero it calls `$7b47`. It then loops through verified F8 preflight `$731a`
while `$da06` is below `$09`; otherwise it clears `$6e2f`, conditionally calls
`$7a9e` when `$da09` is zero, then calls flat-image target `$14124`. These are
strict code operands and branch facts, not inferred gameplay semantics.
Project Eon exposes the F9 evidence immutably; it does not supply native
runtime bytes, invoke native calls, execute the loop, or write archives, saves,
or other original media.

The eighth table record (raw F8 / `$42`) is `2a 30 09 1b 37 07 06 73`, with
handler entry `$7306`. It clears native runtime byte `$da30`, loads `AL=$02`,
and calls local preflight `$731a`. That preflight reads `$da39`: its nonzero
path calls `$7b47`; its other path reads `$da0a`, returns if it is zero, or
decrements it, applies `XLAT` through `BX=$db4b`, and jumps to `$7948`. Back
in the handler, `$09fa` is called and the following `SHR BL,1` carry branch
can repeat that call. F8's `C6 06 30 DA 00` write is the first F-key effect
that Project Eon reconstructs, because its byte-level semantics are fully
established and it executes before any runtime-dependent branch or call: its
private runtime overlay changes `$da30` to zero. The overlay begins with
`$da30` **unknown**, rather than deriving an initial value from `2200SAVE.I`;
a second F8 therefore records `0 -> 0`. It is not a mutable view of the
original COM image or a save serializer, and is never exported. The later
preflight/call path has no inferred initial state: `$da39`, `$da0a`, and `BL`
must remain explicit external observations and native helper semantics remain
unimplemented.
The preceding `$731a..$7338` preflight is independently hash-locked (31 bytes
at file `+0x721a`, SHA-256
`71c2c4189e66104aea08d4f7040e9d6bc873eb6717607eed30cf61ce27f5ac2e`).
Its two runtime bytes are explicit evaluator inputs: nonzero `$da39` reaches
opaque helper `$7b47`; zero `$da39` with zero `$da0a` returns; otherwise it
decrements `$da0a`, uses the decremented `AL` as an `XLAT` index through
native-memory table `$db4b`, then jumps to `$7948`. The table lies beyond the
COM image, so Project Eon records only its original address and index—never a
fabricated table byte or jump effect.
When a caller has independently observed that external XLAT result, the exact
local `$7948..$7967` prefix is now also hash-locked (32 bytes at file
`+0x7848`, SHA-256
`c52d83152fef75a81d8956b76e7c6931ced4de6a579f4233faf8a28c3cdc72c9`).
It clears `$da09`, writes the explicit translated `AL` to `$da06`, and uses it
as a bounded index into the ten-word in-image selector table at `$78f4`
(20 bytes at `+0x77f4`, SHA-256
`c42e986a183a46d7b4cdf7787766e5f81446b444180e0cf34d9fa5f4b8d50a0d`).
The resulting original pointer and the `$6e2f` zero/nonzero gate are exposed
as facts only. No XLAT result is invented, no selected pointer is executed,
and the following pointer-controlled interpreter remains a boundary.
The first local branch in that interpreter is now separately hash-locked
without crossing a native call. Its `$7948..$799b` bytes (84 bytes at file
`+0x7848`, SHA-256
`99267e09fea1f7d3227b49b3c80a2eacf6673df542bb063da7c54ce87df8a666`),
the ten-word selector at `$78f4` (the hash above), and the selected-record
bank `$77f8..$788f` (152 bytes at `+0x76f8`, SHA-256
`53315644dbe9478d9e8b919d3958cf64cac95260fd3f89b600d92275f97e089c`)
show the following bounded control flow. An explicitly observed nonzero byte
at `$6e2f` branches to `$799a`, restores DS, and returns at `$799b` before any
selected record byte is read. An explicitly observed zero byte selects one of
the ten original pointers, reads its first byte and following word as local
register facts, then reads the record's byte `+3` as a first-list count. All
ten supplied records have a nonzero count; their first list byte reaches the
opaque `CALL $7924` at `$797f`. For example, selected index `$02` identifies
pointer `$7815` and raw prefix `04 73 28 01 84`, reaching that boundary with
the first list byte `$84`. The byte/word fields are not assigned gameplay
semantics, `$7924` is never invoked, no return from it is assumed, and no
later record bytes, calls, runtime writes, saves, or original media are
executed or changed. The same three hashes are present in the supplied
Spanish FAT12 `2200AD.EXE`; this proves only the narrow shared byte path, not
the unrecovered Spanish executable ABI.
After a preflight return, the eight original bytes at `$7312` (file
`+0x7212`, SHA-256
`2bf85a49d14034fb5562af6188745810721fd42e495877464d04f69783525a0a`) are
also modeled as a bounded local trace: `CALL $09fa`, `SHR BL,1`, `JC $7312`,
`RET`. The evaluator requires explicit low-byte `BL` returns from the opaque
`$09fa` helper, records each shift, repeats only when the shifted-out bit is
one, and rejects an unterminated sequence. It does not call `$09fa`, invent a
preflight result, or write original runtime/save media.

`MillenniumDosGameSession` can retain one ordered, evidence-only F8 prefix
when a capture has already supplied the action at poll `$0f05`, bytes
`$da39`/`$da0a`, and—only after the external XLAT boundary—the translated
`AL` plus `$6e2f` gate byte. It records the proven local writes in order:
the handler's `$da30 := 0`, the conditional `$da0a` decrement, then
`$da09 := 0` and `$da06 := AL`. Each stage rejects detached or out-of-order
observations and stops at `$7b47`, local return, XLAT, or `$7924`; no current
launcher route constructs this session, maps an SDL key to F8, or uses these
facts to enter `2200AD.EXE`. This is a capture-consumption boundary for a
future fully admitted game trace, not a game-input implementation.

For the distinct local-return arm of `$731a` (`$da39 = 0`, `$da0a = 0`), that
same session may additionally consume one ordered sequence of observed `$09fa`
BL returns and execute only the hash-validated `$7312` `SHR`/`JC` tail. It is
one-shot and rejects a table-jump/opaque-helper preflight or an unterminated
sequence; neither `$09fa` nor BL is emulated or synthesized.

The tenth table record (raw F10 / `$44`) is `36 3c 09 1b 39 09 84 73`, with
handler entry `$7384`. It returns when native runtime word `$a19e` is nonzero.
Its admitted path clears AX and calls `$d0c9`, clears `$da30`, loads `AL=$02`,
clears `$dad7`, and sets code-local byte `$6e2f` to `$01`. If `$da39` is
nonzero it calls `$7b47`. It loops through the verified F8 preflight `$731a`
while `$da06` is below `$02`; it then clears `$6e2f`, conditionally calls
`$7a9d` when `$da09` is zero, and reaches direct calls `$4140`, `$7bcb`, and
`$a2a0`. A final poll at `$09fa` depends on `$da41` and can repeat according
to `SHR BL,1`/carry before call `$4111`. These are exact code operands and
control-flow targets only. Project Eon presents this immutable F10 trace; it
does not provide native guard bytes, execute calls, run the polling loop, or
write original archives, saves, or executable media.

### Millennium DOS GX startup-record boundary

The original English `2200GX.EXE` contains a further bounded continuation of
the already validated `2200AD.EXE` startup selector. The four genuine overlay
entries `+$0090`, `+$0097`, `+$009f`, and `+$00a7` select source records at
`+$0070`, `+$0080`, `+$0078`, and `+$0088`, respectively. The exact 94-byte
entry span `+$0090..+$00ed` has SHA-256
`8d412472415d513482b5c70198bb1aa04fa0d25798dd5f4b40b262151c489736`; its
four contiguous eight-byte source records have SHA-256
`1b92e08f514f6b6dee4683550e2d9363d39e6ed0375ac9c9e2b652754326965f`.

All four paths converge at `+$00b2`. The original code copies four words from
the selected source to `+$0065`, stores the low byte of the final copied word
at `+$006d`, then updates words at `+$00f4`, `+$00f0`, and `+$00f2` through
the encoded wrap masks before storing literal `$47ea` at `+$005c` and
returning. `MillenniumDosGxOverlayStartupRecordEvidence` accepts only the
full supplied GX executable and the independently hash-identified caller
selector. `MillenniumDosGxOverlayStartupEvaluation` executes this exact,
call-free suffix in a transient overlay only after explicit observations
establish both the preceding private-wrapper return and the adapter's `RETF`
return. It preserves the selected source record, every little-endian word
copy, the final-byte store, and the three masked-state results
`+$00f4=$00f4`, `+$00f0=$00f2`, and `+$00f2=$00f6`; the `$000f` entry's prior
`+$005a=$b800` write is recorded too. The encoded `RET`/`RETF` chain reaches
caller return site `$d376`. No missing return is fabricated, and all writes
remain offsets in a disposable host overlay: no original executable, archive,
save, screen, layout, resource, or display state is modified or inferred.

`MillenniumDosGxStartupSession` exposes the same suffix to runtime code only
through ordered trace observations: a private-return AX word, the original
`$da05` byte, then a separately observed adapter return. It begins at boundary
`$0129`, stops at adapter call `$d373`, and reaches `$d376` only after the
last observation. From there it composes the independently hash-locked caller
continuation: six individually observed local-call returns advance to its
second `$da05` read at `$d388`, and a separately observed byte selects the
existing `$d1a1` or `$d1b5` private-INT boundary at `$0129`. The second byte
is never inferred from the earlier selector because the original rereads it
after opaque calls. The public overlay view retains only bytes written by the
verified evaluator. Its admission object privately owns exact in-memory copies
of both verified leaves for the lifetime of the span-based session; these are
never exposed, installed, written, or persisted. Out-of-order or repeated
observations are rejected, and the session neither starts from the launcher
nor emulates DOS, private calls, overlay loading, or a title handoff.

An external capture can now document exactly this boundary with adapter
`millennium-dos-en-gx-startup-v2`. Its event stream is restricted to ten
ordered records: the `$0129` private-return observation (including, but not
interpreting, AX), the `$d349` `$da05` read, the GX `+$00ed` `RETF` to
`$d376`, returns from each of the six call sites `$d376` through `$d385`, and
the independently observed `$d388` `$da05` read. Both observed byte values
are opaque lowercase hexadecimal provenance. The trace parser requires the
exact clean English DOS outer-release hash and reports recovery-map
boundaries. A separate admission helper accepts *only* that complete grammar
after its caller has independently passed `validate_reference_trace` (which
pins the manifest, event-file size/hash, and source release). It then creates
a fresh transient session and feeds the three already-validated raw values in
their recorded order. It rejects partial, reordered, malformed, altered-media,
or out-of-order input. This is not emulator replay: it executes no original
instruction, DOS service, private interrupt, or opaque local callee, and it
stops at the second `$0129` private-INT boundary without drawing a frame or
starting a title/game session.

The generic trace validator and the GX runtime gate are deliberately separate
checks. The gate rehashes the event file after validation, reopens the pinned
outer archive through `VerifiedReleaseMedia`, and extracts only the two
hash-addressed executable leaves required by the call-free suffix. Private,
destruction-ordered backing buffers close the session-span lifetime inside the
admission result; no original bytes or capture path cross its public API. The
standalone gate does not acquire or replace the coordinator's active release
and cannot publish a runtime session, input route, frame, audio route, title
handoff, or game state. A changed event file, archive, adapter, or release
identity fails closed at this second boundary.

The release coordinator also defines an engine-owned successor state,
`MILLENNIUM DOS GX STARTUP BOUNDARY`. It can be entered only when this strict
trace names the exact active English DOS release and the live session has
already reached `MILLENNIUM DOS TITLE HANDOFF BOUNDARY`. Publication is
atomic: failure preserves the handoff state; success owns the complete trace
admission and exposes only its terminal state, six observed local returns,
the `$d376`/`$0129` boundaries, and reconstructed sparse overlay writes.
Reset and source revocation destroy it before another release is admitted.
The state admits no host input, presentation, or audio. The current English
path still stops at the independently unresolved sound-driver ABI, so this
successor cannot yet be reached honestly from CLI or SDL; the trace is never
preloaded and never substitutes for that missing handoff.

### Millennium DOS GX dispatcher slot 13 boundary

The already hash-identified `2200GX.EXE` dispatcher table maps raw selector
slot 13 to `+$08d0`. Its exact 148-byte handler span, `+$08d0..+$0963`, has
SHA-256 `afd0e53d6588f8576da75c48155d63b8f1b2380f02c9d2adfc65a27e78e25ee0`.
It saves ES/DI/DS/SI, contains direct near calls at `+$08d4`, `+$08d7`,
`+$08fd`, `+$093d`, `+$0940`, `+$094f`, and `+$0952`, and encodes zero stores
to words `+$00f0`, `+$00f2`, and `+$00f4`. A literal `CMP AL,$20` branches to
`+$090b`; an independently native word flag branches at `+$0909` to `+$0910`,
while its fall-through is the local return. The
other literal route advances/wraps encoded state offsets, makes more direct
calls, and uses a short back edge at `+$0962` to `+$08fc`.

`MillenniumDosGxOverlayDispatch13Evidence` accepts only the full known GX
SHA-256 and the independently verified dispatcher table. It exposes these
instruction operands and bounds as a conditional static trace; it does not
choose dispatcher input, supply call returns or branch values, interpret the
stored words, or execute/write any original media. The evidence is available
to both Original and Modern presentation modes without altering Original
semantics.

### Millennium Spanish DOS floppy evidence

The original-media selector is separate from Project Eon's launcher locale:
`--release-language es` with `--game millennium --platform dos` selects only
this FAT12 edition. It never maps a Spanish UI locale onto English media or
falls back to English if the Spanish hash is absent. When both DOS editions
are installed and no original language is selected, English is the documented
default only when it identifies one outer release for that already selected
DOS platform; Spanish remains an explicit hash-bound `--release-language es`
choice. If one language identifies multiple outer containers,
`--release-sha256` is required.

### Launcher platform-card admission

Platform cards are driven only by the scanner's hash-verified
`ReleaseArchive` identities. A platform with no such identity is visibly
unavailable and cannot advance or launch. English is an automatic choice only
when it resolves to one exact outer SHA-256. Otherwise each verified outer
container receives its own release card, labelled with language and a short
identity hash; direct CLI launch requires `--release-sha256`. This keeps an
unrecognised, incomplete, or ambiguous real-media candidate from becoming an
implicit Atari ST (or other platform) fallback or a scan-order selection.

After CLI or card resolution, the launcher first constructs one immutable
`LaunchRequest` whose game, platform, language, and outer SHA-256 have all
been copied from the same resolved `ReleaseArchive`. Every runtime loader then
receives that resolved archive object directly as part of the same session
admission result. It does not search the incrementally scanned release list a
second time by language, platform, or hash. Modern-pack admission and F10
diagnostics use the same resolver. The identity therefore stays fixed from
card selection through Original/Modern resource reads, bootstrap/opening
construction, and provenance display.

Mouse and touch cards share this exact admission route. On iPadOS an
`SDL_EVENT_FINGER_DOWN` normalized coordinate is converted through the active
letterboxed renderer before the same game/platform/release/profile handler is
called. SDL's compatibility touch-mouse events are ignored, so one physical
tap cannot advance two card pages. Touch never bypasses media hashing, release
selection, or the Original/Modern profile choice.

Every verified card displays its explicit recovery coverage: **Recovered
startup**, **Recovered opening**, or **Bootstrap only**, rather than implying
full runtime parity. The label preserves exact-media admission while making the
next unsupported native-API/callback boundary visible before a user enters the
profile flow; it does not disable, replace, or emulate the documented bounded
session.

The same selector is strict for read-only provenance inspection: `--inspect
--game millennium --platform dos --release-language es` reports only the
Spanish hash-verified release. If that exact identity is absent, inspection
returns no verified release rather than listing a sibling language as fallback.

The first Modern pack profile is explicitly English-only. Combining
`--modern-pack` with `--release-language es` is rejected before scanner, SDL,
or external-pack I/O. This prevents an enhanced English title image from
being used as a fallback for Spanish pixels; a Spanish pack needs its own
source-release and renderer mapping evidence.

The verified Spanish outer archive contains one 737,280-byte FAT12 image
(SHA-256 `1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d`).
Its 39 root entries include distinct `TITLE.LIB` (18,998 bytes, SHA-256
`30d6ccb95e7f501d59e72fc2e34583302116bd88f6eceaae989f6ad986ef7f19`) and
`2200AD4.BIN` (13,254 bytes, SHA-256
`8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31`).

The same native LIB reader finds 38 resources at directory `$486e`. `P00`
starts at `$000006`, has 10,555 bytes, and decodes to a 320×200 indexed frame.
Its index bytes match the supplied English release (`85ec…f4ee`), while the
Spanish palette/translation produces its own RGBA SHA-256
`667e297e1cd2860fa5dd6b10749d3af7859dad0844408a32a4d04a682153bc92`.
The reader therefore retains the release's actual palette rather than
substituting an English one.

Spanish `2200AD4.BIN` has its 41 NUL-terminated celestial labels at `$03db`,
not the English `$03d2`; Project Eon reads the media bytes at the observed
layout and preserves labels such as `Tierra ` and `Asteroides ` unchanged.
The live FAT12 `MILL.BAT` is the only standalone launcher documentation in the
recognised corpus: 437 bytes at first disk offset `$2c400`, SHA-256
`1fbb8246d496a6b3a35759a917ef7ae7ba36487de73104f2df81f5a1f8d9f474`. Its
verbatim original text describes `IBM`, `IBM e`, `IBM m`, `TANDY`, and `EGA320`
launch choices, corroborating the byte-validated driver request chain but not
any gameplay control. Two apparent `README` directory-like records at raw
`$38240` and `$3bc40` are allocation slack, not live FAT12 entries, and are
explicitly excluded. The actual Spanish launcher evidence is `IBM.COM` (1,587 bytes, SHA-256
`84b7d158c770117aeaa07cb5ea2e7ed4a6bcc288d6b352d82569ff4d97b2fda9`). Its
hash-locked caller `$023d..$0252` first loads literal `TITLES.EXE` from
`$071d`, calls local `$0339`, then conditionally reaches the second literal
`2200ad.exe` at `$0728` and calls that same local callee. The 48-byte callee
`$0339..$0368` has SHA-256
`c2f5b915a0fbbc7a25d8a3f4c0e5fcc97eb197d44048eaff53e2046eb6e7c32c`; the
Spanish FAT12 targets are independently hash-identified as `TITLES.EXE`
`02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7` and
`2200AD.EXE` `9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6`.
The local callee has now been decoded through its complete 48-byte span. It
inherits the caller's `DS:DX` filename, establishes `ES:BX = CS:$0708`, then
issues `INT 21h` with `AX=$4b00`; after restoring its segment/stack setup, its
carry branch at `$0362` targets `$0369`, skipping the local `AH=$4d` child
status request and `RET` at `$0368`. These are register and control-flow
facts anchored in the original COM bytes, not an emulation contract: Project
Eon does not invoke either DOS service, supply a carry or AL result, assume a
child return, or run either target's unrecovered ABI.

The three exact Spanish FAT12 programs (`IBM.COM`, `TITLES.EXE`, and
`2200AD.EXE`) now have a joint byte-complete linear candidate report. Its
external report SHA-256 is
`9d9834ecf9acc62877e4d757d1c0ba1b87d9045fa7f918238f7d8d00171bfd61` (29,513
lines). Each entry uses the COM-style `$0100` candidate origin, but the report
does not choose DOS/private/BIOS return values, child status, code/data
classification, input semantics, or a game state.

Spanish `TITLES.EXE` is separately accepted only at its own SHA-256 above. Its
own bytes retain entry `$1b80`, private wrapper `$0122`, and post-title
`$1968 -> $1931`: five AX=`$0013` private calls followed by helper `$1917`.
The exact Spanish binary also contains one unique `INT $21/AH=$06/DL=$ff`
console-availability poll at `$0d0a`; its `AND AL,AL` / `JNZ` path enters the
same local exit at `$1c54`. Eon therefore permits a host key event to record
this strictly availability-only Spanish title hand-off. It neither assigns a
DOS character nor executes the following private driver, DOS return, IBM.COM
child status, or `2200AD.EXE`. This locks a real local input/control path
without substituting English resources, drivers, ABI effects, or frames.

`MillenniumDosSpanishTitlePresentationEvidence` separately binds the visible
Spanish title to this same executable identity rather than merely finding a
similarly named library entry. The exact `$1c14..$1c1f` selection bytes set
AX to zero, call `$1725`, then call the local codec path `$1004` and title
transition `$1941`. They are bound to the full Spanish `TITLE.LIB` SHA-256
`30d6ccb95e7f501d59e72fc2e34583302116bd88f6eceaae989f6ad986ef7f19` and its
`P00` resource at `+$000006`, 10,555 bytes, SHA-256
`91c315133e58634d7327c7d3a3e95ecaa035580200f609f161db6b044261b43b`.
This proves the source provenance of the already-rendered original Spanish
title only; it does not execute codec/transition calls, assign cadence, or
cross any private-driver boundary.

Spanish `2200AD.EXE` has a separately recovered COM startup prefix, not an
English substitute. Its entry preserves `DS=CS` and `ES=CS`, then jumps to
`$d2cd`. The next 70 original bytes are SHA-256
`acbfcacc4cfac948944e42181f2fe0dfec11b9ab2c9b79b8aff79d958c5469c6`: they
set `SS=CS`, `SP=$da00`, prepare `AX=$001f` and `ES:BX=CS:$d1bb`, then call
the wrapped private entry `$0124`. If that original call returns, its AL is
compared with `$01`: the equal route calls `$d1be`, while the other route
calls `$d1d2`. The original word store at `$d14a` is recorded as a code
operand only. Project Eon supplies neither the private return nor AL, takes
neither branch, and creates no Spanish game state or English fallback.

Both resulting local callees are now bounded in the Spanish executable. The
AL-equal target `$d1be..$d1d1` is 20 bytes, SHA-256
`fdfc8f02550ee226dea27b1ac0204d1ead083c9d5585e18103bfe67435f0a5bb`: it
prepares `AX=$0004`, `ES:BX=CS:$d1bc`, calls private `$0124`, then local
`$044e`, and only after both returns stores literal `$01` at `$da05`. The
other target `$d1d2..$d1ed` is 28 bytes, SHA-256
`6b8180c8f3b01e1f8810b2132756486dc761aee980949643129eeb53f6e86472`: it
prepares the same AX/ES:BX pair, calls `$0124` and `$0466`, then reads `$da05`
and compares it with `$02`; only its equal route stores `$b800` at `$0107`.
The two follow-up routines, private return values, and predicates are still
unrecovered. Thus neither branch becomes a presentation or simulation path.

The follow-ups are distinct Spanish byte evidence. `$044e..$0455` is eight
bytes, SHA-256 `38889279a8b89e0e600bb25298015ccd8aadc09ea3858a1790097b3f7ff4ea8f`:
it writes literal `$01` to `$da05` then returns. `$0466..$047c` is 23 bytes,
SHA-256 `b17db26fa4fa8b7307fb767ff98351bd6dcca202829dd2d9348ff4991942d779`.
It initializes `CX` to 16 before a local `LOOP` back edge over the in-image
table `$0456..$0465`
(`00 01 02 03 04 05 06 07 38 39 3a 3b 3c 3d 3e 3f`, SHA-256
`ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a`), makes
`INT $10` request with `AX=$1000`, increments `BL`, and returns. The external
interrupt's register effects are unrecovered, so the initial `CX` value does
not establish a runtime request count. This is a static palette-request trace,
not proof of the startup branch or authorization for a host palette mutation
or screen.

The Spanish FAT12 image also supplies its own `EGA640.BIN` (4,630 bytes,
SHA-256 `ef031b0b6e720ab2dafc1eb6373ddb76e0ff15f7b59ac785265c5136be153daf`)
and `MCGA.BIN` (4,346 bytes, SHA-256
`3fb76b2ccccffc304b0525cd410b940bbb61e3d1a7a90340d72e5683d7f0211d`).
Both are parsed only after these Spanish identities and retain their own
function-$06 and function-$13 dispatch targets. Matching offsets are evidence
of local code structure, not permission to load an English driver or execute
the private ABI.

The English DOS `TITLE.LIB` (18,907 bytes, SHA-256
`6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678`)
and `GX.LIB` (312,748 bytes, SHA-256
`4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f`)
share a verified banked container. Its six-byte header stores a little-endian
entry count, 16-bit directory offset, and 64-KiB bank byte. Each 12-byte entry
stores a 16-bit asset offset, bank byte, reserved zero byte, and NUL-padded
eight-character name. `TITLE.LIB` has 38 entries at directory `$4813`;
`GX.LIB` has 180 at `$4bd3c`. Native parsing rejects duplicate names,
non-monotonic resources, invalid padding/flags, and any range outside the
directory boundary. The English DOS gameplay-canvas reader additionally
requires the full `GX.LIB` leaf hash before it reads IMG00/IMG01, so matching
resource names or image prefixes alone cannot substitute a different bank.
The catalogue reader now visits all 180 resources only after that full-leaf
check. It retains each original name, range, leaf hash and—where the bounded
codec-2 decoder accepts it—dimensions, compressed-span length and decoded
pixel hash. A decoder rejection is recorded as an explicit per-resource format
boundary; it does not select an alternate codec, fabricate pixels, or infer a
screen role. In the verified English leaf the sole current boundary is `IMG19`
(resource 25, library `+$1b8dd`, 1,398 bytes, SHA-256
`e86a92133716dc7a54cc4d113a72af25d307c0e338bf77491205d19493403838`): its
codec-2 run would overrun the strict output extent by one pixel. Static
disassembly of the original reader `$1390..$14dc` confirms that the `$e` run
path adds two to `CX`, subtracts `CX` from remaining `DX`, then executes
`REP STOSB` without an underflow guard. Thus this record reaches an original
destination-buffer-overflow boundary rather than proving a safe clipped image
or an alternate format. The resulting inventory is diagnostics-only and
discards decoded pixel buffers after hashing them.
The single-entry English `LAST.LIB` is likewise hash-locked before its `last`
bitmap and palette are decoded. Its directory shape and bitmap profile are
additional bounded-format checks, never a substitute for original leaf identity.

The English `2200AD4.BIN` static-data file (12,494 bytes, SHA-256
`1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d`)
contains a NUL-terminated celestial-label table at file `$03d2`. The native
reader preserves all 41 labels and their exact byte offsets, from `Inner
System` through `Asteroids `. It does not trim the original trailing spaces or
invent a mapping from those labels to mutable simulation records: the loaded
file proves this immutable display table, not the full game-state layout.

`2200AD.EXE` also contains a hash-locked static request for this exact file:
CALL `$d332 → $101a`, source name `$100d` (`2200AD4.BIN`, 12 bytes,
SHA-256 `91032791cbe9e4cfaa88d2f3d9d4882e58dd66ccfbc8a0c457af21dfcefd63ae`).
The 39-byte loader `$101a..$1040` (SHA-256
`d81719b0293c15ad5edbc5c816feb0c44e78abdde749473e5b5795848e4c86cb`)
uses original DOS open/read/close wrappers. DOS results, the destination
segment/buffer, and all returns remain unmodelled preservation boundaries.

The same data file begins with a verified 435-entry, 16-bit static-text
pointer table ending at `$0365`. It maps to 434 distinct raw records in the
English release (one target is intentionally shared) and is not target-sorted.
Project Eon preserves pointer order and raw record boundaries without assigning
meaning to the native control bytes. Each boundary is retained as an original
offset, length and SHA-256 rather than a copied byte vector; the supplied
`2200AD4.BIN` remains the only byte source. The exact cross-edition evidence
is in [the static-text report](generated/millennium-dos-static-text.md).

`MillenniumDosStaticDataEvidence` now admits this diagnostic only for the two
complete, hash-recognised `2200AD4.BIN` leaves: English (12,494 bytes,
SHA-256 `1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d`)
and Spanish (13,254 bytes, SHA-256
`8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31`). Both
have 435 pointers and 434 raw record extents; their real celestial-table
sources remain separately located at `$03d2` and `$03db`. Inspection reports
the exact per-edition topology anchors plus the five
hash-locked original control-text records for each edition as provenance only.
It neither binds their printable literals to SDL input nor reads a catalog as
mutable state, UI layout, or a DOS runtime result.

Five parallel English/Spanish pointer records preserve genuine control-related
**text** without proving a host binding: indices 271 (`left button / space` /
`boton / espacio`), 350 (`press space bar to continue...` / `pulsa espacio para
continuar..`), 390 (`press left button to continue...` / `pulsa el boton
izquierdo para seguir`), 398 (`MOUSE MODE` / `MODO RATON`), and 399
(`KEYBOARD MODE` / `MODO TECLADO`). The English source spans are `$12a7`,
`$1d88`, `$2aef`, `$2bcd`, and `$2be3`; their Spanish counterparts come from
the independently validated FAT12 `2200AD4.BIN`. These literals are available
to inspection as original data only. No caller-connected code proves which
input selects a mode or continues a prompt, so Project Eon does not convert
them into SDL mappings or a reconstructed keyboard reference.

`MillenniumDosControlTextEvidence` accepts the two separately hash-identified
original static-data files, never a translation fallback. For English, indices
271, 350, 390, 398, and 399 select records at `$12a7`, `$1d88`, `$2aef`,
`$2bcd`, and `$2be3`; their raw-record SHA-256 values are
`4ff26c46bfaba03c12a1a29271499c81d044ce2cccc8db06ad3e07535ad5445c`,
`ab5a128110d288c166213ef0e64b8593d1945ab8e9624363c573fe8ef942f818`,
`b0676d538a2ef6b07cdf467bb10a4dbea34af96fccafc90180a01825935c1d4f`,
`220c3cd2cb86c2353f8f9320e6ec7c469007e4bd31e11dce52c847f8c510c5cc`, and
`0951952248daef3634e418d0bed0cfa2ea8cd58f7975ee5e77880c54ad731f2d`.

For the supplied Spanish FAT12 `2200AD4.BIN` (13,254 bytes, SHA-256
`8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31`), the
same pointer indices select that edition's raw records at `$1351`, `$1f41`,
`$2d99`, `$2e98`, and `$2eae`. Their SHA-256 values are
`1644bb8d9ecb1e41a50804e6966a9f91e99433968d6ce690aa1e8aaad79e00c1`,
`5d3b18d963f840dce41371210411578f218add619a52c39e49b382db2bb7f0b6`,
`6af12fa55735c3a6a6a986af3242472e47c01480d2b3e180c21e1996df04cdfd`,
`cc2cbde218ba9d86e805bf2247d6acf13a15019d28a840fdb67345be5efb28c2`, and
`e9d50a0d17dd4d11a008b88ccedb3f8b60dd6bdb3ec126ef8e28199b96f143d0`.
The parser returns only those exact supplied printable substrings while
retaining each raw record's native prefix and boundary as provenance. The
original static-data loader establishes that the file is requested, but its
runtime destination and any input-dispatch caller remain unrecovered; these
texts are therefore not host controls.

### Millennium DOS GX canvas

The first two `GX.LIB` entries establish a separate authentic bitmap path.
`IMG00` is a codec-2 240×33 resource containing a 256-entry RGB6 DAC after
its stream; `IMG01` is a codec-2 320×167 indexed canvas. Its 68-byte
post-stream index table selects entries from the `IMG00` DAC. The bounded DOS
LIB reader now returns borrowed ranges into the hash-verified supplied library
rather than retaining a whole-LIB or per-entry copy; callers retain the source
for the lifetime of each view. Project Eon decodes this pair in memory and
records the remaining resource-table tails only as original-library offset,
length and SHA-256 rather than retaining opaque source-byte copies or
inventing UI/state meaning. Exact offsets, sizes and pixel hashes are in [the
GX canvas evidence](generated/millennium-dos-gx-canvas.md).
GX and `2200SAVE.I` are inspection-only preservation evidence. The verified
`TITLES.EXE` poll establishes neither a process exit nor a DOS return to
`MILL.COM`, let alone `2200ad.exe` startup, GX selection, or save-state
initialization. The SDL launcher therefore keeps the original P00 title frame
at this boundary and neither draws the GX canvas nor opens a save panel.

`TITLE.LIB` entry `P00` is the first genuine title image: extent `$000006` to
`$002941`, 10,555 bytes. Its codec-2 record declares 320×200 indexed pixels,
maximum index 35, and a `$25d7`-byte stream. The decoder consumes low nibble
then high nibble; controls `$0`–`$d` add one of 14 verified deltas modulo 36,
`$f` supplies an absolute index, and `$e` repeats the previous index. The
64,000 row-major indices hash to
`85ec11c9f943672df2ba2a4e2837ce1f3158d61648ec07bcdc84b381bd24f4ee` with
7,386 nonzero pixels.

The remaining `$348` bytes of P00 are verified VGA colour data. Relative to
the P00 record, the `$300` bytes at `$25f3..$28f2` are 256 consecutive RGB6
triples (DAC index 0 first, component order R/G/B); their SHA-256 is
`b6dd34314102e429fdd98390b1fda27d3ea94d16bfcefa2983e3e319a2a20eae`.
The 36-byte table at `$28f3..$2916` (SHA-256
`652ea21cfa18c27470daaee4521d863a3d377f803a5f80ba0132af49b24083d4`) is
retained as an original-record offset, length, and SHA-256 but neutrally
named: this path does not prove its purpose. Project Eon deliberately does not
make a second byte copy of this opaque table; the supplied record remains its
only byte source.
The final 36 bytes at `$2917..$293a` translate the decoded logical indices
`0..35` to VGA DAC indices; their SHA-256 is
`cd7a7f81dd75249a8669e0f4c1792d99b37f3ea28c54319a3f2e84b4a86ff3e2`.
`TITLES.EXE` selects this exact latter address for its mode-1 `XLAT` path:
`record + $1c + word[record+$1a] + $300 + byte[record+$01] + 1`
(file `$139f..$13d6`, loaded `$149f..$14d6`). Its `P00` values resolve to
`$2917`. The verified display drivers write their supplied triples unchanged:
`VGA.BIN` `$104e..$105d` writes DAC index to `$3c8`, then three `LODSB` values
to `$3c9`; `TITLES.EXE` `$1226..$1232` uses the same order for animated DAC
entry 9. The hardware values are 6-bit, `$00..$3f`; Project Eon expands them
for SDL with `(v << 2) | (v >> 4)`, an explicit host-presentation adaptation,
not an invented original conversion. The resulting 320×200 RGBA title hashes
to `500a1451ab435a9c8ffaf1dbfaacee52cca0e32b375c883a45dd8f879a952888`.

`MillenniumDosTitlePresentationAssets` makes the full recovered data boundary
available to an SDL renderer in one hash-locked object. It admits only the
English `TITLE.LIB` SHA-256 above and the independently byte-validated
`TITLES.EXE`/`MILL.COM` profile: P00 is retained with its original record
extent (`+$000006`, 10,555 bytes), decoded indexed pixels, RGB6 DAC tables,
logical-to-DAC translation, and in-memory RGBA expansion. The same object
contains the complete contiguous P01–P25 bank, its 37 decoded 368-pixel
records, their original order, offsets, per-record identities, and the
mode-two logical-to-DAC byte ranges already established by the static title
code. This is a renderer-ready asset index, not a reconstructed title movie:
it neither composites a patch onto P00, invents a destination buffer or
palette mode, assigns a delay, nor maps a host key to title input. No media is
written, unpacked, copied, or included in the executable; all derived buffers
exist only while the caller holds the in-memory read of the original archive.
The English DOS runtime admission uses that same complete object before it
publishes P00 to SDL, so an independent P00-only decoder cannot admit a title
whose linked transition bank or title-flow profile has failed validation.

## Automation integrity

The repository's sole GitHub Actions workflow has read-only repository
permission and runs Gitleaks over complete history plus native build/test jobs
on Linux, macOS, and Windows. It handles no releases, tags, or publication. It
uploads non-published verification artifacts (packages and platform builds) for
CI inspection only. Releases require an explicit maintainer request outside CI;
normal development is pushed directly to `main`.

All action invocations are pinned to full Git object IDs, with the reviewed
release label preserved in a comment; the same immutable-reference rule already
applies to SDL3, zlib, and libpng source fetches. Each platform upload also
contains an adjacent schema-1 JSON integrity manifest. The manifest is generated
after that platform's artifact validation and records the source commit, artifact
basename, byte length, and SHA-256. It is a transport-verification ledger only:
it neither signs nor publishes a release, includes no workspace path, and never
opens or embeds user-supplied game media.

Before upload, CI validates each generated ledger independently with
`packaging/verify-artifact-manifest.py`. The verifier accepts only the exact
schema, a full lower-case source revision, uniquely sorted safe basenames,
non-symlink regular artifacts, exact byte lengths, and SHA-256 values. Its CI
mode also rejects unrecorded entries in the upload directory. This makes
the ledger a checked boundary rather than a write-only claim; the same command
can verify a downloaded artifact directory without unpacking it.

## Evidence levels

- **Verified bytes:** hashes, sizes, checksums, geometry, and fields asserted by
  automated tests.
- **Verified code path:** opcodes and operands checked before constants are
  accepted.
- **Observed behaviour:** reference execution with platform, version, inputs,
  timestamps, and capture hash recorded.
- **Inference:** a testable interpretation supported but not proven end to end.
- **Unknown:** undecoded material; no compatibility claim is made.

Plausibility alone never promotes an inference to verified behaviour.

### Deuteros Amiga title-display trace ownership

The native runtime has a fail-closed successor to the exact Deuteros Amiga
title-stage boundary for the strict v4 and v5 title-display trace contracts.
Admission is possible only after the live clean English release has reached
that boundary. At consumption time the engine reopens and rehashes the event
file, reruns the complete ordered title-bridge/display grammar, and, for v5,
reopens and rehashes all seven declared artifacts. It publishes the successor
atomically only after every check succeeds; a failure leaves the existing
title-stage boundary unchanged.

The owned checkpoint contains hashes, formats, sizes and observation counts
only. It retains no capture path, event or artifact bytes, emulator state,
original-media span, pixel buffer, sample buffer, callback value or input
event. Its runtime declaration therefore remains a bootstrap boundary with
`decoded_presentation`, `audio_observations` and `admitted_input` all false.
Reset and source revocation remove the checkpoint. This is evidence lifecycle
infrastructure, not title execution or parity: a genuine complete capture and
a separately reviewed compatibility/presentation bridge are still required
before any observed frame, audio or input semantics can become runtime
capabilities.

## Reproduction

Keep source data outside the repository:

```sh
cmake -S . -B build -G Ninja \
  -DEON_REAL_DATA_DIR="/path/to/complete-archive-corpus" \
  -DEON_DIRECT_DATA_DIR="/path/to/installed-direct-media"
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -v
./build/project-eon --data "/path/to/original-data" --verify-data millennium
./build/project-eon --data "/path/to/original-data" --verify-data deuteros
```

`EON_REAL_DATA_DIR` is a strict six-release archive-corpus test input and
cannot be satisfied by a partial collection or an installed directory set.
`EON_DIRECT_DATA_DIR` separately exercises recognised direct media in place.
The distinction keeps the all-platform/cross-language guard strong while
allowing an installation such as `~/.projecteon` to be verified without being
misrepresented as a full archive corpus.

Static reports can be regenerated from separately extracted, hash-verified
inputs with `tools/analyze_dos.py` and `tools/analyze_m68k.py`. Capstone output
is evidence navigation; structures must still be connected to callers, ranges,
and tests.

`tools/extract_static_control_flow.py` is the companion for compact external
control-flow sidecars. It admits only exact archive members and hash-locked
ranges, emits direct decoder candidates as `static-candidate-unclassified`,
and does not retain game bytes or a full listing in Git. Candidate edges are
not reachability, ABI, input, timing, or gameplay evidence. The committed
sidecar identities in `disassembly-inventory.json` can be checked with
`tools/verify_static_control_flow.py` while keeping the actual sidecars under
the maintainer's external cache.

## Adding evidence

1. Record SHA-256, byte length, language, platform, and known dump provenance;
   never normalize the original file.
2. Keep acquisition paths, personal information, and commercial bytes out of
   Git.
3. Add a minimal parser with explicit endian and range rules.
4. Assert results against genuine data and test malformed boundary conditions.
5. Record both disk/file offsets and relocated runtime addresses.
6. Preserve cross-platform disagreements instead of prematurely merging them.
7. Label generated launcher/menu artwork so it cannot be mistaken for original
   preserved art.

Git history records interpretation changes. Corrections must explain their new
evidence instead of rewriting earlier uncertainty away.

## Reference trace admission

A genuine execution trace is admitted only as external, hash-addressed
preservation evidence. It must bind a manifest and event stream to one exact
scanner-recognised outer release, including game, platform, language, byte
size and SHA-256; a similarly named archive or another platform is rejected.
Project Eon validates that provenance and event ordering only, then reports it
without replaying an event or creating a platform return value. The full
bounded grammar, required capture hashes and rejection rules are in
[REFERENCE_TRACE_FORMAT.md](REFERENCE_TRACE_FORMAT.md). Trace artefacts,
emulator snapshots, ROMs and all game media remain user-owned and excluded
from Git and packages.

The `deuteros-amiga-en-title-stage-v1` v2 adapter additionally pins the clean
system ADF SHA-256 and the `$13000` title-stage image SHA-256 before accepting
only raw observations at the already documented Exec, OpenLibrary,
graphics-vector, custom-register and callback boundaries. Its raw result
fields are retained as external provenance only: the adapter does not emulate,
replay, shim, call, write, invoke, or make a title-stage runtime input from
them. This preserves evidence beyond the present hard ABI boundary without
claiming that the original title stage has started.

## Current boundary

### Release runtime capability admission

`src/engine/release_runtime_capability.cpp` is the compiled, declarative
runtime-capability map for every recognised outer archive. Each row binds the
outer SHA-256 together with game, platform and language to one adapter,
initial session kind, visible recovery coverage and allowed presentation/input
observations. Runtime admission requires all four identity fields to match
before opening original media; a missing or forged row fails closed. This
prevents a release from inheriting a sibling platform or language's recovery
claim. For example, Spanish Millennium DOS is admitted only to its documented
title-presentation boundary and reports `BOOTSTRAP ONLY`; it does not inherit
the English executable-startup evidence. The native tests assert a one-to-one
mapping between the capability rows and the release manifest.

Release recognition, archive traversal, selected FAT12 content, Deuteros ADF
geometry/checksums, its first two load stages, two resource headers, and the
first verified palette bank and both bitmap layouts are implemented and tested. Audio
mapping, full resource semantics, simulation, AI, saves, and timing remain incomplete. The SDL app
must report those areas honestly rather than presenting fabricated gameplay.

The SDL Deuteros launch view performs the complete verified chain at runtime:
outer archive SHA-256 → nested clean system ADF SHA-256 → boot/load plan →
bundle 0 → channel VM / original VBL source → indexed bitmap → palette → RGBA
texture. `DeuterosAmigaOpening` independently rechecks the complete clean ADF
identity before it constructs any VM or renderer state, so callers cannot use
a matching boot or bundle prefix as a substitute. The session advances on a 20 ms scheduler cadence and supplies only a
recovered held input signal to the VM; the VM controls whether that signal is
accepted.
Thus the displayed pixels remain derived from user-supplied original data and
are not packaged in the executable or repository. Archives and ADFs are read
in place: no game file is unpacked, copied, installed, or written by runtime.

For a recognised installed direct-media set, runtime admission now verifies
every declared member and retains only its immutable hash, expected size and
direct-child path. It deliberately discards the verification buffers. A later
hash-addressed parser read reopens its one member as a regular non-symlink
file, checks the expected size and rehashes the returned transient bytes before
they are parsed. This keeps direct installs out of a second all-media cache
and closes the admission-to-parser replacement interval without constructing a
replacement archive or writing any game data.

An explicit CLI platform is never a request to substitute another release's
runtime. Therefore `--game deuteros --platform amiga` may run this ADF-backed
opening, while `--platform atari-st` remains a verified protected-media boot
report boundary. The latter has no recovered presentation/runtime chain, so the
SDL view deliberately does not load Amiga art, audio, or generated Atari state.
The runtime also does not create its default data directory; it reports a
missing path until the user supplies original media there or passes `--data`.
Linux package CI proves this against the extracted installed executable with
an isolated `HOME`: `--inspect` must report the absent
`~/.projecteon` location and must leave it absent. This supplements the
payload-media rejection check, so package metadata alone is never treated as
proof of the runtime's read-only boundary.
The Windows Inno Setup installer follows the same rule: it installs only
Project Eon and its own runtime resources, and does not pre-create
`<install-directory>\\data`.
The Linux AppImage is built from the same CMake install tree, with a reviewed
AppImage runtime and builder verified by SHA-256 before use. It is extracted
only into an external cache directory for CI validation; the verifier checks
the launcher, the complete dynamic-loader graph (private SDL3, SDL_image and
SDL_ttf must resolve from the AppDir with no unresolved or `/usr/local`
build-host fallback), cards, branding, fonts, localization catalogues, absent
default `~/.projecteon` path, and rejected media extensions.
The artifact therefore neither embeds nor initializes original game data.
Before Inno runs, CI also applies an explicit allowlist to the complete staging
tree. This is stricter than rejecting game-media extensions: an unexpected
non-media file inside recursively packaged `assets` or `po` is rejected rather
than becoming an unreviewed installer payload.
The unsigned iPadOS IPA is equally media-free: its Files-enabled default is
`Documents/ProjectEon`, an app Documents location reached without copying or
unpacking a selected archive. The runtime does not create that directory; it
only reads user-supplied media in place after the user provides it. Before an
IPA is uploaded, the archive verifier applies a closed payload allowlist: the
only regular members allowed in `ProjectEon.app` are the arm64 executable,
Info.plist, reviewed launcher cards and branding, reviewed font/licence files,
and the 20 PO catalogues. This is stronger than a disk-image suffix denylist:
an unknown file or directory cannot be smuggled into the sideload artifact as
unreviewed data.

At the exact opening event `$0f,$00000b38` (observed at scheduler tick 82 for
the recovered held input route), the live Amiga session now terminates its
opening VM. It preserves the final original-backed compositor frame and opens
only the existing SHA-validated title-stage boundary (ADF `+0x6e000`, length
`0x6ca00`, RAM `$13000`, entry `$40426`). Later host ticks cannot advance the
opening VM, VBL random source, compositor, or PCM mixer; SDL clears queued
preview audio at that edge. The title stage's Exec, graphics-library and custom
hardware requirements remain unexecuted, so this is not a fabricated title
screen or a claim that title-stage timing has begun.

`DeuterosAmigaOpeningRunner` is the SDL-free host scheduler for this already
admitted opening only. It is owned by `NativeSessionController`, which starts
it only for the matching active session and revokes it before a replacement
launch or return to the menu. SDL supplies a monotonic timestamp and receives
the original VM events unchanged through the controller; it cannot retain or
advance a scheduler after the source-bound native session is gone. The runner
uses a narrow tick callback rather than a media or coordinator borrow, advances
at a fixed 20 ms cadence, and permits at most four catch-up ticks before
resynchronising the host clock. Native tests exercise an under-period advance,
one tick, four-tick catch-up, resynchronisation, the terminal handoff and
return-state rejection. The runner does not own a path, archive bytes, save,
SDL device, frame interpolation, or game rule. Once the `$0f` event is
returned it stops permanently; later host time cannot manufacture opening
ticks, PCM, title execution, or bypass the lifecycle controller.

While that opening state is active, `DeuterosAmigaOpeningCheckpoint` exposes
only a read-only native self-consistency record: scheduler tick, VBL counter,
the recovered input-gate fact, and SHA-256 values for the already composed
indexed and RGBA frames. It is absent before a first frame and after the title
handoff, cannot tick the VM or retain media, and is not an emulator trace,
capture receipt, title-display comparison, or parity claim. Its purpose is to
provide exact future capture-matching checkpoints without crossing the Exec or
graphics ABI boundary.

The terminal handoff also executes only the title entry's proven profile-one
prefix in memory. Bootstrap `$12b0e` (ADF `+0x2f0e`, 14 bytes SHA-256
`858d0a08e8d6fe8200fb71a0866731feabffcadc232bfdeff5be669446bae0fd`)
reloads D0 from `$12a34`, which is profile one on the exact `$0b38` route.
The title code then writes `$4040e=1` and `$19d52=1`; its `$40426..$40437`
and `$40448..$4044f` byte spans hash to
`833374022042225f1bfeeedd56c05d7011168531fa121494cef04174453e5387` and
`8d15b73f389c05fc214b9440c0a0b77df33782c6400d455cef96f338aa5f1211`.
The preceding `A1 -> $206a0` transfer remains deliberately unmaterialized:
its controller pointer is unknown. The next instruction is nevertheless
wholly local and now executes as a register fact only: `MOVEA.L #$00040b62,A7`
at `$40450` (ADF `+$9b450`, six bytes, SHA-256
`5751cf8005bff79d636488a9e0292ecb5821879b1cb2c432e7a5332a0f7b5e3a`).
`DeuterosAmigaTitleExecPrelude` exposes that literal A7 value and stops at
`$40456`, before `MOVEA.L $4.W,A6` reads the unknown Exec base. It does not
allocate stack memory, invoke Exec, or cross into graphics/custom hardware.
The following 16-byte Exec boundary hash is
`f0c847a4d443e26fc08f6c6864afeca3b33da514f8708f76f2f05314a4c88067`.

That stop is now engine-owned rather than implied by a boolean. After the
local prefix executes, `DeuterosAmigaTitleExecBoundarySession` advances once
to `awaiting_exec_base_read` and records `$40456` as its terminal program
counter. It independently validates the complete 28-byte hard-ABI span and
retains the calls at `$4045a/-$96` and `$40468/-$9c` as deferred requirements;
the latter also retains the intervening literal `D0=$7fff0`. Without a
separately admitted observation, the session cannot supply address `$4`,
advance either call, or invent either return. Thus this typed continuation
improves lifecycle and diagnostics while executing exactly zero instructions
beyond the last value proved by the original bytes.

The same boundary now has a narrow ingestion API for a future independently
admitted genuine trace. Each value-only return must name source address `$4`,
the exact next call PC/vector/return PC, and a strictly increasing trace
sequence. The first accepted record advances only to the second Exec-base read
at `$40464`; the second advances to `$4046c`, immediately before the unresolved
`$1ed80` setup/library route. D0 and SR are retained verbatim without labels or
side effects. Missing, duplicate, reversed or address-mismatched records leave
the boundary incomplete. This API does not itself validate a capture, supply
an Exec base, call either vector, interpret privilege state, or admit display,
audio, input or gameplay behavior.

The wider hard-ABI span `$40450..$4046b` is 28 bytes at ADF `+0x9b450`
(SHA-256 `24f5fb4f5019bf450f8b6931fe1c77747461704b139bbe14ec079b1008af1f49`).
It installs stack `$40b62`, reads the unknown Exec base from address `$4`,
and calls the conventional `SuperState` and `UserState` vectors at `-$96` and
`-$9c`; no Kickstart image, vector implementation, return value, condition
code, privilege transition, or stack state is present in the supplied disk
media. If both vectors and the following internal calls returned, the static
span `$4046c..$404d7` (108 bytes, SHA-256
`69b1aaefdd169565901ae166f15c1a17487f9bd63b7303748869bc7955c7380f`) would
reach custom-register literal `$dff000` and setup offsets `$40`, `$42`, `$9a`
and `$96`. That conditional post-boundary code is recorded solely as raw
preservation evidence; Project Eon does not cross the Exec ABI or claim a
visual hardware effect.

The conditional static continuation extends through `$40573` / ADF `+0x9b46c`
for 264 bytes (SHA-256
`f96f93da561b1758dd559a93e2fe97b7b6cc11d482dc5de6460c709b8190bc56`) and
stops before `$40574`. It contains direct-call operands for `$1ed80`,
`$1f172`, `$1f182`, `$1ef74`, `$206d4`, `$206be`, `$403e6`, `$403f4`,
`$204c8`, `$389e2`, `$1fb9a`, `$38912`, `$2022a`, `$41bb4`, `$20e18`,
`$20ba8`, and `$37180`, plus an indirect `JSR (A0)` after `LEA $20cfe,A0`.
The original mode-cell comparison at `$4040e` statically selects `$36a8c` for
low word `$0005` and `$1fb9a` otherwise. Calls, returns, cell values, target
semantics, and display effects are unmodelled; this records no executable
title-stage path.

Within that continuation, the first direct wholly local callee that has a
complete, caller-connected byte path is `$403e6`. Its call site is
`$404c2..$404cd` / ADF `+0x9b4c2`, which loads D1 with literal `$00013000`
then executes `JSR $403e6`; its twelve bytes hash to
`a617235dd94a6c0b3f5fb9f9e078652ed8f1e45213e85c80b10ec165a6b7216f`.
The callee prefix `$403e6..$403f1` / ADF `+0x9b3e6` hashes to
`1e1ccdae97d5849873d3d2e785f5a8be585ffa0e104b5c550ecade6bc37a33a2` and
contains `MOVE.L #$1c482,D0` and `MOVE.L D0,$1f97c`; the separately validated
following word at `$403f2` is RTS.
`DeuterosAmigaTitlePostExecPointerSeedProfile` hash-locks both spans and
reports their literal operands. Reaching the call still requires the two Exec
vectors and all earlier original calls to have returned, so Project Eon does
not execute it, materialize D0/D1, write `$1f97c`, or claim startup progress.

The immediately following call at `$404ce..$404d3` / ADF `+0x9b4ce` is a
separate six-byte static edge to `$403f4` (SHA-256
`555513267ef304f2a5cec2303f8565db8e4ed9ecb2abd7bc87b73dbe5d6c0976`). Its
26-byte callee `$403f4..$4040d` / ADF `+0x9b3f4` hashes to
`5353ab8b18d63a51e12ef2f586a68d872981fa491ca13531198f18a2a38edf07` and is
exactly four direct `JSR` operands followed by `RTS`: `$403c8`, `$20510`,
`$1f37a`, and `$40698`. `DeuterosAmigaTitlePostExecServiceBatchProfile`
validates the caller, complete callee, and return address `$4040e`. This does
not establish that any nested call returns, or assign effects to their code;
the profile records only the caller-connected static call batch after the
unexecuted Exec boundary.

The first nested call of that batch is independently bounded at
`$403f4..$403f9` / ADF `+0x9b3f4` (6 bytes, SHA-256
`2a90f1020af64bd1a6f7f6e7e7503bea4133a2a569bba55987f6edb23442cec3`). Its
complete callee `$403c8..$403e5` / ADF `+0x9b3c8` is 30 bytes, SHA-256
`3f9cf2302a4078faddd0796fc05268386d46c4be64f294b8082ba085b9609f5f`:
it assigns `$1ed24` to A1, `$12e12` to A0, literal D0 `$14`, and A6 from
`$12fec`, then executes `JSR -$c0(A6)` and RTS. `DeuterosAmigaTitlePostExecGraphicsVectorProfile`
hash-locks the caller and routine, including return `$403e6`, but does not
call the graphics-library vector, establish its ABI or return, or name any
visual/title effect.

The next batch edge at `$403fa..$403ff` / ADF `+0x9b3fa` hashes to
`f31dc5923e4b39eb1726fc9b05ac7f56c0209f5d60c9499b979ebfc7c08a58a2` and
targets `$20510`. Its complete 38-byte local routine `$20510..$20535` /
ADF `+0x7c510` hashes to
`60ee2fcb4a18f62cd2066aba2429e760a64f14cd3f07f3cfe8467972030008bc`. It is
four straight-line operands followed by RTS: word literals `$0000` and
`$f690` target `$202c4` and `$2027e`, long literal `$00000001` targets
`$20280`, then the word at `$20276` is copied to `$2027c`; the return is
`$20536`. `DeuterosAmigaTitlePostExecStateInitProfile` records this exact
byte provenance. It does not perform those writes or infer their meanings:
reaching this second batch call still requires the preceding graphics vector
and every earlier unresolved original call to return.

The third batch edge at `$40400..$40405` / ADF `+0x9b400` hashes to
`901b0ad5740a3e6aea3eba28b6aadf5ac5c187e961cc848f6f1a882b3592f464` and
targets `$1f37a`. Its primary 18-byte entry `$1f37a..$1f38b` / ADF
`+0x7a37a` hashes to
`58e85705bc821d42834936342b242162c749889b9d9c23c3d5896f7bcf06e4ff`: it
first calls local `$20094`, then—only if that call returns—loads A6 with
literal `$1f372` and tail-jumps to `$201d2`. The independently bounded
`$20094..$200f9` routine / ADF `+0x7b094` is 102 bytes and hashes to
`7427cdaa0f716496e21c5ef0f6a8d0850a9606a9b4ffe6e56df599109b0ca947`.
It clears D0 and calls graphics-library vectors `-$19e`, `-$198`, and
`-$1a4` through A6 loaded from `$12fec`; it records the first result byte at
`$20092`, literal pointer `$1ffe6` at `$2008e`, and three literal words
`$000a/$000a/$000c` at offsets `$0006/$0008/$0004` from `$1ffda`, before RTS
at `$200f8`. `DeuterosAmigaTitlePostExecThirdServiceProfile` hash-locks all
three spans and these operands. It does not call any vector, supply a vector
result, write any title-stage cell, or execute the tail jump. Reaching this
third edge requires the earlier graphics vector, state-init routine, and all
prior original calls to return.

The active title session now accepts the third typed, same-library return only
at `$200f4`, vector `-$1a4`, returning to `$200f8`, and only after the first
two graphics returns. It retains D0 and SR as value-only evidence. The exact
local continuation executes the RTS, resumes at `$1f380`, loads literal A6
`$1f372`, tail-jumps at `$1f386` to `$201d2`, and enters the first BSR target
`$200fa`. That wrapper supplies literal A0 `$12e12` and A1 `$1ffda`, reads A2
only from pointer cell `$2008e`, and reloads the same graphics-library base
from `$12fec`. Execution stops before its next graphics.library vector
`-$1a4` at `$20112`; Eon records neither pointer-cell contents nor a graphics
effect or rendered output.

The next session edge accepts a typed return from that exact `$20112`
`-$1a4` call only when it uses the same observed library base and follows the
preceding return sequence. D0 and SR remain value-only evidence. The wrapper
RTS resumes at `$201da`, restoring the instruction-saved A6 value `$1f372`.
The following code derives destination A0 `$1ffc8`, but its four source words
are runtime memory reads at `$1f372..$1f379`; Eon therefore requires a second
typed observation at the first copy instruction `$201e4`. It then records only the literal
copy layout to `$1ffca/$1ffcc/$1ffd0/$1ffd2` and the two encoded `$ffff`
writes to `$1ee12/$1ee10`. The next BSR at `$201fe` enters `$20118`, whose
first instruction reads runtime word `$1ffc8`; execution stops there without
inventing that value, choosing its branches, or invoking the later `-$1aa`
graphics vector.

At the `$20118` re-entry, the session requires all eight words used by the two
mirrored selection blocks, with exact source addresses
`$1ffc8/$1ee10/$1ffca/$1ffcc` and
`$1ffce/$1ee12/$1ffd0/$1ffd2`. It applies the original 16-bit wrapping add,
signed tests and unsigned bound comparisons, then records the selected words
back to `$1ffc8` and `$1ffce`. The graphics arguments are derived exactly as
`D0 = (selected_first - $10) >> 1` and
`D1 = selected_second - 6`, with word-width wrapping. Literal A0 `$12e12`
and A1 `$1ffda` precede the same-library `-$1aa` call at `$201b6`.

An exact typed return from that call advances through the register restore and
RTS to caller `$20202`, records literal word one at `$1ee12` and `$1ee10`, and
follows BSR `$20212` back to `$20118`. The repeated first read from `$1ffc8`
is the next genuine runtime boundary; Eon does not reuse the previous value,
choose a new branch, or infer any graphics result.

That repeated `$20118` entry is now a distinct lifecycle step. It requires a
fresh observation of the same eight addressed words and re-evaluates both
selection blocks; no result from the first pass is silently reused. A fresh
same-library return is then required for its `-$1aa` call at `$201b6`. The
exact local return restores registers, executes RTS, and resumes at `$20216`,
whose BSR enters the separately hash-locked wrapper `$200dc`. The wrapper
again supplies A0 `$12e12`, A1 `$1ffda`, pointer cell `$2008e`, and library
base cell `$12fec`, then reaches `-$1a4` at `$200f4`. The session stops there:
the repeated call has its own return observation and cannot borrow the earlier
`$200f4` result.

The repeated wrapper return is generation two of the `$200f4` boundary. It is
accepted only after the second `$201b6` return and with a strictly newer trace
sequence, so neither `$200f4` observation from the earlier service path can be
reused. After retaining D0 and SR as value-only evidence, the session follows
RTS `$200f8` to `$2021a`, restores the saved A0 without assigning its value,
and executes tail RTS `$2021c`. The original call stack returns to batch edge
`$40406`, whose exact local target `$40698` immediately returns to batch RTS
`$4040c`; that RTS resumes at `$404d4`. The continuation loads source-table
address `$12ff4`, then stops before the first runtime longword read at
`$404da`. No table contents, Exec result, or later service effect is supplied.

The `$404da` boundary now accepts exactly two ordered longword observations
from `$12ff4` and `$12ff8`. It records their instruction-defined copies to
`$37ef2` and `$37ef6`, then follows JSR `$404ea` to the hash-locked `$204c8`
wrapper. That local code writes byte `$02`/`$c4` at descriptor `$204aa`
offsets 8/9, longwords `$204c0`/`$202ca` at offsets `$0e`/`$12`, and sets D0
to five. It reads Exec base from address `$4` and stops before vector `-$a8`
at `$204f4`.

A separate typed Exec observation must provide a nonzero base, exact call and
return addresses, vector, monotonically newer sequence, and the returned D0
and SR values. Those results remain value-only evidence. The local RTS at
`$204f8` resumes the caller at `$404f0`, where the next direct call targets
`$389e2`; the session stops before entering it because its result and effects
are not established by this boundary.

The next call site `$404f0..$404f5` hashes to
`1385698c6c854ab133e3e7cd75417c90025916dd0a1dd303347dce0636114bea`
and targets `$389e2`. The complete 78-byte local routine through RTS `$38a2e`
hashes to
`4400342704c0c26b934dec5db314e8853b0963d6757432f3aad275cab965811d`.
Its prefix computes D7 `$13400`, supplies D1 `$26cc0` and D0 `$5800`, then
calls `$208c0` at `$389f4`. The active session requires an explicit typed
return from that exact local edge and retains returned D0/SR without assigning
semantics. It then requires the runtime word at `$12fd8` read by `$389fa`.

Only the low byte selects the proven tail. Zero reaches the retry/display
boundary at `$389aa`; one executes RTS `$38a04`, returns to `$404f6`, and
stops before call `$404f8`; two selects copy source `$26cc0`; all other byte
values select `$29540`. Both copy variants target `$1c482` for `$0a20`
longwords and stop before the first runtime source read at `$38a28`. Eon does
not fabricate the 10,368 source bytes, execute the copy, or infer what the
selector or copied region represents.

The active copy boundary accepts those 10,368 bytes only as ordered chunks of
at most 256 longwords. Every chunk must name instruction `$38a28`, the exact
next longword index, monotonically newer sequence, source address and
destination address; gaps, overlap, replay, empty chunks and overrun fail
closed. Selector two fixes the source base at `$26cc0`; every other copy-path
selector fixes it at `$29540`; destination is always `$1c482`. Each admitted
plan contains only that chunk's observed values and instruction-defined
destination addresses. The shared bounded-memory-transfer owner retains the
admitted effects for checkpoint/diagnostic replay, never source bytes that
were not explicitly observed.
After exactly `$a20` longwords, DBRA falls through to RTS `$38a2e`, returning
to `$404f6`. Selector one reaches that same address without the copy. The
shared continuation `$404f6..$404fd` (SHA-256
`596e3b08acbe94bbe512d87ca7aeb9e8eb686af82c891c5a0e4dd1c4fcdbe3c3`)
loads literal D0=1 and calls `$1fb9a`.

The complete 24-byte dispatcher `$1fb9a..$1fbb1` hashes to
`173ecec3e880b6b6d9022567622548e25629dd23d671f6f8f9e46fe700edcbbb`.
It reads a runtime table base from `$1f97c`, selects the signed word at
`base+2`, adds that offset to the base and calls `$1fa00` with the resulting
command-stream pointer in A4. The active session requires both reads as
separate typed, monotonically ordered observations; it neither invents the
base nor the table word. The nested parser's exact ten-byte prefix hashes to
`2605ca4af737e72accc1c6dc91bf640aac370f6037da3ec284ba440377a6b4cf`,
clears byte `$1f98c`, executes NOP and clears D0. Eon exposes that one literal
byte effect and stops before the first runtime command-byte read at `$1fa0a`.
It does not interpret the command stream, infer presentation, or claim the
outer call returned.

The byte-complete command dispatcher `$1fa00..$1faeb` hashes to
`9f3ee558ee309d4f8b5f73ee11ac085fa9d11510c7536180ee515f40b01546ef`.
Its active state machine admits every opcode only at the exact next A4 source
address through instruction `$1fa0a`, with a strictly newer sequence. It does
not assign semantic command names. The first four recovered paths are local:
zero returns through `$1fa26`, `$1fbae` and `$1fbb0` to caller `$404fe`; byte
`$07` copies an explicitly observed longword from `$1f974` to `$1f978`; and
bytes `$10`/`$11` consume one explicit operand byte and store the address
`$1f8ec + ((operand & $0f) * 8)` in `$1f96c`/`$1f970`. The two complete
26-byte helpers hash respectively to
`d15a4fe158160fd3aee3a439755fa108a403d09bd0d92c72ce0fea3505c1f3e5`
and
`f488d16abd19300d49deeb62d7d4227c0bdbb1b0d40b7bc305383414f1d195d3`.
After each admitted nonterminal local path the next opcode must come from the
advanced command pointer; pending operands prevent opcode re-entry.

Opcode `$08` is also recovered without assigning a meaning. It requires the
longword at `$1f978` and byte at `$1f98e` as separately sequenced
observations. A zero byte adds literal `$140`; a nonzero byte additionally
requires the longword at `$1f994`, applies the original `LSL.W #3` to only
its low word, and adds the resulting longword. Both exact branches write the
result to `$1f978` and `$1f974` before returning to the next stream byte;
32-bit arithmetic wraps exactly as the 68000 instructions do. The otherwise
unmatched below-`$20` opcodes enter `$1fde4`, whose entire body is the
hash-proven `RTS` (`1ceeabf0c6a5a30bad12cdac0e3ab015a7188a42e6aebb556aad00bb9cd693ad`),
so those paths also return to the loop without an invented effect.

All remaining byte values stop fail-closed at their first unresolved fact:
the local calls `$1fde6`, `$402ac` or `$1fbe6`,
the fixed-table byte reads used by values at least `$90`. Their addresses and branch selection are retained as
raw instruction facts only. No call return, display effect, command meaning,
or further stream advancement is fabricated.

For direct call boundaries that occur before any further command-stream read
(`$1fde6`, `$402ac`, and `$1fbe6`), the session may continue only
after a typed observation names the exact call address, target, return
address, newer sequence, returned A4, D0 and SR. The observed A4 becomes the
next stream address; Eon does not assume a callee preserved or advanced it.
Returned registers remain value-only evidence and no unobserved callee memory
or presentation effect is synthesized.

Opcode `$16` now continues through its complete 104-byte target
`$1fb00..$1fb67`, SHA-256
`a431f810a110c1640f0f99460a7685054406b0312260cdc400a52e62ee4da2ac`.
The session first requires the branch byte at `$1f98e`, then the two ordered
stream bytes read at `$1fb0c` and `$1fb10`. On the zero branch it additionally
requires the longword at `$1f168`, clamps the second byte to `$30`, applies
the literal word/unsigned-multiply sequence and writes the exact resulting
longword to `$1f974`. On the nonzero branch it requires `$1f994`, performs
the original low-word shift/multiply and low-word addition, adds literal
`$256c0`, and writes that result to `$1f974`. Both branches consume exactly
two stream bytes and return to `$1fa0a`. Every runtime value is explicit;
word operations discard carries exactly as the 68000 does, and no command
meaning is assigned.

The complete-disassembly ledger currently reports zero discovered-unmapped
scanner candidates for both recognized Deuteros releases. This continuation
therefore closes a previously unimplemented caller-connected function inside
the already mapped Amiga clean title-stage image; it does not manufacture a
new code image or change the byte-complete inventory.

Those target bodies, including the now engine-owned `$1fe3c` wrapper, are
bound to the clean title stage rather than trusted merely because a caller
names them. Their exact spans and
SHA-256 values are: `$1fde6..$1fe0d` (40 bytes,
`3b063fa7f0f401beabd986787d4eb36e828b6eea37f474f0a4d04f70374ba2d3`),
`$1fe3c..$1fe53` (24 bytes,
`59c2d528a565a523205ce7506fa7e1c7ba8c26a2e6a7246c28ee5b7607f8837e`),
`$402ac..$40355` (170 bytes,
`7cfbdbe94faf764157dbe22bc9003fc4362a5657a7d7b7c34b0413d4391783be`),
and `$1fbe6..$1fde3` (510 bytes,
`66dd6a4297fdfd52c1b81b7bd00f3f54611597bf31551939cf0f40bf9fd8d13e`).
These hashes prove the local dispatch/call topology and return instructions;
they do not turn nested calls, runtime reads, or writes inside those spans
into engine-owned effects. A fresh explicit outer return remains mandatory
for every still-opaque invocation, preventing stale return reuse across
command bytes.

Opcode `$12` now owns the complete `$1fe3c..$1fe53` repeat wrapper rather
than accepting one opaque outer return. It observes the count byte at
`$1fe40` and the following repeated byte at `$1fe44`, preserving the latter
stream address across every call. The original `SUBQ.B` plus `DBRA` sequence
produces 1–256 calls to the already hash-bound `$1fbe6` target: count zero
means 256, otherwise the byte value is the count. Each iteration requires a
fresh typed `$1fe46 -> $1fbe6 -> $1fe4a` return with monotonically newer
sequence, value-only D0/SR, and returned A4 equal to the repeated-byte
address. Only after the exact final return does `$1fe4e` advance A4 once and
the command interpreter admit the next opcode. Replays, early completion,
wrong A4, and out-of-order returns fail closed; no graphics effect is inferred
from the observed calls.

Opcodes `$90..$ff` now own their complete two-call table dispatch
`$1faca..$1fae8`. The low nibble selects one of 16 two-byte entries at
`$1c47a + nibble*2`; the immutable original 32-byte table hashes to
`876532af8a5aec1ac7f230c6ffaeec1d82ba3dec23feaa570ba2784924530149`,
but active execution still requires each byte as an ordered typed memory
observation. The first byte is read by `$1fada` and admitted to
`$1fadc -> $1fbe6 -> $1fae0`; only that exact fresh return unlocks the second
read at `$1fae0` and call `$1fae2 -> $1fbe6 -> $1fae6`. Both returns must
report A4 at the second table byte, matching the original postincrement and
nonincrementing reads. The saved command-stream A4 is restored only after the
second return, then execution resumes at `$1fa0a` with exactly one opcode byte
consumed. D0/SR remain value-only evidence and the service's graphics effects
remain outside this state machine.

For direct opcode bytes `$20..$8f`, the zero/zero mode route through the
hash-bound `$1fbe6` service is now engine-owned rather than admitted as one
opaque return. The exact zero-route span `$1fc22..$1fc9b` has SHA-256
`14bad66df34c5d4200afe7ba9cef8ac114afaf31d9be133d428c1af727c0fe89`.
Typed observations must prove zero at `$1f98c` and `$1f98e`,
then supply the five longword cells `$1f99c`, `$1f974`, `$1f96c`, `$1f970`
and `$1f9a0`. The opcode selects eight glyph bytes from
`[$1f99c] + (opcode-$20)*8`. For each glyph row, the original loop rereads
four words from each of the two mask pointers; all 64 word reads and their
repeated source addresses must be supplied in instruction order.

The recovered byte effect is exact: for each of eight rows and four planes,
`(low_byte([$1f970+plane*2]) & ~glyph) |
(low_byte([$1f96c+plane*2]) & glyph)` is written at
`[$1f974] + row*$28 + plane*$1f40`. Thus one admitted invocation exposes 32
concrete original bitplane byte writes, preserving the 68000 byte-width
operations and the four `$1f40` plane strides. After the loop, the observed
`$1f9a0` longword is added to the original destination pointer and written
back to `$1f974`; the service returns through `$1fc9a` and `$1fac6` to the
next command byte. The generic call-return API rejects this opcode family,
so a register-only return cannot bypass the recovered writes. Nonzero mode
routes remain explicit evidence boundaries. No host renderer, glyph meaning,
colour assignment, or unseen memory effect is inferred from these writes.

The other three non-negative dispatch variants are now owned by the same
typed command state machine instead of being collapsed into a register-only
return. Their complete routines are independently locked to the clean ADF:

- positive/clear `$1fca6..$1fd09`, ADF `+$7aca6`, 100 bytes, SHA-256
  `806ad8916bbcdd2b6e01806f56cde2905cd8f9d2af63c877c2242371e2659141`;
- zero/set `$1fd0a..$1fd79`, ADF `+$7ad0a`, 104 bytes, SHA-256
  `13e86b16e732da32a9cbdcd1b0b387c042b6a6bedd9b46cb5664d0a4a121318a`;
- positive/set `$1fd7a..$1fde3`, ADF `+$7ad7a`, 102 bytes, SHA-256
  `bd6bfbd42d3b6471a8166e14228fe177f5afe3a7ff3c8372cf291b0c37c44f82`.

Positive means the signed byte at `$1f98c` is `$01..$7f`; `$80..$ff` remains
the separately documented negative service/timing path. Set means the byte at
`$1f98e` is nonzero. All three variants select the same eight glyph bytes by
`[$1f99c] + (opcode-$20)*8`. Positive variants use each current destination
byte as the first blend input; zero/set uses the low byte of the word at
`[$1f970] + plane*2`. The second input is always the low byte of the word at
`[$1f96c] + plane*2`, and the exact written expression remains
`(first & ~glyph) | (second & glyph)`. The clear variant uses literal row and
plane strides `$28`/`$1f40`; set variants read longword strides from `$1f994`
and `$1f998`. Each advances `$1f974` by one after its 32 writes.

Admission retains the width distinction: positive destination reads must be
bytes, while table inputs retain their observed 16-bit words. If an already
owned native-memory byte contradicts a positive-route destination
observation, the whole command is rejected without changing command, memory,
or surface state. Dynamic-stride effects remain valid sparse native memory;
they enter the 320x200 renderer surface only when the observed strides are
exactly `$28` and `$1f40` and every derived address passes the surface's own
bounds/order check. No mode value, pointer, glyph, existing destination byte,
or source word is synthesized.

The signed-negative dispatch is now a separately owned command completion,
not a planar write. Its exact `$1fbe6..$1fc21` span is 60 bytes (ADF
`+$7abe6`, SHA-256
`cbddd93eb43c498079e7e2175f8f7d6178c357aa6b5241631e717f9037cff414`).
A typed observation must prove the read at `$1fbe6` from `$1f98c` is
`$80..$ff`. Character `$20` suppresses the nested service. Every other
character must additionally prove the exact `$1fc08 -> $3fbf8 -> $1fc0e`
call/return with literal D0/D1 inputs `$13`/`$0c`. Both paths execute the
literal `$4e20` (20,000 iteration) decrement delay and return through
`$1fc20` and caller continuation `$1fac6`; Eon advances the command stream
without claiming a graphics write.

The `$3fbf8` service's internal reads, writes and meaning remain opaque. Eon
therefore requires its observed return but does not emulate or invent its
effects. A mismatched signed mode, suppression claim, call address, target,
return address or literal input rejects the complete transition without
advancing live command state. The delay is retained as an original timing
fact in the native plan; it is not implemented as a host busy-wait.

After opcode zero returns to `$404fe`, the native state machine now follows
the next two caller-connected services to the first post-command state
effect. A typed observation must prove the opaque `$404fe -> $38912 ->
$40504` return, the following `$40504 -> $2022a` call, and the byte test at
`$20238` from `$1ffd8`. The `$2022a..$20275` routine remains independently
bound to ADF `+$7b22a`, SHA-256
`a7f7c0c3efa60284b3d292249b3560da4d832ff0c5dfa34711b72604760b39a9`.

For a nonzero observed flag the local subroutine returns immediately, after
which the caller clears byte `$1ffd9` and returns to `$4050a`; Eon applies
that one byte atomically. For zero, the proven pre-call effects set longword
`$2008e` to literal `$1ffe6` and byte `$1ffd8` to `$01`, then stop before the
still-opaque graphics wrapper at `$200dc`. That branch does not yet clear
`$1ffd9` or claim that `$2022a` returned. Contradictory call, flag or address
evidence changes neither command nor runtime memory. No game text is exposed
or altered by this transition; future localized user-facing text remains a
separate presentation concern while original media text stays immutable.

The zero-flag branch now continues across its same-library graphics return.
The wrapper `$200dc..$200f9` is independently fixed to the genuine clean ADF
at `+$7b0dc` (30 bytes, SHA-256
`6e36c860c280c651947ad0ea6ef868759fbc7bfac67d89af219135e4751e6e6f`).
Its typed observation requires the session's already observed graphics base
from `$12fec`, call `$200f4`, vector `-$1a4`, and return `$200f8`. The local
RTS then unwinds to `$2022e`, which clears byte `$1ffd9`, returns through
`$20236` to `$4050a`, loads D0 literal `$004d`, and stops before the next
opaque call `$4050e -> $41bb4`.

The clear is committed to owned runtime memory only after previewing the full
transition, so a wrong base, vector, call, return, sequence, or revoked title
session leaves both memory and lifecycle state unchanged. The graphics
vector's internal effect is not reproduced or inferred, and `$41bb4` is not
entered. This transition presents no text and does not modify the localized
presentation contract or original in-media strings.

The first `$41bb4` dispatch for literal D0 `$004d` is now executed through
its complete deterministic prefix. The dispatcher code remains bound by its
existing `$41bb4..$41c31` hash
`fba4dff4da954290d970f5ec129220c179a2ef73f010def6512401380b8640cc`.
Three genuine title-stage table leaves are independently locked before use:
the 14-byte `$416d0` entry hashes to
`dd1ef0e747524f3b48c3cca3e81f9601b7f19587cfdb841064e4072324e8c3d7`,
the 12-byte `$4128e` descriptor to
`f1e6aa704e116355d7475b93167fb52fb8b9ae8310db046c230e74cf83aa5da5`,
and the four-byte `$422ae` pointer cell to
`08512d1c9cc7d8b5d700c917e2b750cddc6816bd7946ef45cc44c486ec2919ff`.

`$004d * $000e` selects `$416d0`; its `$00c1` high-bit descriptor redirects
through nested index `$00c1`. Pointer offset `$000367ae` plus base `$422fa`
yields compressed source `$00078aa8`. The fixed descriptor selects
destination `$000256dc`, wrap address `$0002bfc0`, wrap subtraction
`$000068fe`, and row advance `$0038`. Native execution stops at `$41c72`
before reading the first source width word. No byte at `$78aa8`, decoded
dimension, decompression write, or visual meaning is fabricated. Repeating
the local advance or invoking it after session revocation fails closed.

The first two external reads are now a narrow typed header observation. They
must occur in order at `$41c72` and `$41c7e`, from `$78aa8` and `$78aaa`, as
big-endian words. The original stores them unchanged at `$410c8` (width) and
`$410ca` (height); Eon applies both word writes atomically. It also retains
the exact 16-bit `width-1` counter behavior, including wrap for a zero width.
Height below `$00c8` selects the low-height packet decoder at `$41c98` with
next source `$78aac`; height `$00c8` or above selects `$41d44`. These are
control-flow and width facts, not assertions that either observed dimension
is safe or visually meaningful.

The genuine header is `$0044/$8010`, so this release selects `$41d44`. The
seven-byte source prefix `$78aa8..$78aae` is inside the immutable loaded title
stage (ADF `+$d3aa8`) and hashes to
`96c277c906c4179741d1de3383fa161bc415b93771dad9b8377f9d9190491ce9`.
The high decoder derives 17 two-byte column groups and 16 rows, stores those
values at `$410cc/$410ce`, and reads first control byte `$01`. That literal
packet copies genuine bytes `$0f,$ff` to `$256dc,$256dd`, leaving source
`$78aaf`, destination `$256de`, 16 column groups, 16 rows and four planes at
the next packet dispatch `$41d60`. All four writes—the two decoder geometry
words and two literal bytes—commit atomically, and replay is rejected.

The following four command families are
statically visible—literal pairs, repeated duplicated bytes, repeated
byte-swapped pairs, and extended-count byte-swapped pairs—but their counts,
payloads, destination writes, and completion after `$78aaf` remain unowned.
The runtime does not allocate or render an inferred surface; the two admitted
destination bytes are sparse original memory effects only.

The remainder of this first high-path decode is now complete. The exact
453-byte compressed span `$78aac..$78c70` (ADF `+$d3aac`) hashes to
`684e83c90a64bd8829ad01f3b7615b5d686d4d054e25c7483d29b7b50046fb1d`.
It contains 130 packets: 65 literal-pair packets, 64 duplicated-byte fill
packets, one extended-count swapped-pair packet, and no short swapped-pair
packet. The original loops produce 1,088 pairs / 2,176 bytes across four
planes, 16 rows and 17 two-byte groups. The final extended packet terminates
through the plane counter at `$41e40`; no unused repetitions are emitted.

Eon decodes the immutable span with explicit source exhaustion, output-count,
32-bit address-overflow, row, column and plane bounds. The already committed
first pair is excluded from the follow-up effect batch, so the remaining
2,174 byte writes commit exactly once and atomically. Completion leaves the
original routine at RTS `$41e40`. These bytes remain sparse recovered runtime
memory: no width-to-pixel interpretation, palette, planar layout, complete
frame, or renderer surface is claimed merely because decompression finished.

After RTS `$41e40`, the nested `$41c32` call resumes at `$41be6`. The outer
`$004d` entry supplies X word `$0003` and Y word `$00b8`; the caller derives
destination `$27f06`, mask base `$256dc`, three outer iterations and 184
words per iteration. Its four first mask reads are already-owned decoded
words at `$256dc`, `$2711c`, `$28b5c` and `$2a59c`. Eon validates their
presence in the sparse decode and computes the exact combined mask, then
stops at `$41ed8` before reading the first pre-existing destination word at
`$27f06`.

No merge write is admitted at this boundary because the destination word is
not yet owned. The caller-tail advance is replay-proof and session-scoped;
revocation discards it with the title state. This preserves the precise
read-before-write dependency of `$41eb0..$41f30` without treating zero-filled
host storage as original display memory.

The complete merge loop is admitted through narrowly typed observations of
the 960 unique pre-existing destination words and the 352 unique mask words
that lie in the decoder's `$38`-byte row gaps, each in exact first-read order.
The first such external mask is `$256fe`; decoded mask words remain owned and
are never redundantly observed. The original performs 2,208 word writes: three outer iterations,
184 words per iteration, and four planes. Later writes to overlapping
addresses consume the value computed by the preceding write rather than the
initial observation. For each word, the four decoded plane masks are ORed and
the selected plane is merged as `(old | combined) & (plane | ~combined)`.
All 960 final big-endian word effects commit as one bounded atomic batch; an
owned byte or observation ordering mismatch rejects the entire transition.
Replay and source revocation likewise leave memory unchanged. The routine
returns at `$41f30` through caller `$40514`, loads D0 `$004e`, and stops before
the next opaque call `$40518 -> $41bb4`. The resulting sparse bytes remain
runtime memory only: no complete framebuffer, palette, or renderer surface is
claimed.

The immediately following `$004e` call at `$40518 -> $41bb4` is now complete
through the same return. Its outer-table entry at `$416de` is hash-bound and
contains descriptor `$00c2`. Although the dispatcher initially loads outer
table base `$4129a`, the high-bit branch resets A5 to the fixed descriptor at
`$4128e`; `$4129a` is never reused as that descriptor. Index `$00c2` selects
pointer cell `$422b2` (`$00036978`), whose `$422fa` base resolves the source
header at `$78c72`. The entry, pointer cell, seven-byte `$0014/$8010` prefix,
and 229-byte payload `$78c76..$78d5a` are independently SHA-256 bound.

The high decoder exhausts that payload in 66 packets (33 literal, 32 fill,
one extended swapped, no short swapped) and produces exactly 320 pairs at
base `$256dc`, with wrap `$2bfc0` and row stride `$38`. The merge resumes with
A0 `$256de` and A1 `$256dc` for 16 rows, five words and four planes. Only the
last word of each row/plane is not already owned by the decode, so a typed
observation supplies exactly 64 words at `$256e6 + row*$38 + plane*$1a40`.
All 320 unique big-endian word writes commit atomically; contradictory owned
bytes, malformed address order, replay, and revocation reject without partial
effects. Execution returns through `$41f30` to `$4051e` and stops before the
next caller effect. These sparse words still do not establish a framebuffer,
palette, or parity claim.

The next caller effect at `$4051e` is now owned through the first opaque call
inside `$20e18`. The caller atomically writes longword `$0002151a` to
`$222ae`, clears D0, and enters the hash-bound service. With D0 exactly zero,
the deterministic prefix writes word zero to `$13006`, pointer literals
`$13050/$13008/$13024` to `$13052/$1301a/$13036`, reads the genuine static
word at `$19646` (zero for this hash-addressed release) into `$13126`, and
writes pointer `$1303a` to `$1304c`. All seven effects commit in one atomic,
replay-proof batch. The prefix then loads D0 `$00f9` and stops before JSR
`$20e6a -> $1fb9a`; that callee's result and side effects remain opaque.
An exact typed return observation now retains its D0 and SR without assigning
meaning to either. It must identify call `$20e6a`, target `$1fb9a`, return
`$20e70`, and a strictly later sequence. The deterministic continuation then
reloads word zero from owned `$13006`, adds literal `$00a0`, and reaches D0
`$00a0` at the next stop-before boundary `$20e7a -> $1ff08`. The opaque
callee's observed registers do not leak into this overwritten D0. An exact
typed return from that second call now preserves its D0/SR at `$20e80`.
The continuation again reloads owned `$13006` (zero), scales it by four, and
selects genuine immutable longword `$127a3980` from table `$13792`. That value
commits atomically to `$1378e`. An exact typed `$20e96 -> $22bca` return at
`$20e9c` retains its opaque D0/SR, then enters local `$20ba8`. Two ordered word
observations cover the genuinely mutable sources selected through owned
pointer `$1301a`: `(A0)` at `$13008` and counter `$202bc`. The first loop
iteration shifts the low byte of D7 and decrements the low byte of D5. Its
carry and zero outcomes are recorded exactly; when the clear-carry decrement
reaches zero, the resulting word is atomically written back to `$202bc` and
execution stops before `$20bd6 -> $41a68` with D0 `$0048`, D1 `$0010`.
The carry/zero skip instead reaches branch `$20bea` without a call. A bounded
loop state machine now accepts an exact `$41a68` return whenever the selected
iteration calls it, restores D7, performs the literal DBRA, and repeats using
the next immutable pair in `$20a14..$20a33`. Carry/zero iterations skip the
call locally; clear-carry zero decrements atomically update `$202bc`. After
exactly eight iterations D6 becomes `$ffff`, `$20bf0` returns to `$20ea0`, and
the local branch reaches `$20bf2`. An exact typed `$20bf4 -> $1f9b8` return
at `$20bfa` retains opaque D0/SR. The following pointer chain begins from
owned `$222ae == $2151a`; ordered typed reads supply the two genuinely mutable
longword links and final word. Their addresses must form the exact chain used
by `$20c00/$20c02/$20c04`. The final low byte selects descriptor `$00b0` only
when nonzero with bit 6 clear, otherwise `$00bd`; it commits atomically to
`$417a2`. D0 `$005c` selects that exact outer entry. Because `$00b0/$00bd`
have no high bit, `$41bb4` takes the direct `$41c32` route; their immutable
resource pointer cells are `$4226a/$4229e`, offsets `$0003227c/$00034b2a`,
and source headers `$74576/$76e24`. The remaining descriptor fields are X
zero, Y `$00b8`, destination zero, wrap `$1f3e`, and row stride `$0028`.
Destination zero explicitly falls back to mutable cell `$1f168`, so one typed
nonzero pointer observation is required at `$41c48`. Execution then stops
before the selected header read at `$41c72`; no earlier high-path payload is
silently substituted. Replay and revocation reject the boundary. No service,
object, display, or gameplay meaning is inferred.

That selected-header boundary is now crossed from original bytes. Descriptor
`$00b0` binds header `$74576` (`000c 0010`, SHA-256
`5b5f874b3e3dcaf3ab874493a0483ce5be66dbf1ce24f1a3b850594eaa93d61d`)
and payload `$7457a..$74aa1` (1,320 bytes, SHA-256
`67251004cede98024d69fff3b1bac02f7df956aca5422086f00a88825ad1366c`),
which yields 768 pairs across 12 words, 16 rows, and four planes. Descriptor
`$00bd` binds header `$76e24` (`000c 8010`, SHA-256
`3c388d6dfefe53e270955f50b2cfe452bb072e0c0025c2bfef2cb4518d97f054`)
and payload `$76e28..$76e2b` (four bytes, SHA-256
`8dca78516efa8b24c5a195cd4427fe196b4e15759c00882aa1a229ae99edd173`),
which yields 192 zero pairs across three words, 16 rows, and four planes.
Both use row stride `$28`, plane stride `$1a40`, and the already observed
destination pointer. Since every byte is overwritten, no destination-state
observation is needed; the writes commit as one atomic effect batch. `$41be6`
returns to `$20c2c`, loads D5 from the descriptor and D6=`$000b`, and stops
before the first reachable opaque `$20c4c->$41ad2` graphics helper (immediate
for `$00b0`, after one carry skip for `$00bd`).

The following caller loop is now bounded by the immutable 24-word table at
`$20a3c..$20a6b`. It shifts the selected descriptor once per iteration and
uses DBRA D6 from `$000b` through zero; D6 values four and five are explicit
no-call cases. `$00b0` therefore admits eight typed `$20c4c->$41ad2` returns
(D6 11, 10, 9, 8, 3, 2, 1, 0), while `$00bd` admits five (10, 3, 2, 1, 0).
Opaque D0/SR are retained at every return but never used to invent helper
effects. Loop completion deterministically stores `$00bd` at `$416b4` as one
atomic word and stops before mutable byte read `$20c6c` from `$20a10`.

The `$20c6c` read is now an exact typed byte observation. The caller performs
the admitted `add.b` against base descriptor `$00bd`, retaining only the low
byte, and atomically replaces word `$416b4` with the adjusted descriptor. D0
is then overwritten with selector `$004b`; execution stops before
`$20c7a->$41bb4`. This does not assume that a nonzero adjustment preserves
the `$00bd` resource route.

The adjusted route is now recovered for the genuine observed byte `$03`.
It produces descriptor `$00c0`; Eon preserves the separately hash-bound
12-byte suffix at runtime `$416b6` (SHA-256
`d36e52f85876496cb0123b9c5d4c0ad2bce2b7ecfad168c1c1fe247507060c2d`)
and never substitutes the `$00bd` descriptor or stream. The `$00c0` pointer
table entry selects runtime header `$770a0`, whose `0044 00a8` dimensions
(68 pairs by 168 rows) hash to
`fbaaa7db8117a83718be8cea03749e455893d2e0feb5e72c74d1158749bc7095`.
Its exact 6,659-byte compressed payload at `$770a4..$78aa6` hashes to
`8b535aadc3aaa48055ffaf3ce03339455ebcee3951e737ddedfb62d8b690b840`.
The bounded decoder consumes 1,271 packets (families 576/484/2/209) and
produces 22,848 bytes at 22,848 unique destinations using the descriptor's
X `$0000`, Y `$0088`, row advance `$0028`, and wrap `$1f3e` geometry.
Only the dynamic zero-pointer fallback read at `$41c48` is externally typed;
the complete decode commits atomically and returns to `$20c80`. These sparse
runtime writes are recovered execution state, not a renderer-surface or
visual-parity claim.

At `$20c80`, the caller's exact eight-byte pointer-load and zero-branch prefix
hashes to
`457462f38e994a97b0d37b21cbead532d6bfdb685fc3a8c8784cea654422357d`.
A typed longword observation at mutable cell `$19d1e` is committed atomically.
Zero reaches the local RTS at `$20cb8`; nonzero becomes A0 and stops before
the byte comparison at `$20c8a`.
On the qualifying nonzero route, one atomic typed observation supplies only
object bytes A0+`$ee` and A0+`$f0`. Values at least 8 and 1..2 respectively
cross the exact 30-byte compare/branch/table-load path at `$20c8a..$20ca7`,
SHA-256
`ee46bb6621a91b5c5055ed0c93b775b7015f12807b939d6f78a8203f558b3195`.
The immutable table prefix `$20a6c..$20a73` hashes to
`f366ff0abe5ea96505ad1c30bf834e5da3753159f02fc6892bc39d0f5c1dbc3c`
and supplies D0 `$0009`, D1 `$0398`. Execution stops before the first
`$20ca8->$41ad2` helper. A typed external return now retains its raw D0/SR
without assigning effects. The exact four-byte continuation at
`$20cae..$20cb1` hashes to
`405d4903b7cc8571505f6ed5c89f6f2a7d5fc9ed28d249f73d314932a8758db8`;
it loads the next immutable pair D0 `$0017`, D1 `$03a8` and stops before the
second `$20cb2->$41ad2` call. Eon does not infer either helper effect, the
second return, object meaning, or any visual meaning from sparse output.
A typed second return now retains its raw D0/SR without assigning helper
effects. The exact second-call-plus-local-RTS span `$20cb2..$20cb9` hashes to
`889c758fbfd514bc3633787bc2736b39a95aa712af79d4dc8f119eee6bbb65ab`.
Execution stops at RTS `$20cb8`: its stack-supplied destination and caller
continuation are not inferred, and the sparse descriptor bytes remain only
recovered runtime state.
A typed stack-frame observation now admits exactly the longword return
address `$40530` and commits it atomically at the observed A7 frame. This is
the instruction following the already hash-bound `$4052a->$20e18` call; no
arbitrary caller destination is accepted. The exact six-byte caller prefix at
`$40530` hashes to
`8a7c8b9593ae8d101806072aafa8cc8aa91a34dd4802e67af4fd16f3dc56c362`
and stops before its repeated `$40530->$20ba8` local service call. Its return,
effects, and any display meaning remain external boundaries.
That repeated call is now native through its complete hash-bound local body.
The six-byte caller instruction supplies return address `$40536`; no second
stack destination is inferred. The service reuses the exact 74-byte
`$20ba8..$20bf1` span (SHA-256
`25dcfad3d1b9298771e33cab73a4de86cd8ff9c27d7fdef787be5ef750f7035b`).
One ordered typed observation reads words `$13008/$202bc` at
`$20bae/$20bb6`. The original low-byte shifts, decrements, carry tests,
counter write, immutable D0/D1 table operands, and D6=`$0007` DBF geometry
then drive exactly eight iterations. Each reached `$20bd6` or `$20be4` call
requires its own typed `$41a68` return; raw D0/SR are retained without helper
effects. Counter changes at `$202bc` are applied atomically before session
commit. After the eighth iteration, local RTS `$20bf0` returns to the
caller-owned `$40536`. No service, counter, table, or display semantics are
assigned. From `$40536`, the exact 28-byte span through `$40551` hashes to
`58b17754e42e00bee2c320083fbe09c0fe79b0bda626b71f98fc043598033752`.
It loads A0 with literal `$20cfe` and calls it indirectly at `$4053c`. Only a
typed return to `$4053e` may continue. A typed long read from `$12fe4` is then
shifted right three bits and its low word is atomically stored at `$1f42a`.
Execution stops before external call `$4054c->$37180`; D0/SR from `$20cfe`
remain opaque and no rendering or gameplay meaning is assigned.
After a typed `$4054c->$37180` return to `$40552`, the exact 20-byte caller
span `$40552..$40565` / ADF `$9b552..$9b565` hashes to
`a208f64d43c08c1363f67586f924a5a7ae8143a8b40e691097ef3c29503666c3`.
A typed longword at `$1378e` is copied atomically to `$1c26c`; a typed word at
`$4040e` then follows the exact comparison and branch. Value five stops at
external `$40566->$36a8c`; every other word stops at `$4056e->$1fb9a`.
The service result and both selected callees remain opaque, and neither route
is assigned rendering or gameplay semantics.

The native runtime validates the two possible selected-call returns through
the coordinator/controller/host/menu facades. For typed mode five it
requires `$40566->$36a8c` to return at `$4056c`, then follows the exact
two-byte branch to the join. For every other typed mode it requires
`$4056e->$1fb9a` to return directly at `$40574`. Both paths converge only at
`$40574` and stop before external `$40574->$222c0` (return `$4057a`). D0 and
SR from the selected callees remain opaque. The next exact 82-byte span at ADF
`$9b574` / runtime `$40574` hashes to
`8050e7583fcee0b88783cfc53ad35b934145507c59ba23d746c598e4645ed98d`.
Typed returns are required for `$40574->$222c0` and `$4057a->$23e4e`. The
caller then compares typed words at `$1ffc8` and `$40414`; a change atomically
writes the new word to `$40414` and clears longword `$40410`. Otherwise the
typed `$40410` value is compared with `$ea60`; values below it join at
`$405c6`, as does typed inhibit word `$11` at `$22d34`. The remaining due path
requires a typed external `$405b6->$4069a` return at `$405bc`, then atomically
clears `$40410` and joins the not-due routes at `$405c6`. The 128-byte span at
ADF `$9b5b6` hashes to
`eee034f14ab5d9af283984b4d7f7f3c32763150cf1abcefb0dace2292e38cb9f`.
At the join, a typed byte from `$1bf36` follows the exact zero branch to
`$40638`. A nonzero byte enters external `$405d0->$1f9a4`. The exact 32-byte
caller span at ADF `$9b5d0` hashes to
`c40858408c0ce8e2754dd87f827e60bbcaf1dfa685cd2c8a418d25d17dcec5ce`.
That helper removes the stacked return PC and interprets the embedded bytes
at `$405d6..$405dd`, so its typed return must be `$405de`, not the ordinary
JSR fall-through `$405d6`. The caller then reads typed word `$22a0` and
requires `$405e4->$1fe88` to return at `$405ea`. The following 72-byte caller
span at ADF `$9b5ea` hashes to
`520c189626cdbcca3d175837cc8e5a3ee95887ef002d75d0d371aef8ae9f41e7`.
It requires typed returns for `$405f0->$1fe6c`, `$405fc->$1fe6c`, and
`$40608->$1fe7a`, paired with typed words `$1ffc8`, `$1ffce`, and `$22d34`.
The final gate reads `$1ffc8`; only zero reads `$1ffce`, and only a low byte
at least `$b4` reads `$1ffd4`. A nonzero first word, a smaller second byte, or
an even third byte joins at `$40638`; an odd third byte reaches the external
tail jump `$4062c->$37f56`. Helper effects and word meanings remain opaque;
this bounded caller block has no memory writes.

The joined `$40638` path begins with external `$1f238`, typed to return at
`$4063e`. Its exact 42-byte prefix at ADF `$9b638` hashes to
`3346989d04ad8b866f13f74c083f43976ba44ec83aab04ce0ef47fd66662fac4`.
A return whose D0 low byte is not `$43` follows the exact branch back to
`$40574`. For `$43`, a typed word at `$1bf36` is XORed with `$0101` and stored
atomically; zero selects literal `$00f0`, nonzero selects `$0f00`. The path
then stops before repeated external `$40662->$1f238` at return `$40668`.
The helper result beyond its typed D0/SR and the hardware colour meaning are
not inferred; no custom-chip write is committed at this boundary.

After that boundary, the exact 28-byte loop span at ADF `$9b656` hashes to
`f2f0719628f34964ec9c8554b08950a3c12e399aede501411898c3a07036f0c2`.
Each iteration writes the already selected `$00f0` or `$0f00` word to
`$dff180`, calls `$40662->$1f238`, and requires a typed return at `$40668`.
A D0 low byte other than `$43` repeats at `$40656`; `$43` exits through
`$40670` and the exact branch to `$40574`. The number of iterations and all
helper effects remain observation-driven. Each hardware word is committed as
one atomic native-memory effect, without assigning a renderer or gameplay
meaning to it.

The selected `$4062c->$37f56` tail is admitted separately. Its exact 40-byte
prefix at ADF `$92f56` hashes to
`51b8d6875ea6d0c35557c358d4fe22e4cac6cff79ead9df604d213cab1adfe1c`.
Typed returns are required for `$37f56->$3880a` and `$37f5c->$204fa`. The
caller then performs its literal DBF copy of exactly `$9392` bytes from
runtime `$13006` to `$66000`; the coordinator sources the bytes from the
hash-bound original title-stage span overlaid with every later byte tracked at
the runtime source, then publishes the destination as one atomic effect batch.
The sparse runtime-memory ledger need not materialize untouched load bytes
merely to prove this copy, while earlier admitted mutations remain observable.
Execution now continues through local `$37f7a->$37f9a`. The complete 152-byte
routine is independently fixed by SHA-256
`b076611efd33354e311dc9f64b57454e31cddd69c0749a05034f0d828a5b36c1`.
Its `$37fac->$208c0` service return is typed; only after that return does one
atomic batch write word `$000a` to `$1ef16` and long `$0001ef48` to `$1ef22`.
All Exec boundaries remain explicit and ordered. The common prefix calls
vectors `-$1ce`, `-$1c2`, and `-$168` with A1 `$1eefa`, `$1eefa`, and
`$1eed8`. Typed longs at `$20698/$2069c` select whether calls for `$2063e`
and `$20676` are skipped; both routes then call `-$1c2` with `$205e4` and
`-$168` with `$2061c`, returning locally at `$38030`. No Exec result is given
invented semantics.

After the proven RTS to `$37f7e`, a typed runtime read at `$206a0` supplies
the controller longword. A final atomic batch copies it to `$12ff8` and writes
profile longword `2` to `$12ffc`, exactly matching the hash-bound 28-byte
caller tail. The resulting `JMP $12800` is reported as the next bootstrap
boundary; it is not silently executed by this title-tail API. Wrong order,
address, vector, argument, comparison source, or replay rejects without
memory effects, and reset revokes the entire chain. The copied `$9392` region
is not classified as pixels, game state, or any other inferred semantic.
Helper registers, status registers, compared-long meaning and Exec effects
remain opaque.

The profile-two handoff is now connected to the recovered bootstrap rather
than ending at its reported jump. Advancement first requires the session-owned
longwords at `$12ff8/$12ffc` to equal the typed controller and profile `2`.
The clean ADF's `$12800` reset, `$12a4e` dispatcher, table `$12a36`, profile
routine `$12b44`, and its `BRA.B $12b1c` are revalidated through the complete
hash-locked title-stage profile. Profile zero supplies decoded-disk source
`+$5800`, destination `$20000`, byte count `$4200`, and entry `$21734`.
The exact 16,896 source bytes hash to
`a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6`
and are installed as one atomic native-memory batch. No caller may supply or
replace those bytes. Failure, an altered owned profile/controller cell, or a
replay leaves the previous title memory unchanged. The resulting state stops
at the genuine main-stage entry: entry registers, its first service calls and
the meaning of the selected title exit remain separate boundaries.

The renderer-facing consequence remains bounded by known pixels rather than
claiming a complete title frame. After the existing v4/v5 trace independently
admits four contiguous 320x200 planes (`$b5f0`, `$d530`, `$f470`, `$113b0`),
`$28` bytes per row and the palette at `$1ed24`, Eon can still expose the most
recent 32-byte command result as one immutable 8x8 Original patch. Every index
is reconstructed from its four plane bytes, most-significant bit first, and
every RGBA value comes from the original RGB4 palette.

The native runtime now also owns a session-scoped sparse planar surface. Each
accepted `$20..$8f` command is applied atomically only after a second check of
all 32 addresses against the recovered row/plane order. Successive commands
therefore accumulate into the proven 320x200 geometry, including legitimate
overwrites, instead of discarding every patch except the newest one. The
surface reports its last command generation, patch count, unique initialized
plane-byte count, decoded-pixel count and the matching runtime-memory
checksum. A pixel becomes valid only when all four of its plane bytes came
from admitted command effects. Unwritten pixels retain a zero validity byte
and zero alpha; their zero-filled storage is not asserted to be original
black. Both the 64,000-byte validity map and RGBA buffer are revoked with the
source session. Before display-trace admission, after revocation, or without
one complete command, no surface is published.

SDL consumes that snapshot through a dedicated streaming texture rather than
reusing the opening texture. The texture uses alpha blending: valid pixels
have alpha `$ff`, while every uninitialized pixel has alpha zero and reveals
the launcher's ordinary background. In particular, Eon does not composite a
sparse title patch over the last opening frame, clear unknown pixels to an
assumed Amiga colour, or pass the partial surface to Modern reconstruction.
The SDL cache key is the command generation plus native-memory checksum; the
texture is destroyed before the source-owning runtime is revoked. The visible
counter states the exact decoded-pixel coverage out of 64,000, so this
presentation cannot be mistaken for a complete recovered title screen.

The fourth and final batch edge `$40406..$4040b` / ADF `+0x9b406` is a direct
call to `$40698` and hashes to
`b214a93028755289cb8dcefb5e4013d307dc2e8a4bb27ae2e798a7bf10298606`. Its
complete target is exactly the two-byte `RTS` at `$40698..$40699` / ADF
`+0x9b698`, SHA-256
`1ceeabf0c6a5a30bad12cdac0e3ab015a7188a42e6aebb556aad00bb9cd693ad`.
`DeuterosAmigaTitlePostExecFourthServiceProfile` also validates the enclosing
batch `RTS` at `$4040c`, preserving caller return `$4040c` and batch return
`$4040e` as byte facts. It does not assert that earlier calls return, cross
the preceding Exec boundary, or execute either return.

The third-service dispatcher only reaches its absolute `JMP $201d2` after the
three graphics-library vectors in `$20094` return. The target `$201d2..$2021d`
is a complete 76-byte local dispatch at ADF `+0x7b1d2`, SHA-256
`6947fb7ffcbfaadd0ce420648741b46539f5dce188e4c26ba7fd18351852c658`. It
saves A0/A6, has static BSR operands to `$200fa`, `$20118` (twice), and
`$200dc`, then restores A0 and RTSes at `$2021c`. Those destinations contain
further graphics ABI boundaries, so `DeuterosAmigaTitlePostExecTailDispatchProfile`
records only the exact control-flow bytes and return `$2021e`; it neither
executes a BSR, supplies a vector result, nor infers any title or display
effect.

The first of those BSRs is `$201d6..$201d9` / ADF `+0x7b1d6`, targeting
`$200fa`. Its four-byte operand hashes to
`fd55349ce2476b466426a5addfa7eedae100cddaac5a480512c6eff31a06a450`. The
complete callee `$200fa..$20117` / ADF `+0x7b0fa` is 30 bytes and hashes to
`6e36c860c280c651947ad0ea6ef868759fbc7bfac67d89af219135e4751e6e6f`. It
loads A0/A1 from literals `$12e12`/`$1ffda`, A2 from pointer cell `$2008e`,
and A6 from graphics-library base cell `$12fec`, then calls vector `-$1a4`.
`DeuterosAmigaTitlePostExecTailFirstCalleeProfile` binds those exact bytes,
the vector return `$20116`, local RTS boundary `$20118`, and caller
continuation `$201da`. It does not call the vector, read the pointed-to A2
value, assume a vector return, or infer any graphics effect.

The following BSR in the same dispatcher is `$201fe..$20201` / ADF
`+0x7b1fe`, which hashes to
`8919a0658d9b7a79bca49d3ca3f38227e3ee6a043491ebac0dbb395504b33fd9` and
targets `$20118`. Its complete 168-byte local routine `$20118..$201bf` / ADF
`+0x7b118` hashes to
`9b16e7cdc97495a1b52656d49c7a3612e7e1617ce88996e2c5e7138e3f183ec3`.
It contains two mirrored bounded selection blocks over literal cells
`$1ffc8/$1ffca/$1ffcc` and `$1ffce/$1ffd0/$1ffd2`, then loads A0/A1 from
`$12e12/$1ffda`, applies literal opcodes `$0440 #$0010`, `$5d41`, and
`$e248`, loads A6 from `$12fec`, and calls graphics-library vector `-$1aa`.
`DeuterosAmigaTitlePostExecTailSecondCalleeProfile` records these byte facts,
the vector return `$201ba`, RTS `$201c0`, and caller continuation `$20202`.
It neither supplies input cells, invokes the vector, assumes any ABI return,
nor labels the selection or graphics effect.

The third BSR in that same dispatch is a separate, later call site at
`$20212..$20215` / ADF `+0x7b212`. Its four-byte operand is
`61 00 ff 04` (SHA-256
`a760d59c7213517e7d3427b30915f9c586be5448e40a0a3980f9dded55f9f994`) and
re-enters `$20118`; its caller continuation is `$20216`. The re-entered
168-byte routine is the same hash-locked span already recorded above, ending
at RTS `$201c0`; it must not be mistaken for the earlier call at `$201fe`.
`DeuterosAmigaTitlePostExecTailThirdCalleeProfile` therefore binds this
distinct caller edge to that existing local routine hash. It does not infer
that any preceding graphics vector returns, execute the re-entry, or ascribe
selection/display semantics to the code.

The fourth and final BSR in `$201d2` is `$20216..$20219` / ADF `+0x7b216`:
bytes `61 00 fe c4`, SHA-256
`6b8c80452bd43c82d8ce91fa551b3067dfc33bb85e553d555aaec65ea6a8ce26`, target
`$200dc`, continuation `$2021a`. Its target `$200dc..$200f9` / ADF `+0x7b0dc`
is a separate 30-byte entry with SHA-256
`6e36c860c280c651947ad0ea6ef868759fbc7bfac67d89af219135e4751e6e6f` (the
same bytes as the independently reached `$200fa` wrapper). It loads literal
A0/A1 `$12e12/$1ffda`, A2 from pointer cell `$2008e`, A6 from `$12fec`, calls
graphics-library vector `-$1a4`, then has vector-return `$200f8` and local
RTS boundary `$200fa`. `DeuterosAmigaTitlePostExecTailFourthCalleeProfile`
binds the distinct caller and target spans without collapsing duplicate bytes
into one edge. It does not read pointer cells, invoke the vector, assume that
any BSR/vector returns, or infer a graphics/title effect.

If—and only if—all four tail BSRs and their unresolved vector calls return,
the enclosing `$403f4` batch returns to `$404d4`. The 28-byte continuation
`$404d4..$404ef` / ADF `+0x9b4d4` hashes to
`32a750150f115f5c012e99811313916078a8657c6100b50e92acadca0708965d`.
It sets A0 to literal `$12ff4`, transfers two successive longwords into
`$37ef2` and `$37ef6`, then calls local `$204c8`. The complete local span
`$204c8..$204f9` / ADF `+0x7b4c8` is 50 bytes and hashes to
`76f4163c15e6761168f1d267e3feae94f0430975efa75b1c3576d7b88947e596`.
It loads A1 with `$204aa`, writes literals `$0002`/`$00c4` at offsets
`$0008`/`$0009`, writes long literals `$204c0`/`$202ca` at `$000e`/`$0012`,
loads D0 `$00000005`, then obtains the unknown Exec base from `$4` and calls
vector `-$a8(A6)`. The vector-return instruction is `$204f8`; the local RTS
ends at `$204fa`. `DeuterosAmigaTitlePostExecTailReturnProfile` records only
these caller-connected byte facts. It does not read the table, perform the
writes, invoke or identify the Exec vector, presume any return, or infer a
display/title effect.

Only if that final `-$a8(A6)` vector returns and the wrapper RTS at `$204f8`
unwinds to `$204fa` does its caller continue at `$404f0`. The following
296-byte span `$404f0..$40617` / ADF `+0x9b4f0` has SHA-256
`10a96a2c80f83b32530ed9355cb2988bcac233c49f66d93484b31d0c0e3667c6`.
It has direct absolute-long operands (in instruction order) to `$389e2`,
`$1fb9a`, `$38912`, `$2022a`, `$41bb4` twice, `$20e18`, `$20ba8`, `$37180`,
the `$4040e == 5` alternatives `$36a8c`/`$1fb9a`, `$222c0`, and `$23e4e`.
Its one indirect `JSR (A0)` follows literal A0 `$20cfe`, which is not
dereferenced. The same bounded span has raw timer operands `$40410`/`$ea60`,
inhibit comparison `$22d34 == $11`, and local call `$4069a`; it ends before
the next flag-gated instruction's operand at `$40618` (`$1bf36` is merely
recorded as a cell operand). `DeuterosAmigaTitlePostExecTailReturnContinuationProfile`
binds those bytes without entering calls, providing ABI results, or assigning
game/display semantics.

The fourth direct operand in that continuation has an independently bound
caller edge: `$40504..$40509` / ADF `+$9b504` encodes `JSR $2022a` and has
SHA-256 `ce9c44a0a83e370fdf54b5ec8ef0ffd72c170b007419176403293d2a54f91188`.
Its 76-byte local region `$2022a..$20275` / ADF `+$7b22a` has SHA-256
`a7f7c0c3efa60284b3d292249b3560da4d832ff0c5dfa34711b72604760b39a9`.
The entry records a local BSR to `$20238`, a raw clear of `$1ffd9`, and RTS
`$20236`. Its first local subroute tests `$1ffd8`, retains raw literal
`$1ffe6` for pointer cell `$2008e`, stores raw byte `$01` at `$1ffd8`, and
branches to `$200dc`; adjacent entry `$20258` records the complementary raw
test/clear of `$1ffd8`, literal `$2001e` for the same cell, branch `$200fa`,
and RTS `$20274`. `DeuterosAmigaTitlePostExecPointerRouteProfile` hash-locks
the caller and this whole region. It does not read either flag, choose a
subroute, write the pointer cell or flags, enter either graphics wrapper, or
assign a title/display meaning to the literals.

The immediately following paired callers are independently bounded. `$4050a`
loads D0 `$004d` then calls `$41bb4` at `$4050e`; `$40514` loads `$004e` then
repeats the call at `$40518`. The respective four-byte literal spans hash to
`ff1173e9ce1a06c3bc789122e2ee27b0a2b74aaeb8a832269f3e6a0a0475ec8a` and the
identical six-byte calls hash to
`d24863f099c973ddfd0f1567378d2a5e15b9753567fb3e6f71f75f19b10471c6`.
The local dispatcher `$41bb4..$41c31`, low block `$41c32..$41e41`, and high
continuation `$41eb0..$41f31` hash respectively to
`fba4dff4da954290d970f5ec129220c179a2ef73f010def6512401380b8640cc`,
`765489ec36d727a326bfae44e34918cb85070d4ed3ef959cdcba9c41a102dd7e`, and
`96e344839df3e0fc7b2106541b7fea45de269e0c14e5d592a4ad3debbfe7448f`.
It records table literal `$4129a`, bit 15, local target `$41c32`, continuation
`$41eb0`, low-block return sites `$41d42`/`$41e40`, and RTS `$41f30`.
`DeuterosAmigaTitlePostExecPairedLocalRouteProfile` never reads the table or
cells, takes a branch, performs a copy/write, or assigns visual semantics.

The following caller `$4051e..$40531` has SHA-256
`98306b421ce3f0216642ad091dc72ffb63ab1325b68c839b8814d4e4fc25dac6` and
reaches local `$20e18..$20ea3` (140 bytes,
`037c48dd824e064d3734fb4b72b6e649bfda6b9a7a764147a76690f4ce9506e0`). Its
first `JSR` is at `$4052a` after `MOVEQ #0,D0`. Its three absolute call operands
are `$1fb9a`, `$1ff08`, and `$22bca`; a local
BSR enters `$20ba8..$20bf1` (74 bytes,
`25dcfad3d1b9298771e33cab73a4de86cd8ff9c27d7fdef787be5ef750f7035b`), whose
DBF loop has RTS `$20bf0`. After its return the parent branches `$20bf2`.
`DeuterosAmigaTitlePostExecServiceRouteProfile` hash-locks these spans without
entering any external call, reading source cells, selecting loop branches, or
writing original state.

The verified BSR return joins a caller-owned continuation at `$20bf2`. Its
200-byte span `$20bf2..$20cb9` / ADF `+0x7bbf2` hashes to
`98f43a011e13678af312563611740122ee9eb4fc163d1290a2c5e3dc66315385`.
It calls `$1f9b8`, then conditionally calls `$41bb4` twice and `$41ad2` up to
three times before RTS `$20cb8`. Raw table operands are `$20a3c` and `$20a6c`;
the two display-word destinations are `$417a2` and `$416b4`; the later object
pointer cell is `$19d1e`. `DeuterosAmigaTitlePostExecServiceContinuationProfile`
records these literal boundaries only. It does not take predicates, dereference
the pointer/table cells, enter an external service, dispatch graphics, write
display words, or infer a title/display result.

The next complete instruction starts at `$40616`, so
`DeuterosAmigaTitlePostExecTailFlagGateProfile` deliberately overlaps the
preceding span's final opcode word. Its 94-byte span `$40616..$40673` / ADF
`+0x9b616` hashes to
`fcf7c15552302b6b902352380a5b5d454eba190be2a7e89af9701822eac1f80e`.
It records raw word operands `$1ffce`/`$1ffd4`, comparisons `$00b4`/`$0043`,
the two branches to `$4063a`, absolute jump `$37f56`, calls `$1f3f8` and
`$1f238` (twice), comparison `$1bf36 == $0101`, literals `$00f0`/`$0f00`,
and the word destination `$dff000 + $0180`. Its local loop is `$40658`; the
exit branch has raw target `$40576`. The profile stops before padding at
`$40674`. It never reads cells, takes branches, follows calls/jumps, writes
the custom-chip address, or attributes hardware/game semantics to these
bytes.

The flag gate's first direct call is independently caller-connected at
`$40632..$40637` / ADF `+$9b632`: `JSR $1f3f8`, SHA-256
`c3998d07f8e89408b9332ae19f449256087b1eb8843256751c03e52700cbbec4`.
Its complete local target `$1f3f8..$1f419` / ADF `+$7a3f8` is 34 bytes with
SHA-256 `101f4026b51a3c0bef3758f4244fffd3fe12c93d76e37b44d0728295b5e27aa6`.
It tests byte `$1ee16`; the zero branch reaches RTS `$1f400`. Otherwise it
reads word `$1ffd4`, applies raw immediate byte mask `$03`, and has a BNE
backedge to `$1f402`; a second `$1ffd4` read, one-bit word shift, and BCC
backedge reach terminal RTS `$1f418`. The caller continuation is `$40638`.
`DeuterosAmigaTitlePostExecTailFlagGateFirstCalleeProfile` hash-locks the
caller and every byte of that routine. Project Eon supplies no cell values,
does not enter either polling loop, and does not assume a return.

The flag gate's two remaining direct calls are separately caller-connected.
Both `$40638..$4063d` and `$40662..$40667` / ADF `+$9b638` and `+$9b662`
encode the identical `JSR $1f238` and each six-byte caller hashes to
`88e2b3531aa5cb582d1ed1a672f9a524c89cbdf572c7a7d77c8cc7f4e6db695d`.
Their continuations are `$4063e` and `$40668`. The complete local target
`$1f238..$1f259` / ADF `+$7a238` is 34 bytes and hashes to
`9c0ffcff9d88feedca2b8079b14f5a32fb51dac94bee60e1c477c746e7c6c4f0`.
It reads word `$1eed6`; its zero branch reaches the increment at `$1f252`.
The nonzero path sets both A0 and A1 to `$1eec0`, executes one `MOVE.B
(A0)+,(A1)+` at `$1f24c`, loads D1 with `$13`, then `DBRA`s from `$1f24e`
back to that same DBRA instruction before incrementing `$1eed6` and returning
through `$1f258`. `DeuterosAmigaTitlePostExecTailFlagGateCopyCalleeProfile`
hash-locks the callers and routine without supplying the gate value, reading
or writing those cells, executing the transfer or delay loop, choosing its
branch, or assuming a return.

# Millennium DOS sixth-function runtime boundary

# Millennium DOS second-function runtime boundary

The exact English `2200AD.EXE` F2 handler `$71ca..$7220` is owned only after
the authenticated post-overlay dispatcher admits table index 1 and handler
`$71ca`. The native session observes byte `$da26`; values below two enter the
original `$09fa`/BL wait boundary. The admitted path records the literal
callback/list-mode stores, observes calls `$4d2c` and `$4d36`, and only then
reconstructs the original `(availability-1)` word list at `$6e99`, starting at
`$1384` with stride `$00c0`. Calls `$72b5` and `$0b76` remain externally
observed boundaries. The callback beginning at `$7221` is excluded: Eon does
not invent its runtime table values, call returns, selection, or game meaning.

The callback itself is now independently modeled as a hash-gated continuation
from `$7221` through the tail jump at `$7253`. It requires observations for
`$6e93`, the computed word cell `$27c4 + 2*(selection+1)`, each native call
return, and the `$09fa` BL loop. The `$ff` wrap route stops at jump `$702c`;
the normal route stops at jump `$0bdf`. Merely storing `$7221` in `$6f98` does
not prove invocation. Runtime ownership requires both the retained completion
checkpoint for F2 index 1 (`$71ca`, terminal RET `$7220`, dispatcher return
`$d40d`) and a separate exact entry-IP observation at `$7221`. Wrong,
premature, stale, and source-revoking observations are rejected. No record
content, menu selection meaning, callee result, input, rendering, or
destination-jump behavior is manufactured.

The callback's `$ff` wrap branch advances only after an explicit observation
of jump `$7228 -> $702c`. The exact `$702c..$7040` continuation observes
returns from calls `$0bd7`, `$4bf7`, `$0b76`, `$0bdf`, and `$0b0c`, then
records the literal byte write `$da1e = 0` before its local `RET`. No callee is
executed or assigned a result. The normal tail jump `$7253 -> $0bdf` remains
an external handoff: `$0bdf` is a shared runtime-branched routine and its
eventual return target belongs to the unobserved callback caller. Eon does not
claim that it returns to the post-overlay loop.

The normal F2 callback tail admits the shared `$0bdf` routine only after a
sequence-bearing external-transfer observation exactly matching `$7253 ->
$0bdf`. The exact-identity service observes `$07d8`; its nonzero path stops at
RET `$0be6`. The zero path observes words `$0107` and `$07d6`, plus a second
unchanged `$07d8` read, records only the literal XOR toggle, then stops at call
`$09fa`. Its explicit return observation must supply CX/DX before the literal
stores to `$07da`/`$07dc` are recorded and the next `$da05` read becomes
visible. ES memory, poll results, mode, rendering, and the callback caller's
return address remain evidence boundaries.

From `$0c19`, the service retains the observed poll CX and reproduces only the
literal shift selection encoded for raw `$da05` values 3 and 4 before exposing
call `$04ef` at `$0c34` with its exact AX input. After an explicit return and a
second `$da05` observation, value 2 stops at jump `$11f7`, values other than 1
stop at jump `$0caa`, and value 1 observes `$07d8` before stopping at either
far-memory boundary `$0c6d` or `$0c8b`. Eon does not dereference ES:DI, infer
the `$04ef` result, identify the modes, or enter either jump target.

The exact English `2200AD.EXE` sixth-table handler `$7415..$7454` is available
through the native runtime only after the active post-overlay state machine has
reached the authenticated `$d40a -> $76f1` dispatch boundary with scaled index
5 and handler `$7415`. Typed observations and value-only checkpoints propagate
through the coordinator, native controller, host, and launcher layers. Source
revocation destroys the span-backed session before its admitted media owner and
rejects subsequent observations. See `MILLENNIUM_DOS_SIXTH_FUNCTION.md` for the
instruction-level evidence and excluded `$7455` restoration routine.
### Millennium DOS `$0c6d` / `$0c8b` far-memory copy continuations

The hash-bound `$0bdf` service now owns both mode-1 copy branches through their
local returns.  For the non-zero `$07d8` branch, `$0c6d` reads exactly 15 rows
of eight bytes from the observed `ES:DI`; `$0c70` records each byte at
`CS:$07fa..$0871`, `$0c74` observes the corresponding `CS:$08ea..$0961` byte,
and `$0c7b` records the selected byte written back to `ES:DI`.  A zero source
byte selects the observed far-memory byte; a non-zero source byte selects
itself.  `DI` advances eight bytes and then `$0138` per row, ending at the
proven RET `$0c8a`.

For the zero `$07d8` branch, `$0c8b` loads the previously observed segment from
`$0107`; `$0c9b` observes 60 consecutive words from `CS:$07fa..$0871` and
records four far-memory word writes per row at the observed mapping-return
`DI`, with the same `$0138` row stride, ending at RET `$0ca9`.  Segment, offset,
width, value, and instruction address are retained in typed checkpoint effects.
No framebuffer meaning, display mode, or pixel semantics is inferred.  The
admitted executable remains SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`.
### Millennium DOS BDF mode-two `$11f7` zero branch

After the existing one-way `$0c4b -> $11f7` transfer is explicitly admitted,
the runtime now requires the caller-observed entry `DI`; it never supplies a
default video offset. The exact executable hash
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`
binds the continuation. At `$11f7` it observes `CS:$07d8`. For the zero branch,
`$129d` observes the segment word at `CS:$0107`, then `$12af` consumes exactly
64 consecutive words from `CS:$07fa..$0879` and records the corresponding
`ES:DI` word effects. Four words form each of 16 rows. The next row applies the
literal `+0x2000`; when bit 15 becomes set it applies
`(value & 0x7fff) + 0x00a0`. The state ends at the proven RET `$12cb`.
For the nonzero branch, `$1203` explicitly observes the same segment cell and
the runtime then accepts exactly 64 `DS:SI` word observations at `$1218`.
Their segment, offset and value are retained, while the instruction-defined
copies are recorded consecutively at `CS:$07fa..$0879`. Source offsets use the
same four-word rows and plane/wrap calculation as the zero path. At `$123a`
the runtime explicitly observes `CS:$07da` and retains only its instruction-
defined low bit. For each of 16 rows, `$1251` and `$1256` observe the words at
`CS:$0982+2*row` and `CS:$0962+2*row`; their byte swaps and the conditional
single-bit rotates are reproduced exactly. Eight explicit `ES:DI` byte reads
per row are then transformed only by the four proven shift/carry mask steps at
`$1266..$127c`, and their `$127e` writes are retained as typed byte effects.
`DI` advances eight bytes and applies the same plane/wrap calculation before
the next row. The state ends at the proven RET `$129c`. No framebuffer, plane,
pixel, colour, or display meaning is inferred from these addresses.

Both mode-two exits are now caller-connected only as observed external-return
events. The transfer admitted at `$0c4b -> $11f7` accepts `$12cb` for the zero
path or `$129c` for the transformed path only after the corresponding native
session has reached that exact RET. The return event must have a sequence
strictly later than its entry and a non-zero, explicitly supplied destination.
The destination is retained in the transfer checkpoint. No static evidence
currently proves which caller instruction receives these RETs, so accepting
the event does not resume or synthesize any caller state. Revocation clears
the session and transfer together, making late return observations invalid.

### Millennium DOS BDF other-mode `$0caa` zero branch

The independently admitted `$0c4e -> $0caa` terminal edge now creates a
hash-bound session from explicit entry `DL` and `DI` observations. Entry
`DL == 4` is retained as the exact `$0caf -> $0d68` boundary and is not
followed here. Other values observe `CS:$07d8` at `$0cb2`. Its zero branch
observes the segment at `$0d3e` / `CS:$0107`, then consumes exactly 64 words
from `CS:$07fa..$0879` at `$0d56`. The four instruction-defined output-port
writes to `$03c5` retain values 1, 2, 4, and 8. Each group of 16 words records
far writes to the explicit segment at entry `DI + row * $28`; `DI` is restored
between groups exactly as the push/pop sequence specifies. The session ends
at RET `$0d67`, whose external-return contract requires a later sequence and
explicit destination. The `$0cbe` nonzero branch remains an explicit boundary;
no display-plane meaning is inferred from the ports or addresses.

The `DL == 4` continuation is now advanced through its stronger zero-toggle
path. It observes `CS:$07d8` at `$0d68`; nonzero stops explicitly at `$0d74`.
Zero observes the segment at `$0e29` and then consumes 64 three-byte records
from `CS:$07fa..$08b9`: a word at `$0e41` followed by a byte at `$0e42`.
The exact writes use entry `DI + row * $50` and the following byte, with `DI`
restored for each of four groups. Port `$03c5` again receives 1, 2, 4, and 8.
The local path terminates at RET `$0e53`; return ownership uses the original
`$0c4e -> $0caa` transfer and does not invent a caller destination.

The `DL == 4`, nonzero-toggle path is also owned through RET `$0e28`. `$0d74`
observes the segment at `$0107`; `$0d8e/$0d8f` then consume four groups of 16
three-byte far-memory records and retain their exact copies to
`CS:$07fa..$08b9`. The four `$03cf` writes are recorded. `$0da5` explicitly
observes `CS:$07da & 3`. For every one of four output groups and 16 rows,
`$0dd1` observes the mask word at `CS:$09c2+2*row`; the literal two-step
carry rotations are applied that many times before explicit far word/byte
observations are ANDed at `$0dec/$0def`. `$0df3` observes the merge word at
`CS:$09a2+2*row`, applies the corresponding shifts/carry rotations, and
records the OR results at `$0e0a/$0e0d`. Far offsets advance by `$50` per row
and reset per group. Exact `$03c5/$03cf` selection effects are retained. No
meaning is assigned to these buffers, ports, masks, or output groups.

The remaining non-`DL == 4`, nonzero-toggle branch is owned through RET
`$0d3d`. `$0cbe` observes the segment, and `$0cd8` consumes four groups of 16
far words into `CS:$07fa..$0879`, retaining the four `$03cf` selector writes.
After explicit `$0cee` observation of `CS:$07da & 7`, each of four groups and
16 rows observes its mask word at `CS:$0982+2*row`, rotates it by the proven
count, observes and ANDs the far word at `$0d1e`, then observes, rotates, and
ORs the pattern word from `CS:$0962+2*row` at `$0d21/$0d24`. Far offsets use
the exact `$28` row stride and reset per group; `$03c5/$03cf` effects are
retained. No memory or port semantics are inferred.
