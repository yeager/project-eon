# Project Eon

Project Eon is an open-source, cross-platform reimplementation of Ian Bird's
science-fiction strategy games **Millennium 2.2: Return to Earth** (1989) and
**Deuteros: The Next Millennium** (1991).

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
