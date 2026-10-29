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
not presented as the address selected by an original TOS machine. Project Eon
has no general 68000 or TOS emulator; the later BSS copy routine is admitted
as one bounded instruction sequence with an explicit stop before the next
GEMDOS call.

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

The bounded interpreter applies 68000 word-MOVE condition codes as well as
register and memory effects: N/Z follow the moved word, V/C clear, and X plus
the upper status register remain unchanged. A following recovered branch can
therefore consume only flags produced by the original instruction.

## Post-config filename provenance

The BSS entry at `$1d636` now executes its exact self-initializing 68000 copy
routine. The instruction subset is limited to `MOVEA.L #imm,An`,
`MOVE.W #imm,Dn`, `MOVE.W (A0)+,(A1)+`, `DBF`, and absolute-long `JMP`.
Its 28-byte instruction span is hash-anchored as
`bae3f526a7a7e42ca59d840ed80606f0f2b5a9f420fe221ddd30f04d9388e30b`.
The routine executes 518 instructions, including 257 word copies, reads only
the 514-byte source interval at `$1d652`, and writes only `$77000..$77201`.
It stops at the verified `JMP $77000` before executing the target code. The
source consists of 188 hash-identified DATA bytes followed by 326 bytes from
the PRG's loader-zeroed BSS. Any opcode outside this routine's small grammar,
out-of-range access, unexpected step count, or different jump target is a
hard failure. This does not emulate TOS, execute GEMDOS, or establish game
playability.

The staged caller at `$77042` pushes filename pointer `$1d6d8` before its
second GEMDOS `Fopen`. This address is `+$86` into the exact bootstrap source
buffer at `$1d652`. The loader copies that byte range from the original PRG's
DATA tail into BSS before copying the 514-byte target to `$77000`. The
hash-bound caller span `$77042..$77057` has SHA-256
`dc2a50400e22fdbe4870f790d4f70c7446caa379dc68281a0445db4ee027fe4d` and
passes that exact pointer.

The 12-byte NUL-terminated source string begins at `MILENIUM.TOS` file offset
`$121c` (loadable-image offset `$1200`) and hashes to
`393a936fc20d9f40ecace75f74947833d28e947e3ba9a987761a7d1eb92a575b`.
Address `$1d6d8` therefore names `MILL22B.inf`. This is a static data-flow
fact: it does not establish that GEMDOS `Fopen` succeeds, that the following
Fread returns bytes, or that the loaded file is decoded or displayed.

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

The later `$77056` open is a separate boundary from this first config read.
Its name is now proven as `MILL22B.inf`, which exists in the same hash-bound
Equinox FAT12 root at cluster 11 (84,720 bytes; SHA-256
`e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9`). The
Fopen, Fread, and Fclose remain typed service boundaries. A nonnegative
admitted Fread count up to the FAT file size (84,720 bytes) copies that prefix
of the hash-verified original file to `$11e00`; a negative result copies no
bytes, and a count larger than the file is rejected. This advances the
original caller to its post-Fread path.

## `MILL22B.INF` entry bytes

The supplied 84,720-byte file begins with the original 68000 instruction
`JMP $1c62c`. The proven Fread destination is `$11e00`, so the target is file
offset `$a82c`, inside the bounded payload. Its next 24 bytes hash to
`f97319598c3c193dc292abbf86c0b94c814f4b9ecafcdba612bb0116660c93b6` and
decode as `MOVE SR,D0`, `BCLR #13,D0`, `BEQ`, then the first three hardware
initialization instructions. This makes the file's code-shaped entry and
address mapping explicit. The entry parser rechecks the complete file hash,
JMP target, bounds, and prologue digest.

The typed `MillenniumAtariPostConfigEntrySession` accepts the entry transfer
only when `$11e00` contains the original six-byte jump in native memory. Before
accepting the generation-owned SR observation at `$1c62c`, it also requires the
full 24-byte entry prologue and the local helper bytes through its `$1f92e`
TRAP to be present in native memory and match the hash-verified module. A short
Fread that contains only the leading jump therefore cannot advance. It does
not pick user or supervisor mode. For an observed supervisor SR, it follows the exact
`BCLR`/branch path, records the three ordered PSG writes, and applies
`MOVE #$0300,SR`. For user mode, it follows the taken branch and emits no PSG
writes. Both paths execute the local `JSR $1f924` and its two argument pushes,
then stop before XBIOS `TRAP #14` selector `$26` at `$1f92e`. The relative
argument frame is selector `$26`, pointer `$1f934`; no host A7 value or XBIOS
return is supplied. This reaches one verified module initialization boundary;
it does not establish a visible screen, controls, or playable state.

The exact bytes after that boundary show a local `ADDQ.L #6,A7` at `$1f930`
and `RTS` at `$1f932`, returning to `$1c64e`. The caller then pushes a zero
longword and selector `$15`, reaches its next XBIOS trap at `$1c654`, cleans up
six bytes at `$1c656`, and calls `$11e18` at `$1c658`. This is a static successor
map only. It does not establish that XBIOS selector `$26` returns, its hardware
or memory effects, or its register-preservation contract. Before a typed
continuation can cross it, a runtime trace must bind the trap's actual A7 and
six argument bytes to the recorded relative frame; the post-XBIOS PC, SR,
registers, and memory effects; and the return-stack address consumed by the
local `RTS` (which must resolve to `$1c64e`). The next boundary must then
confirm selector `$15` at `$1c654` with its actual A7 and zero argument. No
selector meaning or firmware result is inferred from these bytes.

`MillenniumAtariPostConfigEntrySession` now has a typed continuation for this
boundary. It accepts selector `$26` only with the actual six argument bytes,
the observed post-service PC/SR and register image, the memory effects recorded
by the trace, and a post-service A7 equal to the observed argument-frame A7.
The local `ADDQ.L #6,A7` then places A7 at `argument_stack_address + 6`; the
four bytes there must resolve through `RTS` to `$1c64e`. It then records the
caller's original zero-longword and selector-`$15` frame at `$1c654`. This
models a supplied trace and the local stack arithmetic only. It does not
synthesize an XBIOS return or claim the external service has been observed in
a running game.

`ReleaseRuntimeCoordinator` owns this session for the active Atari generation
and exposes typed entry-PC and SR observations through the runtime host. It
accepts the entry-PC observation only after the typed post-config GEMDOS caller
sequence has completed. The session then checks the loaded jump, prologue, and
helper bytes against native memory. The coordinator does not infer that the
post-config caller transfers control to `$11e00`; that transfer must be
observed explicitly. The presentation snapshot contains only the typed
checkpoint, and the first XBIOS result remains external.

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
The following A3/D0.W-indexed source resolves to `$2be08` on the first pass.
A typed word observation is checked against owned memory before the 20-byte tail (SHA-256
`82379ace33d5464b74e03aa0669f8a1097498fd21ce3639c180ab5e21cac810b`)
derives A0 `$56eee4`, atomically stores it at `$2b620`, increments D0.W,
decrements D7 from 2 to 1, and takes DBF back to `$2b5b8`.
The checkpoint is generation-owned and disappears
with the same coordinator revocation as its PRG and Fread memory.

## Remaining boundary

The materialized image and exact configuration occupy native runtime memory.
With explicit SR, selector-2, selector-3, selector-4, and Line-A observations,
both entry branches and the complete three-iteration indexed table loop are
native under typed source bytes and words. Later D0.W values and source
addresses are carried deterministically from the preceding original word and
`ADDQ`, rather than supplied again. TOS basepage fields, other XBIOS results,
Line-A state, input, timing, and every unclassified indirect target remain
explicit preservation boundaries.

No original bytes are written to disk, copied into a package, or committed.

For the English Equinox Atari ST `MILL22B.INF`, the opaque Line-A boundary at
file `+$42` / `$11e42` is now followed only by typed trace data. The 22 local
bytes after the opcode (file `+$44`, through RTS to `$1c65e`) hash to
`5ab9d1696078069db836401df103614c0cb0862dd25d8dde549ee7f96dd2aa06`. The
observation supplies the post-handler register image, exact bytes read at
A0+8 and A0+12, and the return address bytes; those facts are not interpreted
as Line-A behavior. At `$1c65e`, the caller stores post-Line-A D0 at `$1ff66`;
the local suffix leaves D0 unchanged, so the typed observation's raw D0 is
used there (not the earlier selector-3 return value). The hash-verified caller span at file `+$a85e` / `$1c65e`
(96 bytes, hash `3eac6059a1d4d063b2d8f107b236aa76f63e324aea672e5fc49561ac25309a35`)
checks the already observed longword at `$11dfc` against `$361436a7`. Its
unmatched path stops before JMP `$2637e`; its matched path executes the local
address setup and `JSR $1c5a0`. That 14-byte callee at file `+$a7a0` hashes to
`08f40fb3653f5428f32760d322ce08e49e55f940b74bee9472ada9637e13ba0c` and
returns to `$1c6be`. The local callee at file `+$1160c` / `$2340c` currently
admits its 22-byte caller prefix, the helper copy, and the first macro command;
the native session stops at the next token `$10` at `$1ded2`. Static
disassembly follows the remaining route to GEMDOS `TRAP #1` at `$11ebc`,
selector 7, but that continuation has not been admitted by runtime code. The
22-byte caller prefix hashes to
`e773ba06fbe710796e42e0b323b3ed2b63a9d97d6b087e1f8386e060c1096be2`; the
40-byte helper at `$14e20` hashes to
`5a59557110f2435a7c795c82d11a2bde496acacf30be05f532024a62f21a9f39`. That
helper copies exactly `$5f00` bytes from the observed source address
`longword[$11e0a] + $1680` to `$27326`. The runtime requires those source bytes as a
typed trace, checks any bytes already known in native memory, rejects wrapping
or overlapping source/destination ranges, and applies the copy atomically.
The exact dispatcher, handler, and macro prefix are also hash checked: 24
bytes at `$2010e` (`02d0c1902077eccefdd88b85e64823bd1a004f7375afa357ac679074c8af3377`),
51 bytes at `$1fffc` (`07c62426cd5874d72fac9ef033d9c5a53512248062c7b59afa7f2f6d855066d5`),
60 bytes at `$2009e` (`1c40ffe44b9d4365635b1e802e7472bc32f45dcd0cbe64f474b7361074c71e33`),
and the macro stream at `$1decf`
(`dec3e22f141ef825023ad1a061227f60183668a5d254310d9f057c06a466183a`). The
table word at `$1cd36` resolves the interpreter input to `$1decf`. Its first
command is `16 01 06`, followed by token `$10` at `$1ded2`. The handler consumes
only the opcode and two operands, computes `$1e01`, and stores
`$00081e01` at `$1ff66` when the selector-3 longword is `$00080000`. In
instruction order the dispatcher sets `$1ff76` to 1 before the handler stores
`$1ff66`. The first local step records the next-command boundary at `$1ded2`;
the runtime immediately follows it with the hash-gated continuation described
below.

A read-only full-macro disassembly confirms the remaining bytecode through its
NUL at file `+$c0fa` / runtime `$1defa` (44 bytes from `$1decf`, SHA-256
`c4c430dfed566d9f150ff3bc759f97d9642d6b819db86a00d638cd44cfc2f037`). It
contains operands `$10 0a` and `$11 00`, the text `Insert Disk 2 Then`,
command `$16 01 07`, and `Press Any Key..`. The `$10`/`$11` handlers at
`$1ffa0` and `$1ffca` select addresses within the `$2380c` table using the
runtime word at `$11e0e`; both loaded spans are hash-identified (40 bytes,
`2fcb317f626e51083cc64349aa163c086c887339e6acc0b3977c9a50fa3e60d9` and
`163b347c6a3fe378ece8ee2382581ff88a6f28f682dbb10df84e7f15860b781c`). The
outer dispatcher span at `$2000c` (284 bytes) hashes to
`6ecf4af4b37b7e5d4bee56a475cc1d12cc765239d8858cdcf92188876ac00ac9`. The
positive-mode text raster path is `$201e0..$2024f` (112 bytes, SHA-256
`b17d55dd892c07ea84a53cd1c6ba2a07e212ed2f6cfa9f3632363c6cdaabf970`); it
updates the screen buffer addressed by `$1ff66` using glyph data at `$1fc86`
and mask data selected through `$1ff5e`. The native continuation now checks
these exact loaded spans, requires the selector-4 word at `$11e0e` and the
typed `$5f00`-byte source screen, applies the original glyph/mask operation to
owned runtime memory, and stops at GEMDOS selector-7 trap `$11ebc` with the
selector word at `$1cad0`. No Crawcin result is executed or synthesized. The
test's patterned buffer is only a mechanics fixture and does not establish
pixel parity or Atari gameplay.

Atari's
November 26, 1985 GEMDOS quick reference identifies selector 7 as Crawcin, a
raw character read from standard input without echo, and states GEMDOS returns
LONG values in D0 ([reference, GEMDOS Quick Reference, pp. 74–75](https://bitsavers.org/pdf/atari/ST/Atari_ST_GEM_Programming_1986/GEM_0087.pdf)).
Separately, the static return path from `$11ebc` has been followed through the
local caller at `$23422`, helper `$14e48`, and its `$1fc10` flag check. When
the observed byte at `$1fa0f` is zero, the code reverse-copies the already
materialized `$5f00`-byte range from `$27326` to
`longword[$11e0a] + $1680`, then prepares GEMDOS selector `$3d` in the stub
beginning at `$11fc8` and reaches `TRAP #1` at `$11fd8`.
On this path, raw Crawcin D0 is neither tested nor stored: the code replaces
D0 with `$17bf` before the copy. When `$1fa0f` is nonzero, the helper clears
that byte and reaches opaque Line-A opcode `$a00c` at `$1fc82`; D0's use and
all service effects there remain unknown. The module's initial byte at
`$1fa0f` is zero, but runtime ownership or later mutation has not been
observed, so the continuation must require an explicit current-byte
observation. This corrects the earlier characterization of Crawcin as a
likely wait boundary: the verified zero-flag path continues to a file-open
boundary without consuming the returned key. No host key mapping or gameplay
action is inferred. The next external result needed on that path is GEMDOS
selector `$3d` in the `$11fc8` stub; actual file result and later gameplay
remain unverified.

The `$11fc8` stub is now source-bounded: its 18 bytes at file `+$1c8` hash
to `eeb5dbed91cf6866d70615df0c093a325fd1c6660440b7e3c3f96a9e24357b28`.
At runtime it begins with `MOVE.L #$1204a,D7`, pushes read mode 2, that
filename pointer, and selector `$3d`, then reaches `TRAP #1` at `$11fd8`. The
11-byte NUL-terminated filename at file `+$24a` hashes to
`c1e91c254fbd9599cbcc171800552a8f802e0192f1be7d1c6ac7c78acd79c4a3` and is
`2200AD.PRG`. A bounded root scan found no such entry in the four supplied
one-disk FAT12 variants, nor in either two-disk physical STX root: Disk 1's
six entries are `EXEC.TOS` and `MILL22A.INF` through `MILL22D.INF`; Disk 2 has
`MILL22E.INF` and `MILL22F.INF`. Thus a host file result cannot be supplied
from these inventories without more evidence. The actual GEMDOS result,
retry behavior, and any disk-change route remain unobserved.

The separately supplied English Millennium DOS `2200AD.EXE` is a different
platform artifact and is not a valid substitute for this Atari ST GEMDOS
filename.

The native entry session now admits a typed branch observation at `$1fc6e`.
It requires the current byte at `$1fa0f`, the post-BEQ PC (`$1fc84` for zero,
`$1fc76` otherwise), and exact loaded caller/helper bytes through the boundary
before opaque Line-A `$a00c`. It records the branch outcome without executing
the successor, changing memory, or interpreting the Crawcin result. The
observation is available through the launcher runtime API; no Atari emulator
capture has supplied a real observation yet.

On the zero-flag successor, a typed observation can stop at GEMDOS `TRAP #1`
`$11fd8`, after the hash-bound Fopen setup beginning at `$11fc8`. It verifies
the 18-byte stub and
NUL-terminated `2200AD.PRG` name against both the module and loaded native
memory, rechecks the loaded `$23422` caller and `$14e20` copy-helper spans,
and binds the eight-byte observed stack frame to the actual A7 value at the
trap before checking it in native memory:
selector `$3d`, pointer `$1204a`, mode 2. It records the trace sequence and
stack address and stops before `TRAP #1`. It does not choose or synthesize a
GEMDOS result, resolve a drive, or infer what happens when this name is absent
from the supplied roots.

The exact 12-byte return suffix at `$11fda..$11fe5` hashes to
`b8822e86ef570519d8a3ebedfb3164a8e05d3e6305b3f7ca862bdf874439ae59`. It
executes `ADDQ.L #8,A7`, stores D0.W at `$12056`, performs `TST.L D0`, and
returns through `RTS` at `$11fe4`. A second typed observation requires GEMDOS's
observed PC, A7, SR, and D0 at `$11fda`, verifies that suffix in module and
runtime memory, and reads the dynamic RTS address from the owned stack. The
bounded 68000 executor now runs those exact 12 loaded bytes with the observed
D0, SR, and stack. It must execute the four supported instructions (`ADDQ.L`,
absolute `MOVE.W`, `TST.L`, `RTS`), store the resulting D0.W at `$12056`,
reproduce the observed post-RTS PC/A7, and reach the stop address within five
steps. The session commits that instruction-produced word only after all
checks pass. It does not infer whether Fopen succeeded or what the caller does
next.

The exact Equinox continuation at `$11fe6` is bounded conditionally. The RTS
destination remains the address read from the observed stack; only when that
destination is `$11fe6` does the session verify the following 18-byte local
span through the next `TRAP #1` against SHA-256
`352b6ca9a375e016e129667085dbd6fcd89d1e2eece201d75d97972c60b94cd4`, and
executes its four deterministic 68000 instructions. The resulting external
boundary is GEMDOS selector `$3c` at `$11ff6`, with attribute 0 and the
`2200AD.PRG` pointer `$1204a` on the native stack. The coordinator commits
only the D0.W store and this exact stack frame; it does not invoke Fcreate or
create/modify a file on the original disk. This records the target's next
request without claiming that a missing executable has been recovered. No
recorder-backed trace has confirmed that the dynamic RTS destination is
`$11fe6`; other destinations stop after the typed Fopen return. The Fcreate
return suffix at `$11ff8..$12003` is also hash-bound to
`b8822e86ef570519d8a3ebedfb3164a8e05d3e6305b3f7ca862bdf874439ae59`. A typed
service return executes only its four local instructions, checks the raw D0,
SR, stack, and dynamic RTS destination, and commits the resulting D0.W store
to `$12056`. It does not interpret the Fcreate result or emulate file
creation. The actual service return, disk state, and caller after RTS remain
unobserved.

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
The correctly mapped `$2b2be` callee executes 32 deterministic setup bytes,
commits its D6/D7 words atomically, and stops before the source-byte read at
`$2b2de`. The older file-`+0xde0` candidate is not executed.
One typed byte at `$2c250` drives the exact mask/bit dispatch. Production
acceptance also requires every typed token byte to match the corresponding
byte of the immutable, hash-admitted `MILL22A.INF` Fread image. Contradictory
or unavailable bytes fail atomically instead of creating display data. On the normal
zero-bit path, one bounded pair observation admits `$2c251..$2c252`, copies
the bytes atomically to the owned A5 destination, advances A4/A5 and
decrements D6 through the exact eight-byte prefix whose SHA-256 is
`8b97786735b1f1be41f931a62098f2f1080b5067b2db2a9835125619ad3b7623`.
The normal-path counter continuation `$2b2f2..$2b321` is hash-bound as
`9b3476f5d2ecb028149eec6ee575cd79c7c9f94589a7e7398d794ecd176f04ef`
and natively dispatches the remaining run, row and four-plane counters back
to the next pair/token boundary or to RTS. The three alternate token paths
are also native: repeated-byte `$2b332..$2b375` hashes to `6429d7b0...6701`,
swapped-pair `$2b376..$2b3b7` to `dbf80460...a9b9`, and the extended 14-bit
run prefix `$2b3b8..$2b3c5` to `72fa6338...b0c5`. Each requires exactly its
consumed payload bytes as a generation-owned typed observation and commits
all derived destination words atomically.
After the token engine returns, the exact caller continuation reaches
`JSR $2b448`. Its native deterministic prefix clears eight longwords and
copies the hash-bound 96-byte source at `$2a66c` to `$2b3c8` atomically.
The native palette loop now accepts the 16 existing destination words as one
typed observation, performs all 16-by-3 byte additions and weighted carry
updates, and commits its 48 byte plus 16 word effects atomically. Execution
stops at `TRAP #14` `$2b4ac`, with XBIOS selector 6 and palette pointer
`$2b428` retained. A typed raw D0 return now admits the exact stack cleanup
and 20,000-iteration D0 delay loop. The following `DBF` changes D7 from 6 to
5 and reaches the corrected loaded address `$2b46e`. Six further passes use
typed current source bytes and destination words, commit each complete pass
atomically, and require a fresh typed selector-6 return. The final D7 fallthrough
owns the terminal selector-6 return and reaches local RTS `$2b4c6`. A typed
stack observation admits the exact return `$2ab04`. Its hash-bound caller
loads D7 `$2a634`, calls the reused
`$2aa0c` helper, and reaches its existing GEMDOS selector-`$3d` boundary. A
dedicated typed result now atomically records the raw handle word. Its negative
branch reaches `$2a632`; its nonnegative branch reaches the bounded Fread
helper with the exact `$7d42`/`$2c24a` arguments. Eon does not infer any
XBIOS, GEMDOS, filesystem, display, or wall-clock timing semantics from the
raw results.
The second-config success route also owns typed raw Fread and Fclose returns.
Fclose reaches RTS `$2a5ec`; only a typed stack destination `$2ab10` admits
the separate caller. Its exact 22 bytes restore A3/A4 and reach XBIOS selector
`$26` at `$2ab24`, with PC-relative pointer `$2ab2c`. No bytes read,
filesystem operation, or firmware result is synthesized. A typed raw XBIOS
return owns its exact cleanup and RTS `$2ab28`; a second typed stack return is
fixed to the staged PRG caller `$77042`. Its hash-bound 22-byte continuation
reaches GEMDOS selector `$3d` at `$77056`, with mode 2 and filename pointer
`$1d6d8`. A typed signed result now follows both exact branches. A negative
result retains its low handle word and reaches the self-loop at `$77060`; the
10-byte branch span hashes to
`d124b586e52a783689925186d8cc93366870526fd894567b7c55761a617807c7`.
A nonnegative result reaches GEMDOS selector `$3f` at `$77074`, with original
buffer `$11e00` and count `$20000`; that 30-byte span hashes to
`2ceb9e3c6a8c2882f13708d64367b0a9f8bf18ee7456ea396a3e600734825476`.
The Fread result remains external.
Both negative and nonnegative typed Fread results take the same exact
eight-byte continuation (SHA-256
`368338a18784d37b5867fa551121703b2fb0ab613db51cbc5b2c08e14f474558`):
12 stack bytes are removed and the already-pending selector `$3e` reaches
GEMDOS trap `$7707c`. A typed Fclose result then admits the exact cleanup and
branch to `$770a2`, two atomic longword writes of `$361436a7` to `$2ab2c` and
`$11dfc`, and local RTS `$770ba`. The concatenated executed bytes hash to
`aa177208872c4125af13601feb4566003e5fb01c851c44f8b7f4904fb5f52b52`.
Because the staged target was entered by the bootstrap's absolute `JMP $77000`,
no caller return is encoded in the program bytes. The RTS destination
is therefore accepted only as a typed, even 24-bit address and retained as the
terminal preservation boundary; no caller continuation, service, or
target-data meaning is invented.

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

### Post-config selectors 2, 3 and 4

On the exact Equinox image (`3f090651ee586cf32a3f37f41b748ba36c78799e7`),
`MILL22B.INF` is 84,720 bytes with SHA-256
`e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9`. Its
original FAT12 root record starts at cluster 11 and the verified chain has 83
clusters. File offset `$a854`, loaded address `$1c654`, begins the 10-byte
continuation `TRAP #14; ADDQ.L #6,A7; JSR $11e18` (SHA-256
`b553e819435703a8ba790781ccc1137f9e88d1d4827a47d289695445f7256131`). The
typed selector-$15 return must confirm selector `$15`, its exact six argument
bytes, and post-service PC/A7 before this local caller suffix advances.

The target is file offset `$18`, loaded address `$11e18`. Its 42-byte local
sequence (SHA-256
`569b54f0d351f7db543b15c4e0227fd9b21e078dadffaa45ee767d1d1ceae474`)
performs three word-selector traps and preserves each observed D0 verbatim:
selector `$2` returns to `$11e1e` and `MOVE.L D0,$11e06`; selector `$3`
returns to `$11e2c` and stores D0 at `$11e0a`; selector `$4` returns to
`$11e3a` and stores D0's low word at `$11e0e`. The admitted trap frames are
two bytes (`$0002`, `$0003`, `$0004`) at the stack position reached by the
observed caller return. The coordinator applies each store only after its
matching external return observation, then checks prior stores in native
memory before accepting the next one. No XBIOS service meaning is assigned.

The next word at file offset `$42`, loaded address `$11e42`, is `$a000`; its
two-byte SHA-256 is `20b64b56f584ec6c5184846cbeb974347b5e4c25cb49628ce1426fd0d2128ae7`.
The combined selector sequence plus that word is 44 bytes, SHA-256
`3b64ebbfce7fcec18135159b6d2338fcf3695e67e378c1983d28c63cb0b35e88`. The
runtime stops at the Line-A instruction boundary and does not execute it or
infer a graphics operation. This is the shortest bounded continuation proved
by the original module; it does not yet yield an admitted frame or playable
input.
