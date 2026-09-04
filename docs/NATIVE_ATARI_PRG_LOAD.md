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

## Atomic runtime ownership

The admitted image is converted to one ordered `NativeRuntimeEffectBatch` and
applied to a temporary 24-bit `NativeRuntimeMemory`. A release acquisition
publishes that memory only after the complete batch succeeds. The batch ID
contains the Atari session generation; duplicate IDs, reordered effects,
partial images, changed image digests, overflow, or an address outside the
native map fail before publication. Returning to the launcher revokes the
coordinator's whole memory object during `RuntimeHost`'s source-revocation
interval, so no image or derived buffer survives into a later media identity.
The UI-facing snapshot contains only sizes, digests, addresses, and the first
and last relocation facts. It never copies the executable image or complete
configuration payload across the renderer boundary.

## Narrow read-only GEMDOS replacement

The exact local bootstrap requests `Fopen("MILL22A.inf", 2)` and then prepares
`Fread(handle, 0x20000, $2a500)`. Mode 2 is write-capable in original GEMDOS,
but Project Eon never opens supplied media for writing. For this one proven
chain, `MillenniumAtariReadOnlyGemdosSession` instead owns a private handle to
an immutable FAT12 snapshot of the exact requested file:

| Property | Native compatibility result |
| --- | --- |
| Requested file | `MILL22A.inf` |
| Exact source SHA-256 | `74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6` |
| Requested / returned bytes | 131,072 / 7,506 |
| Destination | `$2a500` |
| Source access | read-only FAT-chain snapshot |
| Source mutation | never |
| Config callsite / target | `$7703c` / `$2a500` |

The 7,506 bytes form a second atomic runtime-memory batch. Their destination
overlaps the PRG's zeroed BSS by design; publication replaces precisely those
bytes while retaining every other initialized byte. A stale generation cannot
revoke the session, exact revocation clears its payload ownership, and a
revoked session cannot create another Fread batch.

The compatibility handle and successful byte count are Eon-owned native
service results justified by the exact present FAT entry. They are not claimed
as captured TOS register values. No general path lookup, create, write, seek,
close, directory service, basepage, error mapping, or additional GEMDOS
selector is implemented.

## Config consumer entry

The caller-connected instruction at `$7703c` is `JSR $2a500`. With the exact
Fread batch present in native memory, `$2a500` contains the file's original
`JMP $2aa88`. `MillenniumAtariConfigConsumerSession` executes those two local
control transfers and records `$77042` as the encoded JSR return address. It
does not synthesize an A7 value or write that return address into an invented
stack.

The initial checkpoint stops before the first instruction at `$2aa88`:

| Property | Exact evidence |
| --- | --- |
| Mapped file offset | `+$588` |
| 34-byte prelude SHA-256 | `dede20eddbd8015da1d1a4f2f5e53424c2bc2195bff238d830ea24c9f522ea59` |
| Boundary opcode | `$40c0` (`MOVE SR,D0`) |
| Unresolved input | original 68000 privilege/status register value |
| Local control transfers completed | 2 (`JSR`, absolute `JMP`) |
| Status reads / hardware writes completed | 0 / 0 |

Advancement requires `MillenniumAtariStatusRegisterObservation`: generation,
monotonic sequence, exact `$2aa88` PC, the complete observed SR word, and an
independent typed `user`/`supervisor` classification. The value's S bit must
agree with that classification. A mismatch or stale observation is rejected
without changing session or native memory.

`BCLR #13,D0` makes the original `BEQ` take the direct path when S was clear.
That path performs no hardware write and sets CCR.Z as defined by BCLR. When S was set, the fall-through
executes these instruction-defined effects:

| Instruction | Exact effect |
| --- | --- |
| `MOVEP.W D0,0(A0)` at `$2aa98` | `$07 -> $ffff8800`, `$ff -> $ffff8802` |
| `MOVE.B #$0e,(A0)` at `$2aa9c` | `$0e -> $ffff8800`, intentionally overwriting `$07` |
| `MOVE #$0300,SR` at `$2aaa0` | resulting observed-path SR `$0300` |

The two hardware instructions become two ordered atomic memory batches so
the deliberate overwrite remains explicit. They are admitted only for an
observed supervisor SR. Both branches converge at `JSR $2a51c`; the native
session records return `$2aaaa`, executes the local selector-2 stack prefix,
and stops before XBIOS `TRAP #14` at `$2a520`. It does not synthesize an A7
address or invoke XBIOS. A typed, generation- and sequence-owned selector-2
observation may provide the returned D0. The exact continuation then executes
`ADDQ.L #2,A7`, atomically stores D0 big-endian at `$2a50a`, pushes selector 3
relative to the unmaterialized A7, and stops before the next `TRAP #14` at
`$2a52e`. The 20 verified bytes `$2a51c..$2a52f` have SHA-256
`751915c217471e4763ebeef2928dc4cca68bc481dae3113adabb441c2446ee2f`.
An explicit typed selector-3 D0 result admits the next exact local block:
`ADDQ.L #2,A7`, an atomic big-endian `MOVE.L D0,$2a50e`, and
`MOVE.W #4,-(A7)`. Its 16 bytes `$2a52e..$2a53d` have SHA-256
`f4a7b019591ccff43e4478ac1549e262387ebfb22c16ded18457fe2aca6bbcc2`.
Execution then stops before opaque XBIOS selector 4 at `$2a53c`.
A typed selector-4 result consumes only D0's low word, exactly as the original
`MOVE.W` requires. The 12 bytes `$2a53c..$2a547` hash to
`42c6d7ede7609ced9c859e6222d678edf861018b86ee80be2cfe6f8a23010e44`:
they clean two stack bytes, atomically store that word big-endian at `$2a512`,
then stop before the opaque Line-A `$a000` instruction at `$2a546`.
A typed Line-A observation supplies only returned A0 and the two longwords
which the original immediately reads from `8(A0)` and `12(A0)`; it does not
model firmware internals. The 24-byte local block through RTS hashes to
`1705523f57debe7644c3a874cd76e42464f1f34f227c9ee1247026afdb2f3539`.
It atomically stores the observed values at `$2a514` and `$2a518`, returns to
`$2aaaa`, then executes the 8-byte caller continuation (SHA-256
`37f9fb95e45dc6c4807821ac79189a2d764fffe6bbbef6196ee17f3ad1a18684`)
and stops before XBIOS selector `$15` at `$2aab0`.
A typed selector-`$15` return records D0 without assigning it meaning: the
following code never reads it. The exact 16 bytes `$2aab0..$2aabf` hash to
`de3f0996c3b76c20c1e83a686f9a97f7a5ad8f9575a03d8f01b7f4cadf45a233`.
They clean six stack bytes, push pointer `$2a612` and selector 6, then stop
before XBIOS `TRAP #14` at `$2aabe`.
A typed selector-6 return similarly records otherwise-unused D0. The exact
10 bytes `$2aabe..$2aac7` hash to
`ba614a28f861921a263225ef85209b20dc2673ea3444cb556b88ca29b2b23163`.
They clean six stack bytes and stop before absolute `JSR $2b55a` at `$2aac2`.
The old file-`+$107c` candidate is correctly rejected because it maps to
`$2b57c`. The genuine bytes at loaded `$2b55a` are instead
`48e7fffe61000038`, SHA-256
`b1b4328c9f54737553994259dac4dfb0247bf422414ed05a1c5c6166ec37ba62`:
`MOVEM.L D0-D7/A0-A6,-(A7)` followed by `BSR.W $2b59a`. Eon executes this
exact prefix and stops before the BSR callee at `$2b59a`.
The genuine callee prefix `$2b59a..$2b5a9` is hash-bound by the first 16 bytes,
SHA-256 `967cb0022c8e29e0bef0dae618b95750fff3afa255094f9356210f1c89686fa3`.
BSR records return `$2b562`; `LEA -$4b6(PC),A3` yields `$2b0e8`, and
`CLR.B $5d0(A3)` atomically clears `$2b6b8`. Execution stops before the
D0-indexed `MOVE.B` at `$2b5a6`, since its source index is external state.
The typed continuation records D0, the derived source address
`$2bdfd + sign_extend(D0.W)`, and the byte observed there. The two exact
MOVE.B instructions hash to
`e87859079e18a266cc359d7e0be47667c5cfe79dbffa05daad80ee951fa777d7`
and atomically copy that byte to `$2b6b0` and `$2b6b1`. The next local
boundary is the A1 setup at `$2b5b2`.
The next 48 genuine bytes hash to
`4345389397550c90280802d10a3f03b3e181745bcb98f8c693a2c0980722a1ef`.
They derive A1 `$2b61e`, D7 `2`, A0 `$2bdcc` then `$2bdfc`, atomically apply
five byte initializers and two `$2bdcc` pointer stores, and stop before the
next D0-indexed word read at `$2b5de`.
The following A0.W-indexed source resolves to `$26ee4`. A typed word
observation is checked against owned memory before the 20-byte tail (SHA-256
`82379ace33d5464b74e03aa0669f8a1097498fd21ce3639c180ab5e21cac810b`)
derives A0 `$56eee4`, atomically stores it at `$2b620`, increments D0.W,
decrements D7 from 2 to 1, and takes DBF back to `$2b5b8`.
The checkpoint is generation-owned and disappears
with the same coordinator revocation as its PRG and Fread memory.

## Remaining boundary

The materialized image and exact configuration occupy native runtime memory.
With explicit SR, selector-2, selector-3, selector-4, and Line-A observations,
both entry branches
reach the D0-indexed write at `$2b5a6`. Its caller register is the next boundary. TOS
basepage fields, other XBIOS results, Line-A state, input, timing, and every unclassified indirect
target remain explicit preservation boundaries.

No original bytes are written to disk, copied into a package, or committed.

The caller-connected path now continues through absolute `JSR $2aa0c` to
`$2a5aa`. The six-byte call hashes to
`25939d2a8a98420749b181f742081cc576f302cffd0bea5b8008765af3b5d9f0`;
the 12-byte callee prefix hashes to
`bdfb77219a19903ee730f3361af0958841aae3570ef3ed0d2ea60c3b56a3491e`.
It pushes mode 2, filename pointer `$2a640`, and GEMDOS selector `$3d`, then
stops exactly at `TRAP #1` `$2a5b4`. No host file is opened and no return
value is synthesized.
A typed raw GEMDOS return can advance without host filesystem inference. The
12-byte return body hashes to
`dfe4c3bc4466d6d8772f3633cb125f64ea7a9114d3d0be45aca5be3daf28b30b`
and atomically stores D0.W at `$2a5fa`. Its signed test either loads literal
D0 `$7d42` and D1 `$2c24a` before the `$2aa28->$2a5c2` call boundary, or
reaches the exact failure self-loop at `$2a632`. No host handle is created.
The positive call then hash-binds the 16-byte `$2a5c2` prefix as
`6d2ddd7da4866769c78162433427fb37fe2f885926f429c098fca3062e282921`.
It pushes the exact count, buffer, owned handle word, and selector `$3f`, and
stops at GEMDOS `TRAP #1` `$2a5d0` without performing host I/O.
The typed raw Fread return advances through exact cleanup, test, RTS, and
caller JMP instructions without observing buffer bytes, because no branch
consults the result. The close wrapper pushes the same owned handle and
selector `$3e`, then stops at `TRAP #1` `$2a5e6` without host I/O.
After a typed raw Fclose return, a four-byte observation covers only the two
buffer words immediately consumed at `$2c24c` and `$2c24e`. They commit
atomically, become D6/D7, and execution stops before `$2aaec->$2b2be`; no
unconsumed buffer bytes are materialized.

The named recovery map binds `millennium-atari-config-xbios-3` to runtime
`$2a52e..$2a53b`, immutable `MILL22A.inf` hash
`74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6`,
and the continuation hash above. `millennium-atari-config-xbios-4` binds
`$2a53c..$2a545` to the 12-byte hash above. The named
`millennium-atari-line-a-init` row binds the two hashes above
and terminates at `millennium-atari-xbios-15`, `$2aab0`.
The structural unit fixture checks loader arithmetic only; the canonical
native corpus test constructs `MillenniumAtariBootstrapSession` from the real,
hash-identified supplied disk and therefore also enforces this exact native
image checkpoint.
