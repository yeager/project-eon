# Millennium DOS static-text pointer evidence

Source media: supplied English `2200AD4.BIN` (12,494 bytes, SHA-256
`1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d`) and
the supplied Spanish floppy's `2200AD4.BIN` (13,254 bytes).

Both editions begin with 435 little-endian 16-bit offsets in bytes `$0000`
through `$0365`. The first pointer is `$0366`, exactly the first byte following
the table. Every pointer targets a raw record later in that same file.

The English table has 434 distinct targets: entries 251 and 252 both target
`$0ff1`. Pointer-table order is significant and is not target-sorted: entries
401 and 402 target `$2c1f` and `$2c0c`, respectively. Project Eon therefore
retains both the original 435-pointer sequence and the 434 deduplicated raw
record extents, bounded by the next distinct target (or EOF for the final
record).

Examples anchored in original bytes:

- Pointer 2 targets `$036a`, whose raw record begins with native control bytes
  followed by `OUTER SYSTEM.` in English / `SISTEMA EXTERIOR.` in Spanish.
- Pointer 6 targets `$03d2` in English, the existing `Inner System` label
  record (Spanish pointer 6 targets its edition-specific `$03db`).

This is preservation evidence for the static-data topology only. The control
bytes and each pointer's gameplay/UI role are deliberately not interpreted;
the runtime reads these bytes in memory and never writes, unpacks, or replaces
the supplied game data.
