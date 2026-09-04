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

## Observed function-zero return

The next 29 original bytes at file `+$1a98..+$1ab4` hash to
`4ffa7a86b6e398183f251b7de848cefe76ed4e10fd9ddd95b5c8548539fb2704`.
Eon accepts a result only as a monotonically sequenced observation at the
exact `$0127` interrupt / `$0129` return pair. The observation retains raw AX
and FLAGS without interpreting the driver's hardware mode.

After the wrapper returns to `$1b98`, the exact local instructions produce
four native runtime-memory effects in the child segment:

- word `$1a9c := AX`;
- byte `$1aaa := AH`;
- byte `$0107 := AH`; and
- word `$1aa0 := $da00`, the instruction-defined SP value.

The subsequent `CMP AL,1` operates on the copied high byte. Value one selects
the direct call at `$1bad` to `$1ac6`; every other byte selects the call at
`$1bb2` to `$1ada`. The four writes commit atomically with the typed session.
A wrong address, duplicate sequence, stale session, rejected memory batch, or
revoked release changes neither state nor memory. Eon stops before the
selected callee: its effects and return remain unobserved.

## Selected mode callees

The two selected callees share the same next external contract:

| selected mode | entry | exact 11-byte span SHA-256 | wrapper call |
|---|---:|---|---:|
| `1` | `$1ac6` | `a4db63f6cc6d8ba1004340b3f25b1d21299bd14a3466189d0bb495434c5849a2` | `$1ace` |
| every other byte | `$1ada` | `0dab61c355813642910e49ec8fecc80def19a584a51a8323b3ad0e644468a5fe` | `$1ae2` |

Each exact prefix loads `AX=$0004`, sets `ES=CS`, sets `BX=$1ac5`, and
directly calls the same `$0122` wrapper. The runtime executes the selected
prefix automatically after atomically accepting the preceding function-zero
result. It records the three register effects and publishes function `$0004`,
record `CS:$1ac5`, `INT $91` at `$0127` as the next boundary. It does not
execute the later `$044c` or `$0487` call, read `$0107`, or apply the optional
`$b800` write because all of those depend on the still-unobserved function-four
return.

## Function-four return and BIOS boundary

The second typed result requires the same `$0127` / `$0129` wrapper pair and
also the selected caller return: `$1ad1` for mode one or `$1ae5` otherwise.
Raw AX and FLAGS are retained without assigning them graphics semantics. The
following direct targets are then executed only through their first BIOS
request:

| route | exact code span | SHA-256 | resulting boundary |
|---|---|---|---|
| `$1ad1 -> $044c` | `$044c..$046e` | `1c2afa83de99564ceb8e9168f7d6fa586ef7ba21ec2b7d1bdaad9291ec3efc0a` | `INT $10` at `$046d`, `AX=$1010`, `BX=0`, `CX=0`, known `DH=0` |
| `$1ae5 -> $0487` | `$0487..$0498` | `111aabbae0194a132060f1acd6cc5d6c100ccb9c64facdb64c90785a845e6c6b` | `INT $10` at `$0497`, `AX=$1000`, `BX=0`, `CX=$0010` |

The mode-one path reads its first RGB triplet from the exact 48-byte table at
`$014c` (SHA-256
`9d1fdeadf710e7f0a6736f172415e15d7db87480588ec771327f30128afb43e9`)
and writes byte one to child cell `$0107`. The other path reads the first
index byte from the exact 16-byte table at `$0477` (SHA-256
`ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a`);
that byte is zero and the prefix has no memory write.

For mode one, only the high byte of DX is known at the BIOS boundary because
the code writes DH but retains the incoming DL. The checkpoint therefore
publishes a DX known-mask of `$ff00` rather than fabricating DL. The other
route does not use a proven DX value and publishes a zero known-mask. Eon
stops before either BIOS result and does not iterate the palette loops. The
mode-one `$0107 := 1` write commits atomically with the state transition; the
other route advances state without inventing an empty memory batch.
