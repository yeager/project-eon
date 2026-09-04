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

The separately entered restoration routine beginning at `$7455` is outside
this boundary because no edge from this handler has been proven. The session
does not assign gameplay meaning or host input to the table record, infer
opaque call behaviour, mutate original media or saves, or claim dispatcher
handoff or parity. Runtime integration must preserve authenticated executable
ownership, require the proven `$76f1` dispatch observation, and revoke the
session when its backing media is released.
