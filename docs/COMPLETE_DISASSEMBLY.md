# Complete disassembly preservation index

Project Eon's complete-disassembly manifest enumerates every executable or
code image currently identified for all eight recognised release identities.
It covers 14 hash-bound images, 16 non-overlapping source ranges, and 1,110,705
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

```sh
python3 tools/verify_complete_disassembly.py
python3 tools/verify_complete_disassembly.py --index
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

Only independently mapped executable/code images are enumerated. Other Amiga
disk variants, later Deuteros Atari stages, and non-Replicants Atari variants
remain preservation boundaries until exact image identities and load maps are
proved. The verifier never fills such gaps with bytes from another release,
platform, language, or synthetic source.
