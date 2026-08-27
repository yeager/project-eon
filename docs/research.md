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

## Evidence rule

Real supplied game data is the sole authority for assets, rules, numeric tables,
timing and behaviour. Project Eon does not fill undecoded areas with synthetic
assets or guessed mechanics. Unknowns remain explicitly unsupported until code,
data cross-references, or captured original execution provide evidence.

## Supplied releases

The initial corpus contains Millennium 2.2 for DOS, Amiga and Atari ST, and
Deuteros for Amiga and Atari ST. A Spanish DOS floppy image of Millennium is
also present. Outer archives contain several alternate/cracked disk versions,
so a filename alone is not a stable identity; SHA-256 fingerprints are used.

Native corpus verification covers all six supplied outer archives and all 67
leaf assets inside their nested ZIP structure. Identity is determined from
content hashes, never archive filenames. Extraction rejects out-of-bounds
metadata, oversized entries, unsupported compression, incomplete Deflate
streams and CRC mismatches before an asset reaches a decoder.

## Initial DOS observations

Despite their `.EXE` suffixes, `2200AD.EXE`, `2200GX.EXE`, and `TITLES.EXE` are
not MZ files. They are flat 16-bit x86 binaries managed by the `MILL.COM`
launcher. `MILL.COM` installs private interrupt handlers including `INT 91h`,
`92h`, and `95h`; the game modules call these as a loader/runtime API.

`2200AD.EXE` starts with segment-register setup and a near jump from file offset
`0x0004` to `0xd1b0` (loaded address `0xd2b0`). Its entry routine establishes a
stack, uses DOS `INT 21h` memory services, selects a text/graphics mode, loads
`2200AD4.BIN`, `GX.LIB`, and `LAST.LIB`, then enters the UI loop. Literal names
for bases, asteroid analysis actions, save slots, and time units occur in this
module and provide anchors for code cross-references.

The distribution also contains Creative Voice (`.VOC`) effects,
driver/resource binaries, and `GX.LIB`/`LAST.LIB`. This split is useful:
executable cross-references can reveal record widths and indexes in the resource
libraries. Run `tools/analyze_dos.py` with Capstone installed to regenerate an
entry-point report from an extracted copy.

## Completion criteria

- All supplied platform releases are detected and their required resources load.
- Both games can be completed using the shared simulation.
- Original mode matches reference captures for graphics, audio, timing and UI.
- Modern mode can be switched without changing simulation or save state.
- Saves round-trip deterministically and compatibility fixtures pass.
- No original game assets are distributed by this repository.
