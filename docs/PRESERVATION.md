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
5. Modern presentation may reinterpret graphics and interaction, but cannot
   silently change simulation rules or saved state.
6. This repository contains code, documentation, hashes, and newly created
   menu artwork—not commercial game assets.

## Corpus identity

The initial corpus has six outer ZIP archives and 67 leaf assets. Native
recognition uses complete archive SHA-256 and size, never filenames.

| Game | Platform | Lang. | Bytes | Outer archive SHA-256 |
| --- | --- | --- | ---: | --- |
| Deuteros | Amiga | en | 4,066,771 | `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` |
| Deuteros | Atari ST | en | 3,021,682 | `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653` |
| Millennium 2.2 | Amiga | en | 2,558,009 | `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400` |
| Millennium 2.2 | Atari ST | en | 1,524,836 | `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01` |
| Millennium 2.2 | DOS | en | 328,383 | `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` |
| Millennium 2.2 | DOS | es | 330,050 | `b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4` |

Leaf counts are 17 Amiga ADFs, 18 Atari ST disks, one DOS floppy, three
DOS flat executables, one DOS COM program, 14 audio files, 12 game resources,
and one unknown item. Alternate/cracked dumps are comparative evidence; clean
dumps are preferred as semantic baselines.

### Stable evidence anchors

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Deuteros Amiga clean system ADF | 901,120 | `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38` |
| Deuteros Amiga clean data ADF | 901,120 | `99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a` |
| Millennium Spanish `2200AD.EXE` | 54,566 | `9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6` |
| Millennium Spanish `GX.LIB` | 311,420 | `e27d1c697da677994e2f864a776f4fc900c7feb4ec4b85500b2bfea3bc834767` |
| Millennium Atari ST Equinox disk | 819,200 | `3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7` |
| Millennium Atari ST `DATA12.BIN` | 932 | `6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda` |

## Verified format knowledge

- Nested ZIP parsing validates ranges, Deflate completion, output size, CRC-32,
  and SHA-256 before classification.
- DOS and Atari ST media use a native read-only FAT12 reader with validated
  geometry, bounded cluster chains, loop detection, and directory parsing.
- Standard Amiga ADF geometry is 80 cylinders × 2 sides × 11 sectors × 512
  bytes. Both clean Deuteros boot blocks pass the carry-around checksum.
- Deuteros identifiers are `DOS\0` (system) and `DEU\0` (custom data). Logical
  block 880 is game code/data rather than a normal AmigaDOS root directory.

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

The main loader at `$21932` indexes five disk offsets at `$21708`:
`0x1b800`, `0x4ba00`, `0x37000`, `0x59600`, and `0x6e000`. The first two are
verified bundles:

| Disk offset | Length | Objects | Mode |
| ---: | ---: | ---: | ---: |
| `0x1b800` | `0x2f3f4` | 4 | 0 |
| `0x4ba00` | `0x215f0` | 6 | 1 |

Each 60-byte header has a big-endian length, object count, seven relative
channel pointers, six relative auxiliary pointers, and a mode word. The native
importer rejects an out-of-range bundle or non-null pointer. See the
[annotated disassembly](generated/deuteros-amiga-boot.md).

The first auxiliary pointer is a palette bank. The interpreter's command 4
multiplies its operand by 32 and copies 16 words from this bank to each active
display list. The words are standard 12-bit Amiga RGB4. Bundle 0, palette 1 is
anchored by `000 886 664 442 220 a60 840 620 080 ff0 004 008 02f 0cf fff e40`;
the native decoder expands every nibble exactly to 8-bit (`n × 17`).

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
blob. Bundle 0 reserves 160 index slots, of which the leading 143 address
records in a `0x1ce96`-byte blob. Bundle 1 reserves 128 slots with 75 records in
a `0xb95e`-byte blob. In both, record 0 starts at offset zero, subsequent used
offsets strictly increase, and the unused table tail is zero-filled. The parser
validates these invariants but does not yet label the record contents as
graphics until the consuming routine is fully traced.

### Millennium DOS execution model

`2200AD.EXE`, `2200GX.EXE`, and `TITLES.EXE` are flat 16-bit binaries despite
their suffix. `MILL.COM` provides a private runtime through interrupts 91h,
92h, and 95h. `2200AD.EXE` jumps from file offset `0x0004` to `0xd1b0`, then
uses DOS services and loads original libraries. See the
[DOS analysis](generated/dos-millennium.md).

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

## Reproduction

Keep source data outside the repository:

```sh
cmake -S . -B build -G Ninja -DEON_REAL_DATA_DIR="/path/to/original-data"
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m unittest discover -s tests -v
./build/project-eon --data "/path/to/original-data" --verify-data millennium
./build/project-eon --data "/path/to/original-data" --verify-data deuteros
```

Static reports can be regenerated from separately extracted, hash-verified
inputs with `tools/analyze_dos.py` and `tools/analyze_m68k.py`. Capstone output
is evidence navigation; structures must still be connected to callers, ranges,
and tests.

## Adding evidence

1. Record SHA-256, byte length, language, platform, and known dump provenance;
   never normalize the original file.
2. Keep acquisition paths, personal information, and commercial bytes out of
   Git.
3. Add a minimal parser with explicit endian and range rules.
4. Assert results against genuine data and test malformed boundary conditions.
5. Record both disk/file offsets and relocated runtime addresses.
6. Preserve cross-platform disagreements instead of prematurely merging them.
7. Label generated modern/menu artwork so it cannot be mistaken for original
   preserved art.

Git history records interpretation changes. Corrections must explain their new
evidence instead of rewriting earlier uncertainty away.

## Current boundary

Release recognition, archive traversal, selected FAT12 content, Deuteros ADF
geometry/checksums, its first two load stages, two resource headers, and the
first verified palette bank are implemented and tested. Bitmap encoding, audio
mapping, full resource semantics, simulation, AI, saves, and timing remain incomplete. The SDL app
must report those areas honestly rather than presenting fabricated gameplay.
