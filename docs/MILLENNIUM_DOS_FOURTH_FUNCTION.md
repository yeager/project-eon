# Millennium DOS fourth-function native continuation

For English `2200AD.EXE` SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`,
scaled dispatch index 3 selects `$72f9`. The typed continuation observes the
`$a19e` guard, follows the encoded AL=2 jump to `$ba5e`, and stops at each
opaque call. Only after exact `$ba61 -> $4d2c` return does it report
`$ba64: [$da13] := 7`; only after `$ba69 -> $9dd5` return does it report
`$ba6c: [$da1e] := 9` and `$ba71: [$75a9] := 0`, then return `$ba76`.

No call effect, runtime value, gameplay meaning, or input is inferred.
Admission requires authenticated media and the active `$d40a -> $76f1`
dispatch with index 3 and handler `$72f9`.
