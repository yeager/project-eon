# Native Atari ST PRG load boundary

## Recovery-gap decision

The largest isolated native execution prerequisite found in the 2026-09-04
recovery audit was Millennium Atari ST's complete `MILENIUM.TOS` image. The
other active paths had already crossed smaller startup transfers, while this
49,010-byte TEXT+DATA program still existed only as an image-relative linear
listing. Project Eon parsed its relocation table but did not materialize the
program state that every later native instruction depends on.

The source contract is:

| Property | Exact value |
| --- | --- |
| Equinox disk SHA-256 | `3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7` |
| `MILENIUM.TOS` SHA-256 | `4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686` |
| TEXT / DATA / BSS | 4,446 / 44,564 / 81,382 bytes |
| Loadable source SHA-256 | `57017c09dd58c608d713fa3ad44af48ef1e07c1ac90caf303e6f17179719b3c0` |
| Relocation count / range | 227 / image `+$0006..+$1150` |
| First relocation | `[$0006] $0000115e -> $0001115e` |
| Last relocation | `[$1150] $000139c8 -> $000239c8` |

`MillenniumAtariPrgLoadSession` now reparses and rehashes the immutable PRG at
the consumption boundary. It copies TEXT+DATA into owned memory, appends the
loader-defined zero BSS, and applies all 227 big-endian relocation longwords.
The complete 130,392-byte native image at Eon's explicit `$00010000` base has
SHA-256 `92eac35edb2b5db721dd5353cfc3260dfb5fb4120026b76788659aaa342f887c`.
Every relocation records its image offset, runtime address, source value, and
result value. Overflow, metadata disagreement, changed relocation source
words, and any result outside the bounded 24-bit address space are rejected
before the image is admitted.

The `$00010000` base belongs to Eon's native address map. It is deliberately
not presented as the address selected by an original TOS machine. This is a
native loader operation, not 68000 emulation and not a claim of instruction
reachability.

## Remaining boundary

The materialized image makes the whole PRG available to future native
translation, but execution still stops at the existing verified local
bootstrap/GEMDOS boundary. The next high-impact Atari task is to connect the
relocated entry and its proven copy effects to `NativeRuntimeMemory`, then
replace the first `Fopen`/`Fread` dependency with a narrowly scoped,
read-only compatibility service. TOS basepage fields, XBIOS results, Line-A
state, input, timing, and every unclassified indirect target remain explicit
preservation boundaries.

No original bytes are written to disk, copied into a package, or committed.
The structural unit fixture checks loader arithmetic only; the canonical
native corpus test constructs `MillenniumAtariBootstrapSession` from the real,
hash-identified supplied disk and therefore also enforces this exact native
image checkpoint.
