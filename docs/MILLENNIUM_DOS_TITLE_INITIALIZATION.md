# Millennium DOS native title initialization

This boundary continues the English DOS native startup after the exact
`TITLES.EXE` child-entry jump. It is manual recompilation of a bounded,
hash-addressed instruction path; it is not an x86 emulator and does not claim
that DOS or the original video driver returned successfully.

The admitted `TITLES.EXE` is exactly 7,022 bytes with SHA-256
`3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6`.
Its 24-byte file span `+$1a80..+$1a97` has SHA-256
`6bb7c15471e42155d44449cf6e814a538f3a0ee686126f7c2befa91cfb0d08d7`.
Starting at loaded `$1b80`, these instructions establish:

- `DS=ES=SS=CS`;
- `SP=$da00`;
- `AX=$0000`; and
- `BX=$1ac4` before the direct call at `$1b95`.

The called wrapper begins at loaded `$0122`. Its seven-byte request prefix
(`1e 56 57 55 06 cd 91`) has SHA-256
`f7dee937ac756b0aa6c9b287ba8dcf985d7a6fe539612de66cd4871184d85680`.
It pushes DS/SI/DI/BP/ES and reaches its `INT $91` opcode at `$0127`. Therefore
the native session can publish the exact known function-$00 request with
`ES:BX=CS:$1ac4`, then stops at the private-interrupt result boundary.

The compatibility child reserves only the original image extent; it does not
establish a DOS PSP, memory-control block, environment, or the storage behind
the original `$da00` stack pointer. The native session consequently records
the instruction-defined register values but does not synthesize x86 stack
words. It also does not assign an interrupt return, BIOS mode, driver write,
title transition, or frame. Those remain separate evidence boundaries.

The production compatibility runner performs this continuation atomically
after loading the exact child leaf and executing its `$0100 -> $1b80` prefix.
The checkpoint retains the compatibility-arena child segment, monotonically
sequenced register effects, exact call/wrapper/interrupt addresses, record
pointer, and explicit `result_observed=false` and
`stack_storage_modeled=false` diagnostics. Reset and release revocation destroy
the owned session.
