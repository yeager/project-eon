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

The caller helper is now opened through its exact `$cc4e..$ccbc` prefix
(111-byte SHA-256
`d75209624e29337eaf228ef56d678c3a5738317aba17a57670c6230424cb7f60`).
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
