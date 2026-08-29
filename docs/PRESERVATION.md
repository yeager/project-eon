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

### Scanner admission and duplicate accounting

The direct-media scanner first enumerates regular files in lexical path order,
then hashes only files whose byte length occurs in the outer-release manifest.
It never opens a ZIP, extracts a leaf, or selects a game by name during
recognition. A digest match is one *verified occurrence*. Equal digest matches
at multiple locations (including a user-created link) are deduplicated to one
release card and one CLI launch target; the first lexical path is retained as
the deterministic in-place source and later occurrences are counted, not
silently treated as separate editions.

`--inspect` prints aggregate `SCAN SUMMARY` counters: candidates, manifest-size
matches, hashed candidates, verified and duplicate occurrences, unique
releases, and unreadable candidates. The report deliberately does not print
unrecognised filenames or infer their platform: it makes admission and scanner
failures auditable while preserving the strict content-addressed boundary.

### Scan-to-use identity binding

Recognition alone is not authority to parse a later version of a path. Before
each inspection report, title/bootstrap load, or reference-trace report,
Project Eon reopens the selected outer archive, verifies that exact in-memory
byte stream against the `ReleaseArchive` SHA-256, and only then walks its ZIP
directory or extracts a hash-addressed leaf. The release's game, platform and
language must also still form one exact compiled manifest record. A renamed,
replaced, truncated, or metadata-forged path is rejected rather than inheriting
an earlier scanner result. The original archive is still read in place; this
does not create a cache, unpacked copy, or mutation.

The same binding applies to external trace admission. A valid trace manifest
cannot lend provenance to an archive that changed after scanning: Project Eon
re-verifies the trace's selected outer archive before emitting its report.

### ZIP structural boundary

After an outer archive has passed that byte-identity check, its classic ZIP
directory is still treated as untrusted structure. Project Eon accepts only
unique, relative `/`-separated entry names (no NUL, backslash, absolute,
`.` or `..` component), and verifies every local header name, flags, sizes,
CRC and payload range against the central record before any leaf is exposed.
Local payloads and optional classic data descriptors must end before the
central directory; descriptors must repeat the central CRC and sizes. This
prevents a malformed archive from presenting ambiguous inventory paths or
from treating directory metadata as disk data. ZIP64, multi-disk, encrypted
and oversized records remain explicit unsupported boundaries.

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

## Machine-readable release/profile manifest

[`release-manifest.json`](release-manifest.json) is the canonical interchange
manifest for the currently verified corpus. Each parser profile names an outer
release SHA-256, a leaf SHA-256 and size, plus the exact byte span used as its
present evidence. The compiled `release_manifest` table uses the same records
for native recognition and tests prove every declared profile against the
user-supplied archive. A profile is not transferable to a matching filename,
a different language, an alternate/cracked dump, or another platform. The
manifest contains no game bytes and does not ask the runtime to extract, copy,
or mutate media.

The manifest has a source-parity test: the compiled profile table must exactly
reproduce each JSON record (ID, outer and leaf identities, size and span). This
prevents preservation tooling and the runtime from silently diverging. It now
includes every English DOS leaf consumed by a recovered parser (game flow,
static data, overlay, both video drivers, last screen, title library, launcher
and first decoded voice), alongside narrower instruction/resource windows where
a parser intentionally has a smaller evidence boundary.

It also records whole-image evidence for parser families whose inputs are
recovered from an original FAT12 image rather than exposed as an archive leaf:
the Millennium Atari ST PRG, configuration and auxiliary-resource chains, and
the separate physical control-text dump. Spanish DOS title, static-text and
launch-manual parsing is likewise anchored to its original floppy image. These
full-image spans are explicit preservation boundaries; a future sector-accurate
span may replace one only with a measured mapping and a regression test.

### Stable evidence anchors

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Deuteros Amiga clean system ADF | 901,120 | `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38` |
| Deuteros Amiga clean data ADF | 901,120 | `99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a` |
| Millennium Spanish `2200AD.EXE` | 54,566 | `9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6` |
| Millennium Spanish `GX.LIB` | 311,420 | `e27d1c697da677994e2f864a776f4fc900c7feb4ec4b85500b2bfea3bc834767` |
| Millennium Atari ST Equinox disk | 819,200 | `3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7` |
| Millennium Atari ST `DATA12.BIN` | 932 | `6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda` |
| Millennium Atari ST `MILL22B.INF` | 84,720 | `e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9` |
| Millennium Atari ST `MILENIUM.TOS` | 49,269 | `4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686` |

## Verified format knowledge

- Nested ZIP parsing validates ranges, Deflate completion, output size, CRC-32,
  and SHA-256 before classification.
- ZIP parsing additionally rejects multi-disk/ZIP64 archives, encrypted
  entries, inconsistent central-directory extent, and local-header method,
  flag, filename, size, or CRC disagreement. For data-descriptor entries,
  central-directory CRC and sizes are authoritative because local fields may
  be placeholders; non-descriptor local values must match. The classic
  central directory must end exactly at the EOCD, and the EOCD comment length
  must reach the physical end of the supplied stream; a marker-looking value
  inside a comment cannot hide the actual directory. These checks are applied
  before recursively reading any DOS, Amiga, or Atari ST leaf bytes.
- DOS and Atari ST media use a native read-only FAT12 reader with validated
  geometry, bounded cluster chains, loop detection, and directory parsing.
- Standard Amiga ADF geometry is 80 cylinders × 2 sides × 11 sectors × 512
  bytes. Both clean Deuteros boot blocks pass the carry-around checksum.
- Deuteros identifiers are `DOS\0` (system) and `DEU\0` (custom data). Logical
  block 880 is game code/data rather than a normal AmigaDOS root directory.

### Amiga and Atari ST corpus boundary census

The platform labels in outer and inner archive names are catalogue metadata,
not release identity.  The following census is anchored in the complete leaf
image hashes and container bytes, rather than treating a `cr`, `a`, or `save`
name as a semantic property of the original program.

| Corpus | Supplied leaf media | Container fact | Admitted entry evidence | Documentation/control status |
| --- | --- | --- | --- | --- |
| Millennium Amiga | Six ADFs: five 901,120-byte images and one 698,368-byte image | `DOS\0` is present, but the usable program path is raw-sector data; the valid Razor filesystem has no game files. | The Defjam-family bootstrap requests `$24200..$923ff` to `$41000`, then `$16400..$423ff` to `$68000`; the shared resident span is hash-identified. | There is no live standalone manual recognised by the bounded filesystem readers.  Visible function-key trainer text occurs only in altered variants and is not original control evidence. |
| Deuteros Amiga | Clean system/data ADFs plus comparative alternate, save, and modified images | The clean system disk is `DOS\0`; clean data disk is `DEU\0`, whose logical block 880 is custom raw data rather than an AmigaDOS root. | The clean system boot path loads `$5800` to `$20000` and has entry `$21734`; title-stage transfer remains separately bounded. | A genuine on-disk text block contains load/save prompts, but no caller-connected input binding is yet recovered. |
| Millennium Atari ST | Two physical-dump `.stx` images, one save image, and four one-disk `.st` variants | `.stx` is retained as a physical-media container and is not silently converted to a flat FAT image.  Five of the seven supplied images have a valid FAT12 volume. | The hash-identified Equinox FAT12 image admits `MILENIUM.TOS` and the `$77000` bootstrap only; its initial `MILL22A.inf` `Fopen` remains a GEMDOS boundary. | Original physical-dump bytes contain visible mouse/keyboard and prompt text, but no code-to-input map has been recovered. |
| Deuteros Atari ST | Eleven 737,280/1,056,768-byte `.st` images | The 737,280-byte game-media candidates have a BPB-shaped boot sector, but their apparent root records are not a live FAT12 namespace: entries carry impossible cluster/size combinations.  The raw protected boot chain is authoritative. | The hash-identified raw chain reaches the first and second stages through explicit nine-sector reads; its XBIOS callback and state selection remain boundaries. | The supplied game-media variants contain no standalone manual.  Embedded prompts are preserved as raw text only; a separate 1,056,768-byte development/tools disk is excluded from game-control evidence. |

This protects two easy-to-make mistakes: a structurally plausible BPB does not
prove a usable FAT filesystem, and a printable string does not prove an input
binding.  A future decoder must preserve the original container selected by
its hash, read it in place, and reject rather than substitute a different
platform's filesystem or executable.

The clean Deuteros Amiga system ADF has a directly observed embedded text
region at ADF `$78bc0..$78d0f` (336 bytes, SHA-256
`66b312b5e7b148bdfe0e43af4d6cc6f4b451ed05f83be8edd7a1e11f17264680`).
It includes the original byte sequences `LOAD`, `Press 'L'`, `SAVE`, and
`Press 'S'`, followed by the original data-disk prompt.  The raw text is
retained unchanged, but neither its record framing nor a code reference from
the input dispatcher to this block has been demonstrated.  Project Eon
therefore does not expose `L`/`S` as reconstructed gameplay controls.

The Millennium Atari ST Disk 1 physical dump has a separate raw text
span at `$12420..$1258f` (368 bytes, SHA-256
`6330b762858bb4b1fb0bc17f4f577eca3b1e8de4c078fd3fc01192bcd05a89f7`).  It
contains `SAVE GAME`, `LOAD GAME`, `press left button to continue...`,
`MOUSE MODE`, and `KEYBOARD MODE`.  This is direct content evidence from the
SHA-256 `081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd`
physical-dump leaf, not an STX filesystem claim, executable entry point, or
SDL mapping.  It remains inspection-only until physical-track decoding and a
caller-connected control trace establish its use.

`MillenniumAtariPhysicalControlTextEvidence` accepts only that full
physical-dump SHA-256 and the exact 368-byte span. It locates the five
printable literals at raw offsets `$12425` (`SAVE GAME`), `$12436` (`LOAD
GAME`), `$12445` (`press left button to continue...`), `$1255d` (`MOUSE
MODE`), and `$12572` (`KEYBOARD MODE`). This is a bounded preservation parser,
not an STX decoder, a menu model, or a control map: it neither assigns a key
or mouse action nor attempts to execute the protected physical media.

### Millennium Atari ST relocation evidence

The Equinox FAT12 `MILL22B.INF` chain is separately hash-identified (84,720
bytes, SHA-256 `e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9`).
At file `+$11600` it has the isolated NUL-terminated literal `MILL22E.INF`;
the immediately preceding 14 bytes end in `RTS` at `+$115fe` and hash-lock the
literal's local provenance. This is not evidence that any routine opens
`MILL22E.INF`, chooses one of its records, or decodes graphics. Project Eon
records the name only through its original FAT chain and does not render its
packed contents until a loader ABI, codec bounds, palette association, and
planar layout are independently recovered.

### Millennium DOS `LAST.LIB` screen evidence

The English DOS release contains `LAST.LIB` (18,117 bytes) with one literal
directory entry, `last`, at offset `0x6`.  The entry is a complete codec-2
indexed bitmap: flags `0x07`, 318 × 197 pixels, palette indices 0–15, and a
native 256-entry VGA RGB6 DAC plus its 16-entry logical-to-DAC translation.
Project Eon decodes that resource directly from the supplied archive in memory
and retains its original indexed pixels and palette before presenting RGBA.
The indexed-pixel SHA-256 is
`b13d52cab4ee715be28bca56997157fa102eaf86f53b0771c6b072dc0b701136`; the
derived RGBA SHA-256 is
`c0a556f3e618585967b9ed3d6c0606f958434c94def1afd0940658786a88dd17`.
The name `LAST.LIB` alone is not treated as proof of a narrative or gameplay
transition; no selection point is inferred until executable control-flow or
an original observation supports one.

The verified Equinox `MILENIUM.TOS` PRG has 227 compact GEMDOS relocation
sites. Project Eon retains each site and the original unrelocated big-endian
longword, without choosing a load base or producing a relocated copy. The
first site is offset `0x6`, value `0x0000115e`; the last is `0x1150`, value
`0x000139c8`. These values are read straight from the TEXT+DATA bytes in the
SHA-identified disk file and are native test anchors for future execution
research.

The earliest literal TEXT path is independently anchored too. Entry offset
`0x0` is `BRA.W 0x24`; that bootstrap loads `A0 = 0x115e`, `A1 = 0x1232`, and
`A2 = 0x1d636`, then post-increment copies longwords while `A0 <= A1`. It
therefore transfers exactly `0xd8` bytes (inclusive source range
`0x115e..0x1232`) from original DATA into BSS at `0x1d636` and makes an
absolute `JMP 0x1d636`. Project Eon validates and reports that path without
choosing a GEMDOS base, creating a relocated image, or executing the
as-yet-unanalysed transferred bytes.

The copied bytes start with a second strict stub: `MOVEA.L #0x77000,A1`,
`MOVEA.L #0x1d652,A0`, `MOVE.W #0x100,D0`, `MOVE.W (A0)+,(A1)+`,
`DBF D0,-4`, then `JMP 0x77000`. Thus it requests 257 original 16-bit words
from the literal address `0x1d652` into `0x77000` before the next transfer.
The source's provenance is now fully accounted for from the PRG layout and
that bootstrap. `0x1d636 - (TEXT + DATA)` establishes the observed load base
`0x116c4`, so `0x1d636` is the first BSS byte. The requested source begins
`0x1c` bytes into the transferred bootstrap: its first `0xbc` bytes are the
original DATA range `0x117a..0x1235`; its remaining `0x146` bytes lie in the
declared BSS and are therefore the loader-zeroed tail. Project Eon can form
that exact 514-byte source only in memory, retaining the original DATA bytes
and explicitly zeroing only the PRG's BSS portion. It neither unpacks nor
writes media, applies no relocation, and still does not execute the jump.

The second copy is now materialized only as that exact in-memory 514-byte
target at `0x77000`. Its verified first instructions are `MOVE.W #0x2,-(A7)`,
`MOVE.L #0x1d6e4,-(A7)`, and `MOVE.W #0x3d,-(A7)` (followed by the original
`TRAP #1`). These bytes are validated against the source transfer and reported
for preservation; Project Eon does not invoke the trap, emulate a GEMDOS call,
or infer anything about the target's gameplay meaning.

The strict next boundary is now accounted for without crossing it. The literal
pointer `0x1d6e4` lies `0x92` bytes into that reconstructed source and names
the original NUL-terminated `MILL22A.inf` string. The target pushes access mode
`0x0002`, that pointer, and selector `0x003d`, then executes `TRAP #1`; this is
the documented GEMDOS `Fopen` interface. It then pushes the returned `D0` word
and selector `0x003e` (the GEMDOS `Fclose` selector), but no second `TRAP #1`
exists in this proven range. The following `TST.L D0; BMI.S -2` tests the
`Fopen` return and branches back to its own branch opcode on a negative OS
return. Project Eon records those exact offsets (`+0x0e`, the prepared
`Fclose` selector at `+0x12`, and the `+0x18` self-loop) and validates the
filename bytes. It does not issue either service, model GEMDOS return values,
turn the loop into host behaviour, or infer the later successful control path
as gameplay.

The requested file is genuinely supplied rather than synthesized. On the
SHA-identified Equinox disk, the FAT12 root entry for `MILL22A.inf` starts at
cluster 3 and is 7,506 bytes. Its exact file-chain SHA-256 is
`74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6`; its
first recovered words are `0x4ef9 0x0002aa88`. Those are retained strictly as
file and machine-word facts, not interpreted as a configuration schema or
executed. A read-only inventory scan covers all seven supplied Millennium ST
images: five expose valid FAT12 volumes and four contain a regular
`MILL22A.inf` entry. The other two are raw/protected media and therefore have
no FAT12 pathname namespace to substitute. Project Eon reads a present entry
only through its original cluster chain in memory, and never creates, changes,
or falls back to a synthetic `.inf` file.

The same exact Equinox FAT12 root is now retained as a 13-file,
cluster-addressed inventory. `DESKTOP.INF` is cluster 2, 555 bytes, SHA-256
`ce2aa85b442be281f25c22456c0d081d01b51108e96716bba9f867b7e791ab19`;
`MILL22A.INF` through `MILL22F.INF` occupy their original root records; the
four 7,313-byte `2200SAVE.*` files, `DATA12.BIN` at cluster 442 (932 bytes,
SHA-256 `6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda`),
and `MILENIUM.TOS` at cluster 540 (49,269 bytes, SHA-256
`4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686`)
complete it. `MILL22E.INF` remains an opaque 302,892-byte original cluster
chain (cluster 122, SHA-256
`9aeb6aafceab228521725ffe687cd3d95406d7f272bca77f855ebb600664b2af`).
The inventory verifies each original FAT chain and digest before exposing it
to the bounded Atari bootstrap session. It establishes neither load order nor
file semantics: no `.INF`, save, data, or desktop file is opened, decoded,
written, or substituted because it appears in this evidence table.

| Original FAT12 entry | First cluster | Bytes | SHA-256 |
| --- | ---: | ---: | --- |
| `DESKTOP.INF` | 2 | 555 | `ce2aa85b442be281f25c22456c0d081d01b51108e96716bba9f867b7e791ab19` |
| `MILL22A.INF` | 3 | 7,506 | `74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6` |
| `MILL22B.INF` | 11 | 84,720 | `e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9` |
| `MILL22C.INF` | 94 | 9,597 | `a28a49eea33a14210193bbe6e36abf95700ac6789681bf1a9eac5d09a0999055` |
| `MILL22D.INF` | 104 | 18,428 | `de0a95d3e4659a305b3e55b3417a7648127b41866de0a0ca344a81c66979dbc0` |
| `MILL22E.INF` | 122 | 302,892 | `9aeb6aafceab228521725ffe687cd3d95406d7f272bca77f855ebb600664b2af` |
| `MILL22F.INF` | 418 | 22,123 | `26ef995a9c6a43647e7905477168980159d1426d90f901d4f4c32f7cf13e455e` |
| `2200SAVE.I` | 440 | 7,313 | `b0b91572a7cc8ca0b7b112a8ce09bcf0c6645c6b32df836ae8c2eb27d86c333a` |
| `2200SAVE.II` | 448 | 7,313 | `fa11ee72b3ca009d8a5d6cece8ff3f95b01b29ed53106e2d3730c9a545400065` |
| `2200SAVE.III` | 456 | 7,313 | `54519e0eebfe3f3a38b04e4b372caf67476148c135dafbfe8d0a4bcae601eae2` |
| `2200SAVE.IV` | 464 | 7,313 | `8c1709bb7aba3adc2e6538867383229c4d6a285d29a78fb431970d0d926ffbd2` |
| `DATA12.BIN` | 442 | 932 | `6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda` |
| `MILENIUM.TOS` | 540 | 49,269 | `4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686` |

The nonnegative fall-through after the self-loop is also byte-verified, but
is not executed. At reconstructed target `+$1a`, 26 original bytes have
SHA-256 `663d5f1418326aa9c0efde064ad95bda21c84d7f23241ce3505f21f1f07474d0`.
They push literal buffer `$2a500`, count `$20000`, the OS-owned `D0` handle,
and selector `$003f`, then issue `TRAP #1`; `ADDA.L #12,A7` immediately
cleans the prepared arguments. `$003f` is the documented GEMDOS `Fread`
interface. This proves only the static fall-through preparation: Project Eon
does not decide whether `Fopen` succeeds, invoke either GEMDOS service,
model a handle/result, or read/fill the target buffer.

The immediate 14-byte suffix after that static Fread boundary is now
hash-locked as the loader-to-configuration-buffer transfer boundary. At
reconstructed target `+$34` (address `$77034`) its SHA-256 is
`845d677c7c17d2152f0e89e0a396b6bbfb1ed6a75479a325b39310bbf0d99e58`.
The original words are `TRAP #1`, `ADDA.L #12,SP`, and `JSR $2a500`; `$2a500`
is exactly the Fread destination previously prepared in the same immutable
target. This is an instruction-edge fact, not a successful loader model:
Project Eon does not invoke Fread or the following trap, decide either return
value, populate `$2a500`, or execute its JSR. The separately read FAT-chain
configuration payload is preservation evidence only and is not substituted
for the native buffer.

The transfer target is deliberately **not** promoted to a proven configuration
load base. The exact first six bytes of the supplied `MILL22A.INF` are
`JMP $2aa88` (`4ef90002aa88`, SHA-256
`5c2fb1d412ca66ba8928a77c22eb0351ab5d3d6fd9c04cff1b037f25a94c7829`).
If file byte zero occupied the `$2a500` Fread/JSR destination, this jump would
name file `+$588`. The independently hash-validated static candidate entry
used elsewhere in this document is file `+$5aa` at `$2a4de`: a literal
34-byte disagreement. Project Eon records both address calculations and
rejects altered bytes, but does not choose an alternate destination, apply a
hidden prebuffer adjustment, or claim that either entry is dynamically
reached. Resolving this boundary requires native GEMDOS load-address evidence
and remains outside the current non-executing recovery.

The live Millennium Atari bootstrap session executes only the two proven
in-memory copies from `MILENIUM.TOS`, materializing the original 514-byte
target at `$77000`, then reaches the literal `Fopen` request above. It resolves
the same read-only FAT12 entry and stops before `TRAP #1`: no host file handle,
D0 result, config execution, or Atari display state is fabricated.
The SDL launcher creates this bounded session only for the exact identified
Equinox image when the Atari ST Millennium card or CLI target is selected; it
does not reuse the DOS title flow for that platform.

The Equinox payload's first JMP is now traced through its first proven control
block. Its absolute references establish an observed load base of `0x2a4de`,
which resolves the target `0x2aa88` to file offset `0x5aa`. At that offset the
original sequence first invokes `TRAP #14` with selector `0x15` and a zero
longword argument, then `TRAP #14` with selector `0x06` and longword
`0x2a612`. It contains literal JSR destinations `0x2b55a`, `0x2aa68`,
`0x2aa0c`, `0x2b2be`, `0x2b448`, and `0x2aa0c`, before `PEA 0x2ab0a`, a final
`TRAP #14` selector `0x26`, and `RTS`. Project Eon verifies this exact byte
sequence in the original FAT chain and reports the control facts. It does not
name or emulate trap effects, execute the JSRs, synthesize a configuration,
or write any disk data.

The second literal `TRAP #14` argument is not a palette and no service meaning
is assigned to it. At runtime address `0x2a612` (file `+0x134`) the exact 24
original bytes have SHA-256
`815bea3862908e01557486cae7d42132853c94348b49b920f9d3e88e14956c51` and form
two NUL-terminated strings: `MILL22D.INF` and `MILL22C.INF`. Project Eon
validates and reports those bytes as a bounded preservation fact only; it does
not open either name, invoke the trap, or presume the following JSR is
reachable because both preceding XBIOS return values are unrecovered.

The first of those literal JSRs is also bounded against the genuine file
chain. Address `0x2b55a` maps to file offset `0x107c`; its eight verified
bytes are `03 5a 4c df 7f ff 4e 75`. Project Eon retains the first word
`0x035a` only as an original dynamic-bit-operation boundary because its effect
depends on register state supplied by the caller. The complete following
local sequence is `MOVEM.L (A7)+,D0-D7/A0-A6` (`0x4cdf`, mask `0x7fff`) and
`RTS` (`0x4e75`). This does not execute the JSR, model the caller's registers,
or claim a routine-level meaning beyond the directly verified bytes.

The entry's second literal JSR target, `0x2aa68` (file `+0x58a`), is bounded
separately. It begins `0x0880 0x000d 0x6714`: an immediate-bit instruction and
its original conditional short branch. The branch target is `0x2aa82`; it
skips the 20-byte middle path and joins it at `0x2aa82`, a literal JSR to
`0x2a51c`. After that call returns, control falls through to the entry block
at `0x2aa88`, whose first original JSR at `0x2aaa0` targets `0x2b55a`.
Project Eon reports this converging control shape and
validates every byte through the following call, but does not invent D0,
evaluate the branch, execute either call, or assign a platform effect to the
intervening instructions.

The shared call target `0x2a51c` is now independently bounded at file
`+0x3e`. Its complete 32-byte local body begins `0x548f`, stores the literal
`D0` word through opcode `0x33c0` to `0x2a512`, contains original Line-A word
`0xa000`, stores longwords to `0x2a514` and `0x2a518`, then returns with
`0x4e75`. The Line-A instruction is deliberately opaque: Project Eon does not
choose a firmware implementation, invent register or RAM contents, execute
the helper, or treat those slots as a host-side configuration model.

The repeated entry-block JSR target `0x2aa0c` is a separately verified
forwarding boundary at file `+0x52e`: `JMP 0x2a5dc`. The 12-byte destination
at file `+0xfe` begins `0x3f01`, pushes literal selector `0x0019`, executes
`TRAP #14` (`0x4e4e`), performs original stack cleanup `0x504f`, and returns
with `0x4e75`. These are exact original machine-code facts only. Project Eon
does not invoke the trap, infer a selector meaning, choose a Line-A/XBIOS or
firmware implementation, or synthesize a result or configuration state.

The direct entry-block target `0x2b2be` is also now bounded at file `+0xde0`.
Its initial original words are `0x1400 0x0200 0x00c0 0x6600 0x003a`; the
conditional branch's exact destination is `0x2b300` (file `+0xe22`), where
the original bytes begin `0x0802 0x0006 0x6700 0x0090`. Project Eon preserves
the two D0-dependent gates and their literal branch shape only. It does not
choose a D0 value, execute either path, or infer a hardware, firmware, or game
state consequence.

Its complete local routine is now hash-locked through `RTS` at `$2b3a4`:
`$2b2be..$2b3a5`, file `+0xde0..+0xec7`, 232 bytes, SHA-256
`85c58759b0cb2f067734fb006aa543fc74926422187506914c823ceaaf9c6cd8`.
Every path remains local original code, but its branches/copies depend on
caller-owned D0, A3, A4, A5, A6 and loop registers; it crosses no native
service boundary and provides no recoverable input or display state. Project
Eon validates this immutable span and does not execute it or use it as a
replacement configuration result.

The direct target `0x2b448` is preserved through its complete local setup
prefix at file `+0xf6a`: it loads `D7=0x0006`, `A5=0x2b428`,
`A4=0x2b3c8`, `D6=0x000f`, `D5=0x0002`, and `D4=0x0100`. This is only direct
instruction/dataflow evidence. Project Eon stops before the ensuing loop body
and does not dereference the pointers, execute its loops or traps, or infer a
meaning for those registers and constants.

The immediately preceding 34 bytes are independently hash-locked as a static
adjacency anchor, not a newly claimed call path. At `0x2b426` (file `+0xf48`)
their SHA-256 is
`6f135d6e68a1b6c48826ae484223166f4e6061cd4b6b5cbc2d0dfcc2bc8fb550`.
They set literal `D0=0` and `D1=7`, contain `DBF D1,-4` back to `0x2b430`,
push `A3`, set `A5=0x2b3c8`, set word `D0=0x17`, and contain `DBF D0,-4`
back to `0x2b442`, before falling through to `0x2b448`. This is an immutable
68000 byte/control-flow fact only. No original callsite to `0x2b426`, loop
entry, pointer contents, native-service effect, or game-state result is
asserted or emulated.

The first local loop after that setup is fully bounded as bytes at `0x2b464`
(file `+0xf86`): a 22-byte original block ending in `DBF` opcode `0x51cd`
with displacement `-20`. The taken backedge returns to `0x2b464` itself; the
adjacent setup had supplied literal `D5=0x0002`. Project Eon validates this
exact backedge but does not run any iteration, read the loop's pointed-to
data, derive an iteration outcome, or translate it into replacement game
state.

The immediate fall-through after that inner `DBF` is also bounded. At
`0x2b47a` (file `+0xf9c`), the original six-byte path is `0x548d 0x51ce
0xffde`: the first word advances `A5`, and the second is an outer `DBF` with
displacement `-34`. Its taken target is `0x2b45c` (file `+0xf7e`), whose
verified prefix is `0x3a3c 0x0002`, the local D5 setup. Project Eon records
the backedge and literal setup only; it runs neither loop, accesses no loop
data, and invokes no native service.

The target's full local prefix is now also linked: `0x2b45c` contains exactly
`0x3a3c 0x0002` (`D5`) and `0x383c 0x0100` (`D4`), then falls through at
`0x2b464` to the already verified 22-byte inner-loop body. This is a strict
control/dataflow continuation, not an execution model: Project Eon does not
run an outer or inner iteration, read the referenced data, or derive state.

The only strict fall-through fact after the outer `DBF` is now documented up
to its first native-service boundary. At `0x2b480` (file `+0xfa2`) the
original pushes longword `0x2b428`, pushes selector `0x0006`, and reaches
`TRAP #14` (`0x4e4e`). Project Eon validates the exact 12 bytes and stops at
that opcode: it does not invoke or emulate the trap, infer its service, read
the argument's data, or manufacture a return value.

The 26 original bytes immediately after that opcode are separately retained as
a hash-addressed preservation anchor: address `0x2b48c`, file `+0xfae`, SHA-256
`34d497b9c4408944ea24d4eede21838f691c43d5a0d772db922187bed0e87fc8`.
This does **not** establish that the suffix executes: reaching it requires a
native `TRAP #14` return that Project Eon does not emulate. Its instruction
words are now also decoded literally: `ADDQ.L #6,SP`; `MOVE.L #0x4e20,D0`;
`SUBQ.L #1,D0` and `BNE.S -4` back to `0x2b494`; then `DBF D7,-78` to
`0x2b44c` (DBF is relative to its extension word); selector `0x0006` and another `TRAP #14`; followed by the same
stack cleanup and `RTS`. This records bytes, operands, and PC-relative
targets only. Loop effects, native service calls, return values, and any
resulting game state remain unrecovered rather than inferred.

The tail's corrected `DBF` target is also linked to its complete 24-byte setup
prefix at `0x2b44c` (file `+0xf6e`, SHA-256
`85f6e69ef8d058c021e0c70fe51375ef2f09a2c67c798c73f066ffdb6f14a187`). That
prefix is the literal A5/A4/D6/D5/D4 setup and falls through to the separately
validated loop body at `0x2b464`. This establishes a static byte/control-flow
relationship only; it does not make the native trap return, recurrence, or
loop effects runnable.

For the next disassembly phase, Project Eon now keeps a fail-closed whole-file
inventory of all 19 original `0x4eb9` absolute-JSR encodings. The first is at
file `+0x50c` to `0x2a5aa`; the last is at `+0xdb2` to `0x2aa78`. This is
explicitly a byte inventory, not a reachability claim: only the six encodings
in the independently verified entry block are established callsites. The
other patterns remain preservation anchors until their surrounding control
paths are proven from original bytes.

One of those inventory-only targets is now retained as a complete bounded
body, without promoting its caller to a live path. The original encoding at
file `+0xdac` names target `$2b576` (file `+0x1098`). The contiguous span
through `RTS` at `$2b5f8` is 132 bytes and has SHA-256
`07e36fd52b00af1557c0da08efc7388d9d7cf6567e9c24102267db80b34adcd8`.
It starts with original words `0x7000 0x47fa` and ends `0x4e75`. Project Eon
records this only as immutable disassembly evidence: the inventory establishes
an encoding, not reachability, register inputs, a calling convention, routine
meaning, native-service behaviour, or a game-state effect. The bytes are not
executed or translated into replacement logic.

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

All six supplied Millennium Amiga images nevertheless share one independently
validated resident raw interval: disk `+0x16400`, length `0x2c000`, linear
destination `$68000`, SHA-256
`d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e`.
`parse_millennium_amiga_shared_resident_layout` validates that exact range
directly from each original image—even the shorter Defjam `[u]` image, whose
complete resident interval remains present. This permits platform evidence to
be shared without claiming its divergent bootstrap or opaque first-stage raw
representation
is equivalent to Defjam's loader, and without executing, unpacking, or
substituting any variant.

| Disk offset | Bytes | Destination | Observed continuation |
| ---: | ---: | ---: | --- |
| `0x24200` | `0x6e000` | `0x41000` | Call loaded stage |
| `0x16400` | `0x2c000` | `0x68000` | Jump to `0x68000` |

The exact caller-side continuation is now independently fail-closed as well.
The 132 original bytes at loader address `$7029e` / ADF `+0x69e` hash to
`b8ca18e61e5372ba4387abd69f6796435671465ddaf48cd3a3e4b41e2528efdc`.
They contain the literal first read setup, indirect `JSR (A3)` at `$702e4`
after `A3` is loaded with `$41000`, then the resident read setup and terminal
indirect `JMP (A3)` at `$70320` after `A3` is loaded with `$68000`.
`MillenniumAmigaBootstrapOpaqueInvocationBoundary` checks the complete span,
both handoff addresses, and both already-bounded targets. It does not invoke
either target, infer that the first opaque stage returns to `$702e6`, interpret
the device calls, or treat the linear source bytes of the loaded stages as
their executable runtime representation.

The latter hand-off places `0xa8d398fb` in `d6` immediately before the jump.
`MillenniumAmigaLoadPlan` recognizes the actual instruction sequence, derives
the two lengths from its immediate values (`0x1600 * 0x50` and
`0x1600 * 2 * 0x10`), and bounds both ranges against the ADF. It deliberately
does not claim those ranges are filesystem files, decompress them, or write
them to a cache. The alternate supplied crack images alter boot/loader code;
they remain separately fingerprinted media rather than assumed equivalent
executables.

When the Amiga Millennium launcher target is selected, Project Eon creates a
bounded session only for the exact Defjam ADF above. It validates this load
plan, its shared resident range, and the resident entry directly from the
original disk bytes, then stops before the opaque first-stage invocation or
any AmigaOS behavior. Other recognised Amiga images remain preservation
evidence but are not silently substituted for Defjam's path.

For a reproducible chain of custody, the parser also reports a SHA-256 for
each exact raw source range (including the bootstrap). These are fingerprints
of immutable bytes read directly from the supplied ADF, not hashes of an
unpacked representation. The command-line verifier exposes them so a future
analysis can identify the exact input range before making any claim about the
runtime representation at that RAM address.

The raw Defjam first-stage source itself has shared AmigaOS/input text anchors:
within the `0x6e000` source span (SHA-256
`5ed30d5fe99c0dfc905bbe639d626be558f022514c83bc5ff287ad91014ccf7a`),
`exec.library`, `graphics.library`, and `input.device` occur at stage offsets
`+0x4a3dc`, `+0x4a648`, and `+0x4a936` (ADF `+0x6e5dc`, `+0x6e848`, and
`+0x6eb36`). The shared source windows `+0x4a5b0`/`0x160` (SHA-256
`97bb8cbe026ac3bba2c19cc296bc7cef00fbd0c8095c678f4cc303761b8b8309`) and
`+0x4a900`/`0x220` (SHA-256
`ee84336cbf4665bcd2bc48d054c024a20e4c5faaaf26cd5fdcc78e6b8f3931c9`) also
contain table-like keyboard characters. These are source-only facts: nearby
absolute references in the raw bytes do not map to the source's nominal
`$41000 + offset` address. Without an output mapping for the opaque invocation,
they establish no entry point, scan-code mapping, input behavior, graphics
resource, or display mode and are never used by the SDL runtime.

`MillenniumAmigaFirstStageSourceAnchorBoundary` makes this source-only
evidence fail closed. It requires the exact first-stage request
`ADF +0x24200`, `0x6e000` bytes, nominal destination `$41000`, and the full
source SHA-256 before checking the three NUL-terminated anchors at stage
offsets `0x4a3dc`, `0x4a648`, and `0x4a936`. It also verifies the two cited
source windows at `+0x4a5b0`/`0x160` and `+0x4a900`/`0x220` by their exact
hashes. This does not convert the source into an executable stage, establish
that the indirect call returns, model library/device calls, or expose an input
layout to the runtime; it only preserves reproducible input-media evidence.

The destination `0x68000` begins with a separate, directly verifiable resident
entry gate: `JSR $787d4`, test byte `d3`, conditionally OR `0x0100` into `d0`,
then store the resulting word at `0x7b75a` and return. The call target lies
inside the first RAM stage (`0x41000..0xaefff`), but the mapping from its
preceding raw stage invocation to runtime bytes is unknown; its corresponding
raw disk bytes are not treated as an executable or an inferred compression format.
`MillenniumAmigaResidentEntry` therefore records only the literal gate and
fails closed if any opcode, target range, or return instruction differs.

The Defjam boot window ADF `+$6a0..+$717` (120 bytes, SHA-256
`7f5a6cc8b273e8c3f15dc24d62812fe5daa3aba64720760c17c7c040d20ce49b`) proves
the exact request `$24200`, length `$1600 × $50 = $6e000`, destination
`$41000`, followed directly by `JSR (A3)` with `A3=$41000`. It contains no
caller-connected copy, XOR, decrunch, table, or bitstream loop. The raw source
hashes to `5ed30d5fe99c0dfc905bbe639d626be558f022514c83bc5ff287ad91014ccf7a`
and begins `18 c2 fc ff`; its F-line word precludes treating it as direct raw
68000 code. Project Eon therefore records an opaque raw-stage invocation, not
a proven transform algorithm or source-to-output mapping.

The next complete resident subroutine starts at `0x68016`. It takes three
successive words beginning at `A1+0x36`; for each, original `LSL`, `ROXL`, and
`LSR` instructions preserve the lower 15 bits in `0x7b764`, `0x7b766`, and
`0x7b768`, while the former high bit is written as a byte to `0x7b776`,
`0x7b777`, and `0x7b778`. It then calls `0x7ba12`; its return path reads the
last word/byte pair and conditionally negates the word. This is a byte-exact
control/data-flow profile only. Project Eon calls it
`MillenniumAmigaResidentWordSplitter`, does not assign gameplay meaning to the
values, and fails closed on any changed opcode or RAM operand. Its proven
pre-helper operation is also available as an in-memory transform: for three
already-resident source words it emits the exact three low-15-bit words and
three `0`/`1` former-high-bit bytes. It neither reads nor writes game media.

This is deliberately not an implementation of the full subroutine. No raw
resident caller of `0x68016` has been recovered, and the `JSR 0x7ba12` target
does not yet have a validated executable boundary in the supplied raw range.
After that call the original reloads `0x7b768` and `0x7b778`, conditionally
negates the word when the byte is nonzero, then returns; the helper may have
changed either location. Project Eon therefore does not claim a final return
value or invoke this transform from gameplay.

For preservation, Project Eon records the exact raw-media mapping that would
correspond to that helper address if the resident request were viewed as a
linear byte mapping: `$7ba12 - $68000 + $16400 =` disk offset `0x29e12`.
All five supplied Millennium Amiga variants share its first 32 bytes,
`0001200080ac00000100088042000001010080ac000001002080420000010010`,
whose SHA-256 is
`eb11f5c5dfda4234b0214599bffec09402deff2435c58d57db1f7ab84c07c434`.
This is a reproducible raw-byte boundary only—not evidence that those bytes
are the helper's original executable representation. The loader invokes an
unrecovered transform before the resident entry; no decompressor, helper
semantics, caller, or media write is inferred from this fingerprint.

There are two further raw-resident helper staging callsites at `$69624` and
`$69b88`. Each sets `A4` to a live runtime source (`$7cc3c` and `$7cc68`,
respectively), copies three words to `$7b764..$7b768`, then copies three bytes
to `$7b776..$7b778`. Both call `$7b77e`, clear byte `$7b14e`, and directly call
`$7ba12`. `MillenniumAmigaResidentHelperStagingCallsite` checks every
instruction and operand of these common tails in the supplied raw ADF. It
does not read the runtime sources, infer the `$7b77e` or `$7ba12` effects, or
execute either call. This preserves a real caller-side staging protocol while
retaining the helper's unrecovered executable boundary.

Immediately after the original word-splitter return lies a separate gate at
`$68078`: it literally calls `$7b816`, tests byte `D3`, returns at `$68082`
when nonzero, and otherwise continues at `$68084`. The target maps linearly to
raw disk offset `$29c16`; all six supplied variants share its first 32 bytes
with SHA-256 `a16a4738b0f577643c343b344ba8b6c19d935daf97dd2291c86ddb2b29dcd96c`.
`MillenniumAmigaResidentPredicateGate` records this exact control split and
raw provenance only. Neither the target nor continuation is interpreted or
executed, because the transformed runtime representation remains unrecovered.

The zero-`D3` continuation has one further fully local boundary before its
next unknown call. At `$68084` it preserves `A1`, compares word `A1+$12` with
`1`, and branches on inequality from `$6808e` to `$680ca`. The equal path reads
word `A1+$14`, prepares it, and reaches `JSR $7b90a` at `$68096`.
`MillenniumAmigaResidentPredicateZeroPathBoundary` verifies those exact bytes
and fingerprints the in-range raw target at disk `$29d0a` (SHA-256
`bdb907adb3114dbaa58eb3bbe516ab91ffc4e1bf70e536bd47f497f49c8d5042`). It
does not provide `A1`, interpret either selector result, enter `$680ca`, or
execute `$7b90a`.

The inequality target `$680ca` is separate static evidence: it pushes `D0`,
then `D2`, and reaches the same unknown `JSR $7b90a` at `$680ce`. No
post-call stack restoration, loop, or target effect is claimed by
`MillenniumAmigaResidentPredicateNotEqualPathBoundary`.

Separately, resident entry `$68508` begins with two fully local predicates: a
negative-`D3` branch at `$6850e` to `$68598`, then a byte test of `$7b142`
whose zero branch at `$68518` reaches `$6854a`. This is recorded by
`MillenniumAmigaResidentIndependentEntryGate` as byte-exact raw control-flow
evidence only; neither branch target nor the flag's gameplay meaning is
inferred.

The negative target is independently hash-locked: `$68598..$685fb` maps
linearly to ADF `+$16998`, is 100 bytes, and hashes to
`716e8bf1db5d7cad89a0074cf6fe7cc6a0a66d73379814bac181a5f6c4a9e500` in all
six supplied Amiga variants. Its parser reports only the external `JMP $7bcf8`
at `$685ee` and the separately encoded `RTS` at `$685fc`; runtime-dependent
cells, conditional outcomes, and the external jump are not executed or named.
The local alternate terminal tail is separately locked at `$685f4..$685fd`
(ADF `+$169f4`): its ten original bytes are `06 42 28 00 06 43 28 00 4e 75`
with SHA-256 `5b120eaef941ac336d22e4f76adaeefd8c1d6795d105685f048074edd49c3a6c`.
`MillenniumAmigaResidentNegativeD3Terminal` records the two encoded `$2800`
immediates at `$685f4` and `$685f8` and the `RTS` at `$685fc`. It neither
evaluates the two predecessor predicates nor assigns runtime meaning to their
register effects.

The adjacent complete local sequence `$685fe..$68619` maps to ADF `+$169fe`,
is 28 bytes, and has SHA-256
`a45ff5eca6e3594574b464574fa0aae3027bd2ea11472770708c96f4d21b56cc` across
all six supplied Amiga variants. It encodes two absolute byte stores to
`$7b3b5/$7b3bc`, copies D1/D2, tests D0, then records `BNE.S $68612 →
$68616` with the zero `RTS` at `$68614`. The target encodes `BPL.S $68616 →
$6861a` and the alternate `RTS` at `$68618`. `$6861a` remains the strict
continuation boundary. Project Eon does not choose either predicate, assign
meaning to registers or absolute cells, perform stores, or follow that target.

At `$68d62`, a literal local prefix reaches long conditional branch `$68d6e →
$68d78`, then unknown `JSR $778f0` at `$68d7c`. This is strict raw control
flow only; no register, path, target, or continuation meaning is inferred.

If that unknown JSR returns, the next 26 original bytes at `$68d82` / ADF
`+0x17182` have SHA-256
`e49e750f78946956c22d4cd80206139d38808d4ecb3b1579906aeaede0db7b77`.
They load immediate `$2208`, fetch a longword through absolute `$6934e`, add
`D0.W` to `D5`, store the fetched `D0` to `$7c256`, and reach `JSR $7b342`.
The linear raw correspondence for that target begins at ADF `+0x29742`; its
first 32 bytes have SHA-256
`731d016983d29dcb23abad28f3f0f225bd3708073e8c0c8481a97a50b460cdcf`.
This records a static post-return boundary, not a claim that `$778f0` returns,
that absolute RAM is populated, or that `$7b342` is executed or understood.

If both preceding unknown calls return, the six immediately following absolute
JSRs at `$68d9c..$68dbf` are another static-only boundary. Their 36 original
bytes have SHA-256
`08c660de1ed6d0b0f535e451c84450397383a923a1808fa9678d3ae85a8cc17b`:
they target `$7dba8`, `$7d8a8`, `$7d480`, `$7b594`, `$7d5c8`, and `$7b36c`.
The corresponding 32-byte linear raw-prefix fingerprints are respectively ADF
`+0x2bfa8` / `b388a3622caeeccac01d793650e63e192de821abc789ca334b6ba00a1475ca34`,
`+0x2bca8` / `819055da14479352b3f672e6db10424bdebb90230350b0e8088eb0cb0acbd087`,
`+0x2b880` / `dbb41359b827129e186a7cf2f4d79c7f45f11f4cbe53e964a0633b7ee7070df5`,
`+0x29994` / `e9aa8c8f766b3486163339990968f9829d29b69c3c991ed2a7fc71c483d16846`,
`+0x2b9c8` / `de1fdcc69a46a7f661c191fa69cd64a693053f4026708400ca4bc6defe224c79`,
and `+0x2976c` / `cbe69ef816a594b6e9c0e8a27d5cacc660920df3a0aebe9a31849c113a3f909f`.
This proves only an immutable post-return call tail and raw-media
correspondences: it does not establish any return, transformed executable
representation, callee semantics, or call execution.
The identical tail and every target fingerprint are checked against all six
supplied Amiga variants; their differing bootstrap paths remain separate
evidence and are not treated as interchangeable executable provenance. One
shorter supplied dump is checked by direct bounded raw spans, not forced into
the standard-ADF reader it does not satisfy.

The later common convergence at `$68f48` begins with unknown `JSR $7caa6`.
Only if that call returns, the following 42 immutable bytes at
`$68f4e..$68f77` / ADF `+0x1734e` hash to
`3220d65f197163401c649a36d756ecf3005d2f342b81de5a7d4528f9a45da851`.
They encode direct calls to `$7d6d2`, `$7780a`, and `$77b34` at `$68f4e`,
`$68f5a`, and `$68f6c`; literal address loads `$7c21b` and `$7c25c`; and a
terminal `JMP $7c54e` at `$68f72`. Their respective 32-byte linear raw-prefix
fingerprints are ADF `+0x2bad2` /
`4e2f8f40d56a7d2a46f654be0fe5df4edaf4ca6d3d0864cc2c6d41355fa8c5b4`,
`+0x25c0a` /
`dc67f3a81c04fbfb92bfdf7a8b88679dc07e3f61e90708198467ce3877ab5beb`,
`+0x25f34` /
`cfe704f22abb52092c496fdd49802da1d0a461f95474889a35c259cd47ca42c8`,
and ADF `+0x2a94e` /
`502069bdbda2f35899d16237fd1d2aa477be20f0c950231fb71f32583f23de14` for
the jump target. `MillenniumAmigaResidentSeparatePostExternalCallBoundary`
validates the raw continuation and all four correspondences for every supplied
variant. It does not claim `$7caa6` returns, that any later call returns, that
the absolute cells are live, that the target bytes are their runtime code, or
that the final jump occurs.

The terminal-jump target itself is retained as a larger source-only recovery
boundary. The 256 original bytes at `$7c54e` / ADF `+0x2a94e` hash to
`0149a457e657e18805ff61675e80741fa78d25f201f120498193315804b87eea` across
all six supplied images. `MillenniumAmigaResidentSeparateTerminalJumpRawTargetBoundary`
rejects any changed byte in that complete window. It deliberately does not
decode the bytes as instructions: the established loader transform means the
linear raw correspondence is provenance, not proof of runtime representation.
Nor does the boundary claim that the preceding unknown calls return, the
terminal jump executes, or the target has any recovered semantics.

The next local static control-flow prefix begins at `$68dc0` / ADF `+0x171c0`.
Its 14 bytes, SHA-256
`ef2fe6161118a1b0ac6cee838be9a4dc2b0483ba274a213d3ac653ea6f334e3b`, load
the byte at `$7c255`, compare it with `$0c`, then encode `BCS.W`. The
68000 word-branch displacement is based at its extension word `$68dcc`, so
the literal `$0020` resolves exactly to `$68dec` (not `$68dee`). That target
maps to ADF `+0x171ec`; its first 32 raw bytes have SHA-256
`13ed782f5463fd93bbd4376777a1c01d8fd636018de8aef52f5710eb0da11a2b` and
start by setting `A5` from `$7c25c`, testing `A5+$d`, and carrying further
static branch/call encodings. This records only raw control-flow provenance:
none of the prior calls are assumed to return, and no live RAM value, target
semantics, or later call is executed.

The `BNE.W` at `$68e0c` in that target has extension-word base `$68e0e`; its
literal `$005e` therefore resolves to `$68e6c`. The 36 raw bytes at that
destination / ADF `+0x1726c` hash to
`8cb29601f0c76406930e37d44b29853501857c36f3cb833ccdd32e78418597d4` and
contain two local compare pairs: the `D3` pair branches to `$68e80`/`$68e7e`,
and the `D2` pair to `$68e90`/`$68e8e`. Its direct continuation at `$68e90`
maps to ADF `+0x17290`, whose first 32 bytes hash to
`8a81ad1a39efe0442addd9302b3b0e5e0c0bd72ecaf5904d2fa5e1c2834cd964`.
This is byte-exact static provenance only: `D2`, `D3`, condition codes, and
all resulting paths remain runtime-dependent and have no inferred gameplay
meaning.

The next static prefix at `$68e90` / ADF `+0x17290` is 34 bytes with SHA-256
`f4a047914e83ab873a037ea16a4f5aaa9a402c38f48a525efc69d9e49cca15a8`.
It saves/restores `D0`/`D1`, loads literal `D3`/`D2` values, compares the byte
at `$7c24e` with `D0`, and encodes `BEQ.W` at `$68eae`. Its extension base
`$68eb0 + $0026` resolves to `$68ed6`, whose 32-byte raw prefix hashes to
`79871297097662cd29a3659d5399a17c847a8c46d6753e1d968cb27b83c5210b`; the
fallthrough `$68eb2` prefix hashes to
`cd83cab5400642c141e3252fd28302a94e7169d1f5bc7a6021cbe78c5daacd02`.
This is static provenance only: no register contents, comparison result, or
control-flow path is executed or assigned gameplay meaning.

The taken `BEQ.W` target at `$68ed6` / ADF `+0x172d6` is now independently
bounded as 30 raw bytes with SHA-256
`b2d2c6cadc50725eb8b4f0b680c325586ed457b29232481b503f3e337d589341`.
It encodes an `ADDI.B`, then `BCC.W` at `$68ede` (extension base `$68ee0`,
literal `$0014`) and another `BCC.W` at `$68eea` (base `$68eec`, literal
`$0008`), followed by `ADDQ.B`. Both branch encodings and the straight-line
fallthrough converge at `$68ef4` / ADF `+0x172f4`; the first 32 bytes at that
convergence hash to
`93b0d20954d235c624406450161a359968e4f1baefcbaeb47ede08fda0cd1e71`.
The parser checks the exact 30-byte source and 32-byte convergence prefix
against all six supplied Amiga variants. This remains static provenance only:
condition flags, memory-cell meaning, branch decisions, and gameplay effects
are not recovered or modeled.

The `$68ef4` convergence has a separate 34-byte static prefix at ADF
`+0x172f4`, SHA-256
`d63b2de78fbc18f2a4213206d1f05947a604dafc5b23fea56f87b624cb7549ab`.
It reads/stores a byte, then encodes `BEQ.W` at `$68f02`; its extension base
`$68f04` plus literal `$0026` resolves to `$68f2a` / ADF `+0x1732a`, whose
32-byte prefix hashes to
`ba2a0127999eb628ef05008867728fd31952c6d4b268bdb38f35130bab9973ae`.
The untaken static fallthrough is `$68f06` / ADF `+0x17306`, with 32-byte
prefix SHA-256
`5b3ae299a769dcca25b96b3b588ab65b1c44843abf0ef1288a1a74741dec9993`.
All three spans are checked across the six supplied Amiga images. This proves
only byte order, local control-flow encodings, and raw offsets—not flags,
registers, memory-cell purpose, selected path, or game behavior.

The taken `$68f2a` prefix is 36 bytes at ADF `+0x1732a`, SHA-256
`a7f4be625a6a39615f0ace12a1a8e013b781575625858b4f0c257d171b0947f3`.
Its local `BCC.W` encodings at `$68f32` (extension `$68f34 + $0014`) and
`$68f3e` (`$68f40 + $0008`), plus straight-line fallthrough, all converge at
`$68f48`. That address begins absolute-long `JSR $7caa6`; the 18-byte raw
call/following prefix at ADF `+0x17348` hashes to
`dde319f5e57db52df300956d4e3e59dc6dc7967f0ff582674d502109fcfa2f69`.
The JSR is an explicit preservation boundary: neither it nor the following
call is executed, and no call effect, return, flags, cell semantics, or game
behavior is inferred. Every listed span is checked across all six supplied
Amiga variants.

The independent fallthrough at `$68f06` / ADF `+0x17306` has a 24-byte raw
gate, SHA-256
`4a50d1c5f71ada9a3571e09b00437c51037c3949ff8e57a4b153ea032828d061`.
Its `BEQ.W` at `$68f1a` uses extension base `$68f1c + $002c` to reach the
same external-call boundary `$68f48`. Its alternate 12-byte local prefix at
`$68f1e` / ADF `+0x1731e` hashes to
`fc1fca692a8fc07b5fd7c502ae2d772eeff63c0c3d33d298f9c4fac414f337da` and
ends `BRA.W` at `$68f26`, extension `$68f28 + $0020`, likewise at `$68f48`.
This records only exact raw control-flow encodings; D7, flags, cells, selected
paths, external call effects, and gameplay behavior remain unmodeled.

At that zero-target `$6854a`, the next isolated static boundary compares `D2`
with immediate `$0120`; its conditional branch is encoded at `$6854e` and
targets `$68562`. Project Eon records only this byte-exact comparison/branch
pair and assigns neither values nor branch meanings to it.

At `$68562`, the branch-target's next exact static prefix reaches a conditional
branch at `$6856a`, whose encoded target is `$6857a`. This is raw control-flow
provenance only; Project Eon does not assign comparison or branch semantics.

The next target `$6857a` begins with another fixed-cell test and an encoded
conditional branch `$68580 → $68586`. This is recorded solely as raw static
control flow, without assigning meaning to the cell or either path.

At `$68586`, a fixed local register-preparation prefix reaches unknown `JSR
$7b26a` at `$68590`. This is a boundary only: no register values, target
effect, or continuation is modeled.

A separate resident entry at `$68d50` has its own literal load/test and
conditional branch `$68d58 → $68d62`. This is preserved as an independent
static gate; no cell, path, or runtime meaning is inferred.

For each of those callers, the static bytes immediately after the final
`JSR $7ba12` are now also verified, while carefully not treating them as a
runtime helper return. The first has return-address `0x69656` and begins with
absolute operands `0x7cc46` and `0x7b764`; the second has `0x69bba` with
`0x7cc72` and `0x7b764`. This records the exact caller-side continuation
boundary from the original ADF. Project Eon neither executes `$7ba12`, assumes
that it returns, reads either source, nor assigns a meaning to the data flow.

The first caller's longer static continuation is now hash-anchored as well:
86 bytes at `$69656` map to raw disk offset `0x17a56` and SHA-256
`5f42f9d3078d374f8b4a70fcc59c618abb9381d6b33ef25b3f2967876f0afe7b`.
Within that raw range, the original encodings at `$696a0` and `$696a6` are
`JSR $7b77e` and `JSR $7c802`. This is not a claim that `$7ba12` returns or
that either later call runs: it is a fail-closed static preservation anchor
for the next caller-side disassembly step.

The second caller has a distinct, shorter continuation and is anchored on its
own terms: 44 bytes at `$69bba` map to raw disk `0x17fba` with SHA-256
`5616f19900cb96ebc81edf90d0d17a9cde1644be07657801e243514b05e6ee23`.
Its final bytes encode `JSR $68d50` at `$69be0`. This remains a static raw-byte
fact only—not evidence that `$7ba12` returns, that `$68d50` runs, or that any
live data or helper effect can be reconstructed.

No proven direct caller into either staging entry exists in the original raw
resident range. A full raw scan of disk `0x16400..0x42400` finds zero literal
absolute `JSR`, zero literal absolute `JMP`, and zero PC-relative `BSR.W`
encodings that resolve to either `$69624` or `$69b88`. It additionally finds
zero fully local `MOVEA.L #entry,An` immediately followed by `JSR (An)` or
`JMP (An)` pairs. This is a precisely limited negative fact: it excludes only
those static forms. Wider register-indirect calls, other computed branches,
transformed first-stage paths, and runtime dispatch remain unproven, so
Project Eon does not claim that the staging entries are unreachable or
synthesize callers.

The setup target `$7b77e` is now separately fingerprinted at its linear
raw-media correspondence, disk offset `0x29b7e`. Its first 32 original bytes
are `04006e00c200044a00c240007a00c200105200c201005200c200014a00c20800`,
with SHA-256
`a695fd5ead90e07075256b1347220afde1a4439dd804cf1a9d445da4411cb52a`.
As with `$7ba12`, this fingerprint is chain-of-custody evidence, *not* an
executable decode: the preceding loader transform prevents the raw bytes from
proving the runtime routine's instruction boundary or semantics.

For callers that already own their six runtime values, the verified six
`(A4)+` to `(A5)+` transfers are available as the pure in-memory
`stage_millennium_amiga_resident_helper_pre_setup` operation. It returns the
three staged words and three staged bytes exactly as they stand immediately
before `JSR $7b77e`; it does not pretend that this is state before `$7ba12`,
because `$7b77e` may change any of those fields. No game-media read, unpack,
or write is involved.

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
`$0006..$0440` (exactly `$43b` iterations) using seed `$22225555`,
`ADD.B (A0)+,D1` / `ROL.L #8,D1` and
expected value `$7ae26af7`.  Only on that validation path does the recovered
code request the next raw interval: track 2, side 0, sectors 1 through 9 to
RAM `$70000`.  Its callback chain pushes `$70000` at `+$a74`, then after the
read pops that preserved value at `+$ac8` and copies 4,608 bytes to `$1e00`.
These are control-flow facts, not claims that the next interval is a title
screen; the latter remains unclassified. `ADD.B` updates only the low byte of
`D1`, so its carry never propagates into the upper 24 bits before the longword
rotate; Project Eon models that operand width explicitly.

That track-2 interval has SHA-256
`2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7`.
The SDL Atari ST launch path now creates a bounded session for this exact
Replicants Disk 1: it reads those two original raw ranges in memory, verifies
both hashes and the first-stage checksum, then records the second-stage
dispatcher. It stops before `Floprd`, callback/XBIOS behavior, state
selection, or a display is invented; other protected disks are detected but
never substituted for this profile.
At its loaded address `$70000`, it is executable code rather than a resource:
it configures supervisor stack `$7b000`, application stack `$2478`, then jumps
directly to `$1ec4`.  Because the preceding copy has now been proven to source
`$70000`, this target maps exactly to track-2 byte offset `+$c4`. That copied
entry stores a runtime word at `$1eaa`, indexes a vector table at `$1eac`,
calls the selected address, then forwards returned `D1`/`D2` values to raw
reader `$70030`.  The state word and selected handler are runtime-dependent:
Project Eon does not assign a title/game meaning, load a guessed sector, or
manufacture state. Its local raw-reader routine at `+$60` caps each XBIOS
request at nine sectors and maps linear tracks from `$50` onward to side 1.

The first six static table slots are `$1f1a`, `$1f2e`, `$1f50`, `$1f1a`,
`$1f1a`, and `$1f52`; they are all code addresses within the copied track-2
interval. The `$1f1a` vector returns raw-loader arguments: destination
`$13200`, byte count `$4800`, linear sector `$4`. `$1f2e` returns destination
`$b000`, byte count `$5e400`, linear sector `$4c` after an observed GEMDOS
call. These returned values flow to `$70030` through the proven dispatcher,
but state selection is still not emulated and no sector is read by this parser.
Slot 2's `$1f50` is a literal branch to `$1f1a`; slots 3 and 4 point directly
to `$1f1a`. Thus all three aliases share only the already-proven state-0 raw
arguments, without a new state interpretation.

Within the third distinct table body (`+$152`, hash
`eaee587850078d67a72dcf0da4b45e672c89a1352b040db580bedc0ba3b20e97`), the
28-byte interval at `+$170` is independently hash-locked to
`92cb6cf8a41c55df8459a9608c9626ff7cc831cceb69dd2b5531ac766b111552`.
Its literal-pointer and loop encoding loads `$57a00` and `$b006`, sets the
word counter to `$9392`, and contains `MOVE.B (A0)+,(A1)+` followed by a
`DBF` backedge from `+$184` to `+$182`. This is an instruction-layout record
only. It does not establish a table index, table call, loop execution, return,
destination contents, disk operation, or game-state meaning.

`build_deuteros_atari_state0_raw_load_plan` now models that one wholly static
dispatch result without selecting it at runtime: destination `$13200`, length
`$4800`, linear sector 4, represented as four original nine-sector reads from
Disk 1 offsets `+$4800`, `+$5a00`, `+$6c00`, and `+$7e00`. Their independent
SHA-256 values are respectively
`2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7`,
`c5cef5d02d47d09a758487e873ce1e86a9905b0e62241fc3bff7a8bf9114718a`,
`2515d3507aa37eaf5bbc0dd12f72a8dcc44712e4773a1e9e3f57517f8a21777c`, and
`510e1793d5d08ef18d5bc5039f5843aa403024c63abaad000078c61f65011e34`.
The concatenated raw span is hash-locked to
`88afae4bd5182d916183b01bf688ab524d739749e84a092eda1435e386b57b58`.
No consumer, format, state-selection source, or title/game semantics are
inferred for these bytes.

The state-0 span's first `0x1200` bytes are byte-identical to the already
validated track-2 loader (`248925…81d7`): at state-0 base `$13200` they contain
the same stack setup, absolute jump `$1ec4`, copied dispatcher at `+$c4`, and
raw-loader argument vector at `+$11a`. `parse_deuteros_atari_state0_duplicate_stage_prefix`
asserts this identity and the duplicated parser boundaries. It does not claim
that `$13200` or `$132c4` is entered: the proven original path instead copies
the track-2 bytes to `$1e00` and enters `$1ec4`; no loader-return continuation
selecting the duplicate has been recovered. The remaining state-0 chunks stay
unclassified raw data.

`build_deuteros_atari_state1_raw_load_plan` records the second vector's
equally static read as 84 original requests: 83 complete nine-sector side
spans followed by one seven-sector span. It starts at Disk 1 `+$55800`, loads
`$5e400` bytes to `$b000`, and has SHA-256
`0d5ccb3a337fcbd4d34d34b3ad24f20c3bb2edca7e7b734b8abb14f6c0a30f47`.
It has no unique GEM, DEGAS, or other image-resource header paired with a
proved consumer, dimensions, and palette. Header-like words inside this raw
protected-media interval are therefore not promoted to artwork. Project Eon
does not render the interval or choose state 1 merely because its physical
span can be calculated.

Within that unselected state-1 span, Disk 1 `+$9d800` (state-1 `+$48000`)
contains the exact branch encoding `60 00 09 c2` (`BRA.W` with literal
displacement `$09c2`). A byte-bounded printable block begins at Disk 1
`+$9d80a` and spans `$438` bytes; it has SHA-256
`8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e` and 18
printable runs. This is crack-era raw-media metadata, not recovered original
game presentation: Project Eon neither displays, translates, parses, nor
assigns a consumer or control-flow target to it.

The sixth vector (`$1f52`) makes two further static calls to `$70030`.
`build_deuteros_atari_state5_raw_load_plan` records its first as ten complete
nine-sector reads from Disk 1 `+$55800` (length `$b400`) to `$b000`, SHA-256
`9659b21315e5c0528020be0b41eb75d57428f41b3b632fabfebe16d34038d298`.
It then copies `$9393` bytes from RAM `$57a00` to `$b006` before the second
static raw plan: 68 complete nine-sector reads from Disk 1 `+$60c00` (length
`$4c800`) to `$16400`, SHA-256
`6b3e27702649ac201c4ecf92ad54f40656fd4d8633fadf5790014da34ce03ac6`.
These are preserved instruction/dataflow facts only; they do not authorize
Project Eon to select vector 5, perform its runtime callbacks, or infer title
or game semantics for the loaded bytes.

The two vector-5 reads are physically contiguous (`+$55800..+$60c00` then
`+$60c00..+$ad400`) and together form the first `$57c00` bytes of the separate
state-1 interval. `validate_deuteros_atari_state5_state1_prefix` compares the
two in-memory spans directly and locks their shared prefix to SHA-256
`ed55ad2a893a87af9f11d269faa6358420c47ed6beb1fee7a177e9beaed1e77c`, while
also retaining state 1's full independent hash. This is a media-geometry and
byte-identity fact only: overlapping physical reads do not prove vector
selection, load success, ownership, resource type, or any title/game meaning.

The live `DeuterosAtariBootstrapSession` retains these three static plans,
the vector-5 return profile, and the XBIOS callback-byte boundary after
validating the same original second stage. Thus the SDL launch request and
`--inspect` share one hash-validated provenance record; retaining it does not
read a selected state, invoke XBIOS, or materialize a title/game surface.

The vector's immediate static continuation is now bounded too. Track-2
`+$1a2` (Disk 1 `+$49a2`, copied RAM `$1fa2`) is exactly `60 00 ff 70`,
SHA-256 `4d11113ca2040c3c0d8e9fe7fc7ef2b65175cc580b8a4b81466908ae7c537896`.
Its `BRA.W` displacement is relative to the extension word, so it resolves to
track-2 `+$114` / copied RAM `$1f14`. The target is exactly six bytes,
`30 38 1e aa 4e 75`, SHA-256
`506215d03a2272be5f938a8926864075fc50a79d8c2fc23f22955d290fe0c98f`:
literal `MOVE.W $1eaa,D0; RTS`. This links the post-read branch to the original
dispatch-word return only. It does not select vector 5, give the word game
meaning, perform raw reads, or emulate XBIOS.

A separate copied-dispatcher route reaches an explicit supervisor-callback ABI
boundary. At track-2 `+$d2` it pushes literal callback `$1fa6`, then selector
`$0026`; those 10 bytes have SHA-256
`11b26d5900e614547617a9c95611515e8238184756a0a18c7ff18b1ec372657b` and
are followed by `TRAP #14`. The callback at track-2 `+$1a6` is 12 bytes,
SHA-256 `1f8bdb0e61454fef9acb0dc3abcf7bfed2621828937380b415ab85d4f57ef143`:
`MOVE.L (A7),D0; LEA $7b000,A7; MOVE.L D0,-(A7); RTS`. Project Eon records
these literal operations but does not emulate XBIOS, supply a callback frame,
or infer what frame or return path that ABI would establish.

The 20 original bytes immediately after that `TRAP #14`, at track-2 `+$de`,
are SHA-256 `ed326a1d22a28ce5646b242c947c5120cb0855d6d05080e35ce398d48d459f56`.
They read longword `$25f4`, compare it with literal `$00071100`, then use
`BEQ.S +8` to join at `+$f2`; the not-equal route contains `BSR.W +$714`
and `BSR.W +$1032`, resolving from their extension words to `+$800` and
`+$1122`. This is deliberately recorded as a post-service control gate, not
as an XBIOS or callback return value: the read is a distinct RAM location and
neither its provenance nor either subroutine's effect has been recovered.

The not-equal branch's two local BSR targets are now individually
hash-validated, but remain behind that runtime-dependent comparison. The first
target at track-2 `+$800` is 48 bytes, SHA-256
`bb662ff9f02861d2bc40c9d3d2ca97a662abc494ec20a4037807a81b22ca95a6`.
It loads `$00071100`, stores it at `$25f4`, prepares literal stack words
including selector `$0005`, then reaches `TRAP #14` at `+$824`. The following
`ADDA.L #12,A7` and `BRA.W +$08e8` (to `+$1116`, calculated from the branch
extension word) are recorded only as post-trap byte layout; Project Eon does
not claim that the trap returns, invoke it, assign a service meaning, or
execute the branch.

The second target at `+$1122` has a 22-byte prefix, SHA-256
`c74fb6b1e03cf6a123698e0356f3c9dbc45e637d9ce2a9479fef37eec6cbfd8c`.
It loads literal words `$7e00`, `$20000`, and `$9000` into the original
register setup, then its `BSR.W -$1106` resolves from its extension word to
the local range wrapper at `+$30`; that wrapper reaches the already bounded
XBIOS-facing raw-reader at `+$60`. Those values are only caller-side
machine-code facts. Project Eon does not select the comparison path, perform
the raw read, infer a disk result, or follow code after that raw-reader/XBIOS
boundary.

That shared range wrapper is now independently bounded as 48 original
track-2 bytes at `+$30`, SHA-256
`132ce2473e3764453bba01308e1f5044dc748bbea8b01975b67a259aa57cea7e`.
Its `DIVS.W #$1200,D7`, saved-register sequence, and `BSR.W +$24` statically
target the raw reader at `+$60`. Later literal branches target `+$2a`,
`+$5e`, and its save/call loop at `+$34`; `+$5e` itself branches to the
six-byte `MOVE.W $1e28,D7; RTS` helper at `+$2a`. This is a verified control
layout, not an interpretation of `$1e28`: Project Eon neither supplies a RAM
value, infers an XBIOS/raw-reader result, nor claims that any caller reaches
or returns through the wrapper.

The wrapper's direct `BSR.W` target is also separately bound as the complete
74-byte raw-reader call layout at track-2 `+$60..+$a9`, SHA-256
`a5bec9d04daa8ce600add594f6325030acd2ad8535910dee62497da90d572c90`.
Its literal setup begins with `MOVEQ #9,D2`, compares against `$1200`, and
has byte branches to `+$72` and `+$82`; the later fixed ABI encoding has
selector `$0008`, opcode `TRAP #14` at `+$9c`, literal cleanup `$14`, then
the bytes `MOVE.W D0,$1e28; RTS`. These are hash-validated machine-code and
branch-layout facts only. Project Eon neither executes the ABI call, asserts
that it returns, treats `$1e28` as a particular status/result, nor performs a
disk operation from this routine.

The copied dispatch table's three distinct direct target bodies are now bound
separately from the runtime-dependent `JSR (A1)`. Table slots 0, 1, and 5
refer respectively to track-2 `+$11a..+$12d` (20 bytes, SHA-256
`04c8eba86a6259f8d0b175fa18792cc64263863db51e76f9de839eec5c79ce0f`),
`+$12e..+$14f` (34 bytes, SHA-256
`0bc76b22089d008e4ce90d63216c75acbe0786b0a06127fbd66ef0dc252949ac`),
and `+$152..+$1a5` (84 bytes, SHA-256
`eaee587850078d67a72dcf0da4b45e672c89a1352b040db580bedc0ba3b20e97`).
The 2-byte `BRA.B -$38` at `+$150` is likewise bound to the first body's
start. These are direct table/linkage and byte-span facts only: Project Eon
does not provide a table index, claim that an indirect call reaches or returns
from any body, execute a raw read or ABI call, or assign a state/game meaning
to the literal code.

The 26-byte suffix after the third body's independently bounded transfer loop,
track-2 `+$18c..+$1a5`, is SHA-256
`45ac9d176b63fa93e16475543939d2f16b4e98cc839b44d2ce2ba9358e978083`.
It contains two literal immediate-adjust encodings (`$b400`), a literal
`MOVE.L #$4c800,D0`, then `BSR.W -$170` to the already bounded local range
wrapper at `+$30`, and `BRA.W -$90` to the separately bounded dispatch-word
return at `+$114`. Both 68000 word-branch targets are calculated from their
extension-word address. This connects static byte boundaries only:
Project Eon neither selects this vector, executes its transfer/call/branch,
assigns a meaning to the registers or literals, nor performs raw-media I/O.

The copied dispatcher also contains the first byte-proven state-selection
mechanics, separated around the existing XBIOS boundary. Before that boundary,
track-2 `+$c4..+$cf` is 12 bytes with SHA-256
`03cf620d981a775fd1adabe55deea940e08760e3e49c62cd0643c22b5aa08082`:
`MOVE.L $25fc,D0; MOVE.W D0,$1eaa; LEA $2478,A7`. This records only that the
low word loaded from RAM `$25fc` is written to the dispatch word at `$1eaa`.
It does not supply or identify the value at `$25fc`, or attach a game meaning
to either address. A separate 22-byte table-lookup layout at track-2
`+$f2..+$107` has SHA-256
`8e8551a51a7b989e6d2b7d1535819dea658a4e3e64562737755125c13c8f0d3c`:
it restores `A7`, loads table `$1eac`, reads `$1eaa`, shifts the word left by
two, loads `0(A1,D0.W)` into `A1`, and encodes `JSR (A1)`. That block lies
after the unmodelled supervisor-service boundary, so Project Eon does not
assert that it is reached, that its JSR executes, that the index is bounded,
or that any table vector is selected. The two independently hash-locked
layouts make the original input-and-lookup relationship inspectable without
inventing XBIOS, callback, or boot-state semantics.

The 18 literal bytes immediately after that indirect call, track-2
`+$108..+$119`, are independently hash-locked to
`e9ae4bd51bb06c6cb57ac7f26e81497995f7639f99a12e2a149194a39589e16c`.
They encode `MOVE.L D1,-(A7); ADDA.L #$1200,D4; MOVE.L D2,D7; BSR.W -$e2`
to the local range wrapper at `+$30`, then `MOVE.W $1eaa,D0; RTS`. This is a
post-indirect-call register and branch layout, not proof that any table
handler returns, that D1/D2 carry a particular game meaning, or that the
wrapper/raw reader is reached. Project Eon records the original `$1200`
increment and branch target without executing or supplying their inputs.

The byte-proven continuation after that wrapper boundary is retained without
asserting that the raw reader returns. At track-2 `+$1138`, the next 38 bytes
have SHA-256 `5b1480495df8defe3e1264dd083ec1c91134c01e56d3d94e060c583ee9b54a89`.
They place RAM `$20000` and literal selector `$0006` before `TRAP #14`, then
lay out `ADDA.L #6,A7`; a copy loop from `$20020` through the longword pointer
at `$25f4`; and `RTS`. The loop begins with `D7 = $1f3f`; its `DBF -4` targets
the preceding `MOVE.L (A0)+,(A1)+` at `+$1156`, giving a literal 8,000-longword
layout if reached. Neither selector `$6`, the pointer's provenance, service
return, copy outcome, nor the enclosing raw-reader return is inferred or
executed by Project Eon.

The other post-service layout is bounded independently. The first callee's
literal `BRA.W +$08e8` at track-2 `+$82c` resolves from its extension word to
track-2 `+$1116`. The 12 target bytes have SHA-256
`8778c08ae16a5f66009dda8d60a0dacba267cca4d29211a11fd2e30c40a7796b`:
`MOVE.L #$0000b000,D0; MOVE.L D0,$25f0; RTS`. This records a target which is
encoded after the selector-5 `TRAP #14` boundary; it does not assert that the
service returns, that the branch is taken, that the target executes, or that
either RAM address/value has a game meaning.

The supplied unlabelled Disk 2
(`5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193`)
branches to `$22` and carries the literal `KILLER_BOOT\0` marker.  Its
post-BPB setup copies ten longwords from boot offset `$f0` (the `LEA
$000e(PC)` at `$de` resolves from its extension word at `$e2`) to RAM `$8`,
then jumps to relocated address `$12`. The 40 copied bytes have SHA-256
`21a5d61e2289fe2f2141d3710fad31faf42e96f59c5fba768819380e8f595a8d`.
There, the relocated continuation clears eight longwords at a time beginning
at `$30`, advances by `$20`, and loops without a counter or return. Project
Eon records the copy and loop profile but does not execute it, wipe host
memory, or infer any resource/title semantics. The runtime reports these
boundaries rather than inventing a GEMDOS title path or unpacking media.

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

Command `$10` has a deliberately narrower meaning than its former
`transition_requested` event name suggested. The original dispatcher at
`$2162a` writes `$ffff` to `$210f4`; after `$21380` returns, the main loop
tests the byte at `$21856` and its nonzero branch at `$2185c` continues at
`$21892`. This is a verified main-stage request edge, not evidence for a
title, menu, or gameplay destination. Exhaustive control-flow walks of the
four bundle-0 and six bundle-1 channel streams (including their valid relative
jumps, calls/returns, and every `$11` branch displacement) contain no
reachable `$10`. The recovered opening input route reaches `$0f`, then `$05`
and `$00`, rather than `$10`. Consequently Project Eon does not synthesize a
channel state or expose a user input that would request this unproven later
continuation.

That continuation is separately preserved as raw static evidence. Main stage
`$21892..$218cb` maps to ADF `+$7092`, is 58 bytes, and hashes to
`120fba90e0b4fa9e96d8a6cf95fbac512d67d7daa42c3776ce0d3066b3f02ee9` on the
clean system disk. Its exact control-flow encodings are: zero branch `$21898
→ $218a2`; two local BSRs `$2189a → $2229c` and `$2189e → $224a2`; JSR
`$218a2 → $22a5a`; an equality loop `$218b6 → $218ae` after reads from
`$2079e`; JSR `$218b8 → $208ba`; bit-6 test at `$218be` with zero loop
`$218c6 → $218be`; and final branch `$218c8 → $217f6`.
`DeuterosAmigaChannelRequestContinuation` reports these byte-addressed facts
only. It does not select a condition, invoke a service, simulate the input
port, or assign names to cells and targets.

The first direct BSR target is independently retained as `$2229c..$2232f`,
ADF `+$7a9c`, 148 bytes, SHA-256
`d1a162af50f92b60d03b1da4ab186a547e46d145b0599cfbbeff7fb5af324ac1`.
It encodes a bit-5 test at `$222ac` with zero branch `$222b4 → $2232c`, a
literal counter `$000f` and DBRA `$222e0 → $222be`, two `-$c0(A6)` ABI calls
at `$222fc` and `$22312` using A6 from `$12fec`, a subtract-eight test at
`$2231c`, two `$21698` calls, and `RTS $2232a`. The complete range is
hash-locked by `DeuterosAmigaChannelRequestFirstCallee`; its custom-register
poll, state writes, vector calls, service calls, and return-dependent paths
are never performed or named by the runtime.

The second direct BSR target is separately hash-locked at `$224a2..$224cb`
(ADF `+$7ca2`): 42 bytes, SHA-256
`d4e9a1ee0065537a627cdd9ee8827f11d5fa28e0f860aacb21bbdc7e11784bd1`.
It encodes a longword transfer `$224e6 → $006c`, four literal word clears at
`$dff0a8/$dff0b8/$dff0c8/$dff0d8`, a literal `$000f` at `$dff096`, and RTS
`$224ca`. `DeuterosAmigaChannelRequestSecondCallee` retains only those raw
encodings; it neither reads `$224e6` nor applies low-memory/custom-register
writes, names hardware effects, or executes the return.

The following JSR target `$22a5a..$22b89` maps to ADF `+$825a`, is 304 bytes,
and hashes to `d5fdbdacd004d2cf377ea0dbaefb9d8b308ba23b568cfb3785456622bde49d19`.
It initializes literal zero at `$22a30`, starts with mask `$000f`, branches
over embedded bytes at `$22a6a` to `$22ab8`, and ends at RTS `$22b88`.
Its static descriptor facts include base `$22a6e`, stride `$000e`, source
record `$22aaa`, payload addend `$32a24`, flag cell `$22a6c`, and encoded flag
values `1/2/4/8`. `DeuterosAmigaChannelRequestFollowingService` reports those
facts only: no embedded descriptor, flag, or runtime-cell write is applied.
The adjacent entry `$22b8a` is deliberately outside this hash range because
it begins a separate caller-state-dependent path.

That adjacent entry `$22b8a..$22be9` maps to ADF `+$838a`, is 96 bytes, and
hashes to `10ed8be15c107dbb56ca98eb8d17ffd2bce3910dd169d67ba058447c9031b1ff`.
It tests `$22a30`, branches `$22b90 → $22b94` or returns at `$22b92`, then
encodes four conditional copies at `$22bb2/$22bc2/$22bd2/$22be2` and final
RTS `$22be8`. Its multiplication literal `$000e`, pointer cell `$22aa6`,
descriptor base `$22a6e`, field offset `$000a`, and stride `$000e` are raw
facts only. No caller register, pointer, branch, read, or write is supplied,
dereferenced, or executed by Project Eon.

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
adds it to the verified main-resource base `$32a24`, stores `$3355c` at state
offset `$0c`, and replaces the selector with `$fe`. The VM separately retains
the raw bundle-relative `$0b38` operand for its alternate-resource event,
without giving either value an invented gameplay name.

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

When—and only when—the live opening VM reaches that exact `$0f` handoff with
raw operand `$0b38`, `DeuterosAmigaTitleStageSession` now opens the same ADF
interval read-only. The session validates the existing title-stage opcode
profile, exposes only disk provenance (`+0x6e000`, length `0x6ca00`,
destination `$13000`, entry `$40426`) and hash-validates whole-stage SHA-256
`48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03` for the
verified clean system ADF; altered stage bytes fail closed. It never creates a title bitmap, inferred registers,
global work memory, or replacement menu: its next execution requirements still
cross unrecovered Exec and graphics-library vector boundaries.
The entry begins by preserving the bootstrap's `A1` value at `$206a0`, storing
the passed mode word at `$4040e`, and comparing its low byte with five. The
meaning of those mode values and the later gameplay dispatch remain unknown.
For mode five it copies the byte to `$3717e` and writes `$0101` to `$38092`;
every other path writes byte one to `$19d52`. The shared prefix is now
opcode-validated through `$40574`: it installs stack `$40b62`, loads Exec base
from `$4`, calls vectors `-$96` and `-$9c` with literal `D0=$7fff0`, calls the
original internal setup sequence, copies `$1f168` to `$1f974` and `$410d8`,
and prepares custom base `$dff000` with words `$7fff/$7fff/$c000/$87ff` at
offsets `$40/$42/$9a/$96`. It then distinguishes a mode-five call to `$36a8c`
from the normal call to `$1fb9a`. `DeuterosAmigaTitleStageProfile` records all
of these as initialization requirements only: Project Eon does not call Exec,
write the custom chip, allocate memory, or infer the calls' higher-level
effects. After that shared setup, the recovered
recurring loop starts at `$40574`, calls `$222c0` then `$23e4e`; a mode/input
change clears `$40410`, and the loop compares it with `$0000ea60` before the
original `$4069a` dispatch, subject to another original-state check. The strict
parser validates these operands directly and does not claim their gameplay
semantics.

The first common internal setup callee is independently hash-locked as a
caller-connected ABI boundary. `$1ed80..$1edf5` (ADF `+0x79d80`, 118 bytes,
SHA-256 `42c96aa502e36711ed274b9ddf4d2d1de53abfebb4ebdf88fa99346d2b03e30b`)
passes the literal NUL-terminated `graphics.library` at `$1ed02`, zero in D0,
and Exec base `$4` to vector `-$228(A6)`. A zero D0 result takes the original
self-loop at `$1edf6`; a nonzero result is stored at `$12fec`, the same raw
cell later supplied to graphics-library vectors. This does not establish the
vector ABI, the result value, or whether the original call returns.

The next direct setup call at `$1f172` enters local helper `$1eda6` and clears
word `$1f16c` after it returns. `$1eda6..$1edf5` (ADF `+0x79da6`, 80 bytes,
SHA-256 `d6b37bc6431a1fe9145ae9403a5165028ccfd856a6529d1752f824b166807223`)
copies the externally established longword at `$12ff4` to `$1f168` and
`$1f164`, copies exactly twenty original RGB4 words from `$1ed24` to
`$12ecc`, then adds `$7d00` to `$1f168` and stores the result at `$1f16e`.
The 40 source bytes hash to
`5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55`.
`DeuterosAmigaTitleGraphicsSetupProfile` reports this provenance only; it does
not resolve `$12ff4`, write title-stage memory, open a library, or turn those
palette words into an SDL title screen.

The immediately following caller-connected local callee is now bounded too.
`$1f182..$1f195` (ADF `+$7a182`, 20 bytes, SHA-256
`9b02afb723e201cacb93d18d87613dee0f56369707867989209a41d9430ec5f3`) loads
its destination only from the externally established `$1f168` cell, clears D1,
and uses original `DBRA D0,-4` with initial D0 `$1f3f`.  It therefore encodes
exactly `$1f40` sequential four-byte zero writes followed by RTS `$1f194`.
`DeuterosAmigaTitleDisplayClearProfile` preserves that loop and its source
hash, but does not resolve `$1f168`, allocate or clear host memory, name the
target a screen, or treat the preceding graphics-library call as having
returned.

The next contiguous routine `$1f196..$1f22f` (ADF `+$7a196`, 154 bytes,
SHA-256 `31fc346d9d2647001899a2e939482aa97bd8bc94221ae81384787997928bb42b`)
is separately hash-locked as `DeuterosAmigaTitleFourPassByteCombineProfile`.
It returns unless unsigned D2 is in `$0040..$0137` and D3 is in
`$0024..$006f`; accepted values have `$40` and `$24` subtracted, respectively.
The low nibble of D0 selects an eight-byte record from `$1f8ec`; the routine
uses a D3 stride of `$28`, then reads its base from the same unresolved
`$1f168` cell. It derives one bit and performs four identical byte-combine
passes separated by `$1f40`, finally restoring A0/A1 and returning at
`$1f22e`. This establishes exact local gates, table/pointer operands, and
four-pass shape only. Project Eon supplies no D0/D2/D3 values, does not read
the table or unresolved base, performs no byte writes, and does not call the
routine a renderer or infer a UI/control effect.

When that counter reaches the verified threshold, the call at `$405b6` enters
`$4069a`. This transition sets byte `$202c6`, saves and clears word `$202b8`,
then copies exactly sixteen RGB4 words from `$1ed24` to `$40678`. Each copied
word is ANDed with `$0eee` and shifted right once before being written. It then
uses the original graphics-library base from `$12fec` for vectors `-$c0` and
`-$1a4`; both vector calls and all operands are opcode-validated. Project Eon
records this as a timed title display transition, not as a guessed description
of a menu or gameplay screen.

The caller's immediate control-flow conditions are also retained. It enters
`$4069a` only when `$40410 >= $0000ea60` and word `$22d34` is not `$0011`.
The fully local gate at `$4059e` (ADF `+0x9b59e`, title-stage `+0x2d59e`) is
40 bytes with SHA-256
`47c56a2ad892d973cc967bca2a8c3b34338ffbdbff3b1b57ecef63cc6d8d7200`.
Project Eon evaluates its unsigned threshold and inhibit comparison directly
from hash-locked original bytes. Both skip routes converge at `$405c6`; only a
dispatched transition whose original call returns would reset `$40410`. The
evaluator does not mutate a counter or invoke `$4069a`.

Its prefix before the first graphics.library vector is independently executable
as a read-only in-memory preservation model. At `$4069a` (ADF `+0x9b69a`,
title-stage `+0x2d69a`), the exact 72 bytes hash to
`fda01edebbc2e99372cb22a858269202343f98d31bee1e473f751048666759ca`.
They set `$202c6` to one; read sixteen RGB4 words at `$1ed24`; apply
`AND.W #$0eee` then `LSR.W #1`; write the results at `$40678`; save and clear
`$202b8`; and prepare A0=`$12e12`, A1=`$40678`, D0=16 and A6 from `$12fec`
for `JSR -$c0(A6)`. The source is ADF `+0x79d24` / title-stage `+0xbd24`, 32
bytes SHA-256 `6920018538a18ca186ef36431678de4fc8f7bc68ac6b481e82086dbda54ff1e1`.
The recovered transformed sixteen-word result serializes to SHA-256
`e8f4bdf6b52bc849b626145464ccbc2701c6869cc97e62ef9dcfecb660a01aa8`.
Project Eon models no graphics vector, custom hardware, or title screen here:
callers must establish the original gate before this prefix can ever be part of
a live session.
Immediately after the routine returns, it writes long zero to `$40410` before
resuming the normal loop. This is a verified reset/gate relationship only:
Project Eon does not assign gameplay meaning to `$22d34` or synthesize either
state value at run time.

Both timer skips converge at `$405c6` (ADF `+0x9b5c6`), whose ten bytes hash
to `68ccbd8edf32800e43fe55c47356e162896b8500b01d2e9fd461191ba1760736` and
test byte `$1bf36` before branching to `$40638`. The clean original state word
at ADF `+0x76f36` is zero (SHA-256
`96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7`). The
zero branch's 60-byte response loop at `$40638` / ADF `+0x9b638` hashes to
`b47192ea229873ef1ae47f841d044393bfd3e7e1a7fc0ca92308a555c2eb84d0`.
It calls `$1f238`, treating its low-byte return from the locally recovered
but runtime-dependent helper as an explicit input; a
non-`$43` response returns to `$40574`. A `$43` response XORs `$1bf36` with
`$0101`, and the verified clean-state route emits literal `$0f00` writes to
`$dff180` before further helper responses. Project Eon records this as a
hash-locked machine-write trace only: it neither calls the helper nor writes
the custom chip, and it leaves the unrecovered nonzero-state route unmodeled.

The same `$4069a` routine has a bounded, verified two-call return phase. Its
first `-$1a4` setup supplies `$12e12`, `$1ffda`, and `$20056`, then stores the
third address in `$2008e`. On return it snapshots words `$1ffc8`, `$1ffce`,
and `$1ffd4`; while all three still compare equal, the original tight branch
loops at `$4071a`. The bytes do not identify a concurrent writer or permit
Project Eon to provide one. When a comparison differs, the second `-$1a4`
setup instead supplies `$12e12`, `$1ffda`, and `$1ffe6` and again stores its
third address in `$2008e`. The routine subsequently clears `$202c6`, invokes
`-$c0` with `$12e12`, `$1ed24`, and count 16, restores the saved `$202b8` word
from the stack, and returns at `$4077c`. The parser opcode-validates every
fact here; the addresses are preserved as raw machine-state boundaries rather
than named as a presumed menu, fade, or gameplay subsystem.

### Deuteros Amiga post-transition control loop

Immediately after the previous transition return at `$4077c`, original code
at `$4077e` clears word `$407e6`, then invokes `$3f7a8`, `$1f9a4`, `$1fe7a`,
`$3fbf8`, and `$1f238` in its original order while preserving that word on the
stack. Its feedback tail at `$407ba..$407e5` (ADF `+$9b7ba`, 44 bytes SHA-256
`b4212844a9f0fb4008caad00950e613b70581a5552cacabc253ea0966ed16df3`)
compares the helper's **low byte** against `$1b`, `$20`, `$2e`, and `$2c`.
`$1b` returns; `$20` and unmatched bytes loop without a local write; `$2e`
increments the low byte at `$407e6`; and `$2c` has the net low-byte effect
`-1`. Project Eon can evaluate that exact local feedback trace with explicitly
supplied helper response bytes, but neither calls the helper nor writes title
state. It does not assign names such as “selection”, “menu”, or “start game”
to the control word, helpers, or literal response values before the original
subroutines are independently recovered.

The title response helper is now recovered as far as its own original runtime
data boundary. `$1f230..$1f259` (ADF `+$7a230`, 42 bytes, SHA-256
`ed2794b7bb16f17ca9690b367c9465c75ff52838356bf6b46d9744cb16da1054`) first
loops on word `$1eed6` while it is zero. It then reads that word again; the
second zero branch reaches RTS `$1f258`. On the nonzero route, it reads one
byte from `$1eec0`, copies twenty subsequent bytes downward by one address
with `DBRA` initial value `$13`, decrements `$1eed6`, and returns. The direct
call sites include the known `$40638` and `$407b4` title response paths.
`DeuterosAmigaTitleResponseQueueProfile` hash-locks this exact wait/shift
shape. It does not supply the pending-word value, read or return a byte, model
concurrency, or make any writes to the original in-stage byte region.

Static backtracking identifies a producer for that same byte region but also
the next hard input boundary. During title setup, `$1ef74` places callback
address `$1f056` in the original descriptor at `$1ef48 + $12`, then reaches
Exec vector `-$1ce(A6)`; neither registration result nor callback invocation
is available in the supplied ADF. The candidate callback itself is wholly
present at `$1f056..$1f14f` (ADF `+$7a056`, 250 bytes, SHA-256
`ff4b055b2d5128465c891debcad00ff4e53cbf661de47b9ee3d6278f33d5e5f8`). Its
local byte-one path reads caller-owned A0 offsets `$8` and `$6`, rejects a
repeat, values at or above `$50`, and a pending count at or above `$14`, then
copies one byte from table `$1ee20` to `$1eec0 + [$1eed6]` and increments
`$1eed6`. That connects the static callback path to the recovered response
queue, but does not identify A0's ABI or values, table semantics, the
registration service, or a real input device. Project Eon consequently leaves
title input and response production unimplemented rather than inventing a
keyboard, mouse, or host event mapping.

`DeuterosAmigaTitleCallbackRegistrationProfile` now hash-locks both the
registration body (`$1ef74..$1f051`, ADF `+$79f74`, 222 bytes, SHA-256
`f571a8e5e48c29fe3d6f493e503e2a3a0b3328ac4cafb425808eff48804c4f27`) and
the callback. Direct instructions establish request `$1eefa + $1c = 9`,
`$1eefa + $28 = $1ef48`, descriptor `$1ef48 + $12 = $1f056`, and the call to
Exec base `$4`, vector `-$1ce`. The immediately following original instruction
at `$1f052` is `RTS`, so the local routine does not inspect a service result
before returning; this still does not identify the service ABI or establish
that it returns. The callback's producer route begins only for
the byte-one comparison at caller-owned `A0 + $4`; its direct bounds are word
`A0 + $6 < $50` and pending word `$1eed6 < $14`, after which it indexes
the exact 160-byte original interval `$1ee20..$1eebf` (SHA-256
`2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265`) and
writes `$1eec0 + [$1eed6]`. The `$50` bound plus the original conditional
`+$50` adjustment proves this is the complete locally addressable source
interval; it does not prove the meaning of any byte. These are an ABI-shaped
data path,
not proof of the request type, callback event names, input device, or any
runtime value supplied by Exec.

The same hash-locked callback has two additional locally bounded routes. Before
its branch checks it mirrors caller byte `A0+$4` into `$1ef2e`. The byte-two
route first rejects nonzero `$1ee16`; with word `A0+$6 = $00ff`, it copies
words `A0+$a` and `A0+$c` to `$1ee10` and `$1ee12` then reaches unresolved
service `$20118`. Otherwise it masks `A0+$6` with `$007f`, accepts only `$68`
or `$69`, derives a two-bit value from word `A0+$8`, and stores it at `$1ffd4`.
The byte-one producer stores word `A0+$8` at `$1ee0e`; its low three bits select
whether the bounded source index from `A0+$6` addresses the first `$50` bytes
of `$1ee20..$1eebf` or the second `$50`. Project Eon records these exact
operands and bounds only. It neither supplies an A0 frame nor calls `$20118`,
Exec, or a device, and does not give the bytes, words, or destinations a
presumed control/input meaning.

The third helper's concrete next boundary is also recovered. At `$1fe7a`, the
raw title image masks `D0` to `$0000ffff`, performs original unsigned divides
by `$0064` and `$000a` (with the two intervening original subroutine calls),
adds `$0030`, clears byte `$1fe54`, and then executes an absolute `JMP
$1fbe6`. Both `$1fe7a` and `$1fbe6` are range-checked against the same
title-stage load interval (`$13000` plus the profile's original length). This
proves the direct recovered route after the transition remains in title-stage
code; it is explicitly not evidence of a handoff to the separately loaded
main/game stage. Project Eon preserves the arithmetic, write, and destination
without inventing menu or gameplay labels.

The selector destination is now also bounded. `$1fbe6` tests signed byte
`$1f98c`: zero enters `$1fc22` (which immediately tests `$1f98e`), while the
positive `BPL.W` target is `$1fc9c`: this is the sibling route's `tst.b
$1f98e`, immediately after the prior route's `RTS`; it does not target the
middle of an instruction or return directly. Its clear/set variants begin at
`$1fca6`/`$1fd7a`, use the same `$1f99c` pattern-table and `$1f974` destination
pointer cells, combine bytes across eight rows by four planes, and increment
`$1f974`. Clear uses literal `$28`/`$1f40` strides; set loads stride cells
`$1f994`/`$1f998`. This is a concrete original byte-combine/pointer effect,
not an inferred resource or UI label.
For zero, a clear `$1f98e` enters `$1fc2c`; a set value enters `$1fd0a`. Both
preserve registers and traverse eight rows by four planes using pattern-table
pointer cell `$1f99c` and destination-pointer cell `$1f974`. The clear route
combines source cells `$1f970` and `$1f96c`, uses literal row/plane advances
`$28`/`$1f40`, then advances `$1f974` by the long in `$1f9a0`. The set route
uses long stride cells `$1f994`/`$1f998`, source cell `$1f96c`, and increments
`$1f974`. These are raw byte-combine and pointer effects, not names for
resources or UI. The negative fall-through calls `$1fc24`. On that negative
path, the original preserves D0/D5, suppresses a service call if D0 is `$0020`,
otherwise supplies literal D0/D1 values `$0013`/`$000c` to `$3fbf8`, then runs
a `$00004e20` decrement loop before restoring registers and returning. These
are opcode-validated control-flow, call, and timing facts from the raw title
stage; Project Eon does not name the state bytes, service, or output, and does
not synthesize their unknown data.

The non-suppressed call's ABI boundary is now bounded too: it pushes A0 and A1
after saving D0/D5, calls `$3fbf8`, then pops A1/A0 before the delay. Both the
suppressed and service paths converge on `move.l (a7)+,D5`, `move.l
(a7)+,D0`, `RTS` at `$1fc20`. Thus this routine returns its incoming D0/D5;
the service's internal output and purpose remain intentionally unknown.

### Deuteros Amiga title-stage exits

The title entry's locally verified low-byte-five arm is also retained without
assigning it a gameplay meaning. At `$40438` (ADF `+$9b438`) its 16 bytes hash
to `c4f5b0fa571dc0c932e9bb3df9f48e4c4336840d49ae2368e69fffa8c05c87a7`.
Only when an explicitly supplied incoming word has low byte `$05` does it
record the low-byte store to `$3717e`, literal word `$0101` at `$38092`, and
branch to the pre-existing hard Exec boundary `$40450`. No controller value,
write to original media, or Exec vector call is performed.

The raw title-stage has three independently validated tails that leave its
loaded interval. They begin at `$37f56`, `$38038`, and `$38068`. Their prior
render/control work is intentionally not named, but each tail copies the
incoming controller pointer from `$206a0` to bootstrap cell `$12ff8`, writes
respectively long profile values `2`, `4`, or `3` to `$12ffc`, and performs
`JMP $12800`.

The first of those exits has one additional bounded local transfer before its
already verified profile-two tail. At `$37f56` (ADF `+$92f56`), the exact
40-byte prefix hashes to
`51b8d6875ea6d0c35557c358d4fe22e4cac6cff79ead9df604d213cab1adfe1c`.
Conditional on its two original calls to `$3880a` and `$204fa` returning, it
copies exactly `$9392` bytes from loaded title-stage address `$13006` to the
original runtime address `$66000`, then reaches the still-unexecuted BSR at
`$37f7a`. The source maps directly to ADF `+$6e006`, remains inside the same
title-stage interval through `$1c397`, and hashes to
`2951d0ae6dd01f84c1fb9b6cbb766c15378af1abb9a91fa5ded748d70b3e90eb`.
Project Eon exposes only a read-only copy trace and those genuine source bytes:
it does not call either helper or the BSR, write `$66000`, infer a title choice,
or create replacement data.

The BSR's return continuation is a separate explicit ABI boundary. Only when a
caller supplies that `$37f7a` returned does Project Eon inspect its following
28-byte tail at `$37f7e` (ADF `+$92f7e`), SHA-256
`bacc75771f84068878d031ad87b0708c08911e85b605436c29d8d4c1faa2884c`.
The original instructions name controller source `$206a0`, bootstrap cell
`$12ff8`, profile-two cell `$12ffc`, and `JMP $12800`. The evaluator exposes
only those instruction destinations and the literal profile: it does not read
or materialize the controller longword and does not execute the jump.

The BSR target itself is retained as static provenance, without treating its
calls as executable behavior. `$37f9a..$38031` (ADF `+$92f9a`, 152 bytes,
SHA-256 `b076611efd33354e311dc9f64b57454e31cddd69c0749a05034f0d828a5b36c1`)
loads literal D1/D7/D0 values `$12800`/`$2c00`/`$0600`, then calls `$208c0`.
It writes word `$000a` and long `$1ef48` at offsets `$1c`/`$28` from `$1eefa`,
and makes raw Exec-vector calls with A1 equal to `$1eefa`, `$1eefa`, `$1eed8`,
`$2063e`, and `$20676` at vectors `-$1ce`, `-$1c2`, `-$168`, `-$1c2`, and
`-$168`. A longword comparison `$20698` versus `$2069c` selects the unequal
path at `$38014`; all paths return at `$38030`. The parser records only these
instruction operands and branch addresses. It neither calls `$208c0` or Exec,
reads the compared state, assigns a purpose to any work area, nor asserts that
the BSR returns.

The analogous second exit is independently bounded after its four preceding
calls. Only when a caller explicitly reports returns from `$3880a`, `$204fa`,
`$37efa`, and `$37f9a` does Project Eon inspect the following 28 bytes at
`$38046` (ADF `+$93046`), SHA-256
`cf80103d5a580dc1e59f1090169c769a66a5d34c1112f14456e00713f1d078da`.
Those instructions name controller source `$206a0`, bootstrap cell `$12ff8`,
literal profile-four value at `$12ffc`, and `JMP $12800`. The evaluator does
not invoke any predecessor, read controller data, manufacture a return, or
execute the jump.

The third exit has its own 48-byte predecessor chain at `$38062..$38091`
(ADF `+$93062`, SHA-256
`e3b5d4b2448f33178f748a9a235c270c45e2c83e2b0ba9f4bd8e41ab3ee2fb80`). Its
four calls are `$3880a`, `$204fa`, `$37efa`, and `$37f9a`; their return is
again an explicit caller-provided boundary. Only then does the evaluator
inspect the distinct 28-byte tail `$38076..$38091` (ADF `+$93076`, SHA-256
`25c2f6bf241a863d0b16359553dfae9a82953dfbc25035db71634a0b369df217`). It
records the same raw controller source/destination operands, literal profile
three at `$12ffc`, and `JMP $12800`, without executing calls, reading the
controller longword, or taking the jump.

`$12800` resets the original stack/Exec state and jumps to bootstrap dispatcher
`$12a4e`. The original six-entry table at `$12a36` resolves profiles 3 and 4
directly to `$12b1c`; profile 2 selects `$12b44`, whose sole instruction is
`BRA.B $12b1c`. `$12b1c` is the already verified profile-zero loader: it
returns destination `$20000`, length `$4200`, and track `$4`, whose raw stage
entry is `$21734`. Therefore these three original title exits demonstrably
re-enter the raw main-stage load path. They are not yet interpreted as named
choices, game modes, or completed gameplay transitions. Project Eon records
only this opcode-validated profile and load-chain evidence; it performs no
media extraction, generated state, or guessed post-handoff simulation.

The full six-entry bootstrap table is also preserved: `$12b1c`, `$12b30`,
`$12b44`, `$12b1c`, `$12b1c`, `$12b46`. The recovered title exits do not
select entry five. Its first instruction is nevertheless independently
verified as `BSR.W $12932` at `$12b46`; no return value or continuation is
assumed, so this remains a hard bootstrap boundary rather than a fabricated
title or loading path.

The direct callee has one further independent, straight-line boundary before
its own unknown library call. `$12932` loads the controller pointer from
`$12822`, writes long `$00000001` at `+$24`, word `$0009` at `+$1c`, and byte
zero at `+$1e`, then loads `A6` from `$0004` and calls vector `-$1c8(A6)`.
Project Eon validates these raw writes and stops at that call; it neither names
the fields nor assumes the vector returns.

### Deuteros Amiga re-entered main stage

After any of those title exits, the original raw track is loaded again at
`$20000` and enters `$21734`. The first instructions save incoming A1 and D0
verbatim to longword `$20976` and word `$21704`, install stack `$22296`, then
request the literal memory ceiling `$7fff0`, then call raw addresses `$20068`
and `$2013a`.

The main-stage parser opcode-validates that straight-line path and the first
recurring loop at `$217f6`. The loop calls `$22a5a`, clears words `$21720` and
`$2171e`, sets `$210f2` to one, calls `$21276` then `$21380`, and probes bit
10 at `$dff016` plus bit 6 at `$bfe001`. These are deliberately raw addresses, values, and bit tests:
their gameplay/UI semantics are not claimed. The parser reads only the
already supplied ADF in memory, rejects mismatching bytes, and neither
unpacks nor writes game data.

The next input-originated branch is also verified at `$21982`: it reads word
`$21704`, compares it unsigned with two, and writes one back to `$21704` when
the value is below two. Both the less-than and equal-to-two paths enter
`$218cc`; values greater than two instead branch to `$2181c`, the scheduler
call already present in the loop. This is retained solely as an opcode-level
clamp and branch map. `$21704`, `$218cc`, and `$2181c` are not assigned guessed
mode, screen, or gameplay meanings.

The greater-than-two route is now bounded further without executing guessed
main-game logic. It branches to `$2181c`, whose first instruction calls the
original scheduler at `$21380`. That routine starts at state base `$210f8`,
loads its channel count from `$21248`, and advances 24 bytes per slot. It
tests the active program longword at `+16` and a selector word at `+6`; the
raw comparisons accept selectors `$03`, `$05`, `$06`, and `$14`. The original
uses the word at `+8` in these paths before resuming its opcode dispatcher.
After the channel walk, it probes bit 5 at `$dff01f` and conditionally calls
`$21698`. These are opcode-validated scheduling and timing/service facts only:
they do not name the channel fields, emulate a hardware interrupt, or invent
any gameplay state. The bounded native VM already uses this original 24-byte
layout for its verified opening commands; parsing this continuation keeps the
post-input route anchored to the same supplied ADF bytes.

The full immediate control-flow consequences are now opcode-validated as well.
Both paths at or below two reach `$218cc`, whose shared tail reads `$21704`,
increments the register value, but does not write that increment back. Result
two branches to `$21a4c`; it writes longword one to `$219f4`, then the common
return tail copies incoming controller `$20976` to `$12ff8` and `$219f4` to
`$12ffc` before `RTS`. Thus an initial value below two clamps to one, produces
result two, and requests bootstrap profile one without asserting a menu or
gameplay interpretation. An initial value equal to two instead produces result
three and branches to `$219f8`. That path writes five to the same `$219f4`
cell, calls `$20b42`, and compares D0 with literal `$4452f018`: equality jumps
to the shared `$21a56` return tail, while non-equality continues its original
polling loop. Values above two bypass `$218cc` entirely and resume `$2181c`
(the scheduler path), leaving this post-service state unchanged. Project Eon
reports these original branch facts only; it does not synthesize a main screen,
interpret the sentinel, or mutate supplied media.

The re-entered stage's raw resource loader at `$21932` is independently
validated. It shifts its incoming D0 index by two, reads the selected longword
from `$21708`, and uses that as a physical ADF offset. It clears `$2ad24`,
transfers exactly four bytes from that offset there, restores the original
offset, and uses the resulting big-endian longword as a second transfer length
to `$32a24` from that same offset. The shared transfer routine chunks requests
at `$1600`; after the transfer it tests bit 10 at `$dff016` and retries from
`$2196e` while clear. This is a verified original resource-to-memory effect,
not a claim about the resource's format, any destination cell's role, or a
request to unpack/copy media. Project Eon exposes the fixed data-flow facts,
and has a read-only in-memory model for a successful nonzero pass: it retains
the exact source table index, source ADF offset, probe/payload destinations,
length word, and original bytes (including that length word). It rejects a
length outside the physical ADF and returns no payload for the original zero
retry condition. It never writes, extracts, or unpacks game data.

The first direct consumer of that transferred memory is now bounded too.
Routine `$2016a` saves A4, loads it with the exact transfer destination
`$32a24`, combines the word at `$20168` with the longword at `$2079e`, masks
the low word with `$3ffe`, reads one big-endian word at that A4-relative
offset, adds `$000e`, adds the result back to `$20168`, restores A4, and
returns. The main-stage command dispatcher reaches this routine from two
separate validated compare/call arms: command words `$000a` at `$2159c` and
`$0011` at `$2163a`. This proves a resource-to-control data path after the
loader without naming the resource, treating it as an extracted file, or
assigning gameplay semantics to the state cells.

The consumer's layout and index behavior are now directly observable too. It
is not a separate table after the loader's four-byte probe: the second transfer
begins at the same ADF offset, so `$32a24+0` remains the resource's big-endian
length longword. `$2016a` forms `(seed + low_word(counter)) & $3ffe`; this is
an even byte offset in the first 16 KiB of the exact raw transfer, from which
it reads one big-endian word. It adds `$000e` modulo 16 bits and adds that
result to the seed. For genuine bundle 0 with zero seed/counter, the observed
word at `$0000` is `$0002`, the intermediate result is `$0010`, and the
resulting seed is `$0010`; with that seed and counter `$00000004`, it reads
`$0a78` at `$0014` and produces `$0a96`. Bundle 1 has the same first observed
`$0002` word. Project Eon exposes this only in verification output and an
in-memory model that rejects malformed length, destination, or range facts.
It neither labels these values nor changes source media or save state.

The next consumer is the already opcode-validated channel interpreter at
`$214aa`, not a resource decoder or renderer. Its `$000a` arm calls `$2016a`,
ANDs the returned word with the following original stream word, stores the
masked result as state `+8`, writes selector `$0003` at state `+6`, and
returns to the scheduler. Its `$0011` arm calls the same routine, masks the
result to four bits, reduces it by the first following stream word when that
value is not smaller, multiplies by the second following stream word, and
adds the product to the current command-stream address. Thus the proven
effects are a timed control yield and a bounded original command-stream
choice. Neither arm uses the word-plus-14 value as a file/resource selector,
bitmap index, palette index, or a rendering address. The live opening now
uses the completed `$21932` transfer held in memory as its `$2016a` source;
it verifies the transfer's leading length and destination through the same
strict sampler before every word read. This replaces no game data, unpacks
nothing, and preserves the previous raw-ADF reader solely for independent
parser tests.

The first renderer use of the input path's `$0f` result is bounded at
`$21448`. The render pass walks the same 24-byte channel states, ignores
selector `$ff`, then compares selector `$fe` before its ordinary bitmap route.
For `$fe` it loads A4 from state offset `+$0c`—the exact `$32a24+$0b38`
pointer installed by `$21610` for the first accepted opening input—and calls
`$20580`; non-`$fe` live selectors call the indexed-bitmap compositor at
`$20c8c`. `$20580` is a real byte-stream interpreter, but it writes through
global original video pointers (`$20510`, `$20508`, `$2050c` and related
state) rather than the channel's X/Y fields. Project Eon records and tests
this exact A4/call boundary, then deliberately leaves the `$fe` pixel effect
unrendered until the setup and all stream control classes are fully recovered.
It does not reinterpret those bytes as an indexed sprite, create a synthetic
frame, or write/unpack media.

The renderer-only evidence now includes the local main-stage video link at
`$21768` / ADF `+0x6f68`: its 20 bytes hash to
`66c68dea1896b857f9cda825ef5511b34254ceed8db8a1b1481c3e3477514194` and
copy `$20128` to `$20510` and `$20c20`. It follows external initialization
calls, whose ABI is still not emulated; the subsequent local copy is recorded
only as raw provenance. The original `$2069c` position helper is also locked
as 48 bytes at ADF `+0x5e9c`, SHA-256
`b167cbda0c4e419b50e8dea16172b80a3db31e52385fe606efd146a54ce4d772`.
It bounds the `$20128`-relative `$20510` calculation used by the observed
`$20580` stream without materializing the unknown runtime value of `$20128`.
For the verified selectors one/zero, the exact original masks at `$20490` and
`$20488` are required. Altering the raw video-link bytes fails closed before
any host-frame pixel is written. The accepted `$20580` glyph writes update the
same persistent four-plane compositor surface as the other original Amiga
channels; a later channel pass retains those pixels. This is restricted to the
one hash-locked stream and does not infer the unresolved external init ABI.

The same first accepted input channel has a bounded post-renderer tail in the
real bundle: immediately after `$0f,$00000b38` are `$05,$0008,$0044,$00`.
The scheduler's selector-five arm at `$213be` resumes only if the low word of
`$22a20 - 1` equals state word `+8` (`$0008`) and state word `+10` (`$0044`)
is strictly below `$22a16`. It then dispatches `$00`: `$214ae` clears selector
`+6` only. On the following scheduler visit, selector zero reaches `$2142a`
and clears the program longword `+16`. Project Eon models those two original
in-memory effects across two scheduler calls; it neither manufactures an
audio clock nor substitutes a completion screen.

The first fully observed `$20580` stream is now executed as a strict in-memory
trace. The opening input path supplies `$32a24+$0b38`; its original bytes are
`$16,$0f,$30,$10,$01,$11,$00`, then the eleven bit-7-clear glyph bytes
`please wait`, followed by `$00`. `$20580` therefore calls `$2069c`, which
sets `$20510` to `$20128 + $1e0f`; `$10` and `$11` select `$20488 + 8` and
`$20488` for `$20508` and `$2050c`; each glyph calls `$206e6`. The latter
writes through original global font/video pointers. This first stream now has
a fully verified pixel path: the raw main stage initializes `$20538` to its
embedded `$201b0` 8-bytes-per-glyph font and `$2053c` to one byte of horizontal
advance. `$206e6` reads each glyph row, then writes each of the four Amiga
planes using `(~glyph & secondary-mask) | (glyph & primary-mask)`. The exact
selector tables are raw bytes at `$20488`; for the observed selectors 1/0,
the result is plane 0 for set glyph bits and no plane bits for clear glyph
bits. The verified `$1e0f` display offset is byte 15 of scanline 192, so the eleven
8x8 original glyphs fit through scanline 199. Project Eon applies precisely
those writes to its existing in-memory four-plane-equivalent frame; it does
not create a font, artwork, or source-media output. Any other command class,
global layout, or out-of-plane write is rejected as a preservation boundary.

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
direct near-call there targets `$1725`. Its first 13 original bytes end in
literal `JLE +$01` at `$172f` to `$1732`; the sequential byte is `RET` at
`$1731`. This is a caller/callee byte boundary only: the comparison condition,
callee return, and any resource effect remain unmodelled.
The `$1732` JLE target's first direct near-call is at `$173d` to `$1390`,
anchored by its first 16 raw bytes; no call or data effect is inferred.
The `$1390` callee is anchored through its first direct near-call at `$13bb`
to `$013c`; this is a static address edge only.
The `$013c` target is anchored for 16 bytes through its terminal `RET` at
`$014b`; no operation or return effect is inferred.
transition routine at `$1941` starts with `CX=$25` and `DX=$0170`, so the
verified title transition contains 37 steps with that original stride.

The title/resource chain is now byte-locked through that transition. Main
`$1c14..$1c1f` (file `+$1b14`, SHA-256
`056142489c8d70d88640c5dd0dea385fd4a3d561efe95cb57625773840ca1327`) sets
index zero, calls `$1725`, then `$1004` and `$1941`. `$1725..$1767` (67 bytes,
SHA-256 `37ec3f970f81219815bf524c6db54a12a0961d7e7452c3e2527fe422401339d9`)
reaches the 333-byte codec-2 reader `$1390..$14dc` (SHA-256
`c4e0a56ba09831d80112d3f630e070886a66b70a62251ae90e89f98281a94b52`) for
P00, then private `INT $91` wrapper `$0122`. The 39-byte `$1941..$1967`
transition (SHA-256
`f22c9595e6b1c590877b354721e6102d8107c5d6c7336b2d3491cfcaf3f8a627`)
advances original runtime offsets by 368 and requests P01 through P25. Those
37 records occupy `TITLE.LIB` `$2941..$4813` (7,890 bytes, SHA-256
`f0ecbfd374b1c6122b407b29a6fe4a872a45a0a21e9ef6584e74829e06b5514d`), but
write only to runtime buffers. Project Eon decodes them as a read-only patch
sequence: P01 through P25 are codec-2 16x23 (368-index) records, matching the
`$0170` stride. P01's decoded index hash is
`330db310a838487f4afea0011c1ba5f381e4ed7ad97d95e4745e7be2d2d8aaa1` and
P25's is `d7e44c796aed167010cdef9ab7ccef38b3b260854b51b2fba818972f30dd35dd`.
The static mode-2 path bounds each post-stream XLAT range (P02: `00 cc 00`),
but does not establish a selected display mode. Segment setup, private-driver
effects, composition, color handling, cadence and host-visible transfer remain
unrecovered; Project Eon does not draw an inferred transition or claim extra
title frames.

The main title loop polls DOS `INT 21h`, `AH=$06`, `DL=$ff` through helper
`$0d0a`; at `$1c28` it branches out of the loop only after the returned `AL`
is nonzero. Cleanup writes zero to the process status byte at `$1a0e`, and the
common exit stub at `$1a12` executes `INT 21h/AH=$4c`. Thus the verified title
program itself does not execute the game binary: it exits with status zero.

The complete caller range `$1c28..$1c69` (file `+$1b28`, 66 bytes, SHA-256
`d08916b10f92fe78e643a3335680c341f2347c361cded2419954422c0c37e6dd`) makes
the input boundary precise. It only performs `AND AL,AL` and sends every
nonzero result to `$1c54`; it neither stores nor compares a scan code, so no
key name or control binding is proven. The static exit chain reaches `$1968`
and embedded bytes at `$1884` spelling `    LOADING    2`. `$1968` loads AX=5
and calls `$1931`; that local loop runs five times, loading AX=`$0013`, calling
the private `INT $91` wrapper `$0122`, then helper `$1917`. This establishes a
caller-connected private-driver boundary only: ABI, helper effect, destination,
composition and BIOS-visible output remain unknown. No post-key loading frame,
transition, resource effect, process exit, launcher return, or game startup is
executed or drawn by Project Eon.

`MillenniumDosTitleExitClosure` now preserves the narrower local tail as its
own hash-locked profile. `$1c54..$1c69` (file `+$1b54`, 22 bytes, SHA-256
`d0981a03e0f8fdc9449e080668b7808952a48d0d3de4beb3a528ba5fc0f05951`) first
calls `$1968`, then `$12c0`. Only if both native calls return do the following
original instructions clear `$1a0e`, restore SP from `$1aa0`, and call `$0916`;
the direct `JMP` at `$1c67` targets `$1a0f`. That 11-byte tail (file
`+$190f`, SHA-256
`b8160617c570a0dafcfea4e57187b7dd9182ced8da1153f6f77c63d5e7fe6a88`) calls
`$112e` before the existing `INT 21h/AH=$4c` bytes at `$1a12`. The local-call
returns, the process termination effect, and whether the program ever returns
to `MILL.COM` remain explicit boundaries; the runtime does not perform any of
them.

The local helper `$1917` is now independently byte-locked. Each of its five
calls starts a fixed 15-iteration selector loop. Selector `$18f9` adds the
unknown word at `$1181` to accumulator `$18f7`, masks the result with `$03ff`,
loads one original byte at that resulting offset, and reduces values at or
above `$24` by `$18`. Its caller then adds one and calls resource loader
`$1712`. This establishes only a bounded potential resource index path; the
state word, accumulator, loaded byte, destination buffers and all resulting
rendering remain unknown.

`$1712` is a local index-to-buffer setup routine, not a recovered renderer. It
zero-extends the selected byte, multiplies it by `$0170`, and stores the word
at `$1341`. It then consumes one 4-byte entry from the fixed 15-entry original
table `$1768..$17a3` (SHA-256
`9c40c1fa63248237383703aa0aaf6659630e8d4fb48bc6ddd1c633ed4d26846f`), copies
its two words to local cells, and calls private INT 91 wrapper `$0122` with
AX=`$0006`. This proves original offset setup and table identity only—not a
resource name, visual coordinates, blit ABI, or visible pixel result.

The caller fixes only part of the AX=`$0006` ES:BX record. `$1712` sets
ES=CS and BX=`$1349`; `$174a` writes the two words of the current `$1768`
table entry to `$134f` (record `+6`) and `$1351` (record `+8`). The related
local segment cell is `$134b` (record `+2`), while codec-2 setup stores decoded
height and width at `$1357`/`$1359` (record `+$0e`/`+$10`). The remaining
record words, pointer provenance, interpretation of the table words, and all
driver writes are deliberately unrecovered.

The complete direct-reference audit separates this record from nearby buffer
state. `$135e` loads one of the original far pointers at `$010c`/`$0110` and
writes its offset/segment to `$1341/$1343`; `$1712` later replaces only the
offset word. `$135e` also writes CS to `$134b`. In contrast, no direct local
writer of `$1349` occurs in the hash-identified `TITLES.EXE`.

Both independently hash-identified AX=`$0006` drivers make that unknown
word an explicit ABI boundary: EGA640 executes `LDS DI, ES:[BX]` at `$08d9`;
MCGA executes `LDS SI, ES:[BX]` at `$072e`. Thus record `+$00` is a far-source
pointer's offset word and record `+$02` is its segment word. This proves the
driver's direct input field, not who owns the offset, the pointed-to byte
format, or any drawing result. Project Eon records neither a reconstructed
pointer nor a host-side transfer for it.

The next original instructions give a stricter but still format-neutral
boundary on that pointer. EGA640 reads words at source `+$04` (`$08dc`),
`+$02` (`$08df`), and `+$00` (`$08eb`). MCGA reads the word at source `+$02`
(`$0731`) and executes `LDS SI,[SI+$04]` at `$0736`, making source `+$04` a
second far-pointer operand on that path. These verified operand accesses do
not establish that the two driver paths share a header format, identify any
pointer owner, or authorize a host-side copy, decode, or draw.

The supplied driver images add one ABI-independent local fact for that exact
function number. In both hash-identified `EGA640.BIN` (dispatch target `$0d37`)
and `MCGA.BIN` (target `$0905`), AX=`$0013` loads DX=`$03da`, repeatedly reads
the VGA status port until bit `$08` is clear, then repeatedly reads until bit
`$08` is set, and returns. Project Eon records this read-only vertical-retrace
wait but does not perform host port I/O or infer that it produces a frame.

The same supplied driver dispatch tables connect `$1712`'s AX=`$0006` request
to EGA640 `$08a6` and MCGA `$0705`. Both begin by clamping the ES:BX record's
word at `+$10` against `320 - word[+$08]`, then return if the resulting height
is non-positive. The ES:BX record, source/destination pointers, branch
outcomes and all writes remain unmodelled, so this is a verified entry-side
clipping boundary, not a host blit.

The accompanying clean `MILL.COM` (1,445 bytes, SHA-256
`4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e`)
has a caller-side sequence at loaded `$023d`: it loads `DX=$068f` (the
NUL-terminated `TITLES.EXE` string at file `$58f`) and near-calls `$031c`
from `$0240`; after the explicit `AND AL,AL` / conditional branch bytes, it
loads `DX=$069a` (the adjacent `2200ad.exe` string at file `$59a`) and makes
the same direct near call from `$024c`. This is the exact DOS title-to-game
hand-off used by the native parser. It establishes only literal dataflow and
control edges: Project Eon does not assign a DOS/EXEC meaning to `$031c`, the
post-call `AL` tests, or either callee return.

The directly called local bytes at `$031c..$034d` are separately anchored by
the parser. Their first local branch is the literal `JC +$05` at `$0345`:
the taken target begins at `$034c`, while the sequential bytes are
`B4 4D CD 21 C3` and end in `RET` at `$034b`. The branch target's next 14
bytes are `BA 70 03 89 D2 B4 09 CD 21 B8 0A 4C CD 21`; the first following
byte, at `$035a`, is the static text-data boundary. This is a static byte/control
fact only; in particular, Project Eon does not interpret the interrupt bytes,
the carry condition, or the return as DOS behavior.

Immediately before the title-string setup, `$0210` starts with a direct near
call to `$0511`, followed by `AND AX,AX` and literal `JE +$03` at `$0215` to
`$021a`; the unbranched bytes `$0217..$0219` are `05 02 00`. The rejoined raw
sequence reaches another direct near-call at `$0231` to `$02cf`, then the
bytes through `$023c` precede `DX=$068f` at `$023d`. These are preserved as
caller-side static dataflow and control edges only. Neither call, the `AX`
condition, nor the interrupt bytes in this range is given an execution or DOS
meaning.

The `$0231` direct call's local target is `$02cf`. Its hash-anchored first 19
bytes are `B8 00 3D CD 21 73 0C 0E 1F 8B 16 D5 05 B4 09 CD 21 EB 87`.
They contain the first local split, literal `JNC +$0c` at `$02d4` to `$02e2`.
The sequential bytes end in literal `JMP -$79` at `$02e0` to `$0269`.
This records only direct byte control edges: no carry condition, interrupt,
callee result, or higher-level behavior is inferred.

The `$02d4` JNC target at `$02e2` is anchored for its first 14 bytes:
`50 93 33 D2 33 C9 B8 02 42 CD 21 72 E7 50`. Its next direct control edge is
literal `JC -$19` at `$02ed` to `$02d6`. This remains raw static control/data
evidence only; Project Eon infers no carry, interrupt, result, or DOS effect.

The `$02ed` JC target at `$02d6` has its own 12-byte anchor:
`0E 1F 8B 16 D5 05 B4 09 CD 21 EB 87`. Its sequential control byte is the
same literal `JMP -$79` at `$02e0` to `$0269`; this is an observed byte-level
join, not evidence about either condition, interrupt, callee result, or DOS.

At the joined `$0269` path, the parser anchors the opening 16 bytes and the
later `B8 08 25 CD 21 58 22 C0 74 14` branch-tail at `$02aa`. The first next
direct control edge in that tail is literal `JE +$14` at `$02b2` to `$02c8`.
This remains a raw static address/byte fact only, with no interpretation of
the intervening interrupt bytes, condition, result, or DOS behavior.

The `$02b2` JE target has the exact seven-byte prefix `B4 4C CD 21 32 C0 CF`
at `$02c8..$02ce`. `$02ce` is the first terminal control-transfer opcode
boundary; the parser stops there and does not infer an interrupt or transfer
effect from any of these bytes.

MILL.COM's first private `INT 91h` boundary now has a proven source ABI without
inventing a DOS segment. Before its `$0204` call to `$02cf`, selector `$01`
keeps `DX=$0617` (`ega640.bin`) and selector `$02` selects `DX=$03ae`
(`mcga.bin`). `$02cf` opens that original file, seeks to the end, rounds its
length to paragraphs, allocates a DOS segment, rewinds, and reads the original
file length to `DS:$0000`, then closes it. There is no embedded blob, copy
loop, or decompression stage in this loader. Its result segment remains
unknown: the only byte-proven relation is `MOV DS,AX` after the allocation and
before the read.

After that loader returns, `$0207` clears `DX`, loads `AX=$2591`, and executes
`INT 21h` at `$0209`. Thus the raw vector request has interrupt number `$91`
and the exact source ABI `DS:$0000`, whose code bytes originate in the selected
original external file. Project Eon does not assign a numeric DOS segment,
assume any DOS call succeeds, or infer handler execution/return behavior.

The loaded `ega640.bin` (4,632 bytes, SHA-256
`ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a`) and
`mcga.bin` (4,366 bytes, SHA-256
`bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228`) both
start an `INT 91h` dispatcher that doubles `AX` into their local word table.

The original `MILL.COM` driver choice is now separately byte-locked. Its
45-byte command-tail scan at loaded `$019d..$01c9` (file `+$009d`, SHA-256
`157c83c6cdef55dfb7531bceee1759884f68237f445437553d36c12f167d6eba`) reads
PSP `$0080`: `e`/`E` sets AL `$01`, `m`/`M` sets AL `$02`, and an unrecognised
token loops. An empty tail calls `$05a1`; its 66-byte detector
`$0593..$05d4` (file `+$0493`, SHA-256
`9228c64e003a093dcdebda600fe969e4112079e454834133559c0b52f4cf351c`) makes
hardware/physical-memory observations and returns `$00`, `$01`, or `$02`.
The subsequent 48-byte map `$01de..$020d` (file `+$00de`, SHA-256
`57d768c01d59a98d7a5cc452a871be2fa2c7e20c272e88da82e644635ac57be3`) maps
AL `$01` to `ega640.bin` at `$0617` and every other value to `mcga.bin` at
`$05f9`, before the existing load-to-`DS:$0000` / vector-install boundary.
This proves original input-to-request control flow, not a host policy: Project
Eon does not read a host command tail as original hardware detection, select a
driver, or assume the loader/DOS/vector calls succeed.

The `$1f` query's low byte is also not a fixed profile constant. EGA function
`$00` writes its derived allocation count to local `$008a` at `$022f`, and
function `$1e` can write a clamped caller byte there at `$0259`; MCGA function
`$00` writes its derived allocation count to `$00ac` at `$0246`, and function
`$1e` writes a recomputed count at `$02d0`. Both supplied file bytes are zero,
but these are conditional driver-internal writes. The recovered `2200AD.EXE`
startup requests `$1f` only and has no confirmed installed driver segment or
successful state-establishing title path. `TITLES.EXE` does contain a separate
candidate: its `$1b80..$1ba7` prefix (file `+$1a80`, 40 bytes, SHA-256
`e6e014d7c03f9efbd7e9bde67686c281cf66acca809b306cc29dfb45d614b535`) prepares
AX `$0000`, ES=CS and BX `$1ac4`, then calls the shared `$0122` wrapper, whose
`$0127` is `INT $91`; the two-byte record at `$1ac4` is `01 00` (SHA-256
`47dc540c94ceb704a23875c11273e16bb0b8a87aed84de911f2133568115f254`). For
the supplied MCGA driver this can select function `$00`; that function reaches
only the `INT $10` boundaries described below, not `INT $92`. `INT $92` occurs
in other MCGA dispatch functions and is not reached by this record. BIOS mode
results, title exit, DOS return, driver lifetime and the later `2200AD` call
remain unobserved. Project Eon therefore never substitutes
the on-disk zero or any calculated value for the unknown runtime AL result.

The parent-side ordering is separately hash-locked as one startup model.
`MILL.COM` calls a common helper first for `TITLES.EXE`, then conditionally for
`2200ad.exe`; its caller `$023d..$0252` is 22 bytes with SHA-256
`829b3d096d593d1ff4f1028eb05af1ccf8ca0b8ead98a5edcb523dba4cd725cf`.
The helper `$031c..$034b` (48 bytes, SHA-256
`62cee56837e015eecc218906046c1e1c19a7ad9ba87e6580f99674eac0976b58`) fills
the three segment fields of immutable parameter block `$067a..$0687` (14 bytes,
SHA-256 `e2b2aa089d2c6a23b14055f3721c6b53836268070c2a727d2d7fa1a75461869b`),
saves parent SP at `$05f7`, invokes DOS EXEC `AX=$4b00` at `$0337`, restores
the parent context, then branches on carry. Only the noncarry route asks DOS
for child termination status at `$0348` before returning. Thus ordering, ABI
operands, and restoration are evidence; child completion, AL, title exit,
vector survival, and `2200AD` execution are deliberately unknown.
Function `$00` resolves to `$01c8` (EGA) or `$01e6` (MCGA). The supplied MCGA
function-$00 body `$01e6..$0209` (36 bytes, SHA-256
`fb21e417ebf59d096edf515db6258423a2e304ce513b125a075e15f0a23723e8`) first
reads `ES:[BX]` into CL and clears CH, but its complete verified local body
does not branch on that caller byte. It compares code-local `$00ae` with
`$ff`: only that sentinel path requests BIOS current-mode information at
`$01f5` and stores the externally returned AL back to `$00ae`; either path
then requests mode `$13` at `$01fc`, requests current-mode information again
at `$0201`, compares its external AL result with `$13`, and returns that
unmodified external register state at `$020a` on equality or a locally zeroed
AX at `$0208` on mismatch. The `JNE` cache bypass lands at `$01fa`. EGA has
the same instruction layout at `$01c8`: local `$008c`, query `$01d7`, bypass
`$01dc`, set mode `$0e` at `$01de`, verify `$01e3`, match return `$01ec`, and
mismatch return `$01ea`. These are only static BIOS boundaries: Project Eon
does not assign either BIOS result, assert that the on-disk `$ff` is the
runtime cache state, or use the caller's title record to select a mode.
Function `$04`
resolves to `$0c17` / `$0815`, reads the input byte at `ES:BX`, masks it with
`$03`, and updates only its code-local byte (`$008d` / `$00af`) before `RET`.
Function `$1f`, which the `2200AD.EXE` startup wrapper requests at `$d2c5`,
resolves to `$0235` (EGA) / `$024c` (MCGA). Its exact six-byte prefixes load
AL from driver-local `$008a` / `$00ac`, set AH to `$04` / `$01`, and return;
neither reads the caller's `ES:BX`. The supplied on-disk bytes are zero, but
their launcher/title-runtime values and the original installed-driver choice
are not established. Therefore this is a driver-specific, read-only ABI fact,
not permission to select a driver, fix `$da05`, or execute the startup/GX
path. This is a strict SDL-adapter boundary for requested video mode and a
masked driver-local option; Project Eon executes neither the driver, BIOS call,
nor any path-dependent initial presentation.

The caller sites that request private functions `$02` and `$04` leave the
pointed `ES:BX` records inside the original executable image, but the supplied
bytes do not yet establish those records' complete layout, ownership, or the
functions' return contract. In particular, the observed AX=`$04` input-mask
operation is not evidence that the caller's adjacent record is a host video
configuration. Project Eon preserves the addresses as control-flow evidence
and deliberately does not build an SDL adapter or mutable substitute for
either call until a real caller-to-driver ABI has been recovered.

The old-vector preservation chain is also raw-byte-validated. `$0167` loads
`AX=$3591`, makes its external call, then stores `BX` and `ES` at adjacent
cells `$05e7/$05e9`. On the terminal cleanup path `$0269`, `LDS DX,[$05e7]`
feeds the same pair to another literal `AX=$2591` call. This connects the
caller-side save and restore operands only; it does not assert that either
external call reads, writes, installs, or restores any particular vector.

The English DOS archive's `SFX1.VOC` is decoded directly as a Creative Voice
File: its verified SHA-256 is
`5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88`.
Its original type-1 PCM block has time constant `$9c`, unsigned 8-bit mono
rate 10,000 Hz, and 738 samples whose SHA-256 is
`811de4108fe6551e09da1865f3ff2e18a8313aad30a6916210c4d5d49b1e1c06`.
The native decoder accepts the original uncompressed sound and continuation
blocks, preserves the source PCM bytes, and rejects encodings not yet proven
by game media rather than replacing effects.

`2200AD.EXE` also contains the exact contiguous DOS name sequence
`SFX1.VOC` through `SFXE.VOC` at loaded address `$cfdd`. This establishes the
original resource family, but it is not a playback trace: no direct code
reference from a startup or UI path to that table, nor a verified sound-driver
call with a selected entry, has yet been recovered. Project Eon therefore does
not attach any VOC file to a host event, title transition, or timer. The
existing decoder is retained as a source-byte parser only until a real caller
and trigger are established.

`2200AD.EXE`, `2200GX.EXE`, and `TITLES.EXE` are flat 16-bit binaries despite
their suffix. `MILL.COM` provides a private runtime through interrupts 91h,
92h, and 95h. `2200AD.EXE` jumps from file offset `0x0004` to `0xd1b0`, then
uses DOS services and loads original libraries. See the
[DOS analysis](generated/dos-millennium.md).

The English DOS `2200AD.EXE` does have a separately bounded caller-connected
overlay load for the original `2200GX.EXE` (SHA-256
`093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb`). The
literal NUL-terminated name is at `$11c2`; loader `$11ce..$11f6` (file
`+$10ce`, 41 bytes, SHA-256
`a8972b74ad9d1dfabe508c42b7fcda0fb45e0d449613449ab8a2763ca8ecff45`) reads
the original segment cell `$0118` and has static calls `$11d1 → $053a`,
`$11e4 → $0574`, and `$11ec → $0596`. The caller at `$d335` reaches `$11ce`.
The independently locked adapter `$6c52..$6c72` (file `+$6b52`, SHA-256
`b34e5abf8ecd790fce3e7a032d7a7fcacc073d03909e98fd33f9503113e3ad87`) reads
the same cell, pushes overlay offset zero, executes `RETF` at `$6c68`, and
has local continuation `$6c69`/return `$6c72`. These are raw loader and
transfer facts only: Project Eon does not choose a segment value, invoke a
DOS/private call, run overlay code, or infer a screen/resource order.

The overlay's actual entry dispatcher is separately locked at `2200GX.EXE`
`+$0000..+$0013` (20 bytes, SHA-256
`f4d657fcbdda23d7f0fdf2bbf48405d0a04e8b8149df064607f49042525fbd55`). It
clears AH, uses AL to index the original 21-word table at `+$0015..+$003e`
(SHA-256 `4d04568e05378787921012654fe9c157419ce7c07f9943b51135258f32a06df3`),
and near-returns to the `RETF` at `+$0014`. The caller-connected observed
selectors `$0e/$0f/$12/$14` map to original overlay offsets
`$0090/$009f/$0097/$00a7`. No selector policy, handler return, overlay state,
resource, or display effect is inferred or executed.

There is a caller-connected selector prefix immediately before the adapter:
`2200AD.EXE` `$d343..$d375` (file `+$d243`, 51 bytes, SHA-256
`571626e83b0787401f89c8586c12dfb4d4221c44e0a9786727d2314b09327091`) reads
the still-unmodelled byte `$da05`. Values `$03/$04/$02` select AX
`$000e/$0012/$0014`; every other value selects `$000f`. The corresponding
literal DX values are stored at `$4b6e`, then `$d373` calls the adapter at
`$6c52`. The four overlay entries converge in a 94-byte local prefix at
`+$0090` (SHA-256
`8d412472415d513482b5c70198bb1aa04fa0d25798dd5f4b40b262151c489736`) that
only copies in-overlay record words and returns. No selector value, record,
return, asset reference, video instruction, resource, or display effect is
provided, inferred, or executed.

The English `2200AD.EXE` COM entry preserves the original segment setup before
the recovered main loop: loaded `$d2b0` first establishes `DS=CS` and `ES=CS`;
the following `$d2b4` block establishes `SS=CS`, `SP=$da00`, and makes its
first direct call to `$0124` (the raw near-call arithmetic crosses `$ffff`,
then wraps in 16-bit IP). It stores the native result in original cells,
compares its `AL` byte with `$01`, and selects direct call `$d1a1` or `$d1b5`.
After another original call it tests `DX`; the nonzero static edge is `$d44b`.
Project Eon validates these bytes, addresses, and branch targets, but does not
assume any call returns or make this into a host-side startup sequence.

The wrapped `$0124` target itself saves `DS`, `SI`, `DI`, `BP`, and `ES`,
executes the launcher's private `INT $91`, restores those registers, and has
its `RET` opcode at `$0130`. This is a bounded instruction-level fact only:
the private interrupt's result and actual return behaviour are not emulated.

If that wrapper returns, its caller's next raw block is independently fixed:
at `$d2c8` it stores `AX` at `$d128`, moves `AH` to `$4368` and `$da05`, and
stores `SP` at `$d12c` before comparing `AL` with one at `$d2d9`. The parser
records these destination operands and the following static branch edges only;
it does not assume a return, assign values to the cells, or execute either
branch.

The two selected static paths are independently byte-validated as well. The
`AL == $01` call target `$d1a1` and the other target `$d1b5` each set
`AX=$0004`, `ES=CS`, and `BX=$d19f` before directly calling the same `$0124`
private-`INT $91` wrapper again, from `$d1a9` and `$d1bd` respectively. Only
if those calls return do their next direct calls reach `$044e` or `$0466`.
These are control-flow operands only: Project Eon neither assumes either
wrapper return nor interprets the register setup, follow-up calls, or their
results.

`MillenniumDosEnglishGameStartupCallees` now keeps those two English selector
targets as their own hash-addressed preservation boundary rather than relying
on the wider main-loop profile. The 20-byte equal block at `$d1a1` hashes to
`6f59df77c567324b41dd6159a6fbac7d8970626fc40e8b908f9f58746a993a3e`; its
private wrapper call is `$d1a9 → $0124`, its conditional local successor is
`$d1ac → $044e`, and only after both encoded calls does it write literal `$01`
to `$da05` and return at `$d1b4`. The 28-byte other block at `$d1b5` hashes to
`2f61098eb45bb48ea7a38ab2fcc2e065ae0d0b2ad08ea9973e3fe464943fba9b`; it
has `$d1bd → $0124` and `$d1c0 → $0466`, then reads native `$da05`, compares
it with `$02`, and conditionally encodes the `$b800` store at `$0107` before
its `$d1d0` return. These are bytes and operands, not a claim that the
private interrupt returns, that either branch is chosen, or that the native
comparison has a particular value.

The immediate local successors are independently preserved by
`MillenniumDosEnglishGameStartupFollowups`. `$044e` is the eight-byte literal
store/RET sequence (SHA-256
`38889279a8b89e0e600bb25298015ccd8aadc09ea3858a1790097b3f7ff4ea8f`).
`$0466` through `$047c`, together with its table at `$0456`, is a 23-byte
in-image BIOS palette-request prefix and a 16-byte table, SHA-256
`b17db26fa4fa8b7307fb767ff98351bd6dcca202829dd2d9348ff4991942d779` and
`ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a`.
It loads initial `CX=$0010`, reads that table, encodes `AX=$1000`, and first
reaches `INT $10` at `$0476`. The BIOS interrupt, any register effects, and
any number of loop executions are explicitly outside this recovery.

The later English startup continuation is independently hash-locked as well.
If either selected private-wrapper path returns, `$d2e5` preserves DX,
restores DS from CS, and calls `$d1fa` at `$d2e8`. The callee's first nine
bytes end at its `INT $21` site `$d201`, with literal AH `$4a`; this is an
external DOS boundary, so Project Eon neither invokes it nor supplies AX, DX,
carry, or a return. Only if that call returns do the following bytes store AX
at `$d128`, test DX at `$d2ee`, branch on zero to `$d2f5` (whose first local
call is `$d2f5 → $1161`), or jump on nonzero from `$d2f2` to `$d44b`. These
are raw static control-flow operands, not an allocation result, startup
decision, or executable path. The 20-byte continuation and 9-byte callee
prefix are both SHA-256-validated by
`MillenniumDosStartupAllocationBoundary` against the full original English
`2200AD.EXE`; mutations are rejected before the facts are exposed.

The DX-zero successor is now independently bounded as a separate static
chain. `$d2f5` calls `$1161`, which reads the native byte `$da05`; literal
comparisons `$01/$03/$02` select the in-image name addresses
`$1131/$113d/$1155`, respectively, while the default sets `$1149`. Those four
NUL-padded names are `VGATXT.BIN`, `EG3TXT.BIN`, `EG6TXT.BIN`, and
`TDYTXT.BIN`, in raw storage order `$1131/$113d/$1149/$1155` (48 bytes,
SHA-256 `153a0b62bdec1702cdd36ff6e7dc33ec4ed6673ad5d3f5f8bc07b748f7e06d76`).
The selector then has direct call `$117c → $053a`. That callee's fixed prefix
replaces DX with the in-image `A:\\2200AD\\SECURITY.HID` name at `$2f6a`
(23 bytes, SHA-256 `1a95edb6109f3db1af0c0389f1aa5d597a184f26725e095f771b6622f654ec6a`)
before first reaching `INT $21` at `$0550` with `AH=$3d`, `AL=$02`.
`MillenniumDosStartupZeroPathBoundary` locks the three-byte zero successor
(SHA-256 `798bd5318e00348848f0ca4b876d687fec5c606abe88236ff4e922a77fe08b65`),
30-byte selector (SHA-256
`fffa1b0e03e9abf90bfde3bfb86bf1125ae579ede767eea68223e098d641992f`), and
24-byte callee prefix (SHA-256
`328e11edf0653b0e0f21db3b61cf9ff95795ec9431f07c0198a700358f75ed74`) against
the full original executable. This does not choose `$da05`, infer a selected
name's use, invoke DOS, provide carry/AX, or claim the code path executes.

There is a further caller-connected static continuation after that selector,
but only if the selector and its DOS-facing loader both return. The 23 bytes
at `$d2f8..$d30e` hash to
`9c7b13c4e0b99e8529e78063b91ae92d967b9fc6de66ebeeaacec01563e4a9d9`.
They load the encoded source address `$0082` through `CS:SI`, subtract literal
`$30` from the resulting `AL`, store it at `$0122`, and call `$d305 → $d07a`.
The subsequent literal `BX=$fa00`, `AH=$48`, `INT $21` reaches its next
external DOS boundary at `$d30d`. `MillenniumDosStartupZeroContinuationBoundary`
locks that exact span and near-call arithmetic against the complete English
executable. It does not assert any byte read, local-call return, DOS result,
or allocation; this is conditional instruction provenance only.

There is one further conditional post-allocation boundary, anchored separately
without interpreting the allocation call. Only if the preceding local helper
and `INT $21/AH=$48` both return, `$d30f..$d318` encodes `CS:MOV [$d130],BX`,
`MOV ES,AX`, `AH=$49`, and `INT $21`. Its ten bytes hash to
`f583faad7bddba301c431adb94fa9d53d5b197dcba2f447b0b654df6f1b452ce`.
`MillenniumDosStartupPostAllocationBoundary` records the encoded `$d130` store,
the `$d314` register-transfer instruction, and the next DOS boundary at
`$d318`; it does not treat AX or BX as a result, infer a segment, invoke DOS,
or claim a return from this new boundary.

There is a caller-connected continuation after that second DOS boundary, but
only on the unproven condition that `INT $21/AH=$49` returns. The 30 bytes at
`$d31a..$d337` hash to
`4d94bf904471cf96a03ce6dd111c0720f396e08ebf2f4603469377db0dc669ef`.
They restore DS from CS, pop the earlier saved DX, use `LDS` twice with the
same encoded far-cell address `$1042` (first into DX, then into SI), and make
three direct near calls: `$d32f → $6bf2`, `$d332 → $101a`, and
`$d335 → $11ce`. The latter two targets are the separately preserved
`2200AD4.BIN` and `2200GX.EXE` loaders; that relationship is raw static
caller evidence only. `MillenniumDosStartupPostReleaseContinuation` validates
the entire original English executable, this exact span, and all three
16-bit wrapped call calculations. It does not assert that AH=$49 returns,
that `$1042` holds a valid pointer, that any local call returns, or that DOS
frees, loads, or transfers any host resource.

The immediate encoded successor of the GX-loader call is separately bounded
as well. Only if `$d335 → $11ce` returns, `$d338..$d342` restores ES from CS,
loads literal `BX=$d1a0` and `AX=$0022`, then calls the existing private
`INT $91` wrapper at `$d340 → $0124`. Those 11 bytes (file `+$d238`) hash to
`64e7dddae2ca6942cddaa4c564d61203b26c469fc898bb923b2ba227d93876ab`.
`MillenniumDosStartupPostGxLoaderBoundary` validates the full 54,391-byte
English `2200AD.EXE`, the raw span, and its 16-bit wrapped call calculation.
The literals are encoded operands, not a reconstructed private-runtime ABI:
Project Eon does not assert that the loader or wrapper returns, interpret the
arguments, invoke `INT $91`, or supply a result.

The call target itself is now retained separately as
`MillenniumDosPrivateInt91Wrapper`. In the same hash-identified English
`2200AD.EXE`, its 13 bytes at `$0124..$0130` hash to
`5d17daad68e9062dc6852ae76740db4afdcb81555ba9fb7d15d4e4aa8d088175`.
The complete raw instruction sequence is `PUSH DS`, `PUSH SI`, `PUSH DI`,
`PUSH BP`, `PUSH ES`, `INT $91`, `POP ES`, `POP BP`, `POP DI`, `POP SI`,
`POP DS`, `RET`. The parser also validates the three caller bytes at `$d340`
and their wrapped near-call target `$0124`; this ties the wrapper to the
post-GX route without claiming that the loader returns. It deliberately does
not infer a stack result, register preservation convention, private interrupt
ABI, interrupt effect, or return from either the interrupt or the wrapper.

The caller's immediate encoded return site is separately retained as
`MillenniumDosPostInt91CallerSelector`, without treating that return as a
runtime fact. In the same 54,391-byte English `2200AD.EXE`, the 51 bytes at
`$d343..$d375` hash to
`571626e83b0787401f89c8586c12dfb4d4221c44e0a9786727d2314b09327091`.
They load the original byte at `$da05`, compare it with literals `$03`, `$04`,
and `$02` at `$d34d`, `$d358`, and `$d363`, respectively, select among four
encoded DX/AX pairs, store DX through a CS override to `$4b6e` at `$d36e`, and
make the first direct local call `$d373 -> $6c52`. The parser validates the
complete executable, the exact span/hash, and the wrapped near-call target.
It does not assert that the private wrapper returns, read or assign a value to
`$da05`, interpret the register pairs/store, run the callee, or infer any
interrupt behavior.

The encoded caller continuation after that adapter call is independently
preserved as `MillenniumDosPostOverlayAdapterContinuation`. It is explicitly
conditional: the adapter's `RETF` transfer need not return. If it does, the
39 bytes at `$d376..$d39c` (file `+$d276`) hash to
`1df4b30f14434eae3a44463402710bcd1b162200a923c0b9cc1f827faf3763ac`.
They make six direct near calls, in order, to `$d152`, `$4f08`, `$4111`,
`$40af`, `$42b2`, and `$107a`. The prefix then compares the original byte at
`$da05` with `$01`: its encoded equal branch reaches `$d394 → $d1a1`; the
other route calls `$d38f → $d1b5`, short-jumps to `$d397`, and the two paths
converge on raw `PUSH CS`/`POP DS` and two `PUSH CS`/`POP ES` pairs. The parser
validates the whole original executable, span hash, and every near-call target.
It does not claim any call returns, choose the byte value or branch, assign a
meaning to the segment setup, execute a target, or provide native state.

The following 69-byte encoded caller span is separately preserved as
`MillenniumDosPostOverlayAdapterLoop`. It begins at `$d39d`, directly after
the prior segment-setup span, and hashes to
`1bbb4fcc18668021306de1e0014a9baab1f526af1514fa7ce9d1a61780972cf0` in the
same English 54,391-byte `2200AD.EXE`. It has fifteen direct near CALL
encodings to `$446a`, `$5b1f`, `$6178`, `$799c`, `$52f9`, `$7b7f`, `$09e4`,
`$11a4`, `$0b0c`, `$0ea4`, `$0b5b`, `$0ebb`, `$7601`, `$7bcb`, and `$0f05`.
It contains AL tests at `$d3ba` and `$d3de`; the first encoded nonzero route
goes from `$d3bc` to `$d3c6`, while the final encoded zero route goes from
`$d3e0` to `$d3d2` before the existing dispatcher at `$d3e2`. Between them,
the raw instructions load, XOR `$01`, and store the original byte at `$07f9`
at `$d3be/$d3c1/$d3c3`. The parser validates the full executable, complete
span hash, and every direct near target. It neither assumes the adapter or
any call returns, reads the runtime byte, chooses either branch, interprets
the encoded instructions, nor gives any target a host-side effect.

The loop's fall-through target is separately fixed as
`MillenniumDosPostOverlayDispatchPrefix`. The 49 bytes at `$d3e2..$d412`
(file `+$d2e2`, SHA-256
`7abec93ec23f7ca3c4b400e16b9e746da7b0b9a1dd4bec88ba891ef04b322065`) first
compare AL with `$0b` and branch to `$d40e → $11a4`; otherwise they read and
test native byte `$da3a`, compare AL with `$0c`, and encode `$d3f4 → $d570`.
The remaining route subtracts `$3b`, bounds against `$0a`, loads table base
`$2fbf`, and has direct calls `$d40a → $76f1` and `$d40e → $11a4`, each with
an encoded jump back to `$d3d2`. This parser validates the full English
executable, raw span and wrapped CALL targets. It does not supply AL or the
guard byte, select a table item, dereference native state, assume any branch
or call return, or attach an action to a host effect.

The complementary DX-nonzero successor is independently bounded before its
first mouse boundary. The static target `$d44b` loads `AL=$08` and its short
jump at `$d44d` lands at `$d41b`, deliberately skipping the adjacent `$d419`
`XOR AX,AX`. The target's 11-byte prefix stores AL through a CS override to
`$2fb2`, restores SP from `$d12c`, then calls `$d423 → $09e4`. That callee
starts with `MOV AX,$0000` and reaches `INT $33` at `$09e7`. The five-byte
four-byte nonzero entry, 11-byte continuation, and five-byte callee prefix have SHA-256
values `92252049901ece1d56c7b17fdd7450ce8ade576650b4f7b032f61dd1e4e59522`,
`7fb9d6276e557976c68a02e9900531347fd95ecbfbd6fc3fa60cd0c176ca5c5d`, and
`d84b931c90a3b7e1baf2a0a6caf2c67fc5834ed6a160750ba6991b77fdb11909`,
respectively. `MillenniumDosStartupNonzeroPathBoundary` validates all three
against the original English executable and rejects mutations. It does not
decide DX, infer AH on entry, call the local routine, supply a return, or
emulate any mouse-interrupt state or result.

The direct follow-up targets are also bounded by original bytes. `$044e`
loads literal `$01`, writes it to `$da05`, and returns. `$0466` sets `DS=CS`,
points `SI` at the verified 16-byte in-image sequence `$00..$07`, `$38..$3f`
at `$0456`,
sets `CX=$0010` and `BL=0`, then reaches `INT $10` at `$0476`; its local loop
back-edge is raw code only. The BIOS interrupt is the first external boundary
for this route. No BIOS behavior, loop iteration, or runtime state is
emulated.

The register setup at that boundary is exact: each iteration moves a byte from
the `$0456` table into `BH`, sets `AX=$1000` (`AH=$10`, `AL=$00`), and invokes
`INT $10` with incrementing `BL` from zero through fifteen. This is the
documented BIOS "set single palette register" operation: `BL` is the palette
register and `BH` its raw color value. Project Eon's narrowly typed SDL-facing
adapter payload therefore exposes the 16 verified `(register, value)` pairs,
but nothing calls it automatically: reaching this routine still depends on
the preceding private-interrupt paths returning, and no BIOS or SDL palette
effect is presumed.

#### Main-loop action dispatch

The supplied English `2200AD.EXE` (54,391 bytes, SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`) has
its flat-image entry at loaded `$d2b0`. After the startup calls, the verified
loop at `$d3d2` reaches the wrapped `$0f05` action poll at `$d3db` and tests its returned `AL`: zero repeats the
loop; `$0b` and `$0c` branch to separate native paths; otherwise it subtracts
`$3b`, rejects values `>= $0a`, and passes a zero-based index through an
eight-byte table at `$2fbf` to `$76f0`. This proves an actionable ten-entry
range `$3b..$44` (the PC F1–F10 scan-code range), but does **not** prove what
the handlers mean or how they alter state.

`MillenniumDosGameFlow` validates the exact entry and loop bytes before
exposing those values to preservation tests and inspection. The SDL launcher
does not map F1–F10 into this loop: the title input boundary proves neither the
DOS return nor `2200AD.EXE` startup. No host action invokes a handler, mutates
`2200SAVE.I`, or claims menu/action names. The special `$0b`/`$0c` paths remain
documented but are not host-bound until their input production and state
prerequisites are recovered.

Project Eon's **host** F10 opens a modern SDL graphics popup with smooth
scaling, scanline, and renderer-frame toggles. It is explicitly consumed before
the title availability poll and is not an original F10 action: it changes only
host rendering and never original pixels, game logic, runtime state, or saves.

`MillenniumDosGameSession` can now retain a non-owning view of the authenticated
English `2200AD.EXE` specifically for offline/runtime-trace observation of the
two special actions. It does not make this path reachable from SDL: the title
return, action poll and native prerequisites are still unrecovered. Given an
explicitly observed native byte, raw `$0b` is revalidated against the exact
dispatcher and `$11a4` handler before recording the one unconditional prefix
write `$07f9 := observed XOR $01`; it stops at its first native helper `$0666`.
The supplied observation, rather than a prior Project Eon trace, is retained as
the pre-write value. Raw `$0c` similarly revalidates its dispatcher and `$d570`
handler, reports whether explicitly observed `$da3a` blocks it or reaches
helper `$6c52`, and records no write. The session owns no game bytes, copies no
media, performs no native call, and never maps these raw action values to SDL
keys. Any altered executable or session without the original byte view is
rejected before an action trace is exposed.

The first table record (raw F1 / `$3b`) is now traced further without assigning
it a game-menu name. Its eight original bytes are
`00 06 09 1b 30 00 9a 6f`, so its handler entry is `$6f9a`. That handler clears
`AX`, calls the common display selector at `$d0c9`, and only then calls
`$771d`. The latter's byte-validated prefix writes runtime selector
`$da1f = 0`, retains `$12cc` at `$da20`, obtains word zero from the embedded
pointer table at `$27c4`, and therefore selects original in-image record
`$12cc`; the observed word is indeed `$12cc`. It selects mode `$07` at
`$75a8`, descriptor `$300f` at `$75a6`, and reaches its first further native
call at `$5b1f`. The selected record begins
`03 00 11 00 00 00 00 00` (notably byte `+02 = $11` and byte `+24 = $00`).
All these F1 setup stores occur after the native `$d0c9` call, whose return and
side effects are not yet emulated. Project Eon consequently exposes this
strict boundary only as immutable evidence: it neither creates a host overlay
for F1 nor names the handler nor writes any original executable, archive, or
save byte.

The second table record (raw F2 / `$3c`) is `06 0c 09 1b 31 01 ca 71`, with
handler entry `$71ca`. This handler reads its runtime byte at `$da26` and
compares it with `$02`. If the byte is lower, it repeatedly calls `$09fa` and
returns; Project Eon neither fabricates that runtime byte nor pretends the
gate was admitted. Its admitted path at `$71de` writes callback `$7221` to
`$6f98`, records the one-byte selector `$01` at `$6e98`, and makes a word list
at `$6e99`: it begins at original in-image `$1384` and advances by `$00c0` for
each unit calculated from `$da26 - 1`. The code then writes `$08` to `$da1e`
and calls `$0b76`. These are strictly address/value observations, not inferred
names for a menu, records, or action. Project Eon surfaces this gate after F2
in the SDL evidence panel while leaving original media and its unknown runtime
state immutable.

The third table record (raw F3 / `$3d`) is `0c 12 09 1b 32 02 aa 6f`, with
handler entry `$6faa`. It returns if runtime word `$a19e` is nonzero. Only
with that word zero does it inspect runtime word `$da27`; while that word is
zero it calls `$09fa` in the original wait loop. Its admitted setup at `$6fc6`
installs callback `$712a` in `$6f98`, writes mode `$00` to `$6e98`, and begins
its list at `$6e99` from the original far pointer stored at `$0112`. Project
Eon presents these two gates and setup addresses after F3, but does not invent
the runtime values, dereference a host-side replacement list, or assign the
handler a game meaning.

The fourth table record (raw F4 / `$3e`) is `12 18 09 1b 33 03 f9 72`, with
handler entry `$72f9`. It first reads runtime word `$a19e`; a nonzero value
returns immediately. Only when that word is zero does it place `$02` in `AL`
and transfer to `$ba5e`. The recovered common bytes first load `AX=$0005` and
call `$4d2c`, then at `$ba64` write `$07` to `$da13`, call `$9dd5`, at `$ba6c`
write `$09` to `$da1e`, clear `$75a9`, and return at `$ba76`. There is no
pre-call literal write. The first write is reached only when `$4d2c` returns;
the final two are reached only when `$9dd5` returns. These are code-validated
addresses and literal writes only: the calls' effects, their return behavior
for live runtime state, and the cells' meaning are not inferred. Consequently
F4 contributes no private-overlay effect, unlike F8's pre-call store. The
guard is not save-backed: original code at `$a557` contains
`mov cx,[$a19e]; mov word [$a19e],$0000`, but the preceding path branches on
native runtime values. This proves the guard's real producer without proving
that a fresh key event passes it. Project Eon exposes the clear site and the
conditional common-path writes as evidence, but never supplies the guard,
applies F4's writes to its overlay, invokes native code, or mutates
executable/archive/save media.

The fifth table record (raw F5 / `$3f`) is `18 1e 09 1b 34 04 97 75`, with
handler entry `$7597`. It loads `AL=$02`, has **no memory store**, then makes
16-bit near calls to `$be28`, `$0b9d`, `$4bf7`, and `$0b76` before returning.
The original handler's first call target begins `call $52f9`; therefore no
F5-owned pre-call state change exists and the first safe post-call boundary is
the return of an unexecuted native call chain. `$0b9d` begins by comparing
native byte `$07f9` to `$01` and can enter a native input/hardware wait;
`$4bf7` itself immediately calls `$0bd7`; `$0b76` begins with the same
`[$07f9] == $01` comparison. These operand and control-flow facts are not
gameplay semantics and do not justify a host-side overlay effect. Project Eon
displays this immutable evidence after F5 but never executes native calls,
supplies state, or writes the original executable, archive, or save media.

The sixth table record (raw F6 / `$40`) is `1e 24 09 1b 35 05 15 74`, with
handler entry `$7415`. It first returns when runtime word `$a19e` is nonzero.
On the admitted path it clears `AX`, calls `$d0c9`, then calls `$4d2c` with
`AX=$0022` and `$c980`. Only after those native calls return, the handler
snapshots bytes `$75a8` and `$75ae` into its own `$7412` and `$740f` scratch
cells and snapshots word `$75ac` into `$7410`; it then writes literal `$0c`
to `$75a8`, `$00` to `$75ae`, and `$3207` to `$75a6` before calling `$09fa`.
The immediately following `SHR BL,1`/carry branch can repeat that poll.

The immediately adjacent but separately entered `$7455` routine proves the
paired cleanup sequence: it copies `$740f` back to `$75ae`, `$7410` back to
`$75ac`, and `$7412` back to `$75a8`, then makes its first native call to
`$0b0c`. The actual dispatcher/callback edge that reaches `$7455` is not yet
proved; in particular `$3207` is recorded only as a literal word written at
`$75a6`, not dereferenced as a host callback. Therefore neither the temporary
stores nor the cleanup are a safe private-overlay effect. Project Eon exposes
this byte-validated, strict no-overlay boundary and never supplies the
guard/carry, invokes native code, applies the writes, or alters original
executable, archive, or save media.

The seventh table record (raw F7 / `$41`) is `24 2a 09 1b 36 06 21 75`, with
handler entry `$7521`. It returns when runtime word `$a19e` is nonzero. On its
admitted path it loads `AL=$1d`, calls `$4d2c`, calls `$073c` with
`AX=$0612`, then calls `$0666` with `AX=$012a`. The recovered bytes read native
runtime words `$da17`, `$da18`, `$da27`, `$da26`, `$da35`, and `$da37`, route
them through repeated helpers `$06dc` and `$05ce`, use helper `$077e` with
literal `AL=$2e` and later `AL=$25`, and end with calls `$0b9d` and `$4bf7`.
These are code-verified operands and control-flow targets, not assigned game
semantics. Project Eon surfaces the immutable F7 gate in its SDL evidence
panel; it never supplies the guard/runtime words, executes the helpers, or
writes original game media or saves.

The English DOS main-loop's first special action is separately bounded. Its
action `$0b` comparison/dispatch slice at `$d3e2` (43 bytes at file
`+$d2e2`, SHA-256
`1e4e43aad1a2507aa7f85189022063db0f0cb481d267ef79789a447c3e184d62`)
branches to `CALL $11a4` at `$d40e`. The handler prefix `$11a4..$11b9` (22
bytes at `+$10a4`, SHA-256
`2cd76e49776b940065ecb01418394984a9e03a6d6a6fc161c218f450faac1ed5`)
reads explicitly observed native byte `$07f9`, chooses AX `$018f` only when
that byte is zero (otherwise `$018e`), XORs the byte with one, and reaches
opaque `CALL $0666`. The evaluator reports that prefix only: it does not
invent an initial byte, invoke `$0666`, assume its return, or persist the
toggle to original executable/save media. The supplied Spanish executable
shares the handler prefix but has no proven matching action dispatch, so this
English-only route deliberately rejects it.

The shared English helper at `$0666..$0681` is separately bounded at file
`+$0566`: 28 bytes, `1e 56 50 2e 8e 1e 16 01 2e c6 06 c8 05 00 d1 e0 8b f0
ad 8b f0 e8 79 ff 58 5e 1f c3`, SHA-256
`8dc7586f3809a14f3ed6acd601cd42486841adb9d9cb09d3e9b1ed727329e485`.
It preserves `DS`, `SI`, and `AX`; loads `DS` through native `CS:$0116`;
writes literal zero to `CS:$05c8`; doubles caller-provided `AX`; reaches
`LODSW` at `$0678`; and stops at `CALL $05f7` at `$067b`.
`MillenniumDosSharedHelperPrefix` accepts only the supplied English executable
(full SHA-256 `427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`).
It does not invent the segment, dereference the selected table word, invoke
`$05f7`, assume a return, or commit any instruction's provenance to host or
original game state.

The next English-only special action `$0c` is likewise bounded. Its admission
prefix `$d3e8..$d3f6` (15 bytes at `+$d2e8`, SHA-256
`e59faad9b95521837b340ff56ef032cb140327bfabb0b39be32d01bb9c05bda3`)
reads explicitly observed byte `$da3a`: nonzero returns to `$d3d2`; zero
matches action `$0c` and calls `$d570`. The seven bytes at `$d570` (file
`+$d470`, SHA-256
`f266d52e554a2e85147994b34eb69e7678cd9339fda1b99206c18fc05361232b`)
load `AX=$000d` and stop at `CALL $6c52`. No native byte is written before
that call; the evaluator neither invokes it nor assumes its return.

The ninth table record (raw F9 / `$43`) is `30 36 09 1b 38 08 39 73`, with
handler entry `$7339`. It returns when native runtime word `$a19e` is nonzero.
Its admitted path clears AX and calls `$d0c9`, clears `$da30`, loads `AL=$02`,
sets code-local byte `$6e2f` to `$01`, and clears `$dad7`. If `$da39` is
nonzero it calls `$7b47`. It then loops through verified F8 preflight `$731a`
while `$da06` is below `$09`; otherwise it clears `$6e2f`, conditionally calls
`$7a9e` when `$da09` is zero, then calls flat-image target `$14124`. These are
strict code operands and branch facts, not inferred gameplay semantics.
Project Eon exposes the F9 evidence immutably; it does not supply native
runtime bytes, invoke native calls, execute the loop, or write archives, saves,
or other original media.

The eighth table record (raw F8 / `$42`) is `2a 30 09 1b 37 07 06 73`, with
handler entry `$7306`. It clears native runtime byte `$da30`, loads `AL=$02`,
and calls local preflight `$731a`. That preflight reads `$da39`: its nonzero
path calls `$7b47`; its other path reads `$da0a`, returns if it is zero, or
decrements it, applies `XLAT` through `BX=$db4b`, and jumps to `$7948`. Back
in the handler, `$09fa` is called and the following `SHR BL,1` carry branch
can repeat that call. F8's `C6 06 30 DA 00` write is the first F-key effect
that Project Eon reconstructs, because its byte-level semantics are fully
established and it executes before any runtime-dependent branch or call: its
private runtime overlay changes `$da30` to zero. The overlay begins with
`$da30` **unknown**, rather than deriving an initial value from `2200SAVE.I`;
a second F8 therefore records `0 -> 0`. It is not a mutable view of the
original COM image or a save serializer, and is never exported. The later
preflight/call path remains deliberately unimplemented because `$da39`,
`$da0a`, and `BL` have no proven initial state or complete helper semantics.
The preceding `$731a..$7338` preflight is independently hash-locked (31 bytes
at file `+0x721a`, SHA-256
`71c2c4189e66104aea08d4f7040e9d6bc873eb6717607eed30cf61ce27f5ac2e`).
Its two runtime bytes are explicit evaluator inputs: nonzero `$da39` reaches
opaque helper `$7b47`; zero `$da39` with zero `$da0a` returns; otherwise it
decrements `$da0a`, uses the decremented `AL` as an `XLAT` index through
native-memory table `$db4b`, then jumps to `$7948`. The table lies beyond the
COM image, so Project Eon records only its original address and index—never a
fabricated table byte or jump effect.
When a caller has independently observed that external XLAT result, the exact
local `$7948..$7967` prefix is now also hash-locked (32 bytes at file
`+0x7848`, SHA-256
`c52d83152fef75a81d8956b76e7c6931ced4de6a579f4233faf8a28c3cdc72c9`).
It clears `$da09`, writes the explicit translated `AL` to `$da06`, and uses it
as a bounded index into the ten-word in-image selector table at `$78f4`
(20 bytes at `+0x77f4`, SHA-256
`c42e986a183a46d7b4cdf7787766e5f81446b444180e0cf34d9fa5f4b8d50a0d`).
The resulting original pointer and the `$6e2f` zero/nonzero gate are exposed
as facts only. No XLAT result is invented, no selected pointer is executed,
and the following pointer-controlled interpreter remains a boundary.
The first local branch in that interpreter is now separately hash-locked
without crossing a native call. Its `$7948..$799b` bytes (84 bytes at file
`+0x7848`, SHA-256
`99267e09fea1f7d3227b49b3c80a2eacf6673df542bb063da7c54ce87df8a666`),
the ten-word selector at `$78f4` (the hash above), and the selected-record
bank `$77f8..$788f` (152 bytes at `+0x76f8`, SHA-256
`53315644dbe9478d9e8b919d3958cf64cac95260fd3f89b600d92275f97e089c`)
show the following bounded control flow. An explicitly observed nonzero byte
at `$6e2f` branches to `$799a`, restores DS, and returns at `$799b` before any
selected record byte is read. An explicitly observed zero byte selects one of
the ten original pointers, reads its first byte and following word as local
register facts, then reads the record's byte `+3` as a first-list count. All
ten supplied records have a nonzero count; their first list byte reaches the
opaque `CALL $7924` at `$797f`. For example, selected index `$02` identifies
pointer `$7815` and raw prefix `04 73 28 01 84`, reaching that boundary with
the first list byte `$84`. The byte/word fields are not assigned gameplay
semantics, `$7924` is never invoked, no return from it is assumed, and no
later record bytes, calls, runtime writes, saves, or original media are
executed or changed. The same three hashes are present in the supplied
Spanish FAT12 `2200AD.EXE`; this proves only the narrow shared byte path, not
the unrecovered Spanish executable ABI.
After a preflight return, the eight original bytes at `$7312` (file
`+0x7212`, SHA-256
`2bf85a49d14034fb5562af6188745810721fd42e495877464d04f69783525a0a`) are
also modeled as a bounded local trace: `CALL $09fa`, `SHR BL,1`, `JC $7312`,
`RET`. The evaluator requires explicit low-byte `BL` returns from the opaque
`$09fa` helper, records each shift, repeats only when the shifted-out bit is
one, and rejects an unterminated sequence. It does not call `$09fa`, invent a
preflight result, or write original runtime/save media.

The tenth table record (raw F10 / `$44`) is `36 3c 09 1b 39 09 84 73`, with
handler entry `$7384`. It returns when native runtime word `$a19e` is nonzero.
Its admitted path clears AX and calls `$d0c9`, clears `$da30`, loads `AL=$02`,
clears `$dad7`, and sets code-local byte `$6e2f` to `$01`. If `$da39` is
nonzero it calls `$7b47`. It loops through the verified F8 preflight `$731a`
while `$da06` is below `$02`; it then clears `$6e2f`, conditionally calls
`$7a9d` when `$da09` is zero, and reaches direct calls `$4140`, `$7bcb`, and
`$a2a0`. A final poll at `$09fa` depends on `$da41` and can repeat according
to `SHR BL,1`/carry before call `$4111`. These are exact code operands and
control-flow targets only. Project Eon presents this immutable F10 trace; it
does not provide native guard bytes, execute calls, run the polling loop, or
write original archives, saves, or executable media.

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
The live FAT12 `MILL.BAT` is the only standalone launcher documentation in the
recognised corpus: 437 bytes at first disk offset `$2c400`, SHA-256
`1fbb8246d496a6b3a35759a917ef7ae7ba36487de73104f2df81f5a1f8d9f474`. Its
verbatim original text describes `IBM`, `IBM e`, `IBM m`, `TANDY`, and `EGA320`
launch choices, corroborating the byte-validated driver request chain but not
any gameplay control. Two apparent `README` directory-like records at raw
`$38240` and `$3bc40` are allocation slack, not live FAT12 entries, and are
explicitly excluded. The actual Spanish launcher evidence is `IBM.COM` (1,587 bytes, SHA-256
`84b7d158c770117aeaa07cb5ea2e7ed4a6bcc288d6b352d82569ff4d97b2fda9`). Its
hash-locked caller `$023d..$0252` first loads literal `TITLES.EXE` from
`$071d`, calls local `$0339`, then conditionally reaches the second literal
`2200ad.exe` at `$0728` and calls that same local callee. The 48-byte callee
`$0339..$0368` has SHA-256
`c2f5b915a0fbbc7a25d8a3f4c0e5fcc97eb197d44048eaff53e2046eb6e7c32c`; the
Spanish FAT12 targets are independently hash-identified as `TITLES.EXE`
`02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7` and
`2200AD.EXE` `9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6`.
The local callee has now been decoded through its complete 48-byte span. It
inherits the caller's `DS:DX` filename, establishes `ES:BX = CS:$0708`, then
issues `INT 21h` with `AX=$4b00`; after restoring its segment/stack setup, its
carry branch at `$0362` targets `$0369`, skipping the local `AH=$4d` child
status request and `RET` at `$0368`. These are register and control-flow
facts anchored in the original COM bytes, not an emulation contract: Project
Eon does not invoke either DOS service, supply a carry or AL result, assume a
child return, or run either target's unrecovered ABI.

Spanish `TITLES.EXE` is separately accepted only at its own SHA-256 above. Its
own bytes retain entry `$1b80`, private wrapper `$0122`, and post-title
`$1968 -> $1931`: five AX=`$0013` private calls followed by helper `$1917`.
This locks shared machine-code facts without substituting English resources,
drivers, ABI effects, or frames.

Spanish `2200AD.EXE` has a separately recovered COM startup prefix, not an
English substitute. Its entry preserves `DS=CS` and `ES=CS`, then jumps to
`$d2cd`. The next 70 original bytes are SHA-256
`acbfcacc4cfac948944e42181f2fe0dfec11b9ab2c9b79b8aff79d958c5469c6`: they
set `SS=CS`, `SP=$da00`, prepare `AX=$001f` and `ES:BX=CS:$d1bb`, then call
the wrapped private entry `$0124`. If that original call returns, its AL is
compared with `$01`: the equal route calls `$d1be`, while the other route
calls `$d1d2`. The original word store at `$d14a` is recorded as a code
operand only. Project Eon supplies neither the private return nor AL, takes
neither branch, and creates no Spanish game state or English fallback.

Both resulting local callees are now bounded in the Spanish executable. The
AL-equal target `$d1be..$d1d1` is 20 bytes, SHA-256
`fdfc8f02550ee226dea27b1ac0204d1ead083c9d5585e18103bfe67435f0a5bb`: it
prepares `AX=$0004`, `ES:BX=CS:$d1bc`, calls private `$0124`, then local
`$044e`, and only after both returns stores literal `$01` at `$da05`. The
other target `$d1d2..$d1ed` is 28 bytes, SHA-256
`6b8180c8f3b01e1f8810b2132756486dc761aee980949643129eeb53f6e86472`: it
prepares the same AX/ES:BX pair, calls `$0124` and `$0466`, then reads `$da05`
and compares it with `$02`; only its equal route stores `$b800` at `$0107`.
The two follow-up routines, private return values, and predicates are still
unrecovered. Thus neither branch becomes a presentation or simulation path.

The follow-ups are distinct Spanish byte evidence. `$044e..$0455` is eight
bytes, SHA-256 `38889279a8b89e0e600bb25298015ccd8aadc09ea3858a1790097b3f7ff4ea8f`:
it writes literal `$01` to `$da05` then returns. `$0466..$047c` is 23 bytes,
SHA-256 `b17db26fa4fa8b7307fb767ff98351bd6dcca202829dd2d9348ff4991942d779`.
It initializes `CX` to 16 before a local `LOOP` back edge over the in-image
table `$0456..$0465`
(`00 01 02 03 04 05 06 07 38 39 3a 3b 3c 3d 3e 3f`, SHA-256
`ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a`), makes
`INT $10` request with `AX=$1000`, increments `BL`, and returns. The external
interrupt's register effects are unrecovered, so the initial `CX` value does
not establish a runtime request count. This is a static palette-request trace,
not proof of the startup branch or authorization for a host palette mutation
or screen.

The Spanish FAT12 image also supplies its own `EGA640.BIN` (4,630 bytes,
SHA-256 `ef031b0b6e720ab2dafc1eb6373ddb76e0ff15f7b59ac785265c5136be153daf`)
and `MCGA.BIN` (4,346 bytes, SHA-256
`3fb76b2ccccffc304b0525cd410b940bbb61e3d1a7a90340d72e5683d7f0211d`).
Both are parsed only after these Spanish identities and retain their own
function-$06 and function-$13 dispatch targets. Matching offsets are evidence
of local code structure, not permission to load an English driver or execute
the private ABI.

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

`2200AD.EXE` also contains a hash-locked static request for this exact file:
CALL `$d332 → $101a`, source name `$100d` (`2200AD4.BIN`, 12 bytes,
SHA-256 `91032791cbe9e4cfaa88d2f3d9d4882e58dd66ccfbc8a0c457af21dfcefd63ae`).
The 39-byte loader `$101a..$1040` (SHA-256
`d81719b0293c15ad5edbc5c816feb0c44e78abdde749473e5b5795848e4c86cb`)
uses original DOS open/read/close wrappers. DOS results, the destination
segment/buffer, and all returns remain unmodelled preservation boundaries.

The same data file begins with a verified 435-entry, 16-bit static-text
pointer table ending at `$0365`. It maps to 434 distinct raw records in the
English release (one target is intentionally shared) and is not target-sorted.
Project Eon preserves pointer order and raw record boundaries without assigning
meaning to the native control bytes. The exact cross-edition evidence is in
[the static-text report](generated/millennium-dos-static-text.md).

Five parallel English/Spanish pointer records preserve genuine control-related
**text** without proving a host binding: indices 271 (`left button / space` /
`boton / espacio`), 350 (`press space bar to continue...` / `pulsa espacio para
continuar..`), 390 (`press left button to continue...` / `pulsa el boton
izquierdo para seguir`), 398 (`MOUSE MODE` / `MODO RATON`), and 399
(`KEYBOARD MODE` / `MODO TECLADO`). The English source spans are `$12a7`,
`$1d88`, `$2aef`, `$2bcd`, and `$2be3`; their Spanish counterparts come from
the independently validated FAT12 `2200AD4.BIN`. These literals are available
to inspection as original data only. No caller-connected code proves which
input selects a mode or continues a prompt, so Project Eon does not convert
them into SDL mappings or a reconstructed keyboard reference.

`MillenniumDosControlTextEvidence` accepts the two separately hash-identified
original static-data files, never a translation fallback. For English, indices
271, 350, 390, 398, and 399 select records at `$12a7`, `$1d88`, `$2aef`,
`$2bcd`, and `$2be3`; their raw-record SHA-256 values are
`4ff26c46bfaba03c12a1a29271499c81d044ce2cccc8db06ad3e07535ad5445c`,
`ab5a128110d288c166213ef0e64b8593d1945ab8e9624363c573fe8ef942f818`,
`b0676d538a2ef6b07cdf467bb10a4dbea34af96fccafc90180a01825935c1d4f`,
`220c3cd2cb86c2353f8f9320e6ec7c469007e4bd31e11dce52c847f8c510c5cc`, and
`0951952248daef3634e418d0bed0cfa2ea8cd58f7975ee5e77880c54ad731f2d`.

For the supplied Spanish FAT12 `2200AD4.BIN` (13,254 bytes, SHA-256
`8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31`), the
same pointer indices select that edition's raw records at `$1351`, `$1f41`,
`$2d99`, `$2e98`, and `$2eae`. Their SHA-256 values are
`1644bb8d9ecb1e41a50804e6966a9f91e99433968d6ce690aa1e8aaad79e00c1`,
`5d3b18d963f840dce41371210411578f218add619a52c39e49b382db2bb7f0b6`,
`6af12fa55735c3a6a6a986af3242472e47c01480d2b3e180c21e1996df04cdfd`,
`cc2cbde218ba9d86e805bf2247d6acf13a15019d28a840fdb67345be5efb28c2`, and
`e9d50a0d17dd4d11a008b88ccedb3f8b60dd6bdb3ec126ef8e28199b96f143d0`.
The parser returns only those exact supplied printable substrings while
retaining each raw record's native prefix and boundary as provenance. The
original static-data loader establishes that the file is requested, but its
runtime destination and any input-dispatch caller remain unrecovered; these
texts are therefore not host controls.

### Millennium DOS GX canvas

The first two `GX.LIB` entries establish a separate authentic bitmap path.
`IMG00` is a codec-2 240×33 resource containing a 256-entry RGB6 DAC after
its stream; `IMG01` is a codec-2 320×167 indexed canvas. Its 68-byte
post-stream index table selects entries from the `IMG00` DAC. Project Eon
decodes this pair in memory and retains the remaining resource-table bytes as
opaque rather than inventing UI or state meaning. Exact offsets, sizes and
pixel hashes are in [the GX canvas evidence](generated/millennium-dos-gx-canvas.md).
GX and `2200SAVE.I` are inspection-only preservation evidence. The verified
`TITLES.EXE` poll establishes neither a process exit nor a DOS return to
`MILL.COM`, let alone `2200ad.exe` startup, GX selection, or save-state
initialization. The SDL launcher therefore keeps the original P00 title frame
at this boundary and neither draws the GX canvas nor opens a save panel.

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
on Linux, macOS, and Windows. It handles no releases, tags, or publication. It
uploads non-published verification artifacts (packages and platform builds) for
CI inspection only. Releases require an explicit maintainer request outside CI;
normal development is pushed directly to `main`.

All action invocations are pinned to full Git object IDs, with the reviewed
release label preserved in a comment; the same immutable-reference rule already
applies to SDL3, zlib, and libpng source fetches. Each platform upload also
contains an adjacent schema-1 JSON integrity manifest. The manifest is generated
after that platform's artifact validation and records the source commit, artifact
basename, byte length, and SHA-256. It is a transport-verification ledger only:
it neither signs nor publishes a release, includes no workspace path, and never
opens or embeds user-supplied game media.

Before upload, CI validates each generated ledger independently with
`packaging/verify-artifact-manifest.py`. The verifier accepts only the exact
schema, a full lower-case source revision, uniquely sorted safe basenames,
non-symlink regular artifacts, exact byte lengths, and SHA-256 values. Its CI
mode also rejects unrecorded entries in the upload directory. This makes
the ledger a checked boundary rather than a write-only claim; the same command
can verify a downloaded artifact directory without unpacking it.

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

## Reference trace admission

A genuine execution trace is admitted only as external, hash-addressed
preservation evidence. It must bind a manifest and event stream to one exact
scanner-recognised outer release, including game, platform, language, byte
size and SHA-256; a similarly named archive or another platform is rejected.
Project Eon validates that provenance and event ordering only, then reports it
without replaying an event or creating a platform return value. The full
bounded grammar, required capture hashes and rejection rules are in
[REFERENCE_TRACE_FORMAT.md](REFERENCE_TRACE_FORMAT.md). Trace artefacts,
emulator snapshots, ROMs and all game media remain user-owned and excluded
from Git and packages.

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

An explicit CLI platform is never a request to substitute another release's
runtime. Therefore `--game deuteros --platform amiga` may run this ADF-backed
opening, while `--platform atari-st` remains a verified protected-media boot
report boundary. The latter has no recovered presentation/runtime chain, so the
SDL view deliberately does not load Amiga art, audio, or generated Atari state.
The runtime also does not create its default data directory; it reports a
missing path until the user supplies original media there or passes `--data`.
The Windows Inno Setup installer follows the same rule: it installs only
Project Eon and its own runtime resources, and does not pre-create
`<install-directory>\\data`.

At the exact opening event `$0f,$00000b38` (observed at scheduler tick 82 for
the recovered held input route), the live Amiga session now terminates its
opening VM. It preserves the final original-backed compositor frame and opens
only the existing SHA-validated title-stage boundary (ADF `+0x6e000`, length
`0x6ca00`, RAM `$13000`, entry `$40426`). Later host ticks cannot advance the
opening VM, VBL random source, compositor, or PCM mixer; SDL clears queued
preview audio at that edge. The title stage's Exec, graphics-library and custom
hardware requirements remain unexecuted, so this is not a fabricated title
screen or a claim that title-stage timing has begun.

The terminal handoff also executes only the title entry's proven profile-one
prefix in memory. Bootstrap `$12b0e` (ADF `+0x2f0e`, 14 bytes SHA-256
`858d0a08e8d6fe8200fb71a0866731feabffcadc232bfdeff5be669446bae0fd`)
reloads D0 from `$12a34`, which is profile one on the exact `$0b38` route.
The title code then writes `$4040e=1` and `$19d52=1`; its `$40426..$40437`
and `$40448..$4044f` byte spans hash to
`833374022042225f1bfeeedd56c05d7011168531fa121494cef04174453e5387` and
`8d15b73f389c05fc214b9440c0a0b77df33782c6400d455cef96f338aa5f1211`.
The preceding `A1 -> $206a0` transfer remains deliberately unmaterialized:
its controller pointer is unknown. Execution stops at `$40450`, before its
first Exec vector; that 16-byte boundary hash is
`f0c847a4d443e26fc08f6c6864afeca3b33da514f8708f76f2f05314a4c88067`.

The wider hard-ABI span `$40450..$4046b` is 28 bytes at ADF `+0x9b450`
(SHA-256 `24f5fb4f5019bf450f8b6931fe1c77747461704b139bbe14ec079b1008af1f49`).
It installs stack `$40b62`, reads the unknown Exec base from address `$4`,
and calls the conventional `SuperState` and `UserState` vectors at `-$96` and
`-$9c`; no Kickstart image, vector implementation, return value, condition
code, privilege transition, or stack state is present in the supplied disk
media. If both vectors and the following internal calls returned, the static
span `$4046c..$404d7` (108 bytes, SHA-256
`69b1aaefdd169565901ae166f15c1a17487f9bd63b7303748869bc7955c7380f`) would
reach custom-register literal `$dff000` and setup offsets `$40`, `$42`, `$9a`
and `$96`. That conditional post-boundary code is recorded solely as raw
preservation evidence; Project Eon does not cross the Exec ABI or claim a
visual hardware effect.

The conditional static continuation extends through `$40573` / ADF `+0x9b46c`
for 264 bytes (SHA-256
`f96f93da561b1758dd559a93e2fe97b7b6cc11d482dc5de6460c709b8190bc56`) and
stops before `$40574`. It contains direct-call operands for `$1ed80`,
`$1f172`, `$1f182`, `$1ef74`, `$206d4`, `$206be`, `$403e6`, `$403f4`,
`$204c8`, `$389e2`, `$1fb9a`, `$38912`, `$2022a`, `$41bb4`, `$20e18`,
`$20ba8`, and `$37180`, plus an indirect `JSR (A0)` after `LEA $20cfe,A0`.
The original mode-cell comparison at `$4040e` statically selects `$36a8c` for
low word `$0005` and `$1fb9a` otherwise. Calls, returns, cell values, target
semantics, and display effects are unmodelled; this records no executable
title-stage path.

Within that continuation, the first direct wholly local callee that has a
complete, caller-connected byte path is `$403e6`. Its call site is
`$404c2..$404cd` / ADF `+0x9b4c2`, which loads D1 with literal `$00013000`
then executes `JSR $403e6`; its twelve bytes hash to
`a617235dd94a6c0b3f5fb9f9e078652ed8f1e45213e85c80b10ec165a6b7216f`.
The callee prefix `$403e6..$403f1` / ADF `+0x9b3e6` hashes to
`1e1ccdae97d5849873d3d2e785f5a8be585ffa0e104b5c550ecade6bc37a33a2` and
contains `MOVE.L #$1c482,D0` and `MOVE.L D0,$1f97c`; the separately validated
following word at `$403f2` is RTS.
`DeuterosAmigaTitlePostExecPointerSeedProfile` hash-locks both spans and
reports their literal operands. Reaching the call still requires the two Exec
vectors and all earlier original calls to have returned, so Project Eon does
not execute it, materialize D0/D1, write `$1f97c`, or claim startup progress.

The immediately following call at `$404ce..$404d3` / ADF `+0x9b4ce` is a
separate six-byte static edge to `$403f4` (SHA-256
`555513267ef304f2a5cec2303f8565db8e4ed9ecb2abd7bc87b73dbe5d6c0976`). Its
26-byte callee `$403f4..$4040d` / ADF `+0x9b3f4` hashes to
`5353ab8b18d63a51e12ef2f586a68d872981fa491ca13531198f18a2a38edf07` and is
exactly four direct `JSR` operands followed by `RTS`: `$403c8`, `$20510`,
`$1f37a`, and `$40698`. `DeuterosAmigaTitlePostExecServiceBatchProfile`
validates the caller, complete callee, and return address `$4040e`. This does
not establish that any nested call returns, or assign effects to their code;
the profile records only the caller-connected static call batch after the
unexecuted Exec boundary.

The first nested call of that batch is independently bounded at
`$403f4..$403f9` / ADF `+0x9b3f4` (6 bytes, SHA-256
`2a90f1020af64bd1a6f7f6e7e7503bea4133a2a569bba55987f6edb23442cec3`). Its
complete callee `$403c8..$403e5` / ADF `+0x9b3c8` is 30 bytes, SHA-256
`3f9cf2302a4078faddd0796fc05268386d46c4be64f294b8082ba085b9609f5f`:
it assigns `$1ed24` to A1, `$12e12` to A0, literal D0 `$14`, and A6 from
`$12fec`, then executes `JSR -$c0(A6)` and RTS. `DeuterosAmigaTitlePostExecGraphicsVectorProfile`
hash-locks the caller and routine, including return `$403e6`, but does not
call the graphics-library vector, establish its ABI or return, or name any
visual/title effect.

The next batch edge at `$403fa..$403ff` / ADF `+0x9b3fa` hashes to
`f31dc5923e4b39eb1726fc9b05ac7f56c0209f5d60c9499b979ebfc7c08a58a2` and
targets `$20510`. Its complete 38-byte local routine `$20510..$20535` /
ADF `+0x7c510` hashes to
`60ee2fcb4a18f62cd2066aba2429e760a64f14cd3f07f3cfe8467972030008bc`. It is
four straight-line operands followed by RTS: word literals `$0000` and
`$f690` target `$202c4` and `$2027e`, long literal `$00000001` targets
`$20280`, then the word at `$20276` is copied to `$2027c`; the return is
`$20536`. `DeuterosAmigaTitlePostExecStateInitProfile` records this exact
byte provenance. It does not perform those writes or infer their meanings:
reaching this second batch call still requires the preceding graphics vector
and every earlier unresolved original call to return.

The third batch edge at `$40400..$40405` / ADF `+0x9b400` hashes to
`901b0ad5740a3e6aea3eba28b6aadf5ac5c187e961cc848f6f1a882b3592f464` and
targets `$1f37a`. Its primary 18-byte entry `$1f37a..$1f38b` / ADF
`+0x7a37a` hashes to
`58e85705bc821d42834936342b242162c749889b9d9c23c3d5896f7bcf06e4ff`: it
first calls local `$20094`, then—only if that call returns—loads A6 with
literal `$1f372` and tail-jumps to `$201d2`. The independently bounded
`$20094..$200f9` routine / ADF `+0x7b094` is 102 bytes and hashes to
`7427cdaa0f716496e21c5ef0f6a8d0850a9606a9b4ffe6e56df599109b0ca947`.
It clears D0 and calls graphics-library vectors `-$19e`, `-$198`, and
`-$1a4` through A6 loaded from `$12fec`; it records the first result byte at
`$20092`, literal pointer `$1ffe6` at `$2008e`, and three literal words
`$000a/$000a/$000c` at offsets `$0006/$0008/$0004` from `$1ffda`, before RTS
at `$200f8`. `DeuterosAmigaTitlePostExecThirdServiceProfile` hash-locks all
three spans and these operands. It does not call any vector, supply a vector
result, write any title-stage cell, or execute the tail jump. Reaching this
third edge requires the earlier graphics vector, state-init routine, and all
prior original calls to return.

The fourth and final batch edge `$40406..$4040b` / ADF `+0x9b406` is a direct
call to `$40698` and hashes to
`b214a93028755289cb8dcefb5e4013d307dc2e8a4bb27ae2e798a7bf10298606`. Its
complete target is exactly the two-byte `RTS` at `$40698..$40699` / ADF
`+0x9b698`, SHA-256
`1ceeabf0c6a5a30bad12cdac0e3ab015a7188a42e6aebb556aad00bb9cd693ad`.
`DeuterosAmigaTitlePostExecFourthServiceProfile` also validates the enclosing
batch `RTS` at `$4040c`, preserving caller return `$4040c` and batch return
`$4040e` as byte facts. It does not assert that earlier calls return, cross
the preceding Exec boundary, or execute either return.

The third-service dispatcher only reaches its absolute `JMP $201d2` after the
three graphics-library vectors in `$20094` return. The target `$201d2..$2021d`
is a complete 76-byte local dispatch at ADF `+0x7b1d2`, SHA-256
`6947fb7ffcbfaadd0ce420648741b46539f5dce188e4c26ba7fd18351852c658`. It
saves A0/A6, has static BSR operands to `$200fa`, `$20118` (twice), and
`$200dc`, then restores A0 and RTSes at `$2021c`. Those destinations contain
further graphics ABI boundaries, so `DeuterosAmigaTitlePostExecTailDispatchProfile`
records only the exact control-flow bytes and return `$2021e`; it neither
executes a BSR, supplies a vector result, nor infers any title or display
effect.

The first of those BSRs is `$201d6..$201d9` / ADF `+0x7b1d6`, targeting
`$200fa`. Its four-byte operand hashes to
`fd55349ce2476b466426a5addfa7eedae100cddaac5a480512c6eff31a06a450`. The
complete callee `$200fa..$20117` / ADF `+0x7b0fa` is 30 bytes and hashes to
`6e36c860c280c651947ad0ea6ef868759fbc7bfac67d89af219135e4751e6e6f`. It
loads A0/A1 from literals `$12e12`/`$1ffda`, A2 from pointer cell `$2008e`,
and A6 from graphics-library base cell `$12fec`, then calls vector `-$1a4`.
`DeuterosAmigaTitlePostExecTailFirstCalleeProfile` binds those exact bytes,
the vector return `$20116`, local RTS boundary `$20118`, and caller
continuation `$201da`. It does not call the vector, read the pointed-to A2
value, assume a vector return, or infer any graphics effect.

The following BSR in the same dispatcher is `$201fe..$20201` / ADF
`+0x7b1fe`, which hashes to
`8919a0658d9b7a79bca49d3ca3f38227e3ee6a043491ebac0dbb395504b33fd9` and
targets `$20118`. Its complete 168-byte local routine `$20118..$201bf` / ADF
`+0x7b118` hashes to
`9b16e7cdc97495a1b52656d49c7a3612e7e1617ce88996e2c5e7138e3f183ec3`.
It contains two mirrored bounded selection blocks over literal cells
`$1ffc8/$1ffca/$1ffcc` and `$1ffce/$1ffd0/$1ffd2`, then loads A0/A1 from
`$12e12/$1ffda`, applies literal opcodes `$0440 #$0010`, `$5d41`, and
`$e248`, loads A6 from `$12fec`, and calls graphics-library vector `-$1aa`.
`DeuterosAmigaTitlePostExecTailSecondCalleeProfile` records these byte facts,
the vector return `$201ba`, RTS `$201c0`, and caller continuation `$20202`.
It neither supplies input cells, invokes the vector, assumes any ABI return,
nor labels the selection or graphics effect.

The third BSR in that same dispatch is a separate, later call site at
`$20212..$20215` / ADF `+0x7b212`. Its four-byte operand is
`61 00 ff 04` (SHA-256
`a760d59c7213517e7d3427b30915f9c586be5448e40a0a3980f9dded55f9f994`) and
re-enters `$20118`; its caller continuation is `$20216`. The re-entered
168-byte routine is the same hash-locked span already recorded above, ending
at RTS `$201c0`; it must not be mistaken for the earlier call at `$201fe`.
`DeuterosAmigaTitlePostExecTailThirdCalleeProfile` therefore binds this
distinct caller edge to that existing local routine hash. It does not infer
that any preceding graphics vector returns, execute the re-entry, or ascribe
selection/display semantics to the code.

The fourth and final BSR in `$201d2` is `$20216..$20219` / ADF `+0x7b216`:
bytes `61 00 fe c4`, SHA-256
`6b8c80452bd43c82d8ce91fa551b3067dfc33bb85e553d555aaec65ea6a8ce26`, target
`$200dc`, continuation `$2021a`. Its target `$200dc..$200f9` / ADF `+0x7b0dc`
is a separate 30-byte entry with SHA-256
`6e36c860c280c651947ad0ea6ef868759fbc7bfac67d89af219135e4751e6e6f` (the
same bytes as the independently reached `$200fa` wrapper). It loads literal
A0/A1 `$12e12/$1ffda`, A2 from pointer cell `$2008e`, A6 from `$12fec`, calls
graphics-library vector `-$1a4`, then has vector-return `$200f8` and local
RTS boundary `$200fa`. `DeuterosAmigaTitlePostExecTailFourthCalleeProfile`
binds the distinct caller and target spans without collapsing duplicate bytes
into one edge. It does not read pointer cells, invoke the vector, assume that
any BSR/vector returns, or infer a graphics/title effect.

If—and only if—all four tail BSRs and their unresolved vector calls return,
the enclosing `$403f4` batch returns to `$404d4`. The 28-byte continuation
`$404d4..$404ef` / ADF `+0x9b4d4` hashes to
`32a750150f115f5c012e99811313916078a8657c6100b50e92acadca0708965d`.
It sets A0 to literal `$12ff4`, transfers two successive longwords into
`$37ef2` and `$37ef6`, then calls local `$204c8`. The complete local span
`$204c8..$204f9` / ADF `+0x7b4c8` is 50 bytes and hashes to
`76f4163c15e6761168f1d267e3feae94f0430975efa75b1c3576d7b88947e596`.
It loads A1 with `$204aa`, writes literals `$0002`/`$00c4` at offsets
`$0008`/`$0009`, writes long literals `$204c0`/`$202ca` at `$000e`/`$0012`,
loads D0 `$00000005`, then obtains the unknown Exec base from `$4` and calls
vector `-$a8(A6)`. The vector-return instruction is `$204f8`; the local RTS
ends at `$204fa`. `DeuterosAmigaTitlePostExecTailReturnProfile` records only
these caller-connected byte facts. It does not read the table, perform the
writes, invoke or identify the Exec vector, presume any return, or infer a
display/title effect.

Only if that final `-$a8(A6)` vector returns and the wrapper RTS at `$204f8`
unwinds to `$204fa` does its caller continue at `$404f0`. The following
296-byte span `$404f0..$40617` / ADF `+0x9b4f0` has SHA-256
`10a96a2c80f83b32530ed9355cb2988bcac233c49f66d93484b31d0c0e3667c6`.
It has direct absolute-long operands (in instruction order) to `$389e2`,
`$1fb9a`, `$38912`, `$2022a`, `$41bb4` twice, `$20e18`, `$20ba8`, `$37180`,
the `$4040e == 5` alternatives `$36a8c`/`$1fb9a`, `$222c0`, and `$23e4e`.
Its one indirect `JSR (A0)` follows literal A0 `$20cfe`, which is not
dereferenced. The same bounded span has raw timer operands `$40410`/`$ea60`,
inhibit comparison `$22d34 == $11`, and local call `$4069a`; it ends before
the next flag-gated instruction's operand at `$40618` (`$1bf36` is merely
recorded as a cell operand). `DeuterosAmigaTitlePostExecTailReturnContinuationProfile`
binds those bytes without entering calls, providing ABI results, or assigning
game/display semantics.

The next complete instruction starts at `$40616`, so
`DeuterosAmigaTitlePostExecTailFlagGateProfile` deliberately overlaps the
preceding span's final opcode word. Its 94-byte span `$40616..$40673` / ADF
`+0x9b616` hashes to
`fcf7c15552302b6b902352380a5b5d454eba190be2a7e89af9701822eac1f80e`.
It records raw word operands `$1ffce`/`$1ffd4`, comparisons `$00b4`/`$0043`,
the two branches to `$4063a`, absolute jump `$37f56`, calls `$1f3f8` and
`$1f238` (twice), comparison `$1bf36 == $0101`, literals `$00f0`/`$0f00`,
and the word destination `$dff000 + $0180`. Its local loop is `$40658`; the
exit branch has raw target `$40576`. The profile stops before padding at
`$40674`. It never reads cells, takes branches, follows calls/jumps, writes
the custom-chip address, or attributes hardware/game semantics to these
bytes.

The flag gate's first direct call is independently caller-connected at
`$40632..$40637` / ADF `+$9b632`: `JSR $1f3f8`, SHA-256
`c3998d07f8e89408b9332ae19f449256087b1eb8843256751c03e52700cbbec4`.
Its complete local target `$1f3f8..$1f419` / ADF `+$7a3f8` is 34 bytes with
SHA-256 `101f4026b51a3c0bef3758f4244fffd3fe12c93d76e37b44d0728295b5e27aa6`.
It tests byte `$1ee16`; the zero branch reaches RTS `$1f400`. Otherwise it
reads word `$1ffd4`, applies raw immediate byte mask `$03`, and has a BNE
backedge to `$1f402`; a second `$1ffd4` read, one-bit word shift, and BCC
backedge reach terminal RTS `$1f418`. The caller continuation is `$40638`.
`DeuterosAmigaTitlePostExecTailFlagGateFirstCalleeProfile` hash-locks the
caller and every byte of that routine. Project Eon supplies no cell values,
does not enter either polling loop, and does not assume a return.
