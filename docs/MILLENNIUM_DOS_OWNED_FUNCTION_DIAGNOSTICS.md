# Millennium DOS owned-function diagnostics

The owned F6, F7, F8, F9, and F10 sessions have a common copy-only diagnostics
schema. It records the exact English release hash, exact `2200AD.EXE` hash,
runtime session kind, named function-map ID, proven table index and handler,
and the current typed boundary. The stable mode string is
`typed-observation`.

The builder fails closed unless the runtime declaration has no input, audio,
or presentation capability and its release, language, platform, media hash,
index, handler, and named function-map address all agree. It accepts only the
five currently owned handler profiles. It contains no paths, source bytes,
mutable sessions, input mapping, gameplay labels, or callable addresses.

This builder does not claim active reachability. The coordinator populates its
boundary directly from the currently owned session, launcher/native/host
facades return only a copy, and reset, replacement, or host revocation hides
the diagnostic. Native behavioral tests cover valid F7 and F9 declarations,
a mismatched handler and broadened presentation capability.
