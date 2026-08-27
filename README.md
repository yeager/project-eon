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
./build/project-eon --data "$HOME/Hämtningar"
```

Or select a game directly from the CLI:

```sh
./build/project-eon --data "$HOME/Hämtningar" --game millennium \
  --platform amiga --presentation original
./build/project-eon --data "$HOME/Hämtningar" --game deuteros \
  --presentation modern
```

The current SDL application is deliberately a data-verification shell, not a
mock game. It lists detected real releases and proves the Original/Modern
presentation boundary while reverse engineering proceeds. It never substitutes
placeholder art or invented game behaviour for undecoded original data.

The launcher follows the same broad structure as OpenCaptive: a native SDL3
start menu, separate game cards, direct CLI game selection, a platform-neutral
data layer and distinct engine/render/audio modules as those systems mature.
The two menu cards are newly generated Project Eon artwork inspired by the
games' broad lunar-recovery and orbital-expansion themes. They contain no
extracted game assets and are never used inside Original mode.

## Current status and research workflow

Project Eon is in the reverse-engineering and foundation phase. It is not yet a
complete playable replacement. Current tooling performs a safe, read-only
inventory of nested release archives and recognises DOS executables and floppy
images plus Amiga ADF and Atari ST disk formats.

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
current findings.

## Repository

Development lives at <https://github.com/yeager/project-eon>.
