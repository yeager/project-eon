# Project Eon

Project Eon is an open-source, cross-platform reimplementation of Ian Bird's
science-fiction strategy games **Millennium 2.2: Return to Earth** (1989) and
**Deuteros: The Next Millennium** (1991).

| Millennium 2.2 | Deuteros |
| :---: | :---: |
| [![Project Eon Millennium 2.2 card](assets/cards/millennium.png)](assets/cards/millennium.png) | [![Project Eon Deuteros card](assets/cards/deuteros.png)](assets/cards/deuteros.png) |

The goal is to make both games fully playable from legally obtained original
game data while preserving their rules and atmosphere. One deterministic game
simulation powers two interchangeable presentation modes:

- **Original** — authentic artwork, sound, layout, timing and behaviour.
- **Modern** — high-resolution graphics, scalable UI, modern input and
  accessibility improvements without changing the underlying game.

Presentation mode must be switchable without restarting a game or converting
its state.

## Original game data

Project Eon does not distribute commercial game files. Players provide their
own disk images or installed data. The importer fingerprints files and decodes
platform-specific containers into one canonical resource model.

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
- Provide a polished Modern mode while keeping gameplay and saved state
  identical between renderers.
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
   canonical assets + state
            │
    deterministic simulation
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
  -DEON_REAL_DATA_DIR="$HOME/Hämtningar"
cmake --build build
ctest --test-dir build --output-on-failure
```

Start the graphical card menu:

```sh
./build/project-eon
```

By default, Project Eon reads user-supplied media from `~/.projecteon` on
Linux/macOS and `<install directory>\data` on Windows. `--data` selects a
different directory, for example a preservation collection in `Hämtningar`.
Archives and disk images are read in place: Project Eon never unpacks, copies,
installs, modifies, or redistributes original game data.

Or select a game directly from the CLI:

```sh
./build/project-eon --data "$HOME/Hämtningar" --game millennium \
  --platform amiga --presentation original
./build/project-eon --data "$HOME/Hämtningar" --game deuteros \
  --presentation modern
```

Verify genuine release archives by SHA-256 without opening SDL:

```sh
./build/project-eon --data "$HOME/Hämtningar" --verify-data millennium
./build/project-eon --data "$HOME/Hämtningar" --verify-data deuteros
```

The current SDL application is deliberately an incremental reimplementation,
not a mock game. It lists detected real releases and proves the Original/Modern
presentation boundary while reverse engineering proceeds. Selecting Deuteros
now displays an authentic bitmap decoded at runtime from the verified Amiga ADF
with its original RGB4 palette; it never substitutes placeholder art or
invented game behaviour for undecoded original data.

The launcher follows the same broad structure as OpenCaptive: a native SDL3
start menu, separate game cards, direct CLI game selection, a platform-neutral
data layer and distinct engine/render/audio modules as those systems mature.
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

The English DOS `TITLE.LIB` and `GX.LIB` are also parsed natively through their
verified banked resource directory, exposing 38 title resources and 180
gameplay resources directly from the hash-identified original archive.
`TITLE.LIB` resource `P00` now decodes into its authentic 320×200 indexed title
image and its original 256-entry VGA RGB6 DAC table plus 36-entry logical
index translation. The SDL launch view shows this user-supplied original title:
nearest-neighbour in Original mode and linear scaling only in Modern mode.

The same FAT12 reader is validated against the genuine 819,200-byte Atari ST
Millennium disk. Nested extraction locates the disk by SHA-256 independently of
its filename and reads its 13 root files, including verified `DATA12.BIN`.

Deuteros' clean Amiga system and data disks are also opened natively as ADF.
Geometry, boot identifiers, carry-around checksums and arbitrary sectors are
validated against the real images. The 68000 bootloader's decoded-track request is
documented in [the generated disassembly](docs/generated/deuteros-amiga-boot.md).
Both discovered four-bitplane RLE layouts are implemented, covering all 216
bitmap records in the first two resource bundles. The SDL launch view exercises
the same importer, channel VM, compositor, original palettes, and VBL tick
sequence. It freezes at the first still-unimplemented stateful
save/restore boundary rather than fabricating later animation frames.

The repository does not contain the commercial games. Point the inventory tool
at a directory containing the original archives:

```sh
python3 -m eon.inventory ~/Hämtningar
python3 -m unittest discover -s tests -v
```

The generated manifest is the reproducible starting point for disassembly,
resource decoding and cross-platform comparison. Original data paths are
ignored by Git.

See [docs/research.md](docs/research.md) for the reverse-engineering method and
current findings. The hash ledger, evidence levels, reproduction procedure,
and contribution rules live in the
[preservation record](docs/PRESERVATION.md).

## Continuous integration and Git policy

GitHub Actions builds and tests Linux, macOS, and Windows, runs the preservation
tool tests without commercial game data, and scans the complete Git history
with Gitleaks. CI also produces non-published test artifacts: `.deb` and
`.rpm` packages, separate macOS arm64 and x86_64 app bundles, and a Windows
Inno Setup installer. Packages contain Project Eon only—never original game
media. CI has read-only repository permission and cannot release, tag, or
publish. Development pushes go directly to GitHub `main`; Project Eon does not
create GitHub branches. Releases are made only when the maintainer explicitly
requests one.

## Repository

Development lives at <https://github.com/yeager/project-eon>.
