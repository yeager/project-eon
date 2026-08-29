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

The only v1 runtime target is deliberately fixed and opt-in:

```text
asset	millennium.dos.title.png-640x400 title.png <size> <sha256>
```

It is eligible only for the English Millennium DOS release selected by the
launch and only with `--game millennium --platform dos --presentation modern
--modern-pack /absolute/or/explicit/pack.eonmodern`. The file must be a
non-empty RGBA PNG, exactly 640×400 pixels (IHDR bit depth 8, colour type 6),
and no more than 8 MiB. After normal manifest admission, Eon reads the file
again, hashes the exact in-memory bytes, validates the constrained PNG header,
and asks SDL_image to decode those bytes from memory. SDL_image failure or a
post-decode dimension mismatch rejects the external surface. It is displayed
only as the Modern replacement for the recovered English P00 title; Original
always uses the recovered original pixels.

This mapping does not infer original behavior, alter game logic, save data, or
original media. It creates no cache or extracted output. Other asset IDs remain
admission-only metadata until separately documented and implemented.

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
