# Millennium DOS third-function native continuation

English `2200AD.EXE` SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`
maps scaled dispatch index 2 to `$6faa`. The typed continuation observes the
`$a19e` guard and `$da27`. A zero low byte enters the exact
`$6fbe -> $09fa` / `$6fc1: SHR BL,1` loop and returns at `$6fc5` only after an
even observed BL. A nonzero low byte reports the encoded `$6f98 := $712a` and
`$6e98 := 0` writes, then requires exact returns from `$6fd4 -> $4d2c` and
`$6fda -> $4d36`. It observes `$da27` again, reports the low-byte write to
`$6e95`, and stops before `LDS SI,[$0112]` at `$6fee`.

The far pointer, selected records, calls, and BL are external runtime facts.
No pointer is dereferenced, call result manufactured, or gameplay/input
meaning assigned. Admission requires the authenticated `$d40a -> $76f1`
dispatch boundary with index 2 and handler `$6faa`.
