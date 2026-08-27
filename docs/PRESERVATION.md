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
| Millennium Atari ST `MILENIUM.TOS` | 49,269 | `4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686` |

## Verified format knowledge

- Nested ZIP parsing validates ranges, Deflate completion, output size, CRC-32,
  and SHA-256 before classification.
- DOS and Atari ST media use a native read-only FAT12 reader with validated
  geometry, bounded cluster chains, loop detection, and directory parsing.
- Standard Amiga ADF geometry is 80 cylinders × 2 sides × 11 sectors × 512
  bytes. Both clean Deuteros boot blocks pass the carry-around checksum.
- Deuteros identifiers are `DOS\0` (system) and `DEU\0` (custom data). Logical
  block 880 is game code/data rather than a normal AmigaDOS root directory.

### Millennium Atari ST relocation evidence

The verified Equinox `MILENIUM.TOS` PRG has 227 compact GEMDOS relocation
sites. Project Eon retains each site and the original unrelocated big-endian
longword, without choosing a load base or producing a relocated copy. The
first site is offset `0x6`, value `0x0000115e`; the last is `0x1150`, value
`0x000139c8`. These values are read straight from the TEXT+DATA bytes in the
SHA-identified disk file and are native test anchors for future execution
research.

### Millennium AmigaDOS filesystem evidence

The Millennium archive contains six independently cracked images. The two
Razor images are standard 880 KiB `DOS\0` ADFs with an intact root block at
block 880. The verified Razor image has SHA-256
`fe83c10119ef9bf2953b6fcd9a13d07f2c276215aaa64e2e541402a527a616f2` and
root volume label `Millennium (Crack Razor)`. Its 72 root hash slots contain
no file entries: game content is loaded from raw sectors (not fabricated as
filesystem files). Four other supplied Millennium variants have game code at
the boot-declared root-block location, so they are correctly rejected as
non-standard AmigaDOS volumes.

`AmigaOfs` is therefore a strict, read-only OFS/FFS reader for future standard
images: it validates root, directory and hash-chain block types; detects
cycles; bounds every block reference; and refuses incomplete file chains. It
does not infer missing files or mutate image data.

### Millennium Amiga raw-loader evidence

Millennium's usable game media are not AmigaDOS files.  In the supplied Defjam
image (ADF SHA-256
`8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c`), the
checksummed boot block first loads 0x400 bytes from disk offset `0x400` to
memory `0x70000`.  The recovered 68000 first-stage code then issues these raw
`trackdisk.device` reads:

| Disk offset | Bytes | Destination | Observed continuation |
| ---: | ---: | ---: | --- |
| `0x24200` | `0x6e000` | `0x41000` | Call loaded stage |
| `0x16400` | `0x2c000` | `0x68000` | Jump to `0x68000` |

The latter hand-off places `0xa8d398fb` in `d6` immediately before the jump.
`MillenniumAmigaLoadPlan` recognizes the actual instruction sequence, derives
the two lengths from its immediate values (`0x1600 * 0x50` and
`0x1600 * 2 * 0x10`), and bounds both ranges against the ADF. It deliberately
does not claim those ranges are filesystem files, decompress them, or write
them to a cache. The alternate supplied crack images alter boot/loader code;
they remain separately fingerprinted media rather than assumed equivalent
executables.

The destination `0x68000` begins with a separate, directly verifiable resident
entry gate: `JSR $787d4`, test byte `d3`, conditionally OR `0x0100` into `d0`,
then store the resulting word at `0x7b75a` and return. The call target lies
inside the first RAM stage (`0x41000..0xaefff`), but that stage has already
been transformed by its preceding loader call; its corresponding raw disk
bytes are not treated as an executable or an inferred compression format.
`MillenniumAmigaResidentEntry` therefore records only the literal gate and
fails closed if any opcode, target range, or return instruction differs.

### Deuteros Atari ST protected-media boot chain

The supplied Atari ST collection consists of protected/cracked raw `.st`
images; it does not include a pristine, ordinary GEMDOS release.  Although the
boot sector retains a 720 KiB BPB (512-byte sectors, two heads, nine sectors
per track), its FAT root area is overwritten by loader/data bytes.  Project
Eon therefore must not present this medium as a valid FAT12 filesystem.

Both verified evidence disks retain the Atari boot-sector word checksum of
`0x1234`.  Replicants Disk 1
(`aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee`)
branches to `$1e`; its literal XBIOS `Floprd` argument setup at boot offset
`$50` reads track 70, side 0, sectors 1 through 9 (4,608 bytes).  The SHA-256
of that direct sector interval is
`dad3594c53375bd8285ef33e2d685bd38a5b38d930f2ea1305d117d63667f168`.
This is a raw first stage, not a resource archive and is only read in memory.
Its word branch enters at stage offset `$9c4`; there it validates bytes
`$0006..$0441` using seed `$22225555`, add-byte / rotate-left-eight and
expected value `$7ae26af7`.  Only on that validation path does the recovered
code request the next raw interval: track 2, side 0, sectors 1 through 9 to
RAM `$70000`.  It then retains the prior 4,608-byte buffer for a byte copy to
`$1e00`.  These are control-flow facts, not claims that the next interval is a
title screen; the latter remains unclassified.

That track-2 interval has SHA-256
`2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7`.
At its loaded address `$70000`, it is executable code rather than a resource:
it configures supervisor stack `$7b000`, application stack `$2478`, then jumps
directly to `$1ec4`.  Its local raw-reader routine at `+$60` caps each XBIOS
request at nine sectors and maps linear tracks from `$50` onward to side 1.
The `$1ec4` target belongs to the preceding protected load chain; no title or
game-stage meaning has been asserted for it. Specifically, the preceding
routine copies `0x1200` bytes from a runtime-supplied `A0` to `$1e00` before
that handoff; no static source address for `A0` is established by the raw
track bytes. Project Eon therefore refuses to reinterpret disk byte `+0xc4`
as executable code merely because it shares the numeric offset.

The supplied unlabelled Disk 2
(`5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193`)
branches to `$22` and carries the literal `KILLER_BOOT\0` marker.  Its
subsequent resource/title semantics remain unclassified.  The runtime reports
these boundaries rather than inventing a GEMDOS title path or unpacking media.

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

### Deuteros Amiga opening audio

Channel opcode `$0b` passes its two words to `$22ab8`: the first is a sound
index and the second is a four-bit Paula-channel mask. The initialization
routine at `$212ca` installs bundle auxiliary pointer 3 in `$22aa6`; pointer 4
is subsequently consumed as the following resource, giving a strict boundary
for this table. In bundle 0 this is `0x121b4..0x122de`: 21 exact 14-byte
records plus four raw tail bytes (`0001ce8e`) which the 14-byte stride cannot
reach. They are retained verbatim rather than treated as padding or a guessed
twenty-second sound record.

Every record contains a bundle-relative DMA address (longword), DMA length in
words, Paula period, volume, and two raw control/parameter words. The
hardware routine at `$22bea` copies the first ten bytes directly to
`AUDxLCH`, `AUDxLEN`, `AUDxPER`, and `AUDxVOL`; the remaining two words are
retained verbatim because `$22c08` onward uses their individual flags for
runtime modulation and looping. The native reader validates the boundary,
nonzero period/length, `volume <= 64`, and that the complete `length × 2` DMA
range remains inside the original bundle. It returns the exact signed 8-bit
Paula PCM bytes in memory only; it neither converts, unpacks, nor writes the
game media.

The opening's second scheduler tick proves live use of entries 1 and 2:
`$0b,$0001,$0001` then `$0b,$0002,$0002`. They share source offset `0x2a8b`
and length `0x40bc` words (`0x8178` bytes; SHA-256
`f23fcd05f543be31726271b08ebfe7d907acfe31d1780aaf286fd2db701ae5d5`), while
their original periods are respectively `0x01c0` and `0x01c2`. SDL playback is
now enabled for the recoverable first DMA pass. The native Paula mixer takes
only an emitted `$0b` event with a nonzero bundle-table index, applies its low
four mask bits to AUD0..AUD3 exactly as the four `lsr.b` tests at `$22ad6`
through `$22b62`, and replaces each selected channel's DMA state. It uses the
original signed 8-bit sample bytes, original `AUDxPER`, and original
`AUDxVOL`; nothing is unpacked, filtered, looped, clipped, or replaced with a
generated waveform. The PAL sample clock is `3,546,895 / AUDxPER` Hz, carried
through the host's 48 kHz renderer as an integer phase accumulator so host
rounding cannot change the sample boundaries. Amiga's physical output routing
places AUD0/AUD3 on left and AUD1/AUD2 on right.

This is intentionally narrower than a guessed general sound driver. `$22bea`
first copies the descriptor to the four AUD register blocks, then executes the
raw words at offsets 10 and 12 through the modulation/loop branches at
`$22c08..$2301a`; their service cadence relative to the title scheduler has
not yet been proved. Project Eon therefore plays the authentic initial DMA
span and stops at its original `AUDxLEN × 2` byte boundary. Sound index zero
is also rejected for playback because `$22abc` selects the private `$22aaa`
descriptor rather than source PCM from the bundle. SDL receives at most one
20 ms host queue of this verified output, so it cannot be padded with made-up
silence. The unresolved control-word service timing remains a preservation
research item, rather than a reason to invent looping or modulation.

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
blob. Bundle 0 reserves 160 index slots, with 143 populated boundaries for 142
records in a `0x1ce96`-byte blob. Bundle 1 reserves 128 slots with 75 boundaries
for 74 records in a `0xb95e`-byte blob. In both, record 0 starts at offset zero, subsequent used
offsets strictly increase, and the unused table tail is zero-filled. The parser
validates these invariants but does not yet label the record contents as
graphics until the consuming routine is fully traced.

Routine `$20c8c` proves that this bank contains four-bitplane bitmap records.
For the normal path, each record begins with total planar words per row and a
height. The RLE control byte's top two bits select literal words, a repeated
byte-pair, a short repeated word, or a 14-bit-length repeated word. Decoded
words cycle through planes 0–3 for each 16-pixel group; bit 15 is the leftmost
pixel. All 74 records in bundle 1 use this normal path. Bundle 0 mixes 72
normal records with 70 bit-15 records; `$20eb2` proves those use the same RLE
classes but store each complete plane sequentially. Both paths now decode
natively, covering all 216 records in the two verified bundles.

As a stable decoded-output anchor, bundle 0 record 1 is 48×17 pixels, has 311
nonzero pixels, and its 816 palette indices have SHA-256
`fca175276cfe376b85e936f455aa9e89d1a0d4c89a61d2b6ce317fa6aa58a6a3`.

### Channel runtime

The native, SDL-independent channel VM mirrors the 24-byte state consumed by
`$21380` and the opcode effects at `$214aa`. Implemented state transitions
include bitmap selection, signed coordinates, palette selection, timer waits,
audio-position waits, stepped vertical motion, relative jumps, one-level
calls/returns, sound events, alternate-resource selection, input gates, and
transition requests. Random opcodes require an explicit original-compatible
random source; absence fails closed instead of inventing a sequence. The
implemented `$2016a` source indexes the current bundle at
`(seed + vblank_counter) & $3ffe`, reads one big-endian word, adds 14 modulo
16 bits, then adds that result to the 16-bit seed. VBL interrupt `$207fe`
advances the 32-bit counter by four between scheduler calls. With the verified
zero start phase, the first opening random command occurs on tick 145 at
counter `$240` and returns/seeds `$0011`. A VBL can race the very first
scheduler call on original hardware, so an alternate startup phase remains an
explicit runtime parameter rather than being erased from the evidence model.

The opening program provides tick anchors from genuine data. Tick 1 only
decrements initial waits. Tick 2 selects palette 1, enables the input gate, and
emits sound `(1,1)` then `(2,2)` and immediately consumes the newly yielded
timer once, as the original scheduler does. Tick 3 selects bitmap 1 at word
coordinate `x=8`, pixel coordinate `y=183`; its blank-backed 320×200 frame has
SHA-256 `d841fd0e6e01c09f7dc8ce6cd2bda1828a0eb62c5f198750403aa996cd7d48d4`.
Tick 4 enters stepped mode 6, moves to `y=181`, and leaves timer 38.

### Deuteros Amiga title input and bootstrap handoff

This is a control-flow fact, not a reconstructed game-menu interpretation.
Channel 3 of bundle 0 begins with `$03,$0050` and then `$14,$0001`. The
scheduler at `$2140c` resumes `$14` only after both the global gate at
`$2171e` is set and the previously-polled input word at `$21720` is nonzero.
The opening's `$13` sets that gate on tick 2. With continuously asserted prior
input, the real channel reaches `$0f,$00000b38` on scheduler tick 82; `$0f`
stores that bundle-relative address at state offset `$0c` and replaces the
selector with `$fe`. The VM reports this exact pointer as an alternate-resource
event, without giving it an invented gameplay name.

Separately, the main loop polls active-low CIA-A port-A bit 6 at `$bfe001`
after `$21380`. Once the gate and recorded input are both set, the first-buffer
path branches to `$21982`. For the opening's initial index zero, `$21982`
writes one to `$21704` and calls `$218cc`; its confirmed post-display path
increments that value to two and branches to `$21a4c`. That routine writes one
to `$219f4`, copies it to bootstrap return slot `$12ffc`, and returns.
Bootstrap table entry one at `$12a3a` is routine `$12b30`: it requests raw
decoded track data from disk offset `0x6e000`, length `0x6ca00`, into memory
`0x13000`. Project Eon retains these load constants as `title_handoff_profile`;
the first word of that real stage is verified as `JMP $00040426`. The target
is range-checked against the loaded interval and retained as `title_stage`;
the runtime still reads this source ADF range in place and does not unpack it.
The entry begins by preserving the bootstrap's `A1` value at `$206a0`, storing
the passed mode word at `$4040e`, and comparing its low byte with five. The
meaning of those mode values and the later gameplay dispatch remain unknown.
For mode five it copies the byte to `$3717e` and writes `$0101` to `$38092`;
every other path writes byte one to `$19d52`. After shared setup, the recovered
recurring loop starts at `$40574`, calls `$222c0` then `$23e4e`; a mode/input
change clears `$40410`, and the loop compares it with `$0000ea60` before the
original `$4069a` dispatch, subject to another original-state check. The strict
parser validates these operands directly and does not claim their gameplay
semantics.

When that counter reaches the verified threshold, the call at `$405b6` enters
`$4069a`. This transition sets byte `$202c6`, saves and clears word `$202b8`,
then copies exactly sixteen RGB4 words from `$1ed24` to `$40678`. Each copied
word is ANDed with `$0eee` and shifted right once before being written. It then
uses the original graphics-library base from `$12fec` for vectors `-$c0` and
`-$1a4`; both vector calls and all operands are opcode-validated. Project Eon
records this as a timed title display transition, not as a guessed description
of a menu or gameplay screen.

The same `$4069a` routine has a bounded, verified return phase. It reads and
compares words at `$1ffc8`, `$1ffce`, and `$1ffd4`; on its original branch it
supplies `$12e12`, `$1ffda`, and `$1ffe6` to vector `-$1a4` and stores the last
address in `$2008e`. It subsequently clears `$202c6`, invokes `-$c0` with
`$12e12`, `$1ed24`, and count 16, restores the saved `$202b8` word from the
stack, and returns at `$40776`. The parser opcode-validates every fact here;
the addresses are preserved as raw machine-state boundaries rather than named
as a presumed menu, fade, or gameplay subsystem.

The compositor draws channels in ascending order into a persistent four-plane
display. X is measured in 16-pixel words and Y in scanlines. Bit 15 alone
selects `$20fb2` masked drawing where palette index 0 is transparent; an
unflagged selector overwrites the complete rectangle. Original code clips
vertically but trusts horizontal coordinates, so native code validates the
horizontal range instead of silently changing it.

The stateful paths are now also recovered from the shipped `$20c8c` code.
Bit 13 branches directly to `$21092`, which restores rows from the single
global scratch buffer at `$23024` at the channel's current Y coordinate.
Selectors with bits 15+14 (`$c000`) first clear both flags, take opaque
`$20d8e` decoding, then `$21034` copies each affected complete 320-pixel row
from all four planes into `$23024` and writes `$ffff` back to that channel's
selector. Thus the later bit-13 route restores exactly that sole buffer; it is
not per-sprite storage and is not inferred as a conventional background erase.
Project Eon follows this order with one persistent compositor buffer, rejects
a restore before a genuine save, and validates the `$ffff` state transition.

### Millennium DOS execution model

#### Title-to-game hand-off

The clean English DOS `TITLES.EXE` (7,022 bytes, SHA-256
`3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6`)
is a separate flat binary, entered at loaded address `$1b80`. Its code at
`$1c14` loads title resource index zero through its resource routine. The
transition routine at `$1941` starts with `CX=$25` and `DX=$0170`, so the
verified title transition contains 37 steps with that original stride.

The main title loop polls DOS `INT 21h`, `AH=$06`, `DL=$ff` through helper
`$0d0a`; at `$1c28` it branches out of the loop only after the returned `AL`
is nonzero. Cleanup writes zero to the process status byte at `$1a0e`, and the
common exit stub at `$1a12` executes `INT 21h/AH=$4c`. Thus the verified title
program itself does not execute the game binary: it exits with status zero.

The accompanying clean `MILL.COM` (1,445 bytes, SHA-256
`4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e`)
uses its EXEC wrapper at `$031c` first with loaded address `$068f`, the
NUL-terminated `TITLES.EXE` string at file `$58f`, then after return with
`$069a`, the adjacent `2200ad.exe` string at file `$59a`. This is the exact
DOS title-to-game hand-off used by the native parser. It establishes ordering,
not a new game-state model or an inference about title timing between calls.

The English DOS archive's `SFX1.VOC` is decoded directly as a Creative Voice
File: its verified SHA-256 is
`5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88`.
Its original type-1 PCM block has time constant `$9c`, unsigned 8-bit mono
rate 10,000 Hz, and 738 samples whose SHA-256 is
`811de4108fe6551e09da1865f3ff2e18a8313aad30a6916210c4d5d49b1e1c06`.
The native decoder accepts the original uncompressed sound and continuation
blocks, preserves the source PCM bytes, and rejects encodings not yet proven
by game media rather than replacing effects.

`2200AD.EXE`, `2200GX.EXE`, and `TITLES.EXE` are flat 16-bit binaries despite
their suffix. `MILL.COM` provides a private runtime through interrupts 91h,
92h, and 95h. `2200AD.EXE` jumps from file offset `0x0004` to `0xd1b0`, then
uses DOS services and loads original libraries. See the
[DOS analysis](generated/dos-millennium.md).

### Millennium Spanish DOS floppy evidence

The verified Spanish outer archive contains one 737,280-byte FAT12 image
(SHA-256 `1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d`).
Its 39 root entries include distinct `TITLE.LIB` (18,998 bytes, SHA-256
`30d6ccb95e7f501d59e72fc2e34583302116bd88f6eceaae989f6ad986ef7f19`) and
`2200AD4.BIN` (13,254 bytes, SHA-256
`8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31`).

The same native LIB reader finds 38 resources at directory `$486e`. `P00`
starts at `$000006`, has 10,555 bytes, and decodes to a 320×200 indexed frame.
Its index bytes match the supplied English release (`85ec…f4ee`), while the
Spanish palette/translation produces its own RGBA SHA-256
`667e297e1cd2860fa5dd6b10749d3af7859dad0844408a32a4d04a682153bc92`.
The reader therefore retains the release's actual palette rather than
substituting an English one.

Spanish `2200AD4.BIN` has its 41 NUL-terminated celestial labels at `$03db`,
not the English `$03d2`; Project Eon reads the media bytes at the observed
layout and preserves labels such as `Tierra ` and `Asteroides ` unchanged.
The floppy has `MILL.BAT`, not the verified English `MILL.COM` launcher, so
the title-to-game control boundary is deliberately not claimed for this
release.

The English DOS `TITLE.LIB` (18,907 bytes, SHA-256
`6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678`)
and `GX.LIB` (312,748 bytes, SHA-256
`4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f`)
share a verified banked container. Its six-byte header stores a little-endian
entry count, 16-bit directory offset, and 64-KiB bank byte. Each 12-byte entry
stores a 16-bit asset offset, bank byte, reserved zero byte, and NUL-padded
eight-character name. `TITLE.LIB` has 38 entries at directory `$4813`;
`GX.LIB` has 180 at `$4bd3c`. Native parsing rejects duplicate names,
non-monotonic resources, invalid padding/flags, and any range outside the
directory boundary.

The English `2200AD4.BIN` static-data file (12,494 bytes, SHA-256
`1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d`)
contains a NUL-terminated celestial-label table at file `$03d2`. The native
reader preserves all 41 labels and their exact byte offsets, from `Inner
System` through `Asteroids `. It does not trim the original trailing spaces or
invent a mapping from those labels to mutable simulation records: the loaded
file proves this immutable display table, not the full game-state layout.

The same data file begins with a verified 435-entry, 16-bit static-text
pointer table ending at `$0365`. It maps to 434 distinct raw records in the
English release (one target is intentionally shared) and is not target-sorted.
Project Eon preserves pointer order and raw record boundaries without assigning
meaning to the native control bytes. The exact cross-edition evidence is in
[the static-text report](generated/millennium-dos-static-text.md).

### Millennium DOS GX canvas

The first two `GX.LIB` entries establish a separate authentic bitmap path.
`IMG00` is a codec-2 240×33 resource containing a 256-entry RGB6 DAC after
its stream; `IMG01` is a codec-2 320×167 indexed canvas. Its 68-byte
post-stream index table selects entries from the `IMG00` DAC. Project Eon
decodes this pair in memory and retains the remaining resource-table bytes as
opaque rather than inventing UI or state meaning. Exact offsets, sizes and
pixel hashes are in [the GX canvas evidence](generated/millennium-dos-gx-canvas.md).
The SDL runtime reaches this resource only after the independently verified
`TITLES.EXE` non-blocking console-poll boundary has handed control back to
`MILL.COM`, which selects `2200ad.exe`. It preserves the resource as a canvas
and does not assign synthetic gameplay or interface meaning to its pixels.

At that identical verified boundary, SDL also opens `2200SAVE.I` through
`MillenniumDosSaveSession`. Its panel shows the archive-verified SHA-256,
fixed version word, and a paged listing of all 38 recovered four-word state
records under their literal `+00`, `+04`, `+06`, and `+08` positions. The
session is a private in-memory byte copy with no setters, save/export command,
or inferred state names; navigating the panel never mutates original media.

`TITLE.LIB` entry `P00` is the first genuine title image: extent `$000006` to
`$002941`, 10,555 bytes. Its codec-2 record declares 320×200 indexed pixels,
maximum index 35, and a `$25d7`-byte stream. The decoder consumes low nibble
then high nibble; controls `$0`–`$d` add one of 14 verified deltas modulo 36,
`$f` supplies an absolute index, and `$e` repeats the previous index. The
64,000 row-major indices hash to
`85ec11c9f943672df2ba2a4e2837ce1f3158d61648ec07bcdc84b381bd24f4ee` with
7,386 nonzero pixels.

The remaining `$348` bytes of P00 are verified VGA colour data. Relative to
the P00 record, the `$300` bytes at `$25f3..$28f2` are 256 consecutive RGB6
triples (DAC index 0 first, component order R/G/B); their SHA-256 is
`b6dd34314102e429fdd98390b1fda27d3ea94d16bfcefa2983e3e319a2a20eae`.
The 36-byte table at `$28f3..$2916` (SHA-256
`652ea21cfa18c27470daaee4521d863a3d377f803a5f80ba0132af49b24083d4`) is
retained exactly but neutrally named: this path does not prove its purpose.
The final 36 bytes at `$2917..$293a` translate the decoded logical indices
`0..35` to VGA DAC indices; their SHA-256 is
`cd7a7f81dd75249a8669e0f4c1792d99b37f3ea28c54319a3f2e84b4a86ff3e2`.
`TITLES.EXE` selects this exact latter address for its mode-1 `XLAT` path:
`record + $1c + word[record+$1a] + $300 + byte[record+$01] + 1`
(file `$139f..$13d6`, loaded `$149f..$14d6`). Its `P00` values resolve to
`$2917`. The verified display drivers write their supplied triples unchanged:
`VGA.BIN` `$104e..$105d` writes DAC index to `$3c8`, then three `LODSB` values
to `$3c9`; `TITLES.EXE` `$1226..$1232` uses the same order for animated DAC
entry 9. The hardware values are 6-bit, `$00..$3f`; Project Eon expands them
for SDL with `(v << 2) | (v >> 4)`, an explicit host-presentation adaptation,
not an invented original conversion. The resulting 320×200 RGBA title hashes
to `500a1451ab435a9c8ffaf1dbfaacee52cca0e32b375c883a45dd8f879a952888`.

## Automation integrity

The repository's sole GitHub Actions workflow has read-only repository
permission and runs Gitleaks over complete history plus native build/test jobs
on Linux, macOS, and Windows. It handles no releases, tags, artifacts intended
for publishing, or package uploads. Releases require an explicit maintainer
request outside CI; normal development is pushed directly to `main`.

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
first verified palette bank and both bitmap layouts are implemented and tested. Audio
mapping, full resource semantics, simulation, AI, saves, and timing remain incomplete. The SDL app
must report those areas honestly rather than presenting fabricated gameplay.

The SDL Deuteros launch view performs the complete verified chain at runtime:
outer archive SHA-256 → nested clean system ADF SHA-256 → boot/load plan →
bundle 0 → channel VM / original VBL source → indexed bitmap → palette → RGBA
texture. The session advances on a 20 ms scheduler cadence and supplies only a
recovered held input signal to the VM; the VM controls whether that signal is
accepted.
Thus the displayed pixels remain derived from user-supplied original data and
are not packaged in the executable or repository. Archives and ADFs are read
in place: no game file is unpacked, copied, installed, or written by runtime.
