# Millennium DOS sixth-function native continuation

This preservation note covers the English `2200AD.EXE` handler at
`$7415..$7454`, reached by the sixth record in the verified ten-entry action
table. The executable is 54,391 bytes with SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`.

`MillenniumDosSixthFunctionSession` is a typed, manually recompiled recovery
entry for this exact region. It first requires the `$a19e` word observed at
`$7415`; a nonzero value returns at `$741c` without effects. The zero arm
requires exact returns from calls `$741f -> $d0c9`, `$7425 -> $4d2c`, and
`$7428 -> $c980`. Only the encoded AX values zero and `$0022` are exposed on
the first two call boundaries; no result or effect is inferred for a call.

After those returns, the continuation observes bytes `$75a8` and `$75ae` and
word `$75ac` at instructions `$742b`, `$7431`, and `$7437`. It reports the
encoded writes to code-local snapshots `$7412`, `$740f`, and `$7410`, followed
by `$75a8 := $0c`, `$75ae := $00`, and `$75a6 := $3207`. The old values of the
two byte destinations are the explicit observations; unobserved previous
values remain unknown.

The terminal loop requires each exact `$744d -> $09fa` return and the BL value
consumed by `SHR BL,1` at `$7450`. An odd pre-shift BL repeats the call and an
even pre-shift BL returns at `$7454`. Both shifted values and the loop index
are diagnostic facts, not interpretations of the opaque call.

## Recovered restoration boundary

The separately entered restoration routine occupies executable offset
`0x7355`, is 86 bytes long, and has SHA-256
`990dfec0e40229d70d100b1e6d4174f069eed46f95dacbc1d958334314a68525`.
Admission requires those exact bytes in place.

The restoration routine puts the three values saved by `$742b`, `$7431`, and
`$7437` back at `$75a8`, `$75ae`, and `$75ac`. It then follows every statically
proved call boundary at `$7467`, `$746e`, `$7471`, `$7474`, `$747d`, `$7487`,
`$748d`, `$749b`, `$749e`, `$74a1`, `$74a4`, and `$74a7`, including the observed
byte read from `$613a` at `$7483`, before returning at `$74aa`.

The exact six-byte caller at executable offset `0x73c1` (runtime `$74c1`) has
SHA-256 `26aa10af6fc9d62f91ab8e1f922622618d61b0f92af3b48e641e8a5ee400a76c`.
It calls `$cc4e`, discards one stack word after the typed return at `$74c4`,
and tail-jumps to `$7455`. The runtime therefore admits restoration through
this caller boundary; it does not bypass the opaque call.

The caller helper is now opened through its exact `$cc4e..$ce44` span
(503-byte SHA-256
`210807455be0bf5e1309da289444c19a1146d287b5ac4fe4803875f3a71fb9d1`).
The typed path follows calls to `$408a`, `$4d36` with `AX=$0028`, `$0666` with
`AX=$00c1`, and `$05f1`; records the literal word writes `$cbbe := $080f` and
`$cbe1 := 0`; then observes the far pointer loaded from `$0112:$0114`. The
bounded `$cc77..$cc7d` sequence owns `DI` from that pointer, `CX=$0528`, and
`AX=0`, and records the resulting 1,320-word (2,640-byte) far clear without assigning
meaning to its destination. After restoring `ES=CS`, it observes the next
external byte at `$da05` (`$cc80`). The saved byte is retained while exactly
325 bytes at `$da02..$db46` are cleared, then restored at `$da05`. Six word
cells receive literal `1`, and `$da26`, `$da42`, and `$db12` receive `$01`,
`$80`, and `$09`. The continuation stops at the typed `$ccba -> $942c` call;
that helper result and behavior remain external.

After the exact `$942c` return, the local continuation records the literal
`$dae1` byte, the 192-byte `$ca38 -> $12cc` copy, the 5,184-byte zero fill at
`$1384`, three configuration words, the 23 two-byte strided records from
`$caf0`, the 39 strided zero words rooted at `$2aac`, and the remaining literal
bytes through `$2b4f`. These are address/value effects only. Execution stops at
the next typed external call `$cd4d -> $40af`.

After the exact `$40af` return, two `$4241` calls remain typed external result
boundaries. Their observed AL bytes are masked by the original `AND AL,$0f`
instructions and recorded at `$cb86` and `$cb87`. After the exact `$cd60 ->
$cbc0` return, the continuation clears 39 bytes at `$cb1e..$cb44`. It then
observes each `$cd72 -> $cc23` result at the `$cd75` AL boundary. The original
mask, fold, increment, `$03..$08` exclusion, and table duplicate check are
reproduced until exactly 18 distinct accepted bytes have been written within
that cleared range. Rejected values commit no memory effect and repeat the
same typed call. The continuation stops at the second `$cd9a -> $cbc0` call;
no randomness, clock source, `$cc23` source, or `$cbc0` behavior is inferred.

After that exact return, the caller consumes all 38 remaining bytes of its own
table. A zero byte selects `$cdb6 -> $ce83`; a nonzero byte selects `$cdbb ->
$cea1`, with the proved table index or byte exposed as AX. The callees remain
opaque. Seven statically selected iterations additionally require the exact
`$cde9 -> $cbc0` return. Once the loop completes, the encoded one-byte zero
fills cover nine cells from `$5dda` and 28 cells from `$6047`, both at stride
12. Execution stops at `$ce0b -> $4f08`; no callee behavior or layout meaning
is inferred.

After the exact `$4f08` return, the helper restores the already observed F6
bytes and word to `$75ae`, `$75ac`, and `$75a8`. It then requires returns from
`$ce20 -> $0b0c`, `$ce27 -> $7b47` with encoded `AX=$002e`, and `$ce2f ->
$6baa`, recording only the literal `$cb9a := 0` between them. The direct local
call at `$ce42` enters the owned `$cf57` body without manufacturing a return.

The `$cf57` callee is now entered through its exact 38-byte prefix
`$cf57..$cf7c` (file `+$ce57`, SHA-256
`03c26d611fc6e6df65a20e483d304ed0179234640b8964dae324c895568bddae`).
Its first pointer is the genuine-image word `$5dd2` from the separately hashed
18-byte pointer table at file `+$5e9a` (SHA-256
`041c6a544bd1fcdd5823cb6aac8dac8010a43b344d8997d21c3385287ba44e7d`).
The session explicitly observes the runtime words at `$5dd2` and `$cb88`.
Inequality records the encoded `$cb9a := 1` and adds two before the `$03fe`
mask; equality only applies the mask. The resulting word write to `$5dd2` is
owned, followed by the explicit byte at `$5dd4`. The exact resulting AX is
carried across `$cf77 -> $7908`; after its observed return, execution stops at
`$cf7a -> $3f6a`. Later iterations and both callee results remain outside the
boundary.

The typed session exposes only addresses, call targets, proved register values,
and memory effects. In particular, the byte read at `$613a` is not assigned a
gameplay or pixel meaning. Calling the restoration before the completed F6
handler, repeating it, or submitting a detached observation is rejected without
committing another effect. The release coordinator, native controller, runtime
host, and launcher facade all expose the same transition, and source revocation
continues to reject observations at the host boundary.

## Remaining uncertainty

The predecessor that chooses to enter `$74c1` is not yet proved. Consequently
the runtime requires an explicit caller-side transition after the admitted
`$7454` return. The stack word discarded at `$74c4`, the `$cc4e` result, and
helper internals reached by the restoration calls remain separate recovery
boundaries; this work does not infer their rendering, timing, or gameplay
semantics.
