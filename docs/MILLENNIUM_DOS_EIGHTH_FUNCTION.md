# Millennium DOS eighth-function native continuation

The English `2200AD.EXE` (SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`)
maps scaled dispatch index 7 to `$7306`. The typed continuation covers exact
instructions `$7306..$7319`: it reports `$7308: [$da30] := 0`, exposes AL=2
at the `$730f -> $731a` call, and requires its exact return before entering
the `$7312 -> $09fa` / `$7315: SHR BL,1` loop. Odd pre-shift BL repeats and an
even value returns at `$7319`.

Both callees remain opaque. In particular, this session does not splice the
separately recovered `$731a` preflight evaluators into a fabricated native
return. It assigns no gameplay or input meaning, mutates no original bytes,
and makes no parity claim. Runtime admission must retain exact authenticated
media, observe `$d40a -> $76f1` with index 7 and handler `$7306`, and revoke
the span-backed continuation before releasing its media owner.
