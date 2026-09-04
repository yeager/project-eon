# Millennium DOS first-function native continuation

English `2200AD.EXE` SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`
maps scaled dispatch index 0 to `$6f9a`. After the exact `$6f9c -> $d0c9`
return, the continuation executes the hash-proven local `$771d` setup: it
reports selector, selected-record pointer, and screen descriptor writes. It
then requires exact returns at `$774d -> $5b1f` and `$7750 -> $7d60`.

The selected record is inside the authenticated executable and its verified
bytes 2 and 36 are `$11` and zero. Accordingly, the continuation reports the
encoded `$da09`, `$da39`, and `$da13` writes, skips the unreachable conditional
calls, requires `$777f -> $0b0c`, and models only observed BL values in the
`$7782 -> $09fa` wait loop before return `$7789`.

All callees remain opaque. No call effects, input, external pointers, or
gameplay meaning are inferred. Admission requires the active authenticated
`$d40a -> $76f1` boundary with index 0 and handler `$6f9a`.
