# Millennium DOS seventh-function native continuation

This preservation note covers the English `2200AD.EXE` handler at
`$7521..$7596`, reached by the seventh record in the verified ten-entry action
table. The executable is 54,391 bytes with SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`.

`MillenniumDosSeventhFunctionSession` is a typed, manually recompiled recovery
entry for those 118 bytes. It first requires the native `$a19e` word observed
at `$7521`; a nonzero value returns at `$7528` without effects. The zero arm
then stops at each of eighteen direct calls and requires an exact return-site
observation before continuing.

The continuation explicitly reads `$da17`, code-local word `$05ca`, `$da18`,
`$da27`, `$da26`, `$da35`, and `$da37` at their original instruction
addresses. Derived AX/AL values and the observed BX operand are carried on the
typed call boundaries. After the `$06e2` return, BX must be observed before
the encoded store to `$05ca`; that is the only retained effect. Its previous
value is the earlier explicit `$05ca` observation.

Calls to `$4d2c`, `$073c`, `$0666`, `$06e2`, `$05ce`, `$06dc`, `$077e`,
`$070a`, `$0b9d`, and `$4bf7` remain opaque. The state machine does not infer
their return values or effects. It does not attach gameplay meaning or host
input to the table record, modify original media or saves, or claim that the
dispatcher reached `$7521`.

Future runtime integration must retain authenticated executable backing,
admit the preceding `$76f1` dispatch with genuine evidence, and revoke the
session before releasing its source. Until then this is an offline native
continuation, not a gameplay or parity capability.
