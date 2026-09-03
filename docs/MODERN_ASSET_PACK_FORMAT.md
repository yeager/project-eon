# Modern asset-pack format

This is the v1 admission format for an optional, separately installed Modern
presentation layer. It exists so future regenerated sprites, redraws, and
high-resolution graphics can be selected without treating arbitrary files as
game data. It is not an import, extraction, conversion, cache, save, or game
logic format.

Project Eon discovers only direct children of a **user-selected Modern-pack
root**. A candidate is exactly `<root>/<pack-id>/pack.eonmodern`. Discovery is
non-recursive, follows no symbolic links, creates no directory, and has no
default search path. A pack root must be kept separate from supplied game
media: admission never writes to either location.

Use the reader explicitly with a rehashing preservation scan:

```sh
project-eon --data /path/to/original-media --inspect --modern-packs /path/to/modern-packs
```

The command reports `MODERN PACK ELIGIBLE` only after the manifest and every
external asset verify *and* the pack's release identity appears in this
invocation's reverified original reports. It reports malformed, changed, and
wrong-release candidates as `MODERN PACK REJECTED`. This is diagnostics only:
neither result selects, decodes, nor renders a pack. `--modern-packs` is not a
launch option and is rejected unless paired with `--inspect`.

## Manifest syntax

`pack.eonmodern` is a UTF-8-compatible ASCII text file. Each non-empty line
is `key<TAB>value`, uses LF line endings, has at most 4 KiB, and the full
manifest is limited to 1 MiB. Unknown or duplicate singleton fields reject the
pack. `asset` may repeat, up to 4,096 records.

```text
schema	project-eon.modern-asset-pack/v1
id	independent-millennium-title
version	1.0.0
license	CC0-1.0
provenance	independently-created
game	millennium
platform	dos
source_release_sha256	e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123
asset	millennium.dos.title textures/title.rgba 123456 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
```

The required singleton fields are:

- `schema`: exactly `project-eon.modern-asset-pack/v1`.
- `id`: ASCII lowercase letters, digits, `.`, `_`, or `-`, at most 96 bytes.
- `version` and `license`: non-empty printable ASCII declarations.
- `provenance`: exactly `independently-created` or `licensed-derivative`.
  The declaration makes the provider accountable; it is not a legal finding
  by Project Eon. Eon packages neither form.
- `game`: `millennium` or `deuteros`.
- `platform`: `dos`, `amiga`, or `atari-st`.
- `source_release_sha256`: lowercase SHA-256 of one compiled Eon release
  manifest entry that matches the declared game and platform.

Each `asset` value is four space-separated tokens:

```text
asset	<asset-id> <relative-path> <decimal-byte-count> <lowercase-sha256>
```

`asset-id` uses the same conservative identifier alphabet as `id`. The path
is normalized relative to the manifest directory: it cannot be absolute,
contain `.` or `..`, contain backslashes, or be a symbolic link. File size is
limited to 256 MiB. Duplicate asset IDs and paths reject the full pack. Every
declared file must be a non-symlink regular file whose byte length and
SHA-256 match the manifest at validation time.

## Current renderer mapping

The only v1 runtime mapping is deliberately narrow and opt-in. It has two
fixed resolutions for the same recovered title target; Eon chooses the higher
available verified target deterministically:

```text
asset	millennium.dos.title.png-640x400 title.png <size> <sha256>
asset	millennium.dos.title.png-1280x800 title-4x.png <size> <sha256>
```

It is eligible only for the English Millennium DOS release selected by the
launch and only with `--game millennium --platform dos --presentation modern
--modern-pack /absolute/or/explicit/pack.eonmodern`. Each file must be a
non-empty RGBA PNG, exactly its identifier's 640×400 or 1280×800 dimensions
(IHDR bit depth 8, colour type 6), and no more than 8 MiB. A 1280×800 asset is
preferred when both are declared, so a pack can offer a genuine 4× redraw
without any filename-based override rule. After normal manifest admission,
Eon reads the selected file again, hashes the exact in-memory bytes, validates
the PNG signature, checksum of every chunk, terminal IEND, consecutive IDAT
sequence and constrained IHDR, then inflates IDAT into the exact bounded
non-interlaced RGBA scanline size. Every scanline filter byte must be one of
PNG's five defined values; truncated streams, trailing compressed payload and
inflated-size surprises are rejected before SDL_image sees the bytes. SDL_image
failure or a post-decode dimension mismatch rejects the external surface. It is
displayed only as the Modern replacement for the recovered English P00 title;
Original always uses the recovered original pixels. The runtime label displays
the active pack identity, dimensions, and declared provenance.

Custom's pre-launch **Modern asset pack** control is an equivalent explicit
selection route for desktop and supported native-dialog platforms. It permits
one user-chosen `.eonmodern` candidate only; it supplies no initial folder,
does not persist a selection, scan directories, or accept multiple paths, and
does nothing on cancellation. SDL's extension filter is convenience only. The
candidate remains untrusted until this document's complete manifest and asset
validation succeeds against the exact selected game, platform, and outer
release SHA-256. F10 reports **Ready** only after that preflight; it reports
**Rejected** and discards the candidate on a malformed, changed, or
wrong-release pack. Changing a game, platform, release card, or data source
also discards any prior admission. Immediately before any renderer asset is
decoded, the loader performs the same strict validation and rehashes its
declared bytes again, so preflight cannot become a time-of-check substitute.
The control is not available to mutate a running session or Original
presentation.

This mapping does not infer original behavior, alter game logic, save data, or
original media. It creates no cache or extracted output. Other asset IDs remain
admission-only metadata until separately documented and implemented.

Both renderable mappings pass through one SDL-free presentation resolver. It
owns only the selected pack metadata and rechecks transient PNG bytes when a
specific native source target is requested; SDL receives a surface only after
that result. The resolver accepts tick zero solely for the Millennium title,
and accepts Deuteros ticks 1–82 solely for the declared held-input route. Tick
82 requires the native opening session's actual title-handoff observation.
This keeps target selection, tier preference and terminal-frame gating outside
the renderer while leaving Original with no pack resolver at all.

### Deuteros Amiga held-input opening sequence

The second renderable v1 mapping is intentionally finite and route-specific.
The verified Deuteros Amiga opening is a live 50 Hz channel VM, not a movie:
without input it continues beyond the currently recovered interval and reaches
later random-dependent state. Eon therefore admits external opening art only
for the **held-input route** that starts with the recovered opening state and
ends at its verified title handoff on source tick 82. The source release must
be the English Deuteros Amiga archive
`f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04`.

An eligible pack provides all 82 IDs below at 640x400, all 82 at 1280x800,
or both complete tiers. There is no frame zero, no per-frame tier fallback,
and no partial high-resolution override. The 4x tier is preferred only when
all 82 of its frames are present and valid.

```text
asset	deuteros.amiga.opening.held-v1.frame-001.png-640x400 opening/001.png <size> <sha256>
...
asset	deuteros.amiga.opening.held-v1.frame-082.png-640x400 opening/082.png <size> <sha256>
```

Each PNG has the same bounded 8-bit RGBA, CRC, IDAT, exact-inflate, 8 MiB,
and rehash-before-upload contract as the Millennium title mapping. Eon hashes
and fully validates the PNG grammar of **all 82 selected-tier frames** before
the sequence becomes eligible, then rechecks the selected file without
following symlinks immediately before renderer use. The sequence is
renderer-only: it cannot advance the VM, change input sampling, replace the
original composed source frame, affect audio, alter the terminal title-stage
boundary, or affect saves. Original mode never selects it.

## Preservation and runtime boundary

Apart from the constrained title mapping above, admission is only an immutable
description of external bytes eligible for a future explicit Modern selection.
It cannot override Original presentation, replace an archive leaf, affect
recovered simulation or input, or read/write an original save. Any future
renderer integration must retain the exact release binding, visibly identify
the active pack, revalidate bytes before use, document its target mapping, and
keep all derived pixels in transient renderer memory unless a separate,
versioned cache contract is approved.

The repository contains no pack and no commercial or unlicensed derivative
art. The format is intentionally suitable for independently created art or
art distributed by an authorised rightsholder, but users remain responsible
for their pack's licence and provenance.
