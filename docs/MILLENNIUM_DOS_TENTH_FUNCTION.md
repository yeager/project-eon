# Millennium DOS tenth-function native continuation

This document records a manually recompiled, non-semantic continuation for the
longest recovered Millennium DOS function-table handler. It applies only to
the English `2200AD.EXE`, 54,391 bytes, SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`.

The eight-byte table record for raw action `$44` points to `$7384`. The exact
handler bytes through `$740e` are already admitted by
`MillenniumDosGameFlow`. `MillenniumDosTenthFunctionSession` turns that bounded
control flow into typed continuations; it does not assign an action name,
accept an SDL key, or claim the dispatcher reached the handler.

The session requires explicit observations for both reads of word `$a19e`,
bytes `$da39`, `$da06`, `$da09`, and `$da41`, word `$07da`, every direct call
return, the zero flag after `$09fa`, and BL before the encoded shift. Every
observation includes its original instruction or branch address and is
rejected out of order.

Only literal writes that occur after all preceding call returns are retained:
`$da30 := 0`, `$dad7 := 0`, code-local `$6e2f := 1` then `0`, and on the long
busy arm `$da41 := 0` plus `$da42 := $80`. Previous values remain unknown
except for the code-local one written earlier by the same session. No original
executable, archive, or save byte is changed.

The reconstructed graph stops at each call to `$d0c9`, `$7b47`, `$731a`,
`$7a9d`, `$4140`, `$7bcb`, `$a2a0`, `$09fa`, `$4111`, `$be28`, `$0b9d`,
`$0ae3`, and `$4bf7` until its exact return is supplied. The `$da06` path may
repeat `$731a`; the `$09fa` path may repeat the `$7bcb/$a2a0` tail according
to explicitly observed ZF and BL. Neither call effects nor return values are
emulated.

This is an offline recovery entry. Future runtime integration must retain the
authenticated executable backing, prove the preceding `$76f1` dispatcher
call with genuine evidence, and revoke the session before releasing its media.
Until then it provides no gameplay, input, rendering, audio, save, or parity
capability.
