# Project Eon parity matrix

This is a preservation status record, not a compatibility claim. “Starts”
means Project Eon can select the identified, user-supplied release from the
card menu or CLI and enter the stated verified session. It does **not** mean
that the original game is playable to completion.

All rows use media read in place. No original archive, disk, asset, save, or
runtime data is unpacked, copied back, changed, or distributed by Project Eon.

| Game | Platform / language | Recognition and data access | Current verified runtime | Original presentation | Hard boundary before parity |
| --- | --- | --- | --- | --- | --- |
| Millennium 2.2 | DOS / English | ZIP, original DOS files and LIB resources are hash-identified | `TITLES.EXE` key-poll reaches its verified local input/exit boundary; the launcher/DOS return and `2200AD.EXE` startup remain unexecuted | `TITLE.LIB` P00; `GX.LIB` IMG00/IMG01; `2200SAVE.I` positional evidence | Full UI, simulation, mutable state, saves, named controls and native helper ABI are unrecovered |
| Millennium 2.2 | DOS / Spanish | 720 KiB FAT12 image and original files are validated through cluster chains | Starts the Spanish P00 title session; `IBM.COM` statically requests its own `TITLES.EXE` then `2200AD.EXE` | Spanish `TITLE.LIB` P00, RGB6 DAC and translation, without English substitution | DOS call results, returned AL, target ABIs, UI, simulation, saves and controls are unrecovered |
| Millennium 2.2 | Amiga / English | Original ADF raw loader ranges and shared resident bytes are hash-identified | Starts a bounded raw-loader session | No frame is claimed beyond the launcher; no generated substitute is used | Opaque raw-stage invocation, decoded runtime image and game loop are unrecovered |
| Millennium 2.2 | Atari ST / English | FAT12 Equinox disk, PRG, config chain and named auxiliary literals are validated | Starts a bounded bootstrap session | No original frame is drawn | GEMDOS/XBIOS results, `MILL22E.INF` loader ABI, codec, palette and planar layout are unrecovered |
| Deuteros | Amiga / English | Clean system/data ADFs, load ranges, bundles, VM records, bitmaps and initial audio descriptors are validated | Starts the recovered opening VM and freezes at the title-stage boundary | Opening pixels, RGB4 palette and initial PCM are sourced from original ADF data | Exec/graphics ABI, title-state initialization, bitplanes and title display selection are unrecovered |
| Deuteros | Atari ST / English | Protected/raw disk geometry and recovered boot stages are hash-identified | Starts a bounded protected-media boot session | No Amiga artwork or synthesized Atari screen is substituted | XBIOS, callback, state selection and later resource semantics are unrecovered |

## What “all supported platforms” guarantees

Every recognised release is selectable from the launcher and explicit CLI
platform selection never substitutes a different platform or language. A
missing data path remains read-only: Project Eon reports it and does not create
the default directory.

The portable entry points are:

```sh
./build/project-eon --data /path/to/original-media
./build/project-eon --data /path/to/original-media --game millennium --platform amiga
./build/project-eon --data /path/to/original-media --game deuteros --platform atari-st
./build/project-eon --data /path/to/original-media --inspect
./build/project-eon --data /path/to/original-media --inspect --game millennium --platform dos
```

`--inspect` is a non-SDL, read-only provenance view. The optional game and
platform filters restrict the reports printed after recognition; they do not
change, unpack, or otherwise prepare the supplied media, and they cannot cause
a platform or language fallback.

## Evidence gates for a new playable path

Before expanding any row into a broader runtime path, add all of the following:

1. Hash-identified source media and bounded offsets/ranges.
2. A caller-connected code path or an observed genuine execution trace.
3. Decoded format/ABI bounds, including malformed-media rejection.
4. Native and/or frame golden tests from supplied original data.
5. A preservation record in [PRESERVATION.md](PRESERVATION.md), including
   addresses, hashes, uncertainty and what remains unexecuted.

Until those gates are met, the launcher must retain the explicit boundary
instead of fabricating a screen, save, input action, or game rule.
