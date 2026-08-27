# Millennium DOS save-layout evidence

This note documents only the serialization recovered from the supplied clean
English DOS `2200AD.EXE` (54,391 bytes) and its bundled `2200SAVE.I`
(9,538 bytes, SHA-256
`a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7`).
It is not a claim that every recovered word has a known gameplay meaning.

## File envelope

The executable's load routine begins at loaded address `$c7e5` (file offset
`$c6e5`). It opens one of four 13-byte `2200SAVE.I`-style filename slots,
reads two bytes, and compares the little-endian word with its constant at
loaded `$2fb0`: `$0056`. It subsequently issues fixed reads whose sizes total
`$2542` (9,538) bytes. The supplied initial save starts with the same word and
has exactly that size.

| File range | Length | Native destination / evidence |
| --- | ---: | --- |
| `$0000..$0001` | 2 | Version word `$0056`, checked at `$c80a..$c80f`. |
| `$0002..$0147` | `$146` | Read to `$da02`; size formed as `$db48-$da02`. |
| `$0148..$014b` | 4 | Read to `$93bb`. |
| `$014c..$0b9b` | `$a50` | Read through the far pointer at `$0112`. |
| `$0b9c..$2093` | `$14f8` | Read to `$12cc`; size formed as `$27c4-$12cc`. |
| `$2094..$2096` | 3 | Read to `$2aa2`. |
| `$2097..$20aa` | `$14` | Read to scratch then de-columnized into `$27fc`. |
| `$20ab..$212a` | `$80` | First 38 words feed the `$2aa6` table's `+6` field. |
| `$212b..$21c2` | `$98` | 38 four-byte elements feed the distinct `$5dd2` table. |
| `$21c3..$21fa` | `$38` | Read to `$db1c`. |
| `$21fb..$21fd` | 3 | Read to `$db19`. |
| `$21fe..$245d` | `$260` | Read to `$97c2`. |
| `$245e..$24f5` | `$98` | 38 pairs feed `$2aa6` fields `+0` and `+4`. |
| `$24f6..$2541` | `$4c` | 38 words feed `$2aa6` field `+8`. |

## Recovered 38-entry table

At `$c87c`, `$c8f9`, and `$c913`, the code reconstructs one 38-entry table at
`$2aa6` using a stride of `$1c` bytes. The complete on-disk portion recovered
for each index is therefore columnar:

| File source | Runtime field |
| --- | --- |
| `$20ab + index * 2` | word at `record + $06` |
| `$245e + index * 4` | word at `record + $00` |
| `$2460 + index * 4` | word at `record + $04` |
| `$24f6 + index * 2` | word at `record + $08` |

The `$80` region also contains 52 bytes for four other tables after its first
76 bytes; Project Eon leaves those bytes opaque. Likewise, the count 38 is
compatible with the body labels in `2200AD4.BIN`, but these code fragments do
not by themselves prove a one-to-one label mapping. The parser therefore calls
this a state table and does not attach speculative body or simulation-field
names.

## Read-only session boundary

`MillenniumDosSaveSession` retains a byte-for-byte in-memory view of a
successfully parsed original save. It exposes the checked version, the 38
positional four-word records, a SHA-256 identity, and bounded read-only spans
for opaque ranges. It intentionally has no setters, serialization/export
method, inferred simulation meanings, or save-file creation path. The CLI
verification report prints each recovered record as `+00`, `+04`, `+06`, and
`+08`, matching the executable's recovered runtime offsets rather than naming
them as game concepts.
