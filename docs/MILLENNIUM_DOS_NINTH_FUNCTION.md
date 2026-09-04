# Millennium DOS ninth-function native continuation

The English `2200AD.EXE` (SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`)
maps scaled dispatch index 8 to `$7339`. The typed continuation follows exact
instructions through the jump at `$7381`: guard `$a19e`, opaque calls, explicit
reads of `$da39`, `$da06`, and `$da09`, and encoded writes to `$da30`, `$6e2f`,
and `$dad7`. Every call requires its exact return. The `$da06 >= 9` arm repeats
only after an observed `$731a` return.

The final `$737e` call retains the parser's wrapped target identity `$14124`;
after its observed return, execution stops at the explicit `$7381 -> $73cc`
jump handoff. This session does not infer callee effects, manufacture runtime
bytes, attach input or gameplay meaning, enter the shared `$73cc` continuation,
or claim parity. Admission requires authenticated media and the active
`$d40a -> $76f1` boundary with index 8 and handler `$7339`.
