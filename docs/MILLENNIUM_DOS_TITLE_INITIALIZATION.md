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

The initial mode-one path reads its first RGB triplet from the exact 48-byte table at
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
stops before each BIOS result. The
mode-one `$0107 := 1` write commits atomically with the state transition; the
other route advances state without inventing an empty memory batch.

## Complete palette request loops

Each BIOS return is a typed, ordered observation. Mode one accepts only
`$046d -> $046f`; the other route accepts only `$0497 -> $0499`. Raw AX and
FLAGS are retained for all 16 observations, but the original loops do not use
those returned values. They restore/decrement their own loop counter and read
the next request from the current palette. Initial requests use the
hash-verified `TITLES.EXE` table. The repeated mode-one invocation follows the
owned TITLE.LIB copy into child `$014c`; it must use those latest local bytes,
including the first RGB triplet, rather than the original executable table.

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
`$0e59`, and the original clears 768 child bytes at `$014c..$044b`. The LDS
at `$0f9e` restores this pointer before reading the header at file `+$0020`.
Its delta `$25d7` selects the complete palette at `[$25f9,$28f9)`, inside
the verified leaf. The earlier `$4865` overread diagnosis incorrectly kept
the directory-relative pointer after LDS and has been removed.

The recovered `REP MOVSW` copies all 768 original bytes into the cleared
child range. Their SHA-256 is
`b6dd34314102e429fdd98390b1fda27d3ea94d16bfcefa2983e3e319a2a20eae`.
Runtime batches preserve the final sequential clear-and-copy state. Setup
requires the first DOS read to have loaded both the directory and palette;
later reads at higher destination addresses cannot fill a missing prefix.
The descriptor driver applies the same loaded-extent rule to its operands.
The palette path then reaches BIOS INT `$10` at `$0fd8` with AX=$1012, BX=0, CX=$00ff,
ES=child CS and DX=$014c. Copying 768 bytes does not change the original
BIOS count of 255. The palette path at file `+$0e6b` (115 bytes) hashes to
`ae8442b1bbef14712cd11183209f523f1d37b861d0d01ee2c60df7eabfbbef86`;
the 22-byte setup/return span at `+$0ec8` hashes to
`82a8f977cb2d26c2699e362afb25e7472edc50abc5359152327a765d54322215`.
Only a separately supplied, correctly sequenced BIOS result at `$0fda`
allows the return to `$1bef`. No BIOS success or visible palette is inferred.

The other mode
follows the exact 14-byte epilogue through `RET $0f6a` (SHA-256
`66c5cf6c6a51f92ec93650c960546e562bd382e96d85f93ab15df8b5a82982b0`).
That return is owned by the still-active `$1bec -> $0e5f` call, so execution
resumes at `$1bef` and stops before call `$1bef -> $1aac` (call-byte SHA-256
`802c3d3da0e9eebe7f5ccfaac938d6c4eba76d4dc6ffb190ef9d719d0a0c4044`).
The `$1aac` title/driver setup remains opaque. No BIOS or private ABI result is
claimed by the local continuation.

The non-mode-1 path now enters `$1aac`: it restores DS/ES from CS and calls
`$10ec`. That callee's exact ten-byte prefix (SHA-256
`a00fdf978777b8b563efc5c4d39f3e3fbafea0ef764f134c6ee308d9927b6e73`)
clears DF, loads `AX=$3500`, and reaches DOS `INT $21` at `$10f4`. Eon stops
before the get-vector result; vector BX/ES, later interrupt replacement and
the two BIOS calls remain unobserved. Mode one reaches this same setup only
after its distinct `$0fd8` palette BIOS result has been admitted.
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
before that call. Mode one instead reaches `$1c07->$1ac6`, requests private
function `$0004`, and repeats `$044c` with the current library palette.
The complete selector `$1bfb..$1c09` is bound at file `+$1afb`, 15 bytes,
SHA-256 `b4c5b260c0b7061bc5c179aafb00bdf53a6be8252985cea8d305ed389724d663`.
The non-mode-one second invocation reuses the same hash-bound `$1ada`
callee contract: function `$0004` through INT `$91`, followed by the `$0487`
palette routine and its sixteen individually typed BIOS INT `$10` results.
Its exact return re-applies the mode-2 `$b800` video segment when selected,
takes caller jump `$1c05->$1c0a`, restores DS/ES from CS, and stops before
call `$1c0e->$135e`. Mode one's preceding palette BIOS boundary at `$0fd8`
remains distinct from these repeated palette calls.
The 42-byte `$135e` callee (file `+$125e`, SHA-256
`c35f93db0d58443d76374684ed2c54ce78ddb7fc8e01ffa809026382450b4868`)
selects the already-owned first DOS allocation (`$010c/$010e`) for mode one
and the second (`$0110/$0112`) otherwise, stores its
far pointer at `CS:$1341/$1343`, stores CS at `$134b`, restores DS, and
returns. The `$0ff3` request prefix (16 bytes, SHA-256
`d17cc200504c832c3062e1c6951c753a8819c0fd1255b7273c28b3fcf1f3e363`)
atomically writes CS into request record `CS:$0fe9`, selects function `$0019`,
and enters the common `$0122` wrapper. Execution stops at the typed INT `$91`
result boundary `$0127`; no graphics/setup result is inferred. A fresh typed
raw AX/FLAGS result now returns through `$0129` and `$1003`, while retaining
the original startup result independently. The caller then loads AX=0 and
stops before `$1c17->$1725`.
The `$1725` caller prefix and `$1390` callee prefix are now separately
hash-bound (`646ada76ab8f0b370cd3e1f3001cf2e21a5105bbcf650cf6239bf801853754dd`
and `f6be40d902e1d36bd640df417e6a3b8e813b4fce0c7bbf7801a33ae44d60a897`).
For AX=0, the verified entry count admits the route, the owned relocated
directory and allocation pointers establish DS:SI and ES:DI, and execution
stops before two external words are read at `$13aa`. The typed boundary names
that exact far source; no descriptor contents are synthesized.
For this admitted `TITLE.LIB`, that source is file offset `$4813` and the two
words are exactly `$0006/$0000`. A contradictory observation fails before any
checkpoint mutation. The `$13aa..$13cc` suffix (SHA-256
`e8b21803c3739aac65b59a9919f03c97d0d55daf7fd2a35e7567973765724921`)
stores normalized pointer `$3000:$0006` at `CS:$138c/$138e` and stops before
the first external record word at `$3000:$001e`.
That word is provenance-bound to `TITLE.LIB+$001e` and must equal `$0140`.
The single-word observation contract rejects any other value atomically,
retains the admitted raw word, and advances only to the second external word
at `$13d0`, source `$3000:$001c` (known media value `$00c8`).
That second word is now admitted with the same provenance check. The exact
18-byte span through `$13e1` hashes to
`787613791d00d3ae372e3ec9b7b02d56a0704b9e14b44e2d6874b125927befe6`;
it atomically stores `$00c8/$0140`, multiplies them to `$fa00`, and stops
before subtracting the external word at `$13e2`, source `$3000:$001a`.
That genuine word is `$0000`. The 7-byte subtraction/store span hashes to
`0653c7fb33f8d3c60d973b7c038f4c724ffd194abd7f21990762340477246ed4`,
keeps `$fa00`, atomically stores it at `CS:$138a`, and stops before the first
external byte read at `$13e9`, source `$3000:$0007`.
The dedicated byte observation verifies genuine `TITLE.LIB+$0007` value
`$23`. Exact bytes `$13e9..$13f1` (SHA-256
`ed46676eb54a03e725cbb96371e4fd13852a350ba5b027e5c59dda07c78b8ecf`)
increment it and atomically store `$24` at `CS:$1389`. Execution stops before
the next external byte at `$13f2`, source `$3000:$000a`.
That byte is genuine `$00`. The 20-byte branch suffix hashes to
`172d30853354efec879699618dd36f3fbda28ddd07d8ea66bc2a23ace6ee6753`.
The caller's next 39 bytes (SHA-256
`d095399b2a968131f10112f1895b1449f6d1572052c032e48289218e5d07355b`)
copy the proven dimensions into its request and reach private INT `$91`
function `$0006`. Execution stops at result address `$0127`.

## Descriptor request return

The function-`$0006` return is admitted only as a fresh typed observation at
the exact `$0127 -> $0129` boundary. Raw AX and FLAGS are retained without
assigning either value graphics semantics. The six-byte common-wrapper
epilogue at `$0129..$012e` hashes to
`a6e3a351304f487a18bc22e460403bfcdb5e702831b037aa0a90a56bf3cf7baf`;
the one-byte helper return at `$1767` hashes to
`ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e`.
The caller then reaches `$1c1a -> $1004`.

The exact 16-byte `$1004..$1013` prefix (file `+$0f04`, SHA-256
`23f2112307ea2992920c08508de31bd2e689247c3444791c443823cab3c6438e`)
loads `ES=CS`, selects request record `CS:$0fdf`, writes CS to its word at
`$0fe7`, selects function `$001a`, and calls the common `$0122` wrapper at
`$1011`. The runtime commits that single instruction-defined word atomically
with the state transition and stops at the next typed INT `$91` result at
`$0127`. The function-`$001a` service semantics remain unknown, so its return
is admitted only as a typed raw observation: AX, FLAGS, and the complete
ten-byte request record at `CS:$0fdf..$0fe8`. A short or detached record is
rejected before any runtime byte changes.

## Owned 37-record descriptor loop

After the explicitly observed function-$001a result, the local caller enters
$1941 and executes 37 descriptor invocations. The complete $1941..$1967
span (file TITLES.EXE+$1841, 39 bytes) hashes to
f22c9595e6b1c590877b354721e6102d8107c5d6c7336b2d3491cfcaf3f8a627.
Each invocation retains the saved loop count and $0170 output stride. Its
$1960 call returns to $1963; no intervening private interrupt is admitted.

### Corrected source provenance

The directory is TITLE.LIB+$4813, not file offset zero. The read at
$3481:$000f therefore names file +$481f. Earlier recovery incorrectly used
file +$000f and admitted a false $5050 pointer; that path and its automatic
lookup alias have been removed. The actual first normalized record is
$3294:$0001 (file +$2941). The second is $32a1:$0006 (file +$2a16).
The directory entry for index i is +$4813 + 12*i, for i=1..37. Every entry,
normalized record, header, payload operand and lookup is validated against
the full 18,907-byte library identity:
6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678.
No physical-equivalent segment aliases are accepted.

The mode-four loop uses those same indexes 1..37; directory index 0 is the
separate `P00` setup object and is not a descriptor invocation. Among these
37 records, 33 have header `$06,$02`, three have `$0e,$02` (indexes 29, 30,
and 33), and index 1 has `$0e,$01`. Each carries delta `$00b3`. Their
normalized lookup tables run from
TITLE.LIB `+$2a12` through `+$4810`; the final 256-byte table ends at
`+$490f`, within the supplied leaf. This excludes the exceptional-looking
index-zero header `$07,$23` from mode-four descriptor processing.

All 37 original records select payload decoder branch two and produce
368 payload bytes. This record selector is distinct from the title's global
display mode. The complete-loop regression explicitly supplies global mode
two to exercise its recovered postprocessing; neither supplied English video
driver establishes that mode. A successful MCGA function-zero return has
AH=1, while EGA640 returns AH=4. Mode-one postprocessing now has its own
complete 37-record regression. The EGA global mode-four path now has a
separate hash-bound continuation from `$14f0` through its two header bytes
and lookup-pointer setup at `$15c5`. It checks each header operand against
the admitted library prefix and stops before the table-driven planar decoder
loop. This does not reuse mode one's translation or mode two's expansion,
does not claim mode-three behavior, and does not prove a complete
installed-driver title path; earlier private and BIOS results remain typed
inputs.

### Mode-one postprocessing

The branch and translation body `$149f..$14dc` (file `+$139f`, 62 bytes,
SHA-256 `42a404d94066eaf9e459169575427bb04a594c88fc1b683db6e3574e32b39e5a`)
locate the record's translation table after its payload and optional 768-byte
palette. The `$14d0/$14d3/$14d5` loop reads each decoded byte from the owned
`$010c/$010e` destination, indexes that table, and writes the translated byte
in place. Each of the 37 genuine records produces 368 translated bytes.
The finite execution quantum can stop between bytes; missing initialized
source memory rejects the entire quantum without committing session or memory.
The recovered return at `$14dc` rejoins the existing descriptor caller and
eventually reaches `$1967`.

Directory and normalized record addresses are relative to the admitted
library allocation. The illustrative `$3000` segment is not a requirement
of the native loop. The first loaded prefix must still cover every header,
payload operand and translation table used; a full immutable leaf alone is
not evidence that a short DOS read populated those runtime addresses.

With `EON_DIRECT_DATA_DIR` configured, CTest also registers
`millennium-dos-title-runtime-real-media`. It uses the normal coordinator
and verified direct-media admission to reach `$0fd8`, checks all 768 owned
palette bytes and rejection rollback, then follows typed setup inputs through
the repeated mode-one palette to the private function `$19` request. Its
external BIOS/vector/private returns are explicitly synthetic contract inputs;
the test does not require the unrelated complete archive corpus and does not
claim a captured hardware result.

The independent bounded source audit observed lookup and
ordinary-run branches only; synthetic arithmetic regressions retain escape,
extended-run and wrapping coverage on correctly normalized record contexts.
Synthetic register/byte inputs are never described as captured observations.

### Native execution and bounds

The driver reuses the typed descriptor, record, nibble, run, header and
mode-two loop observers. Each budget unit consumes exactly one pair, word
or byte observation. Budgets are limited to 1..256; the session and owned
memory commit together only when the whole quantum succeeds. Once admitted
from the exact first directory boundary, this path rejects externally supplied
replacement observations. Every resumed quantum revalidates the library hash.
Mode selection other than two remains an external preservation boundary.

The payload destination comes from CS:$010c/$010e; lookup BX is each
normalized record offset plus five. Later mode-two output uses CS:$0110/$0112.
The $16a2 shift doubles the count before REP STOSW: each admitted record clears
92 words and produces 184 mode-two output bytes. Header source arithmetic
and the 256-byte lookup extent remain within the same supplied library.
The exact $163b..$16b2 span (120 bytes) hashes to
9a18a2349e46afad9b814befcaf0a7ddd715f446f21845453a4e4f6a72ad65b7.

### EGA640 mode-four descriptor execution

The mode-four setup binds the runtime spans `$1488..$149e` (file `+$1388`),
`$14e3..$1513` (file `+$13e3`), `$1514..$153c` (file `+$1414`), and shared
normalizer `$013c..$014c` (file `+$003c`); their SHA-256 values are checked
by the native session constructor. The owned loop reads the genuine first header
byte at `$14f0`, second byte at `$14fc`, and word at `$1500`, with all three
reads constrained to the exact normalized record and the loaded TITLE.LIB
prefix. It applies the code's header count, optional `$0300` offset and
`CX:DX` normalization, then returns an explicit boundary at `$15c5` with the
lookup pointer and source/destination context. The `$15c5..$163a` planar
routine is now hash-bound (runtime file `+$14c5`, 118 bytes; SHA-256
`416385b5fd03b0fec92663dea3d60f6d556c6310a4af97db9d5bd52874abba49`) and
emulated with its flag-sensitive bit loop. It clears the bounded destination,
reads the decoded source only from initialized native runtime memory, and
resolves all 256 lookup bytes against the exact record table. Clear and planar
writes commit transactionally per record; the local return advances through
the established 37-record caller. The existing real-media regression does
not yet exercise a mode-four source/output sequence, so decoded pixels and
full mode-four parity remain unverified.

The final loop edge restores count one, decrements it to zero, and exposes
$1967 before RET. The next owned continuation returns to $1c20 and reads
both bytes of CS:$1896 from native memory. The compatibility child loader
already loads the hash-verified executable image at CS:$0100, so file
TITLES.EXE+$1796 supplies this location. The initial word is 10, but the
continuation uses the current owned word and does not substitute that value.
A missing byte or detached caller leaves the session unchanged.

The caller shifts AX right once at $1c23, calls $1931 at $1c25, and enters
the private wrapper through $1937 with AX=$0013. Execution stops at the
actual INT $91 instruction, $0127. The count and request are instruction
effects; no private-service result, patch rendering, title input or
title-to-game handoff is supplied. Existing result observers reject this
distinct caller state.

The public continuation dispatcher still forwards only typed observations.
The owned loop scheduler supplies no new DOS, BIOS or private-interrupt result;
all earlier external result boundaries remain intact. Supplied-media tests
compare the complete 37-record effects across finite frame budgets and test
single-observation resumption, hash rejection and transactional rollback.

## Public continuation boundary

After the admitted `TITLE.LIB` file transaction, front ends submit a single
tagged continuation observation instead of selecting an untyped runtime step.
The tag admits only the already recovered DOS-vector result, setup-BIOS result,
palette-BIOS result,
far-words, far-word, and far-byte observation forms. Dispatch is a direct
route to the corresponding state-machine observer; it never creates runtime
memory, supplies a value, advances a program counter, or converts a failed
observation into a default result. The single dispatcher is coordinator-owned;
the session controller and launcher forward it without reinterpreting the tag.
RuntimeHost rejects the entire tagged boundary during source revocation before
dispatch. This makes the public SDL/CLI route explicit while retaining every
per-observation sequence, address, media-hash, and state check below it.

The deterministic session driver also reports a value-only requirement when
its current stop is one of those typed continuation boundaries. The report
contains the kind, instruction address and, for a far read, exact source
segment, offset and element width. It never contains a byte, word, register,
or BIOS/DOS return result. The library palette BIOS boundary reports its own
palette-result kind; stops outside this recovered tagged set report no requirement.


## Retained executable span identities

These instruction identities remain valid after correcting data provenance.
Offsets are in the hash-identified `TITLES.EXE`; runtime ranges include both endpoints.

| File offset | Runtime range | Bytes | SHA-256 |
|---|---|---:|---|
| `+$1328` | `$1428..$1487` | 96 | `6486e029d2b5a8a720d7fab7c7e675fb56e267a3d7cfc9e328014a153b545a07` |
| `+$1863` | `$1963..$1966` | 4 | `84ec36cbf00b01304cfbd75024c0ac7571a5776b4e364049f3b84ebfe3315612` |
| `+$12d0` | `$13d0..$13e1` | 18 | `787613791d00d3ae372e3ec9b7b02d56a0704b9e14b44e2d6874b125927befe6` |
| `+$12e2` | `$13e2..$13e8` | 7 | `0653c7fb33f8d3c60d973b7c038f4c724ffd194abd7f21990762340477246ed4` |
| `+$12e9` | `$13e9..$13f1` | 9 | `ed46676eb54a03e725cbb96371e4fd13852a350ba5b027e5c59dda07c78b8ecf` |
| `+$12f2` | `$13f2..$1405` | 20 | `172d30853354efec879699618dd36f3fbda28ddd07d8ea66bc2a23ace6ee6753` |
| `+$1306` | `$1406..$1418` | 19 | `a38148b66817871d8731829b2a0703e48b2e7fecb0fee51112be1e8e3b0332d0` |
| `+$1319` | `$1419..$1427` | 15 | `912d067ef688829815594e9fdf4e2ae8f03051cd3be882dc482a02dae032d39b` |
| `+$1328` | `$1428..$1446` | 31 | `dd7abdeaa64d537ee31fb6c4dffe319a7f824226ca44bb33e0f4cb3986560be7` |
| `+$1841` | `$1941..$1962` | 34 | `8ae5339224f631de9dbf852ab43c5553849b37ef00289e0a34055e73a760357a` |
| `+$12cd` | `$13cd..$13cf` | 3 | `30cefd61e3cc968dfe7b7f54ed07251f1fe9ec99fb33bad8b4ae24ce67b80704` |
| `+$1337` | `$1437..$1487` | 81 | `5fab2565b47896f17a9418a67095c43645b61c02f960f8749dbb3d5b9718a725` |
| `+$1388` | `$1488..$149e` | 23 | `7967c8650f118732cc5c884ea6d332a8dbe6dc060e5736088e7b5d0f1fb081ad` |
| `+$1547` | `$1647..$16b2` | 108 | `9ba1e245431578fbac9c3386bea9a102be68fe6700ca057ff5d9af3f819427fd` |
| `+$15b3` | `$16b3..$16e8` | 54 | `24a597122dd6afe0c434683295f0e97ef04e60b67722db2042178f33f2c361ed` |
| `+$1352` | `$1452..$1457` | 6 | `846a82fa183b14b5fd42d6e0c3bdf5c16cf8863e647825e8fd588d705f655756` |
| `+$12f2` | `$13f2..$1427` | 54 | `49b3ef2683a584b6239f382c4dcefd93171d0839f706f9eb79aac1b64b896f61` |
| `+$153b` | `$163b..$16b2` | 120 | `9a18a2349e46afad9b814befcaf0a7ddd715f446f21845453a4e4f6a72ad65b7` |

The second library record's 28-byte header at `TITLE.LIB+$2a16` retains
SHA-256 `712ac8e1ed83e6beb18680960f1d44899a812c0d5271c0ad68016aebf2440d10`.
Its six-byte stream span at `TITLE.LIB+$2a34..+$2a39` retains SHA-256
`f5e49eddff72cad076c01cc7d4b884404224ea78a24ebe659e320951bca14b29`.
