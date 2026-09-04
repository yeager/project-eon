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

## Complete palette request loops

Each BIOS return is a typed, ordered observation. Mode one accepts only
`$046d -> $046f`; the other route accepts only `$0497 -> $0499`. Raw AX and
FLAGS are retained for all 16 observations, but the original loops do not use
those returned values. They restore/decrement their own loop counter and read
the next request directly from the still hash-verified `TITLES.EXE` view.
Project Eon therefore retains no copied palette table in the session.

For mode one, request index `i` has `BX=i`, reads the RGB triplet at
`$014c + 3*i`, exposes its first byte as known DH and its next two bytes as
CH/CL, and keeps DL explicitly unknown. For the other route, request index
`i` reads the byte at `$0477+i`, uses it as BH with `BL=i`, and carries loop
counter `16-i` in CX. An altered complete executable, wrong result address,
duplicate sequence, seventeenth result, or mismatched active route is rejected.

After result 16, exact local tails join at the main call `$1bb8 -> $1b1f`:

- mode one repeats the instruction-defined `$0107 := 1` write;
- the other route reads the already proven mode byte from `$0107`, and only
  value two writes word `$b800` to `$010a`;
- both restore DS from CS before the common call.

The loop-tail hashes are `$046f..$0476`
`aadfaf1699f751e5de79efcc064c37fedc7db9b0481e152777ba1435cc5b606e`
and `$0499..$049d`
`ddaf4f20a0a9ecce6f4c43aeb48a946177cf834214c1b2306012ec133b7c5fae`.
The selected caller tails hash as `$1ad4..$1ad9`
`ecdc5c4190c6e33928dc5cea98f891731bf3cd941b969f93e17f095ed7418f40`
and `$1ae8..$1af5`
`f3a5aece4755f80806f6f49ba070a03a3d5ae17a4a77496d58eeaeac5b3993dc`.
The common `$1bb5..$1bba` span hashes to
`076161dddab78341dd9a014e90cff175b9f76ea5d0184ec2f6c244f09f659bc6`.

## DOS memory service chain

Production automatically enters the exact `$1bb8 -> $1b1f` call after the
sixteenth BIOS observation. The 67-byte local span at loaded
`$1b1f..$1b61` (file `+$1a1f`) has SHA-256
`62bb857bf927ca3392900f9a8f26b9ab23f0780cd84c0ccf248f084e17c02ba7`.
It makes five ordered DOS `INT $21` requests before the next opaque service:

| request | interrupt / return | exact inputs used by the code |
|---|---|---|
| resize current block | `$1b26 / $1b28` | `AH=$4a`, `ES=CS`, `BX=$1000` |
| large allocation | `$1b2d / $1b2f` | `AH=$48`, `BX=$fa00` |
| free returned segment | `$1b38 / $1b3a` | `AH=$49`, `ES=previous AX` |
| first buffer allocation | `$1b3f / $1b41` | `AH=$48`, `BX=$1000` |
| second buffer allocation | `$1b4f / $1b51` | `AH=$48`, `BX=$0fa1` |

Each typed observation retains raw AX, BX, FLAGS and the carry value, and
requires carry to agree with FLAGS bit zero. The original instructions ignore
carry after the first three calls. They store the large-allocation return BX
at child word `$1aa2`, store the first buffer's AX at `$010e`, and store the
second buffer's AX at `$0112`. Eon reproduces those writes literally without
interpreting them as valid host allocations or manufacturing DOS memory-control
blocks.

Carry after either buffer allocation follows the exact `$1b7c` failure return
(four bytes, SHA-256
`d0f75b0f97509ff14ce1308b5a829de214523523b3fdc6ea7270df3a13e0ea5b`).
The caller span `$1bbb..$1bc4` (SHA-256
`8f78c75697fe56993706c0b6ea69df78c90922b27775feaccb9f40071abbff1f`)
stores raw AX at `$1a9c`, observes nonzero DX, and stops at its jump to
`$1c6a`.

After two carry-clear buffer results, the local code restores DS from CS,
sets `DX=$0e4e`, and enters `$1af6`. Its five-byte prefix has SHA-256
`06a31ffeae96544b136159050eabb961328023c749e073cd9e9e0b752a905884`
and reaches DOS file-open service `$3d00` at `$1af9`, returning at `$1afb`.
The filename bytes, open result, later allocation, file contents, and process
memory semantics remain external. State and each set of instruction-defined
memory writes are committed atomically; a detached address, duplicate
sequence, inconsistent carry/FLAGS pair, revoked session, or rejected memory
batch changes neither session nor native memory.

## `title.lib` size query

The open request names the exact ten-byte NUL-terminated media string
`title.lib` at loaded `$0e4e` (file `+$0d4e`, SHA-256
`62bfc3e4275f23097edf305a3e1144d3eac79b4a4c75cc35cfbb3eb0b9255aed`).
The complete 41-byte helper `$1af6..$1b1e` hashes to
`4fd3a9694c9ea36d7baf33607ed0b70ac764bb1f27bb6b686c3401bce5ef6b3d`.
After an observed carry-clear open return, it retains AX as the file handle and
issues seek service `$4202` at `$1b09` with that handle, `CX=0`, and `DX=0`.
This is a seek relative to the end; there is no DOS read request in this helper.

A carry-set open or seek result follows the exact jump to `$05a3` and stops
there because that error routine is outside this recovered batch. A carry-clear
seek retains raw `DX:AX`, restores the handle, and issues close service `$3e00`
at `$1b12`. The original code ignores close carry and raw close registers. It
restores the seek result's low AX word, computes the 16-bit expression
`(AX + $000f) >> 4`, copies that paragraph count to BX, and returns. It neither
checks DX nor detects 16-bit rounding overflow.

The caller's exact six-byte `$1b62..$1b67` prefix hashes to
`b24d8fd1fa6200c9ea1cf43cfdd413e90089b63d0608efb3866beb8b782b5f3a`.
It changes AH to `$48` and reaches the next allocation `INT $21` at `$1b64`.
The typed checkpoint retains all raw open/seek/close AX, BX, CX, DX, FLAGS and
carry values, the exact source address and length, the handle, and the computed
paragraph request. It does not open host files, infer that `title.lib` exists,
read its bytes, fabricate a DOS handle, or claim that the high seek word is
unused outside this bounded original helper.

## Sized allocation and `TITLE.LIB` load

The sized allocation result at `$1b66` is now a typed continuation. Carry
follows `$1b7c`, returns `DX=1`, stores raw AX at `$1a9c`, and stops at
`$1c6a`. On carry-clear, the original stores AX as the segment half of far
pointer `$0e46`, requests one additional paragraph at `$1b74`, stores that
raw AX at `$1a9e` and `$1a9c` without testing carry, and performs a temporary
`$fa00`-paragraph allocate/free pair at `$1bca/$1bd5`. Those latter results
are also retained literally; the temporary allocation stores raw BX at
`$1aa4`, and neither carry flag changes the local path. The 30 bytes
`$1b62..$1b7f` hash to
`aa3738ee068dcdc02e63c90b3021d9da8672878ef0bae3af7a6ac35f50a3a578`.

The caller then invokes the loader at `$0e5f`. Its open helper requests
read/write mode `$3d02` for the same verified `$0e4e` filename and stores raw
AX at `$19c6`. Open carry stops before the error-display call at `$0e6a`.
Success issues nine ordered DOS reads through `$057c`: eight requests of
`$8000` bytes followed by one of `$a000`, using the exact segment/offset
sequence `base:0000`, `base:8000`, `base+1000:0000`, through
`base+3000:8000`, then `base+4000:0000`. The supplied English `TITLE.LIB` is
required at its manifest identity: 18,907 bytes, SHA-256
`6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678`.

For every carry-clear read, observed AX must not exceed requested CX, the
remaining verified source bytes, or the previously observed allocation size.
Only that exact prefix of the immutable media is copied to the observed DOS
buffer. Carry-set reads reproduce the original unchecked continuation without
inventing written bytes. A zero-byte EOF return is accepted even for a later
request address beyond the allocation because it performs no memory write.
After all nine results the loader closes the retained handle at `$059e`; as in
the original, close carry is recorded but does not branch. The shared DOS I/O
helper span `$0536..$05a2` hashes to
`d74f413ecf61f099d786f957f3f7a17e0044a78027bac844912b087e33d27b27`.

Once at least the six-byte header is actually loaded, the deterministic local
relocation stores its little-endian entry count at `$0e5d` and normalizes the
directory far pointer into `$0e4a:$0e4c`. For the genuine library header
`26 00 13 48 00 00`, an observed base segment `S` therefore produces count
`$0026`, offset `$0003`, and segment `S+$0481`. Mode one stops before `$0f6b`;
other modes stop at the local return `$0f6a`. The loader and relocation span
`$0e5f..$0f6a` hashes to
`63d5b5a645879a0a79ed0a7c880051e98ddf62b91f07616c0a72d035ee9581cf`.
All buffer writes and relocation cells commit as atomic runtime-memory batches;
source revocation or any bound failure leaves the prior checkpoint unchanged.

## Mode-one post-relocation boundary

Mode one continues locally from `$0f6b`. The genuine directory record at
`TITLE.LIB+$4813` produces far pointer `base:$0006`, which is stored at
`$0e59`, and the original clears 768 child bytes at `$014c..$044b`. It then
derives source offset `$4865` and reaches `REP MOVSW` at `$0fc6` requesting
another 768 bytes. That range extends beyond the 18,907-byte verified leaf, so
Eon stops at `$0fc6` instead of inventing the DOS buffer tail. The other mode
follows the exact 14-byte epilogue through `RET $0f6a` (SHA-256
`66c5cf6c6a51f92ec93650c960546e562bd382e96d85f93ab15df8b5a82982b0`).
That return is owned by the still-active `$1bec -> $0e5f` call, so execution
resumes at `$1bef` and stops before call `$1bef -> $1aac` (call-byte SHA-256
`802c3d3da0e9eebe7f5ccfaac938d6c4eba76d4dc6ffb190ef9d719d0a0c4044`).
The `$1aac` title/driver setup remains opaque. No BIOS or private ABI result is
claimed, and the mode-one `$0fc6` boundary is unchanged.

The non-mode-1 path now enters `$1aac`: it restores DS/ES from CS and calls
`$10ec`. That callee's exact ten-byte prefix (SHA-256
`a00fdf978777b8b563efc5c4d39f3e3fbafea0ef764f134c6ee308d9927b6e73`)
clears DF, loads `AX=$3500`, and reaches DOS `INT $21` at `$10f4`. Eon stops
before the get-vector result; vector BX/ES, later interrupt replacement and
the two BIOS calls remain unobserved. Mode one still stops only at `$0fc6`.
An exact typed `$10f4->$10f6` result retains raw AX, BX, ES and FLAGS. The
18-byte continuation (SHA-256
`2b274ecea07db05da2e4f091e648ba5bbec8132d34d60668d47fd57681ae854b`)
stores BX at `$10e4`, ES at `$10e6`, loads DS=CS, AX=`$2500` and
DX=`$1124`, then stops at the DOS set-vector request `$1106`.
The `$1106->$1108` result is retained verbatim even though the original does
not inspect it. The next five bytes (SHA-256
`918d021be641065df0e5519ec984e3d556fb7300e6db321a79cc6b591a54c933`)
load `AX=$3504` and stop at DOS get-vector `$110b`; no additional memory batch
is fabricated for the ignored set result.
The typed `$110b->$110d` result retains its raw AX/FLAGS and old vector
ES:BX. The exact 18-byte continuation (SHA-256
`b78f3be0ba4b6067faaf00309ac1bf821468fae7ef2ec46e43c575de8f95860e`)
stores BX at `$10e8`, ES at `$10ea`, and reaches set-vector request
`AX=$2504`, `DS:DX=CS:$1124` at `$111d`.
The `$111d->$111f` set result is retained raw and ignored. The five-byte
callee epilogue restores the saved registers and returns to `$1ab3`; its hash
is `f32140aa070695a63e56de66fdcdb32c78b2d378318715dc2d8da83a349f0787`.
The next eight bytes load `AH=1`, `AL=$1b`, and `BL=$46` (retaining observed
BH), then stop at BIOS `INT $15` `$1ab9`. Its hash is
`5531aa8efe777cde6344e051bee61deb3e45e685e91c345006caf34bf306b0a7`.
The BIOS result and the second `$1c` request remain external.
The first result now has its own typed AX/BX/FLAGS record. Its request boundary
publishes full known masks because AX is literal and BH comes from the prior
raw DOS result while BL is overwritten with `$46`. After observing the BIOS
return, `$1abb..$1ac1` overwrites AH/AL with `$01/$1c`, overwrites BL with
`$46`, retains the newly observed BH, and stops at the second `INT $15` at
`$1ac1`. No meaning is assigned to either BIOS function.
The second `$1ac1->$1ac3` result is likewise retained as raw AX/BX/FLAGS and
otherwise ignored. The single-byte `RET` returns to the proven caller at
`$1bf2`, where Eon stops before call `$1bf2->$11a7`. The RET hash is
`ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e`;
the next call hash is
`8e9933fc8751a312d2c247e94987439ae91db1d7e288c14190035b2d6c3da1c8`.
That `$11a7` callee is now admitted through its local RET. Its 49 bytes hash
to `9c04e42a78762c9c76a807afa61b40ffc12e61e5aa5408fa11a252bdb81dba54`.
It clears `$118d`, copies the proven zero word at `$1187` to `$1181`, and
copies the immutable words `$0444/$1178` from `$1179` to `$1183/$1185`.
Those four writes commit atomically. The return resumes at `$1bf5`, where Eon
stops before call `$1bf5->$114e`; that callee is the next opaque boundary.

The `$114e` prefix is now hash-bound through the first far dereference. Its 15
bytes hash to
`00c5baf9b1d28d3216e6375b48f79ace14faac8b8037e6689703c6b510941d9d`.
It restores DS/ES from CS, selects child destination `$10dc`, and loads the
original far pointer at `$10e0`: `$0000:$0070`. Execution stops before the two
`MOVSW` reads at `$115d`. The typed boundary requires two external words;
Eon does not synthesize IVT contents or the later handler installation.
An exact typed observation may now supply those two words. The 27-byte suffix
hashes to
`7744cad5e7d132e889a6b64095bd9a8c0d61726b46399e549c923ffa459603ff`.
It copies the observed words to `CS:$10dc/$10de`, atomically installs far
pointer `CS:$11d8` at `$0000:$0070`, writes byte one at `CS:$112c`, restores
the saved registers, and returns to caller `$1bf8`. Execution stops before
call `$1bf8->$12a0`; no interrupt invocation or timer behavior is inferred.
The `$12a0` callee is now entered through its 13-byte prefix (SHA-256
`31e40c32854737bd7eb5e63cfdf1da8d6a4b592793993f02ae1eef102f0d85b4`).
It clears DF, selects destination `CS:$1266`, loads DS=0 and SI=`$0024`, and
stops before two far `MOVSW` reads at `$12ad`. The next typed boundary names
exactly two external words at `$0000:$0024`; no BIOS video vector is invented.
After that observation, the 19-byte suffix (SHA-256
`5f72f7b8f67574d774c5ba8e480cd8257accfab90651d94836d356edbe738861`)
copies the words to `CS:$1266/$1268` and atomically installs `CS:$126a` at
IVT cell `$0000:$0024`. The non-mode-1 caller's next 10 bytes (SHA-256
`a111bf870ff60815e5d9f6a8c5d3a765335dcc8d77e1b0034b185b0872a3ec4d`)
test the established mode byte and reach call `$1c02->$1ada`. Execution stops
before that call. That second invocation now reuses the same hash-bound `$1ada`
callee contract: function `$0004` through INT `$91`, followed by the `$0487`
palette routine and its sixteen individually typed BIOS INT `$10` results.
Its exact return re-applies the mode-2 `$b800` video segment when selected,
takes caller jump `$1c05->$1c0a`, restores DS/ES from CS, and stops before
call `$1c0e->$135e`. The separate mode-1 palette-copy boundary at `$0fc6` is
unchanged.
