# Millennium DOS fifth-function native continuation

English `2200AD.EXE` SHA-256 `427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`
maps dispatch index 4 to `$7597`. The exact handler loads AL=2 and requires
returns from `$7599->$be28`, `$759c->$0b9d`, `$759f->$4bf7`, and
`$75a2->$0b76` before returning at `$75a5`. Every callee remains opaque,
including the known nested entry calls. No effects, input, or semantics are
inferred.
