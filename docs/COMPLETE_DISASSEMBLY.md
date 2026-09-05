# Complete disassembly preservation index

Project Eon's complete-disassembly manifest enumerates every executable or
code image currently identified for all eight recognised release identities.
It covers 19 hash-bound images, 21 non-overlapping source ranges, and 764,867
original bytes. “Complete” here means complete mechanical byte coverage of
those declared ranges. It does not mean that code/data classification,
reachability, relocations, operating-system calls, or gameplay semantics are
proved.

The machine-readable authority is
[`complete-disassembly-manifest.json`](complete-disassembly-manifest.json).
Every image binds its release SHA-256, source-image SHA-256, architecture,
Capstone major-version range, address basis, and exact source ranges. The
verifier cross-checks it against both `release-manifest.json` and
`disassembly-inventory.json`; missing releases or images, changed hashes,
unsupported address bases, gaps, and overlaps fail closed.

`tools/generate_disassembly_candidate_inventory.py` derives a second,
metadata-only view from the release manifest's parser profiles named by the
disassembly inventory. It classifies only whether each already-declared
candidate range is connected to a mapped static span. The current ledger has
13 exactly mapped parser candidates and seven `discovered-unmapped`
candidates. Those seven profile IDs and candidate kinds are committed per
release in the complete manifest, so a
missing load map cannot disappear merely because no listing was produced.
This status is not a new code/data or reachability classification.

The generated v3 candidate diagnostic carries the release identity
(`game`/`platform`/`language` and release hash), exact source-image hash and
interval, related mapped image IDs, address basis, structural candidate kind,
conservative code/data classification, coverage claim, and evidence level.
Consequently a container member can be byte-complete without upgrading its
carrier or runtime load map.

A candidate is mapped only when ranges carrying the same source SHA-256 cover
its complete source interval. An embedded file therefore never maps its
whole-disk carrier. Such a carrier is reported as
`container-with-mapped-members`; 512-byte candidates are `boot`, and other
exact candidate ranges are `raw-stage`. These labels describe ledger
structure, not execution proof.

Boot-sector listings use the `disk-relative` address basis. A complete
512-byte listing proves byte coverage at an offset in the original disk image,
not a runtime address, transfer of control, or load map. Both the span's entry
and the manifest image's `load_status` must therefore remain `unproven`; the
verifier rejects any attempt to upgrade either field merely because all bytes
were decoded.

```sh
python3 tools/verify_complete_disassembly.py
python3 tools/verify_complete_disassembly.py --index
python3 tools/generate_disassembly_candidate_inventory.py
```

The second command produces the English per-release preservation index. Raw
listings contain mechanically rendered commercial bytes and therefore remain
outside Git. Their hashes and line counts continue to be verified separately
by `tools/verify_disassembly_reports.py`.

## Address bases

- DOS members use a linear 8086 candidate listing based at `$0100`; this is
  not an MZ relocation or runtime reachability claim.
- Proven Amiga and Deuteros Atari loaded spans use absolute recovered runtime
  addresses.
- Millennium Atari PRG and configuration images remain image-relative and
  unrelocated; the configuration entry is still unproved.

## Explicit boundaries

The seven current unmapped candidates are intentional and independently
auditable:

- Spanish DOS startup: its 512-byte disk prefix is discovered, but neither a
  boot execution address nor transfer of control is proven.
- Spanish DOS title boundary: the whole FAT12 disk is only a container; its
  three extracted executable members are mapped separately.
- Nested Defjam Amiga bootstrap: the exact 1,024-byte range is known, but the
  nested carrier lacks its own reproducible report identity/load admission;
  the direct-ADF report is not silently reused.
- Nested Equinox Atari boot: exact boot bytes are discovered, but the nested
  carrier has no independently bound disk-relative report/load proof.
- Nested Equinox Atari PRG chain: the disk is a container whose PRG/config
  members are separately image-relative; that does not map the disk.
- Direct Equinox Atari PRG chain: same container/member distinction for the
  directly supplied disk.
- Direct Equinox Atari config chain: a second parser ledger over that whole
  disk records discovery of the config member, not a whole-disk load map.

None is safe to connect to runtime: the strongest byte-identical candidates
still lack carrier-specific report identity or an executable load/entry
relation. Reusing a sibling release's report would create the false
“complete” claim this ledger is designed to prevent.

Only independently mapped executable/code images are enumerated as images.
Discovered parser candidates remain explicitly listed as unmapped. Other Amiga
disk variants, execution after the mapped Deuteros Atari second stage, and
non-Replicants Atari variants
remain preservation boundaries until exact image identities and load maps are
proved. The verifier never fills such gaps with bytes from another release,
platform, language, or synthetic source.
