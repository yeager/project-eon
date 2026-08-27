# Reverse-engineering research

## Method

1. Hash and classify every supplied image, including nested ZIP archives.
2. Select clean, unmodified releases as the baseline; retain cracked variants
   only for comparison when their file systems are easier to inspect.
3. Disassemble the DOS 8086 binaries and 68000 code from Amiga/Atari ST disks.
4. Locate text, graphics, audio and tables through code cross-references rather
   than guessing file formats.
5. Record each decoded structure as a tested parser. Compare equivalent tables
   between platforms to distinguish game rules from presentation data.
6. Build a deterministic, platform-neutral simulation and validate its state
   transitions against observed original execution.
7. Put original and modern renderers over that same state model.

## Supplied releases

The initial corpus contains Millennium 2.2 for DOS, Amiga and Atari ST, and
Deuteros for Amiga and Atari ST. A Spanish DOS floppy image of Millennium is
also present. Outer archives contain several alternate/cracked disk versions,
so a filename alone is not a stable identity; SHA-256 fingerprints are used.

## Initial DOS observations

The English Millennium distribution contains two MZ executables (`2200AD.EXE`
and `2200GX.EXE`), a small launcher (`MILL.COM`), driver/resource binaries,
VOC effects, and `GX.LIB`/`LAST.LIB`. This split is useful: executable
cross-references can reveal record widths and indexes in the resource libraries.

## Completion criteria

- All supplied platform releases are detected and their required resources load.
- Both games can be completed using the shared simulation.
- Original mode matches reference captures for graphics, audio, timing and UI.
- Modern mode can be switched without changing simulation or save state.
- Saves round-trip deterministically and compatibility fixtures pass.
- No original game assets are distributed by this repository.
