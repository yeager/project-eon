# Millennium DOS external-transfer admission

Jump boundaries are not local call returns. The reusable
`MillenniumDosExternalTransferAdmission` therefore requires an explicit,
ordered entry observation binding the original source instruction to its
hash-proven target. A return is accepted only for contracts with a proven
terminal `RET`, at a later sequence, with an explicitly observed nonzero
destination. The destination is retained verbatim and never inferred.

The current contracts cover F9 `$7381 -> $73cc` with the two proven terminal
RET instructions `$73e6` and `$740e`, the F2 reset wrapper `$7228 -> $702c`
with RET `$7040`, and the one-way F2 tail jump `$7253 -> $0bdf`. The tail jump
has no return contract and consequently rejects every return observation.
Checkpoints are value-only and contain no executable bytes, call effects,
input, or gameplay meaning. Owners must discard them on reset and hide them
during source revocation.

The F2 callback runtime now owns the reset-wrapper admission. Its jump-entry
API requires a nonzero sequence and the exact `$7228 -> $702c` edge before
the existing five explicit call-return observations may advance. At the
resulting `$7040` local RET, a separate API records the exact return
instruction, a later sequence, and the externally observed nonzero caller
destination. The callback checkpoint returns a copy of this transfer state;
reset destroys it and host revocation rejects both entry and return. The
runtime does not guess the caller destination or treat the normal one-way
`$7253 -> $0bdf` tail jump as returning.

The tail's first caller-connected exit is modeled separately. After explicit
entry at `$7253 -> $0bdf`, an observed nonzero `$07d8` reaches the hash-proven
RET at `$0be6`. Only there may the host submit a later return observation with
its explicit nonzero destination. The BDF checkpoint retains the transfer by
value; source revocation rejects the return and hides that checkpoint. No
destination, service effect, or subsequent caller behavior is inferred.

The BDF service has two further terminal external jumps. Mode `$02` reaches
the exact `$0c4b -> $11f7` edge; every other nonzero mode reaches
`$0c4e -> $0caa`. Each edge has its own contract and requires an explicit
nonzero sequence-bearing entry observation matching both addresses. Static
evidence does not establish a return instruction for either target, so both
contracts are deliberately one-way and reject every return observation. The
BDF checkpoint exposes only a value copy of the admitted edge. Reset destroys
that admission, while source revocation rejects new observations and hides
the checkpoint. Neither target gains input, rendering, audio, or inferred
caller-continuation capabilities.
