# Project Eon

Project Eon is an open-source, cross-platform reimplementation of Ian Bird's
science-fiction strategy games **Millennium 2.2: Return to Earth** (1989) and
**Deuteros: The Next Millennium** (1991).

| Millennium 2.2 | Deuteros |
| :---: | :---: |
| [![Project Eon Millennium 2.2 card](assets/cards/millennium.png)](assets/cards/millennium.png) | [![Project Eon Deuteros card](assets/cards/deuteros.png)](assets/cards/deuteros.png) |

The goal is to make both games fully playable from legally obtained original
game data while preserving their rules and atmosphere. The target architecture
has a deterministic recovered baseline and two explicitly selected runtime
modes; recovered paths are added only when original media evidence supports
them:

- **Original** — the preservation contract: authentic recovered artwork,
  sound, layout, timing and behaviour, with no host-side feature substitutions.
- **Modern** — an explicit opt-in improvement profile. It may provide
  high-resolution graphics, scalable UI, modern input, accessibility features
  and later evidence-documented gameplay improvements.

Original media is immutable in both modes. Modern features must be visibly
labelled, separately configurable, and must never silently alter Original
mode, original asset bytes, or original save files. The current Modern
implementation is renderer-only and can be switched without restarting or
converting state; later Modern features must document their state contract.

### Mode contract

| Contract | Original | Modern |
| --- | --- | --- |
| Purpose | Reproduce proven original behaviour | Opt-in host-side improvements |
| Logic | Recovered baseline only | Baseline plus individually documented, enabled improvements |
| Graphics and input | Recovered presentation and controls | May add rendering, regenerated/upscaled art, input, accessibility, and quality-of-life options |
| Original media and saves | Read in place; never changed | The same; no alteration or replacement |
| Compatibility claim | Preservation evidence and reference captures | Each improvement states its scope and compatibility boundary |

Modern is allowed to pursue substantial improvements where they are useful,
including the kind of graphics, camera, input, accessibility, and diagnostics
work found in contemporary recompilation projects. Such work is never evidence
for the original game: it must be opt-in, identified in the UI and
documentation, and must not weaken Original-mode verification.

Modern graphics are not limited to scaling filters. They may use
asset-aware reconstruction, high-resolution redraws, regenerated sprites, and
other replacement presentation derived from the player's verified original
media. These renderer-side derivatives are generated or loaded separately at
runtime, are visibly identified as Modern, and never overwrite, cache inside,
or replace the original archive. Project Eon will not distribute unlicensed
original assets or derivative asset packs.

Modern also has a deliberately external **asset-pack admission format** for
future lawful high-resolution art layers. A user may place a pack below a
separate, user-selected Modern-pack root; its `pack.eonmodern` manifest is
bound to one exact recognised source-release SHA-256 and verifies every asset
by path, byte length, and SHA-256. Discovery is read-only and non-recursive.
Eon currently renders only a deliberately bounded mapping: an explicitly
selected, reverified pack may replace the recovered English Millennium DOS P00
title in Modern mode with an independently supplied 640×400 or 1280×800 RGBA
PNG. It never ships a pack, automatically selects one, or changes game logic,
saves, or supplied media. See `docs/MODERN_ASSET_PACK_FORMAT.md` and the
[Modern presentation profiles](docs/MODERN_PRESENTATION.md).

## Original game data

Project Eon does not distribute commercial game files. Players provide their
own disk images or installed data. The importer fingerprints files and decodes
only bounded, hash-identified resources from platform-specific containers;
physical media such as STX remain in their original container form.

| Game | DOS | Amiga | Atari ST |
| --- | :---: | :---: | :---: |
| Millennium 2.2 | Supported target | Supported target | Supported target |
| Deuteros | — | Supported target | Supported target |

The research corpus currently includes English releases on every platform in
the table and a Spanish DOS floppy release of Millennium 2.2. It also contains
alternate disk dumps; these are identified by content hashes rather than by
their often inconsistent filenames.

## Project goals

- Reproduce the complete campaigns, simulation rules, AI and progression of
  both games—not merely their opening screens.
- Load all supplied DOS, Amiga and Atari ST data variants through tested,
  read-only importers.
- Match reference captures in Original mode, including graphics, audio, input,
  interface layout and timing.
- Provide a polished Modern mode with opt-in improvements while retaining a
  separately verifiable Original mode.
- Support deterministic, versioned saves and migrate imported original saves
  where their formats can be verified.
- Run on current desktop operating systems without requiring the original
  hardware or an external emulator.
- Document the reverse-engineered formats and behaviour without committing or
  redistributing copyrighted assets.

## Architecture

```text
DOS / Amiga / Atari ST data
            │
      platform importers
            │
 canonical assets + recovered state
            │
 deterministic simulation (as recovered)
       ┌────┴────┐
 original UI   modern UI
```

The native runtime uses **C++20 and SDL3**. SDL supplies windows, rendering,
audio, keyboard/mouse/gamepad input and OS integration; decoded resources and
the deterministic simulation remain independent of SDL.

## Native build

SDL3 and CMake are required. SDL3_image 3.4.4 is fetched automatically when a
system package is unavailable.

```sh
cmake -S . -B build -G Ninja \
  -DEON_REAL_DATA_DIR="$HOME/Downloads"
cmake --build build
ctest --test-dir build --output-on-failure
```

Start the graphical card menu:

```sh
./build/project-eon
```

The start menu is a three-card journey: click a game card, then choose one of
that game's supported platform cards, then choose a presentation card.
Millennium offers DOS, Amiga, and Atari ST; Deuteros offers Amiga and Atari
ST. Every supported platform card visibly reports whether hash-verified
original media is currently available; unavailable cards are dimmed and cannot
proceed or start a game. Unsupported targets are never shown as if their media
were merely missing. Before either profile starts, the selected game, platform,
release language, and full outer SHA-256 are resolved together into one
immutable launch request, so the menu never substitutes another platform's or
language's media. If a supported platform has several recognised outer
containers, Project Eon shows a release-identity card page before the profile
cards. It displays up to four identities as a two-by-two grid and pages larger
sets with visible previous/next page controls as well as highlighted-card navigation; clicking a card always carries
its complete hash-sorted identity rather than a page-local position. Original and Modern
cards start directly. The Custom card is not a third runtime mode: it opens
Modern's fine-tuning panel, then presents an explicit start action using those
Modern settings. Mouse and iPad touch input activate the same card-admission
route. A device-independent launcher state machine owns those transitions, so
keyboard, gamepad, mouse, and touch cannot follow different release-admission
rules. Keyboard and gamepad users can move through each card page with the
D-pad or arrow keys and activate the highlighted card with Enter, Space, South/A, or
Start; Escape returns to the preceding card page. Press `L`, click/tap the left or right
half of the visible language button, or use gamepad Left/Right Shoulder to select the
previous or next launcher UI language; it changes only Project Eon's interface, never
the original release language. Mouse and touch users can
use the visible `<<` control to go back and the release-page controls to page
through identities.

If a selected profile cannot enter the final runtime boundary, the profile
page reports the safe rejection class—identity, archive hash, or adapter—next
to the cards. It does not expose paths, archive members, original bytes, or
parser exception text. The same status remains available in F10 diagnostics.

Use **Choose Original Data Folder** (or `O`) to select a media folder, or
**Choose Original Archive** (or `A`) to select one original archive without
restarting the launcher. The menu passes the selected path unchanged to the
same bounded, read-only two-phase scanner as `--data`: a folder is discovered
within a frame budget and then hashed in deterministic lexical order; an
archive is one bounded candidate. The picker neither opens nor trusts either
source. The same non-symlink directory-or-regular-file classifier is used by
the launcher, CLI, and scanner, and Project Eon never copies, unpacks, creates,
or modifies selected data. When scanning a directory, links inside it are also
rejected rather than silently following media outside the selected collection.
Press `D` in the start menu to view the same aggregate-only source, progress,
and rejection diagnostics as the CLI inspection report. It never displays
unrecognised names, paths, archive members, or original bytes.

Recognised SDL gamepads provide the same launcher controls: D-pad Left/Right
selects a card on the current page, Left/Right Shoulder changes the launcher UI
language, and South/A or Start activates it. During
the recovered Deuteros Amiga opening, hold South/A for
the same verified physical input signal as Space/Enter; it is not mapped to any
invented title or gameplay action.

The profile card fixes Original or Modern before a game starts; F1 deliberately
does not switch an active session. F10 always opens Project Eon's input-modal
renderer panel and never becomes original-game input. In Original it exposes
only output resolution and aspect ratio. In Modern or Custom it additionally
offers renderer presets, scaling, scanlines, frame, pacing, external Modern
art selection, and read-only diagnostics. Up/Down and Left/Right (or the
gamepad D-pad) select and change the visible renderer-only options, while
Escape or gamepad Back closes the panel. On touch devices, tap an option row
to cycle it or tap outside the dialog to close it; this uses the same
renderer-space coordinates as the card menu and stays modal. Resolution presets
control the SDL window only. Aspect-ratio presets are Original 4:3 (the
default), Square Pixels 8:5, and Widescreen 16:9. The renderer fits and
centres its viewport within the available region; it never crops a recovered
frame or independently stretches width and height. Frame pacing defaults to
display VSync; Modern/Custom can instead cap SDL presentation at 120 FPS or
present uncapped. These choices never change a recovered game tick, input
poll, original pixel or save byte.
The panel is input-modal: it consumes all other keyboard and gamepad controls,
so no launcher command or recovered original-game signal can be sent behind
it. These controls never become original-game input mappings.

### Language

The launcher UI is translated through the portable gettext-style catalogs in
[`po/`](po/README.md). It currently ships Arabic, German, Greek, British
English, Spanish, Finnish, French, Hindi, Italian, Japanese, Korean, Dutch,
Norwegian, Polish, Brazilian Portuguese, Russian, Swedish, Turkish, Ukrainian,
and Simplified Chinese. English is the default launcher language. Select a
launcher language with `--language sv` (or `-l sv`) to choose the initial UI
language, or cycle it in the start menu with `L`/the language button. This
never changes the immutable original-release language.
Only Project Eon's own UI is translated—original game text remains sourced from
the selected original media. All 20 UTF-8 catalogs are rendered through the
bundled, hash-reviewed SDL_ttf/Noto fallback chain; Project Eon never selects a
host font or transliterates a translation. See [the localization rendering
contract](po/README.md#unicode-rendering).

By default, Project Eon reads user-supplied media from `~/.projecteon` on
Linux/macOS and `<install directory>\data` on Windows. On iPadOS, use the
Files-visible `Documents/ProjectEon` folder exposed by the sideloaded app;
the IPA itself deliberately contains no game media. `--data` (or the explicit
alias `--data-dir`) selects a
different directory or one original archive, for example a preservation
collection in `Downloads`.
Archives and disk images are read in place: Project Eon never creates the data
directory, unpacks, copies,
installs, modifies, or redistributes original game data.

The verified Spanish Millennium DOS floppy is supported directly from its
FAT12 image: its original `TITLE.LIB` P00 title and palette are rendered in
place. Its executable hand-off is deliberately kept separate from the
recovered English DOS path until that Spanish ABI has evidence.

Or select a game directly from the CLI:

```sh
./build/project-eon --data "$HOME/Downloads" --game millennium \
  --platform amiga --presentation original --resolution 1600x900 --aspect original
./build/project-eon --data "$HOME/Downloads" --game deuteros \
  --platform amiga --presentation modern --resolution 1920x1080 --aspect widescreen
```

`--resolution` accepts the same 1280x720, 1600x900, and 1920x1080 presets as
F10. `--aspect` accepts `original` (4:3), `square-pixels` (8:5), or
`widescreen` (16:9). Both are renderer-only preferences. A direct `--game`
launch always requires `--platform`: Project Eon will not select a different
platform's release when the choice is omitted. Use the card menu or
`--inspect --game <game>` to see the hash-verified choices first.

To validate the complete hash-bound startup route without creating an SDL
window, add `--launch-check`. It resolves one exact release, rehashes the
outer archive, and constructs its platform adapter, then exits before any
rendering, audio, input, game timing, or save activity:

```sh
./build/project-eon --data "$HOME/Downloads" --game deuteros \
  --platform amiga --presentation modern --launch-check
```

`--launch-check-json` emits the same result as
`project-eon.launch-check/v1`, including the exact release SHA-256, without
creating SDL resources.

For preservation tooling, `--inspect-json` emits one deterministic JSON
document (`project-eon.inspect/v1`) after rehashing every selected release. It
contains only game/platform/language/release hashes plus hash-bound startup and
recovery boundaries, declarative function-map facts, and aggregate scan
counters—never source paths, filenames, archive members, or original bytes.
The CLI and F10 obtain these facts from the same fail-closed diagnostics
composition, so a record whose release identity or parser-profile gate does
not match cannot be displayed. Inspection never admits a trace or runtime
session. It cannot be combined with asset inventory or Modern-pack inspection.

For a separately assembled, validated reference trace, add
`--reference-trace-json` to the normal explicit `--reference-trace` command.
It emits `project-eon.reference-trace/v1`: release and capture hashes,
adapter/checkpoint counts, recovery boundaries, artifact identities, and the
compiled runtime-policy label only. It never emits local trace paths, artifact
paths, original bytes, or replay state. All current policies are
`diagnostics-only` except the separately documented, transient call-free
Millennium DOS GX boundary.

After a Modern F10/Custom panel is closed, its renderer preferences are stored
separately from game media: Linux uses
`$XDG_CONFIG_HOME/project-eon/presentation-v1.ini` (or
`~/.config/project-eon/`), macOS uses Application Support, and Windows uses
`%APPDATA%/ProjectEon/`. The file contains only Eon's output, aspect, preset,
filter, and pacing selections. It is never created while merely reading game
data, never stored in the data directory, and never changes original media or
saves. Explicit `--resolution` and `--aspect` options override its values.

Verify genuine release archives by SHA-256 without opening SDL:

```sh
./build/project-eon --data "$HOME/Downloads" --verify-data millennium
./build/project-eon --data "$HOME/Downloads" --verify-data deuteros
```

Inspect every recognised original release in one read-only scan:

```sh
./build/project-eon --data "$HOME/Downloads" --inspect
```

Add `--inventory` to an inspection for a bounded, hash-addressed manifest of
the selected archive's nested original leaf assets. It is preservation
diagnostics only: the archive is rehashed before the report and its leaves are
read in memory, never unpacked, copied, or used as filename-based admission.

Narrow that same non-SDL inspection to one requested original release when
recording or comparing preservation evidence:

```sh
./build/project-eon --data "$HOME/Downloads" --inspect --game millennium --platform dos
```

Audit separately installed Modern art packs only when explicitly requested:

```sh
./build/project-eon --data "$HOME/Downloads" --inspect --modern-packs /path/to/eon-modern-packs
```

`--modern-packs` is valid only with `--inspect` and never has a default search
path. It reports each direct-child pack as eligible or rejected after the
selected original releases have been rehashed. The explicit Modern launch form
`--game <game> --platform <platform> --presentation modern --modern-pack
/path/to/pack.eonmodern` may render only a documented, hash-revalidated
Millennium DOS title target or the finite Deuteros Amiga held-input opening
sequence; Original never uses an external pack. Each PNG chunk checksum is
verified before decoding. Neither
form creates a directory or cache. See [the Modern asset-pack format](docs/MODERN_ASSET_PACK_FORMAT.md)
for the external, separately installed format.

The **Custom** card offers the same explicit choice through a native file
dialog before a session starts. It asks for exactly one `.eonmodern` manifest,
starts in no default directory, does not remember or scan a pack location, and
does not make a choice when the dialog is cancelled. The selected candidate is
session-local and receives the identical manifest, release-hash, asset-hash,
PNG, and dimensional validation as `--modern-pack`; F10 labels it **Ready**
only after it is bound to the exact selected original release, otherwise it is
**Rejected** and discarded. Changing game, platform, release, or data source
also discards it, and the renderer rehashes it once more immediately before
decoding. Choosing a file never trusts its filename or changes Original mode.

`--inspect` reports each detected game/platform archive and its recovered
preservation evidence directly from the supplied media. Its `INSPECTION`
header identifies the read-only provenance mode. Filtering changes only which
verified reports are printed; it never selects a fallback platform or language.
Inspection neither extracts files to disk nor creates, alters, or substitutes
game data.

The final aggregate `SCAN SUMMARY` makes recognition reviewable without
turning unknown files into a catalogue: it records candidate, size-rejected
(not hashed), size-match, hash, hash-rejected, verified-occurrence,
duplicate-occurrence, unique-release, unbound-direct-media, and read-failure
counts. A hash-verified loose physical-media leaf is preserved as unbound
evidence only: it never makes a card startable without a separately documented
complete direct-media-set identity. Identical
verified archives found more than once are one release, with the lexically
first path used as the deterministic read-only source.

An unfiltered inspection also prints a `PLATFORM ADMISSION` row for every
verified game/platform pair. `READY` means exactly one verified original
language can reach the profile cards; `RELEASE SELECTION REQUIRED` means the
media is verified but an exact original edition must be chosen first. The rows
are derived only after the full report rehashes every release, and are an
audit of launcher availability—not native Atari ST execution or API emulation.
Their separate coverage field says `RECOVERED STARTUP`, `RECOVERED OPENING`,
or `BOOTSTRAP ONLY`. It is calculated per game/platform rather than inferred
from platform names, so admission is never presented as a parity claim. The
same field is available in the read-only `--inspect-json` and
`--launch-check-json` reports and in F10 diagnostics. Every platform card
also shows coverage separately from current original-data admission.
Each verified Atari report also has a release-specific `ATARI LAUNCH BOUNDARY`:
Millennium stops before its GEMDOS `Fopen` result and later launcher control
flow, while Deuteros stops before protected XBIOS/callback behavior and state
selection. These statements are evidence boundaries, not emulation promises.

For scripts, `--inspect` exits `0` after one or more matching reports, `3`
when the supplied path has no recognised original archive, and `5` when a
valid game/platform filter has no matching supported release. In the latter
case Project Eon reports the mismatch and never silently changes platform or
language. A missing data path exits `2` without creating the default location.

When a report reaches a platform boundary, it also states the minimum missing
reference-trace inputs. Those lines are a collection checklist, not an
emulation request: use a hash-identified original release and a trace-capable,
read-only emulator setup. In particular, the Spanish Millennium DOS handoff
never borrows English executable/state evidence, and Atari ST reports never
substitute DOS or Amiga resources for missing GEMDOS/XBIOS/callback results.

The current SDL application is deliberately an incremental reimplementation,
not a mock game. It lists detected real releases and proves the Original/Modern
presentation boundary while reverse engineering proceeds. Selecting Deuteros
now runs the recovered Amiga opening channel program live at runtime from the
verified ADF, with its original RGB4 palette, VBL random source and recovered
held input signal. It never substitutes placeholder art or invented game behaviour
for undecoded original data.

`--game deuteros --platform amiga` selects that verified Amiga opening. An
explicit `--platform atari-st` creates the bounded Replicants Disk 1 raw boot
session: it verifies the two original boot-stage ranges in memory and stops
before XBIOS/callback behavior or state selection. It never falls back to
Amiga artwork, audio, or a synthetic ST session while Atari presentation/runtime
parity remains unrecovered.

The launcher follows the same broad structure as OpenCaptive: a native SDL3
start menu, separate game cards, direct CLI game selection, a platform-neutral
data layer and distinct engine/render/audio modules as those systems mature.
Platform cards are admission gates, not filename guesses: only a hash-verified
release enables one. Missing or unrecognised media stays disabled. When English
identifies exactly one verified outer release for the selected game/platform,
it is the default. If multiple original containers share a language, the menu
shows separate release cards with a short hash and the CLI requires
`--release-sha256`; `--release-language` can narrow but never collapse that
identity. This applies equally to Atari ST media; the launcher never
substitutes an Amiga or DOS release or chooses by scan order.
Release cards reuse only the selected generated platform-card illustration
with a readable Eon overlay; they contain no original pixels or archive data.
Every verified card also states its release-specific recovery coverage: **Recovered
startup**, **Recovered opening**, or **Bootstrap only**. These labels select
their exact original media but never claim unrecovered GEMDOS, XBIOS, callback,
title, or gameplay parity as a completed game start.
The two menu cards are newly generated Project Eon artwork inspired by the
games' broad lunar-recovery and orbital-expansion themes. They contain no
extracted game assets and are never used inside Original mode.

## Current status and research workflow

Project Eon is in the reverse-engineering and foundation phase. It is not yet a
complete playable replacement. The native runtime now verifies the six supplied
release archives by SHA-256, recursively reads nested ZIPs, validates Deflate
streams and CRCs, and fingerprints all 67 contained assets. The verified corpus
contains 17 Amiga ADF images, 18 Atari ST images and both English and Spanish
DOS data for Millennium 2.2.

The Spanish Millennium floppy is now opened as a native FAT12 filesystem. Its
39 genuine root files can be listed and read through validated cluster chains;
integration tests lock the extracted `2200AD.EXE` and `GX.LIB` contents to
their observed SHA-256 hashes.

Its own `TITLE.LIB` is also read directly from that image: `P00` decodes as
the original 320×200 indexed title with the Spanish release's own RGB6 DAC and
logical translation (its final RGBA frame is separately hash-locked). The
Spanish `2200AD4.BIN` celestial display table starts at its observed `$03db`
offset and is exposed byte-for-byte, including `Tierra ` and `Asteroides `.
`--verify-data millennium` reports these FAT12-derived facts without copying
or unpacking the disk. The live original `MILL.BAT` is preserved as
read-only launcher documentation (the `IBM`, `IBM e`, `IBM m`, `TANDY`, and
`EGA320` choices); it does not establish gameplay controls. The
Spanish `IBM.COM` separately provides a hash-verified static request chain for
its own `TITLES.EXE` followed by `2200AD.EXE`. DOS call results, return values
and both target ABIs remain unexecuted, so Project Eon does not infer a
replacement hand-off or substitute the English state.

The English DOS `TITLE.LIB` and `GX.LIB` are also parsed natively through their
verified banked resource directory, exposing 38 title resources and 180
gameplay resources directly from the hash-identified original archive.
`TITLE.LIB` resource `P00` now decodes into its authentic 320×200 indexed title
image and its original 256-entry VGA RGB6 DAC table plus 36-entry logical
index translation. Original uses this user-supplied decoded-pixel title texture
unchanged with nearest-neighbour scaling. Modern can instead opt into transient,
deterministic edge-aware Scale2x or Scale4x reconstruction from those same
decoded pixels (or retain the original-pixel texture), then choose nearest or linear output
sampling. Reconstruction is memory-only: it never writes, caches, replaces, or
packages game media.
When its recovered DOS console poll observes a key, the launch view records
only the title executable's local input/exit boundary and keeps displaying the
original P00 frame. The subsequent `MILL.COM` return, DOS EXEC result,
`2200ad.exe` startup, GX selection, and save-state initialization are not
observed, so the launcher does not substitute a GX canvas or a save panel.
`GX.LIB` and English `2200SAVE.I` remain hash-identified, read-only
preservation evidence available to the inspection tooling, without inferred UI
or mutable-state semantics.

The same FAT12 reader is validated against the genuine 819,200-byte Atari ST
Millennium disk. Nested extraction locates the disk by SHA-256 independently of
its filename and reads its 13 root files, including verified `DATA12.BIN`.

Millennium's Amiga media is now distinguished from those filesystem variants:
the verified Defjam ADF boot chain loads a 1 KiB first stage and then two
authentic raw disk ranges (`0x24200`/`0x6e000` to `0x41000`, and
`0x16400`/`0x2c000` to `0x68000`). Project Eon validates this original 68000
request sequence directly from the in-place ADF; it does not invent files or
unpack the ranges. Selecting Millennium Amiga creates this hash-locked bounded
session and validates its resident entry, but stops before the original opaque
raw-stage invocation rather than inferring an Amiga game loop.

Deuteros' clean Amiga system and data disks are also opened natively as ADF.
Geometry, boot identifiers, carry-around checksums and arbitrary sectors are
validated against the real images. The 68000 bootloader's decoded-track request is
documented in [the generated disassembly](docs/generated/deuteros-amiga-boot.md).
Both discovered four-bitplane RLE layouts are implemented, covering all 216
bitmap records in the first two resource bundles. The SDL launch view exercises
the same importer, channel VM, compositor, original palettes, VBL tick
sequence, and recovered input gate. It freezes at the first still-unimplemented stateful
save/restore boundary rather than fabricating later animation frames.

After the verified held-input route reaches its original `$0f` handoff, the
runtime also opens the original Amiga title-stage track range read-only and
reports its exact provenance and SHA-256. It deliberately does not render a
guessed title menu or execute through the unrecovered Exec/graphics state.

The repository does not contain the commercial games. Point the inventory tool
at a directory containing the original archives:

```sh
python3 -m eon.inventory ~/Downloads
python3 -m unittest discover -s tests -v
```

The generated manifest is the reproducible starting point for disassembly,
resource decoding and cross-platform comparison. Original data paths are
ignored by Git.

See [docs/research.md](docs/research.md) for the reverse-engineering method and
current findings. The hash ledger, evidence levels, reproduction procedure,
and contribution rules live in the
[preservation record](docs/PRESERVATION.md).
The cross-platform whole-program disassembly inventory, including its explicit
code/data and runtime-ABI boundaries, is maintained in
[docs/DISASSEMBLY_STATUS.md](docs/DISASSEMBLY_STATUS.md). Retained external
linear reports can be hash- and line-count-verified without importing their
copyrighted bytes into the checkout through
`tools/verify_disassembly_reports.py`.
The release-by-release distinction between data support, bounded startup and
playable parity is maintained in the [parity matrix](docs/PARITY.md).

The detailed, priority-ordered completion work is maintained in the
[completion plan](docs/COMPLETION_PLAN.md) and its ranked
[P0 work queue](docs/WORK_QUEUE.md). They define evidence gates and
exit criteria for every major recovery phase; it is not a claim that the
currently recognised releases have reached campaign parity.

## Continuous integration and Git policy

GitHub Actions builds and tests Linux, macOS, and Windows, runs the preservation
tool tests without commercial game data, and scans the complete Git history
with Gitleaks. CI also produces non-published test artifacts: `.deb`, `.rpm`,
and x86_64 AppImage Linux packages; separate macOS arm64 and x86_64 app
bundles; a Windows Inno Setup installer; and an arm64 iPadOS `.ipa`. The
AppImage builder uses a runtime supplied by a SHA-256-locked AppImage tool;
an upstream change fails the build until a reviewed hash update is committed.
The iPadOS artifact is unsigned
for sideload signing with the user's own certificate and provisioning profile.
Its packaging step independently validates the final IPA archive before upload:
the archive must contain a structurally valid arm64 executable Mach-O and iPad `Info.plist`, the
required launcher cards, fonts and PO catalogues, and no links, path escapes or
possible game media or unexpected dynamic frameworks. Packages contain Project Eon only—never original game
media. The iPad app enables Files sharing and opens user documents in place, so
place owned original archives or disk images in `Documents/ProjectEon` after
sideloading; do not add them to the IPA. CI has read-only repository permission and cannot release,
tag, or publish. Development pushes go directly to GitHub `main`; Project Eon does not
create GitHub branches. The Windows installer also does not pre-create its
`data` path: a missing default directory remains a read-only runtime boundary.
Releases are made only when the maintainer explicitly requests one.

Every uploaded CI artifact is accompanied by a deterministic JSON integrity
manifest. It names the full source commit and records each downloadable file's
byte size and SHA-256, so a maintainer can verify a downloaded package without
trusting a workspace path or unpacking any game media. The workflow itself and
its third-party actions are pinned to immutable Git commit IDs.

The adjacent verifier validates the manifest schema, expected source revision,
safe artifact basenames, byte lengths, and SHA-256 values. CI runs it before
every upload with an exact-directory check, so no unrecorded entry can
enter an artifact bundle. Maintainers can repeat the same check after download:

```sh
python3 packaging/verify-artifact-manifest.py \
  --manifest project-eon-linux-artifacts.json \
  --directory . --expected-source-revision <40-lowercase-commit> \
  --require-exact-directory
```

## Repository

Development lives at <https://github.com/yeager/project-eon>.
