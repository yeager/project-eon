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
