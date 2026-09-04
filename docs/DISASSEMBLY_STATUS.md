# Whole-program disassembly status

This is the preservation ledger for Project Eon's whole-program
disassembly. It is deliberately an index of reproducible inputs, address
spaces, and coverage boundaries—not a claim that a linear decoder has proved
all bytes are executable code. Original media is never copied into this
repository; commands below stream a member directly from the user's archive.

The machine-readable companion [disassembly inventory](disassembly-inventory.json)
is additionally cross-checked by the platform-neutral
[complete-disassembly manifest](complete-disassembly-manifest.json). See the
[complete disassembly preservation index](COMPLETE_DISASSEMBLY.md) for its
strict release/image/range coverage contract and reproducible verifier.
has one row for every recognised release. Version 2 additionally records
hash-bound source spans, load addresses, and—where the mapping is known—
absolute entries or explicit stage-/image-relative entry offsets, plus retained
linear-report identities. An explicit unproven entry remains a boundary, not a
license to infer one. Each row can
name only parser profiles and source leaves bound to that exact release and
must retain an explicit unresolved boundary; coverage never upgrades unknown
ABI behaviour to executable code.

Run the repository-only cross-ledger check after changing a release manifest,
recovery map, parity row, or static-span record:

```sh
python3 tools/verify_preservation_ledger.py
```

It validates the exact release universe, parser/recovery one-to-one mapping,
parity vocabulary, same-release source provenance, and every disassembly
segment's bounds. It reads only the four committed JSON ledgers; it does not
open original media, generated reports, captures, or saves.

## External report identity verification

### Locally available direct-media Millennium set

`tools/reproduce_millennium_disassembly.py` renders the largest complete
Millennium images available in the normal `~/.projecteon` layout without
requiring historical aggregate ZIP containers. It finds the Amiga and Atari
carriers by SHA-256 rather than filename and reuses the declared DOS directory
set. A verified 2026-09-04 run produced three external reports: DOS English
52,240 lines (`90d3633bf887f23563d444b43fcc9c94155d34a62203ddf29d485032730d0ce5`),
Amiga resident span 77,467 lines
(`ed9267d4c2238bb3f9c0ab374054d47a5cbb8a11a93814206d36337ab4382be0`),
and Atari PRG TEXT+DATA 17,519 lines
(`0e8143c317c9682743c867a7c5e61d29aa748735c4a302667f251d67a1be03b8`).
These identities differ from aggregate-carrier reports only where the report
header records a different provenance path; source-span hashes remain pinned.

The local set does not contain the recognised Spanish DOS carrier or the
aggregate release containers. `MILL22A.INF` is recognised but its current
analyzer still requires the aggregate/nested carrier form, so its 7,506-byte
candidate remains an explicit reproduction gap. Linear completeness means
every selected source byte was rendered, not that code/data, reachability,
relocation, operating-system returns, or runtime load addresses were proved.

The complete linear candidate reports remain outside the repository because
they mechanically render copyrighted executable bytes. Their committed SHA-256
and line-count identities can nevertheless be checked without opening any
original media:

```sh
python3 tools/verify_disassembly_reports.py \
  --report SPAN_ID=/absolute/path/to/retained-report.md \
  # Repeat --report for every static span id in disassembly-inventory.json.
```

For a normal complete-report set, pass one explicit external directory instead.
The verifier matches files only by the committed SHA-256 plus LF line count;
filenames are not evidence, and one report can satisfy several spans with the
same committed identity:

```sh
python3 tools/verify_disassembly_reports.py \
  --report-directory /home/user/.cache/project-eon-tools/whole-disassembly
```

Every inventory static span must be supplied exactly once. A single retained
report can be named for more than one span where the inventory intentionally
records byte-identical aggregate output. The verifier accepts only bounded,
regular, non-symlink report files outside both `/tmp` and the checkout, then
checks their exact byte hash and LF line count. It creates no files and never
copies report or game-media contents. A successful identity check proves only
that the retained report is the recorded linear listing; it does not establish
reachability, code/data classification, ABI results, or gameplay semantics.

For the recognised project corpus, `tools/reproduce_disassembly_reports.py`
builds the seven report identities and invokes that verifier as one fail-closed
operation. It takes every original container explicitly, writes only fresh
derived reports to one empty external directory, and pins the outer/nested
container, disk, program and range hashes in the underlying analyzers. It does
not discover by filename, extract media, or accept an existing report as new
evidence:

```sh
mkdir /home/user/.cache/project-eon-tools/all-linear-reports
python3 tools/reproduce_disassembly_reports.py \
  --output-directory /home/user/.cache/project-eon-tools/all-linear-reports \
  --millennium-dos-directory /home/user/.projecteon/millennium-return-to-earth-2-2 \
  --millennium-dos-spanish-archive /home/user/Downloads/Millennium-Return-to-Earth_DOS_ES_Floppy-Disk-Image-v201.zip \
  --millennium-amiga-archive /home/user/Downloads/Millennium-Return-to-Earth_Amiga_EN.zip \
  --millennium-atari-archive /home/user/Downloads/Millennium-Return-to-Earth_Atari-ST_EN.zip \
  --deuteros-amiga-disk-archive '/home/user/.projecteon/Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).zip' \
  --deuteros-atari-archive /home/user/Downloads/Deuteros-The-Next-Millennium_Atari-ST_EN.zip
```

For a Deuteros-only preservation pass using the directly recognised archives
in the default data collection, run `tools/reproduce_deuteros_disassembly.py`.
It freshly renders the complete three mapped Amiga loaded spans and the mapped
Atari ST Replicants first stage, then writes an external `index.json` containing
only report hashes, line counts, mapped-byte totals, classification, and exact
remaining gaps. Listings and that derived index stay outside the repository:

```sh
mkdir /home/user/.cache/project-eon-tools/deuteros-full-disassembly
python3 tools/reproduce_deuteros_disassembly.py \
  --output-directory /home/user/.cache/project-eon-tools/deuteros-full-disassembly \
  --amiga-archive '/home/user/.projecteon/Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).zip' \
  --atari-replicants-archive '/home/user/.projecteon/Deuteros (1991)(Activision)(M3)(Disk 1 of 2)[cr Replicants].zip'
```

This is complete byte coverage of every currently proven Deuteros load map,
not proof that every decoded byte is code and not complete recovery of every
stage on every recognised media variant. Atari stages after the first 4.5 KiB
and independently unmapped variant images remain explicit preservation gaps.

## External static control-flow sidecars

`tools/extract_static_control_flow.py` generates a separate, hash-bound JSON
sidecar from explicitly named original archive members or a recognised
installed DOS directory. `--dos-directory` verifies the entire declared
direct-media set, including its lexical set hash, before it reads any requested
member in place; it never reconstructs an archive. Each resulting direct-media
document carries that `direct_media_set_sha256` as a mandatory separate
provenance identity; archive, FAT, and nested-media documents reject that field.
Inspection accepts a direct document only when the current scanned sources
contain a directory with that same set identity and the directory is reopened
and fully rehashed immediately before the binding is accepted. It records only
decoder-recognised direct CALL/JMP/conditional-branch/interrupt/return
candidates with source offsets and declared runtime addresses. The output must
be a new file outside both the checkout and `/tmp`; it contains neither a full
listing nor original bytes. Each edge remains
`static-candidate-unclassified`: it is not a reachability, ABI, input, timing,
or gameplay claim. Indirect transfers and M68000 PC-relative transfers whose
decoder binding does not retain a reliable displacement are omitted rather
than guessed.

For Atari ST PRGs, the dedicated `--atari-prg-archive` mode first verifies the
outer archive, the exact nested release archive, nested FAT12 disk, exact root
PRG, PRG header, and TEXT+DATA range. Its sidecar uses
`image-relative-unrelocated` fields, never a runtime address: GEMDOS relocation
and the load base remain preservation boundaries.

An embedded-release route verifies the carrier archive and named inner release
archive separately before it opens the exact disk member in memory. Its output
records both identities, so a byte-identical disk cannot silently stand in for
another recognised release container.

The same carrier-aware route covers the direct Equinox Atari PRG release; its
control-flow addresses remain image-relative rather than claiming a GEMDOS
load base.

The SDL-free `data/static_control_flow` reader admits this sidecar only as a
bounded metadata document. It accepts the exact set/document v1 schemas,
structurally complete source-provenance fields, lower-case identities, non-overlapping
declared ranges, and edges that fit their range and report the extractor's
correct target scope. Its diagnostic output contains only document/range/
candidate counts, declared byte total, CPU/kind counts, direct-target scope
counts, and the structural function-map cross-check described below. It reads
no game-media path or bytes and exposes no original address as a runtime
dispatch target. In particular, its only accepted classification is
`static-candidate-unclassified`; loading a sidecar does not establish code
classification, reachability, ABI, timing, input, or gameplay behaviour.

For a local diagnostics read, pass the retained file explicitly to the JSON
inspection route:

```sh
project-eon --data /path/to/original-media --inspect-json \
  --static-control-flow-sidecar /absolute/external/sidecar.json
```

The path must resolve to an existing regular non-symlink file outside both the
checkout and `/tmp`; it is capped at 32 MiB and is rechecked after the read.
Textual `--inspect` and all runtime/menu routes reject the option. Project Eon
first rehashes the selected original releases, then accepts every sidecar
document only if its `archive_sha256` (or `carrier_archive_sha256` for an
embedded release) binds one of those release identities.
Its JSON emits only the static-candidate-unclassified aggregate and release
identity binding, retaining no sidecar or original-media bytes.

When a sidecar is admitted this way, the JSON also emits a compact
function-map cross-check for the releases named by that sidecar:
`function_map_direct_range_bindings` and
`function_map_not_declared_by_sidecar`. A direct binding requires the exact
effective release identity, CPU, address space, leaf SHA-256, and a
function-map coordinate inside a sidecar's declared range. A map row retains
both `source_asset_sha256` (the owning original object) and its effective
`source_span_sha256`; where the latter is omitted from the JSON ledger, it is
identical to the source asset. This preserves a narrow function-byte identity
without discarding the broader source-object provenance. It retains no
instruction, edge, sidecar path, or media byte. This is a provenance check
only: neither result classifies bytes as code nor establishes reachability,
load state, ABI, timing, input semantics, or execution.

On 2026-09-01, external sidecars were generated from the exact Millennium DOS
English, Millennium DOS Spanish FAT12, Millennium Amiga Defjam, Deuteros
Amiga English, and Deuteros Atari Replicants release archives. Their
identities are in `disassembly-inventory.json`: Millennium's English
`MILL.COM`, `TITLES.EXE`, `2200AD.EXE`, and `2200GX.EXE` sidecar has 43,060 LF lines; the
Spanish `IBM.COM`, `TITLES.EXE`, and `2200AD.EXE` FAT12 sidecar has 36,096;
the Defjam shared-resident range has 55,248; Deuteros's clean
bootstrap/main/title sidecar has 84,352; and the Replicants first-stage
sidecar has 331. Verify retained local sidecars:

```sh
python3 tools/verify_static_control_flow.py \
  --sidecar millennium-dos-mill-com-linear=/absolute/millennium-dos-en.json \
  --sidecar millennium-dos-titles-exe-linear=/absolute/millennium-dos-en.json \
  --sidecar millennium-dos-2200ad-exe-linear=/absolute/millennium-dos-en.json \
  --sidecar millennium-dos-2200gx-exe-linear=/absolute/millennium-dos-en.json \
  --sidecar millennium-dos-spanish-ibm-com-linear=/absolute/millennium-dos-es.json \
  --sidecar millennium-dos-spanish-titles-exe-linear=/absolute/millennium-dos-es.json \
  --sidecar millennium-dos-spanish-2200ad-exe-linear=/absolute/millennium-dos-es.json \
  --sidecar millennium-amiga-defjam-shared-resident-linear=/absolute/millennium-amiga-defjam.json \
  --sidecar millennium-amiga-defjam-direct-shared-resident-linear=/absolute/millennium-amiga-defjam-direct.json \
  --sidecar deuteros-amiga-clean-loaded-spans=/absolute/deuteros-amiga-clean.json \
  --sidecar deuteros-atari-replicants-first-stage-linear=/absolute/deuteros-atari-replicants.json \
  --sidecar millennium-atari-equinox-milenium-tos-image-relative-linear=/absolute/millennium-atari-equinox.json \
  --sidecar millennium-atari-equinox-direct-milenium-tos-image-relative-linear=/absolute/millennium-atari-equinox-direct.json
```

`tools/verify_function_map_coverage.py` is a separate, optional cross-check
between retained sidecars and `function-map.json`. It reports a **direct range
binding** only when a map row and a sidecar range share the exact effective
release SHA-256, CPU, address space, range SHA-256, and an address inside that
declared range. A row reported as not declared by the supplied sidecars is not
a negative code or reachability result: it can be a separately hash-verified
subroutine, or simply outside the selected bounded evidence. It does not read
game media, list instructions, or supply runtime dispatch data.

```sh
python3 tools/verify_function_map_coverage.py \
  --sidecar /absolute/millennium-dos-en.json \
  --sidecar /absolute/millennium-amiga-defjam.json \
  --json
```

Pass `--require-complete` only when the supplied, independently verified
sidecars are intended to cover every function-map row. It rejects incomplete
input rather than inferring coverage.

## Input identities

| Game | Platform | Container SHA-256 | Principal code/media identity |
| --- | --- | --- | --- |
| Millennium 2.2 | DOS English | `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` | `MILL.COM` `4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e`; `TITLES.EXE` `3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6`; `2200AD.EXE` `427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`; `2200GX.EXE` `093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb` |
| Millennium 2.2 | DOS Spanish | `b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4` | FAT12 `IBM.COM` `84b7d158c770117aeaa07cb5ea2e7ed4a6bcc288d6b352d82569ff4d97b2fda9`; `TITLES.EXE` `02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7`; `2200AD.EXE` `9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6` |
| Millennium 2.2 | Amiga English | `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400` | nested ADF variants; raw-stage base must be proven before code listing |
| Millennium 2.2 | Atari ST English | `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01` | nested ST/STX variants; filesystem/raw-stage chain and load base remain separate evidence tasks |
| Deuteros | Amiga English | `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` | clean Disk 1 `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38`; clean Disk 2 `99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a` |
| Deuteros | Atari ST English | `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653` | protected/cracked disk variants are separately identified; no clean release is silently substituted |

## Architecture and anchored coverage

| Target | CPU / origin | Verified static anchors | What is not yet asserted |
| --- | --- | --- | --- |
| Millennium DOS | 8086, flat COM-style images at `$0100` | `MILL.COM` private interrupt setup and sound-selection routine; `TITLES.EXE` availability-poll exit; `2200AD.EXE` action-poll sites | DOS/driver/child return values, code/data classification beyond recovered paths, game execution |
| Millennium Amiga | Motorola 68000 | bounded ADF loader/raw-stage records | raw-stage invocation results, runtime base and reachable code map |
| Millennium Atari ST | Motorola 68000 | disk structures and bounded bootstrap session | executable chain/load addresses and TOS/XBIOS returns |
| Deuteros Amiga | Motorola 68000 | boot to `$12a4e`; main stage disk `+$5800` to `$20000`, entry `$21734`; title stage disk `+$6e000` to `$13000`, entry `$40426` | Exec/graphics/callback returns, display ownership, title/game input and timing |
| Deuteros Atari ST | Motorola 68000 | Replicants Disk 1 protected boot: disk `+$4ec00` to `$1200`, entry `$9c4`, stage SHA-256 `d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc`; next stage `$70000`; Disk 2 KILLER_BOOT vector route | XBIOS read result, callback dispatch, RAM vector contents, all control/state semantics |

The same range tool has been run against Millennium Amiga's Defjam system
disk (`8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c`):
disk `+0x16400`, length `0x2c000`, maps to `$68000` and hashes to
`d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e`.
This establishes a reproducible full byte-coverage candidate listing of the
shared resident stage; it does not turn its opaque loader handoff, indirect
calls, or unknown return values into executable Eon behaviour.

## Reproduced byte-complete candidate reports

On 2026-08-30 the current byte-complete range tool was run against the exact
user-supplied archive members below. Each report remains outside the repository
because it is a mechanical rendering of copyrighted executable bytes. The
report digest makes a retained local copy auditable without turning it into a
repository artifact. A report is a linear candidate listing only; no row
proves reachability, code/data classification, an ABI result, or gameplay.

| Target | Exact source span and runtime address | Source archive SHA-256 | Generated report SHA-256 | Lines |
| --- | --- | --- | --- | ---: |
| Millennium DOS English | `MILL.COM`, `TITLES.EXE`, `2200AD.EXE`, and `2200GX.EXE`; flat 8086 candidate origins `$0100` | `e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123` | `90d3633bf887f23563d444b43fcc9c94155d34a62203ddf29d485032730d0ce5` | 52,240 |
| Millennium DOS Spanish | FAT12 `IBM.COM`, `TITLES.EXE`, and `2200AD.EXE`; flat 8086 candidate origins `$0100` | `b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4` | `9d9834ecf9acc62877e4d757d1c0ba1b87d9045fa7f918238f7d8d00171bfd61` | 29,513 |
| Millennium Amiga English Defjam | system ADF `+0x16400`, length `0x2c000` → `$68000` | `2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400` | `c4eebe04d160ae4fd380cba8906ff7c679cd86978fbfe52d66b24fef1290c66f` | 77,467 |
| Millennium Amiga English Defjam direct container | byte-identical shared-resident ADF range `+0x16400`, length `0x2c000` → `$68000` | `ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd` | `c4eebe04d160ae4fd380cba8906ff7c679cd86978fbfe52d66b24fef1290c66f` | 77,467 |
| Millennium Atari ST English Equinox | `MILENIUM.TOS` PRG TEXT+DATA, file `+0x1c`, 49,010 bytes, **image-relative only** | `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01` | `8c4acf574f52890a407f881e44bf41f4bb51ae5ccc7afd6ad240018bb30cc548` | 17,519 |
| Millennium Atari ST English Equinox direct container | byte-identical `MILENIUM.TOS` PRG TEXT+DATA, file `+0x1c`, 49,010 bytes, **image-relative only** | `0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69` | `8c4acf574f52890a407f881e44bf41f4bb51ae5ccc7afd6ad240018bb30cc548` | 17,519 |
| Millennium Atari ST English Equinox | `MILL22A.INF`, file `+0x0`, 7,506 bytes, **image-relative only** | `ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01` | `f8c6e335c1ebb7eb985bccd6baf1c3a106eeb0eca51ec3e6497e1f2efe89b420` | 2,495 |
| Deuteros Amiga English clean Disk 1 | boot/bootstrap/main/title loaded spans; M68000 origins from the validated load plan | `f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04` | `db4379bb4f50cb18f9ef72fdc1066796d5a8621a798e519d730f5282610c1791` | 162,970 |
| Deuteros Atari ST English Replicants Disk 1 | raw disk `+0x4ec00`, length `0x1200` → `$1200` | `c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653` | `4c4bd8add9873e1ab2a52ba0d23a9c225005a7a6d2ecb7435f24191f32b88c35` | 1,979 |

These reports cover source spans with an independently established CPU. Most
also have a runtime load address. The Millennium Atari ST PRG is the explicit
exception: its complete TEXT+DATA listing uses image-relative offsets because
the original GEMDOS relocation and actual load base are not observed. It must
never be read as a runtime address map. Millennium Amiga's opaque transformed
raw stage remains intentionally outside this table until a loader result
establishes its runtime image and entry relationship.

On 2026-09-01, `tools/verify_disassembly_reports.py` accepted the complete
current set: 14 static spans represented by seven regenerated external
reports. The current Millennium DOS English report has a new identity because
the report generator now records each hash-bound member's entry boundary; it
does not alter the source bytes, recovery claims, or coverage classification.
The check used one explicit external directory, so it neither copied reports
into this checkout nor required them to share a repository path. This verifies
report identity only; it does not change any entry, reachability, code/data,
ABI, or gameplay boundary recorded above.

### 2026-09-03 whole-program re-verification

The retained external report directory
`/home/yeager/.cache/project-eon-tools/whole-disassembly-20260901-02` was
re-verified against the committed v2 inventory. The verifier accepted all
**14 static spans** represented by **7 unique reports**. The corresponding
retained static-control-flow set was also verified: **13 control-flow spans**
represented by **8 unique sidecars**. The remaining inventory span is the
Millennium Atari `MILL22A.INF` data/config listing; it has a complete linear
report but deliberately no control-flow sidecar because its execution entry
is unproven.

This is the complete currently identifiable static disassembly corpus for all
recognised releases and their established load/image spans. It is still not a
semantic disassembly: unresolved loader results, external ABI calls,
indirect-control-flow targets, code/data reachability, timing, input and game
rules remain explicit boundaries until independently evidenced.

## Variant separation

The following identities are recognised forensic inputs, not interchangeable
program releases. A static range is admitted only for the exact image listed
by its parser or trace contract.

| Deuteros Amiga image | SHA-256 | Separation rule |
| --- | --- | --- |
| Clean Disk 1 | `6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38` | primary executable baseline (`DOS\0`) |
| Black Monks Disk 1 | `d0f79fe5b65f1ffa3598c1f999afbd87234649fedd03907bc0cf622d19ae0031` | altered boot/main/title |
| Black Monks `[a]` Disk 1 | `e858490c483ed68447ac06c943d274829d484148ab45907b1100428cc343476a` | separate altered boot/main/title identity |
| SKR Disk 1 | `db205cd1d8cefcd9f61ed8cb4169e6fc7169b41abcc50a1331db12a14994e9f1` | clean main-stage bytes do not admit its changed boot/title |
| `[a2]` Disk 1 | `70489505484e3bfd3a37b61d77493cae8fd4a779ab61825f50ad1e5d6641ed12` | modified boot/title, separate title identity |
| `[a]` Disk 1 | `0a655c33d691d6420332abb1ac6ca00df48facbc02946a64ac2490f584a70c39` | modified boot/title, separate title identity |
| Clean Disk 2 | `99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a` | `DEU\0` custom data media |
| `[a2]` Disk 2 | `5a74a97232369ae6c459ff125516ad8d4389020eb862cf49db99593b9f3460ff` | matching observed layout is not identity equivalence |
| `[a]` Disk 2 | `8d0b5f3e9b330551f608977f7b890f93f6f68748190a081392a3ac64e76f96bd` | matching observed layout is not identity equivalence |
| save disks | `19dcb795782ef2b34cd4aec7ff8f0f0abfb939093a35d597d7d01d6783b2e11a`, `e59a67eec0ad1c464df78ccf9af6b390e5f3c2f04cf26246d507475e1464d520` | `MSCF` nonboot media; never boot/disassembly candidates |

| Deuteros Atari ST image | SHA-256 | Separation rule |
| --- | --- | --- |
| Replicants Disk 1 | `aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee` | admitted first-stage profile only |
| Replicants `[a2]` Disk 1 | `a49f509b33ac3d5e59ce496999f28883014c0242c66cfc5e817dd07eb5e65fc8` | shared stage hashes; boot remains separate |
| Replicants `[a]` Disk 1 | `5def9740c010decd32ac263cf933698a8371c2f542fbceab73abfad44576714f` | shared stage hashes; boot remains separate |
| Elite Disk 1 | `5aabe878d05a5ddbead3ddf17c7d4dc5610ce396c33af28f60fa8ecc97d42b1e` | distinct boot identity |
| Elite Falcon Disk 1 | `a601e40ee5da3e0abe2114eab4cd51e51fa81d96091abbd6557986299500cffe` | distinct boot identity |
| unnamed Disk 2 | `5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193` | KILLER_BOOT vector-copy profile |
| Replicants Disk 2 | `e882651f4c0100773bdb5832b6cb80d8eeb5397fba4be5907e7f908341e8f834` | same observed protection bytes, distinct image identity |
| Replicants `[a2]` Disk 2 | `690c6558a4f6b954cdea3ef91197241af59d154d9eac9df76fde9cd62fa26e05` | unsupported nonstandard geometry |
| Replicants `[a]` Disk 2 | `91e296ddbc005f756d05786a58660e0e4f8feee22016157fa3c751601bdf5fa7` | distinct boot identity |
| Elite Disk 2 | `6d450e8af1f961b63dd858ef690e4c8e85040983862ea037e7fc98307fa8baab` | distinct boot identity |
| Elite Falcon Disk 2 | `ef46cb3692ff1a4bbbd4f87fb686faa2e307f4f514f41abd0f6f836372654182` | unsupported nonstandard geometry |

## Reproduction

For a complete *linear candidate listing* of an identified DOS image, use a
direct pipe. This writes no extracted game file and the result must remain
outside the repository unless it contains only reviewed, minimal evidence.

```sh
unzip -p /path/to/Millennium-Return-to-Earth_DOS_EN.zip \
  'millennium-return-to-earth-2-2/MILL.COM' |
  ndisasm -b 16 -o 0x100 -
```

For the four known Millennium DOS program images in one reproducible report,
use `tools/analyze_dos.py` with either an exact outer-archive SHA-256 or the
declared installed-directory set SHA-256, then repeat each exact `--member`
and matching `--member-sha256 path=hash`. Add `--complete-linear` only when a
complete byte-coverage candidate listing is needed. It rehashes the complete
selected source and every requested member in memory, and rejects a
missing/ambiguous member rather than scanning for a convenient substitute.
The complete-linear mode labels every decoded byte range as
code/data-unclassified; a byte that Capstone cannot decode is retained as an
explicit `.byte` record. Reports must be new external files outside the
checkout and `/tmp`. It is not a semantic control-flow claim.

The same tool accepts `--fat12-archive` and `--fat12-member` for a DOS floppy
image, and then reads only each explicitly named root file in memory. This
keeps Spanish FAT12 programs separate from the English ZIP members:

```sh
python3 tools/analyze_dos.py --fat12-archive /path/to/Millennium-Return-to-Earth_DOS_ES_Floppy-Disk-Image-v201.zip \
  --fat12-archive-sha256 <outer-sha256> --fat12-member MRTE.IMG --fat12-member-sha256 <image-sha256> \
  --member IBM.COM --member-sha256 IBM.COM=<sha256> --member TITLES.EXE --member-sha256 TITLES.EXE=<sha256> \
  --member 2200AD.EXE --member-sha256 2200AD.EXE=<sha256> \
  --complete-linear --output /home/user/.cache/project-eon-tools/millennium-spanish-dos.md
```

For 68000 media, Project Eon does **not** make a temporary extracted stage just
to satisfy an external disassembler. The range is instead addressed through
the in-memory, bounded ADF/ST parsers and decoded only after its source span
and relocation are established. Generic ADF/ST linear decoding would mislabel
resources, compressed data and sectors as instructions. The existing generated
reports are navigation aids: `docs/generated/dos-millennium.md`,
`docs/generated/deuteros-amiga-boot.md`, and
`docs/generated/deuteros-atari-protected-boot.md`.

`tools/analyze_m68k.py` accepts either a direct hash-verified ADF or an exact
direct/nested ZIP source. ZIP analysis pins outer, optional nested, and ADF
member SHA-256 identities before it reads the ADF only into process memory.
This is the reproducible route for the clean Deuteros disk and deliberately
rejects an ambiguous archive/member selection; a nearby crack or save image
cannot become an accidental analysis input. Reports must be new external
files outside the checkout and `/tmp`. Its `--complete-linear` mode covers each
byte-identified loaded boot/bootstrap/main/title range and labels every result
code/data-unclassified until a caller-connected map or trace proves otherwise.
Undecodable bytes remain explicit `.byte` records, so a decoder stop cannot
silently reduce byte coverage.

On 2026-09-03 the clean, direct Deuteros Amiga Disk 1 container was read under
that contract. The retained external complete-loaded-spans report has SHA-256
`6c3390c40d8c5127fe5644845d6de2f50245b36c50c4de95abbd5e1f61f3bf9e` and
162970 LF lines. Its exact identity is recorded in
`disassembly-inventory.json`; the report itself remains outside Git. This is
linear candidate coverage only and does not prove Exec/graphics returns,
display ownership, title input, timing, or gameplay.

For a FAT12 Atari ST PRG whose GEMDOS load base remains unknown,
`tools/analyze_atari_st_prg.py` reads either a direct release ZIP or an exact
nested disk member and its named root-file entry in memory. It first
authenticates the complete selected outer ZIP and, for carrier media, the
complete nested ZIP before it reads the named disk. It then requires both disk
and PRG hashes and emits a full TEXT+DATA report with **image-relative**
offsets. It does not apply PRG relocations or convert those offsets to runtime
addresses. The report destination must be a new absolute regular path outside
both the checkout and `/tmp`:

```sh
python3 tools/analyze_atari_st_prg.py \
  --archive /path/to/Millennium-Return-to-Earth_Atari-ST_EN.zip \
  --archive-sha256 ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01 \
  --nested-member 'Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].zip' \
  --nested-sha256 0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69 \
  --disk-member 'Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].st' \
  --disk-sha256 3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7 \
  --program-sha256 4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686 \
  --output /home/user/.cache/project-eon-tools/millennium-atari-prg.md
```

For a directly supplied Equinox release ZIP, omit both `--nested-member` and
`--nested-sha256`; `--archive-sha256` then binds that direct ZIP itself. A
direct and a carrier-contained release remain different source identities even
when their named `.st` and PRG hashes match.

`tools/analyze_atari_st_config.py` applies the same complete-container rule to
the `MILL22A.INF` candidate listing: it pins the outer carrier ZIP, named
nested release ZIP, disk image and FAT12 file before it reads a byte. This
keeps a matching configuration file from being detached from its release
container. The report remains file-image-relative and has no inferred runtime
entry or GEMDOS load address:

```sh
python3 tools/analyze_atari_st_config.py \
  --archive /path/to/Millennium-Return-to-Earth_Atari-ST_EN.zip \
  --archive-sha256 ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01 \
  --nested-member 'Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].zip' \
  --nested-sha256 0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69 \
  --disk-member 'Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].st' \
  --disk-sha256 3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7 \
  --file-sha256 74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6 \
  --output /home/user/.cache/project-eon-tools/millennium-atari-config.md
```

For an Atari ST raw stage (or any other range with an independently proven
disk-to-RAM mapping), `tools/disassemble_m68k_range.py` requires exact outer
ZIP, disk-member, and interval SHA-256 identities. It supports either a direct
disk ZIP or an explicit nested ZIP with a separately pinned nested hash; it
never searches member names or substitutes one layout for the other. It
refuses an outer, nested, disk, or range hash mismatch. Reports can only be
new absolute files outside the checkout and `/tmp`. This preserves media
provenance while permitting direct in-memory full-range inspection. Its
full-range listing is byte-complete by the same explicit `.byte` rule.

## Completion rule

“Complete disassembly” for any release means every reachable executable range
has a hash, source offset, runtime address, architecture, code/data decision,
successor/unknown edge, and reference to a test or trace. A decoder listing
alone is not complete: it becomes a verified code map only after callers,
relocation and external ABI outcomes have been established. Unknown edges stay
in the recovery map and block gameplay exposure rather than being filled in
with emulator guesses.
