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
7. Put Original and Modern renderers over that same state model.

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

### DOS FAT12

The Spanish 720 KiB `MRTE.IMG` uses a conventional FAT12 layout with 512-byte
sectors, two sectors per cluster, two FAT copies and 112 root directory slots.
Project Eon reads its 39 live root files directly from the supplied nested ZIP.
Cluster traversal is bounded and rejects invalid, cyclic and out-of-image
chains. As evidence anchors, the extracted Spanish `2200AD.EXE` is 54,566
bytes with SHA-256 `9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6`;
`GX.LIB` is 311,420 bytes with SHA-256
`e27d1c697da677994e2f864a776f4fc900c7feb4ec4b85500b2bfea3bc834767`.

The Atari ST one-disk Millennium releases also expose FAT12 filesystems. The
verified 819,200-byte Equinox image contains 13 live root entries. Its
`DATA12.BIN` resource is 932 bytes with SHA-256
`6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda`.

### Millennium Amiga loader path

The supplied Amiga ADFs must not be treated as installed-file distributions.
The Razor image has an empty valid AmigaDOS root, while Defjam's boot-stage
loader demonstrates that actual gameplay media are raw reads. The native
loader-plan parser verifies Defjam's boot checksum and 68000 request sequence:
it bootstraps `0x400` bytes from disk offset `0x400` to `0x70000`, requests
`0x24200..0x923ff` to `0x41000`, then requests `0x16400..0x423ff` to `0x68000`
and jumps there with `d6 = 0xa8d398fb`. This is preservation evidence and a
read-only platform boundary, not a decompressor or a replacement game loop.
This proves a native outer ZIP → inner ZIP → ST disk → file path without using
filenames as release identity.

`MILENIUM.TOS` on that same SHA-identified disk is now parsed as a genuine
GEMDOS PRG rather than treated as opaque data. Its 28-byte big-endian `0x601a`
header declares 4,446 text bytes, 44,564 data bytes and 81,382 BSS bytes; it
has no symbol table, is relocatable (`absflag = 0`), and its original compact
relocation stream has 227 entries from offset `0x6` through `0x1150`. The
parser validates every relocation increment and terminator directly from the
disk file and retains the original big-endian longword at every site. The
verified anchors are `[0x6] = 0x0000115e` and `[0x1150] = 0x000139c8`.
This is the exact input needed for a later GEMDOS-compatible loader to add its
chosen base in memory. Project Eon does not select an invented load address,
apply relocations, write an image, or unpack the disk at runtime.

### Deuteros Amiga custom loader

The clean Amiga disks are standard 80-cylinder, double-sided, 11-sector ADF
images, and both boot blocks pass the Amiga carry-around checksum. Disk 1 uses
the `DOS\0` identifier while data disk 2 deliberately uses `DEU\0`; neither
exposes the game through a normal root directory.

Disassembly of the genuine disk 1 boot code shows an extended decoded read
(`trackdisk.device` command `CMD_READ | TDF_EXTCOM = 0x8002`), not a raw-MFM
read. It requests one complete `0x1600`-byte logical track from disk offset
`2 * 0x1600 = 0x2c00` into address `0x12800`, then returns execution
address `0x12a4e`. Logical block 880 on disk 1 starts with `JMP $00040426`;
the equivalent disk 2 block begins `00 04 bb 1a` and is custom indexed data,
not an AmigaDOS root block. `tools/analyze_m68k.py` regenerates the boot
disassembly directly from an extracted verified ADF.

Following the selected profile (`D0 = 0`) reveals the next stage without
emulation: the loader reads `0x4200` bytes from decoded track 4 (ADF offset
`0x5800`) into memory at `0x20000`. That block begins with `JMP $00021734`.
The main entry establishes a stack at `0x22296`, initialises graphics-library
state, reserves memory up to `0x7fff0`, programs Amiga custom-chip registers,
and enters its input/display loop. These constants are decoded and opcode-
validated by the native `parse_deuteros_amiga_load_plan` implementation.

The main-stage resource loader reads a five-entry disk-offset table from that
verified code block. The first two entries are bundles at `0x1b800` (length
`0x2f3f4`, four objects, mode 0) and `0x4ba00` (length `0x215f0`, six objects,
mode 1). Their 60-byte pointer catalogues are parsed from the clean ADF and all
non-null relative pointers are checked against the declared bundle length. See
[PRESERVATION.md](PRESERVATION.md) for the evidence ledger and reproduction
requirements.

The bundle's first auxiliary pointer addresses a 16-colour RGB4 palette bank.
Interpreter command 4 selects `base + index * 32` and copies the resulting 16
words into both active display structures. The native decoder preserves the
stored order and expands each four-bit component to eight bits.

Each bundle channel starts with a 10-byte initial-state record followed by a
word-opcoded program. Disassembly of interpreter `$214aa` establishes all
operand widths for opcodes `$00` through `$14`; native parsing now rejects
unknown and truncated commands. Higher-level names remain deliberately absent
until their external calls and state effects have been verified.

The final two auxiliary pointers bound an indexed payload: a big-endian
longword table followed by the data it addresses. Verified bundle 0 contains
143 populated boundaries for 142 records and bundle 1 contains 75 boundaries
for 74 records. The native parser checks the
strictly increasing used prefix, zero-filled unused tail, and all blob ranges;
the contents remain neutrally named pending caller-level format proof.

Caller `$20c8c` identifies those records as compressed four-bitplane bitmaps.
The normal `$20da6` path has four RLE control classes and writes interleaved
plane words into a 320-pixel, 40-byte-per-plane row layout. Native decoding now
covers all 74 normal records in bundle 1 and the 72 unflagged records in bundle 0.
The remaining 70 bit-15 records use the same RLE controls through `$20eb2` but
store complete planes sequentially; that path is also implemented and tested.

An SDL-independent interpreter now advances the original channel state at the
same tick boundaries as `$21380`/`$214aa`. Early opening-sequence assertions
lock palette, sound, bitmap, coordinate, and wait-state changes to the real
program. Timing-dependent random commands accept only an explicit compatible
source, preserving the distinction between deterministic decoding and hardware
timing still under investigation.

The first title input boundary is now anchored too. Channel 3's `$14,$0001`
wait reaches `$0f,$00000b38` on tick 82 when the original gate and prior input
state are asserted; the VM exposes the raw alternate-resource pointer. The
main loop independently takes its first accepted CIA-A bit-6 input via
`$21982`, returns profile one to the bootstrap, and table routine `$12b30`
loads `0x6ca00` bytes from disk `0x6e000` into `0x13000`. This documents a real
handoff, not an assumed playable-session implementation.

The timing source is now reproduced: VBL server `$207fe` increments a counter
by four after each scheduler pass, while `$2016a` combines that counter and a
16-bit seed to read a word from the current genuine bundle. The first opening
random command is reached on tick 145 and returns `$0011` in the zero-phase
startup. The possible pre-first-tick VBL remains represented as a phase choice.

## Initial DOS observations

Despite their `.EXE` suffixes, `2200AD.EXE`, `2200GX.EXE`, and `TITLES.EXE` are
not MZ files. They are flat 16-bit x86 binaries managed by the `MILL.COM`
launcher. `MILL.COM` installs private interrupt handlers including `INT 91h`,
`92h`, and `95h`; the game modules call these as a loader/runtime API.

`2200AD.EXE` starts with segment-register setup and a near jump from file offset
`0x0004` to `0xd1b0` (loaded address `0xd2b0`). Its static entry bytes establish
a stack and contain calls for DOS services, video setup and original resource
requests. Their external returns and the subsequent UI loop are not yet
observed. Literal names for bases, asteroid analysis actions, save slots, and
time units occur in this module and provide anchors for code cross-references.

The distribution also contains Creative Voice (`.VOC`) effects,
driver/resource binaries, and `GX.LIB`/`LAST.LIB`. This split is useful:
executable cross-references can reveal record widths and indexes in the resource
libraries. Analysis uses the repository's bounded in-place archive readers; it
does not require or authorize an extracted game-data copy.

The first directly parsed gameplay-static component is the 41-item
celestial-label sequence in genuine English `2200AD4.BIN`, beginning at
`$03d2`. It retains display bytes (including original padding spaces) and
source offsets only. That makes it usable by a future UI without pretending
that labels alone establish the state record layout or world rules.

English `TITLE.LIB` and `GX.LIB` are now opened through their shared banked LIB
container. The header locates a terminal 12-byte directory using a 16-bit
offset plus 64-KiB bank, and each entry uses the same banked addressing with an
eight-byte name. Genuine integration anchors cover all 38 title entries and
180 gameplay entries before image-code semantics are applied.

`TITLE.LIB` P00 (the actual offset-6 first entry, not the small P01 record at
`$2941`) decodes as a 320×200 image through the TITLES.EXE codec-2 nibble
stream. Its 14-entry delta table and literal/RLE controls are native code
anchors; decoded indices are row-major and hash to
`85ec11c9f943672df2ba2a4e2837ce1f3158d61648ec07bcdc84b381bd24f4ee`.
The hardware RGB6 palette and its logical-index translation are decoded from
the same original record; their precise layout is recorded in the preservation
ledger.

The DOS title executable and launcher now have a bounded, byte-validated
handoff model. `TITLES.EXE` selects resource zero, enters a 37-step
transition, polls `INT 21h/AH=06/DL=ff`, then reaches a private interrupt and
an external termination boundary after nonzero input. `MILL.COM` has ordered
DOS EXEC requests for `TITLES.EXE` and conditionally `2200ad.exe`; child
completion and returned AL remain unknown. The parser records those concrete
values without assigning unproven semantics to the transition drawing code.

## Reference-execution protocol

An observed DOS startup trace must use a trace-capable emulator with a read-only
archive-backed or in-memory DOS filesystem. It must not be produced by
extracting, copying, or modifying the user-supplied ZIP media. The record must
identify emulator and configuration versions/hashes, the original archive
SHA-256, command tail, input timeline, CPU control-flow events, DOS/BIOS/private
interrupt events, and file operations. A separately acquired original read-only
disk image is acceptable where its identity is independently hash-locked.

The development workstation has also been checked with a portable DOSBox-X
2026.01.02 installation kept outside this repository. It can mount the
hash-identified English DOS ZIP as a PhysFS source without changing the
archive (SHA-256 `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123`
before and after), but that outer archive layout does **not** expose
`MILL.COM` as an executable DOS file: DOSBox-X reports `Bad command or
filename`. This validates only the direct-archive mount environment, not a
game launch.

An external, portable Archivemount/FUSE read-only view of the same archive
does expose the original files without extracting or copying them. Through
that view, a time-limited DOSBox-X run executed `MILL.COM`, loaded its original
`MCGA.BIN`, installed its observed `INT 91h` vector, and reached DOS console
input. This is a coarse emulator log, not an Eon reference trace: it has no
program-counter records, register/flag returns, private-driver writes, or
controlled original input timeline. It neither proves title/game execution
nor authorizes an Eon runtime transition. The binary does offer debugger,
`INT 21h`, and DOS file-I/O logging; admissible observations require the full
protocol above and the resulting external evidence must be independently
reviewed before it can extend an adapter.

The observed original prompt is `Please Select Sound Effect Type`, offering
`0 = IBM Speaker`, `1 = Sound Blaster`, and `2 = Covox Sound Master`. Controlled
external selections `0` and `1` both reached `Thank You. Please Wait...`, then
the current DOSBox-X machine configuration entered repeated unhandled
interrupts (`INT 6`, followed by `INT 0`). This is an emulator/configuration
failure observation, not a statement about original-game behavior, a recovered
sound mapping, a valid title hand-off, or a license to select that value in
Eon's runtime. Further capture requires a separately validated machine/
driver configuration and the complete raw trace contract.

A second, deliberately independent probe used `core=normal`, `cputype=386`,
`cycles=fixed 12000`, and DOSBox-X's documented
`mcb corruption becomes application free memory=true` option against the same
Archivemount/FUSE read-only view. It reached the same original sound-selection
prompt, but the automated Xvfb key injection was not observed by the guest.
It therefore supplies no post-selection behavior and does not validate that
configuration as a remedy for the earlier interrupt loop. The next experiment
is a real interactive X/Wayland or DOSBox-X mapper input run with a
one-variable CPU-core matrix (`normal`, then `full`), retaining the read-only
mount and archive rehash before and after each run. Its logs remain external
evidence, not game data or a runtime input.

Project Eon's external trace admission format is specified in
[`REFERENCE_TRACE_FORMAT.md`](REFERENCE_TRACE_FORMAT.md). Generic v1 verifies
only a trace's hash, capture provenance and exact recognised source release.
The narrowly declared v2 adapters additionally validate literal, hash-pinned
observation sites for their own listed release; neither form replays events or
manufactures a platform-service result.

`--inspect` repeats release-specific collection prerequisites without turning
them into an execution request. The Spanish Millennium report requires a trace
of that image's own DOS child return/AL, file operations, and CPU/interrupt
events; English executables, state, and title behavior are never substituted.
Millennium Atari ST requires GEMDOS `Fopen` D0/handle behavior, TRAP #14 and
Line-A returns, configuration load address, and any codec/palette/planar
destination. Deuteros Atari ST requires the XBIOS `Floprd` result, callback
entry/return frame, dispatch word, and selected vector D1/D2 returns. These
are evidence-collection lists only: Project Eon does not fabricate their
values, perform the reported raw loads, or use another platform's assets.

### Deuteros Amiga title-stage protocol

The first title-stage hard ABI boundary is `$40450`. Its vectors are
`exec.library` `SuperState` (`-$96`) and `UserState` (`-$9c`); later code opens
`graphics.library` through `OpenLibrary` (`-$228`) and has a `LoadRGB4`
(`-$c0`) call shape. A later `-$1a4` graphics vector is `ChangeSprite`, not
evidence of display creation. The supplied ADFs contain none of the required
Kickstart/graphics implementations, library bases, ViewPort/ColorMap/VSprite
objects, or the bootstrap-established longword at `$12ff4`.

A future trace must therefore include user-supplied legal Kickstart/Workbench
media, ROM SHA-256 and version, emulator/configuration identity, Exec base,
privilege-transition result, `OpenLibrary` result, `$12ff4..$12ffb`, initial
memory writes, graphics call arguments and affected structures, and custom-chip
register writes. A transcript replay may be implemented only against that
hash-locked input; unknown ROM/version or missing cells must be rejected. No
synthetic library handle, pointer, palette target, controller or return value
is permitted.

## Completion criteria

- All supplied platform releases are detected and their required resources load.
- Both games can be completed using the shared simulation.
- Original mode matches reference captures for graphics, audio, timing and UI.
- Modern mode can be switched without changing simulation or save state.
- Saves round-trip deterministically and compatibility fixtures pass.
- No original game assets are distributed by this repository.
