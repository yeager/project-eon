# Deuteros Amiga title dependency-chain checkpoint

The active Deuteros Amiga title-stage session now exposes one copy-only view
of its proven dependency chain. It contains the exact title-stage SHA-256,
the current Exec-boundary checkpoint, an OpenLibrary checkpoint only after
that nested session genuinely exists, and a count/terminal summary only when
the custom-chip boundary genuinely exists. `stop_before_address` always names
the deepest currently owned boundary.

The normal recovered opening currently reaches the title stage after its
local prefix and stops before the Exec-base read at `$40456`. Consequently its
checkpoint reports `awaiting_exec_base_read`, with no OpenLibrary or
custom-chip state. It does not preload later states from static profiles.

Coordinator, launcher controller, native-session controller, and runtime host
return this DTO by value only for the active no-capability title-stage state.
Reset, title-display trace transition, wrong state, and source revocation hide
it. The DTO contains no ADF bytes, paths, library handles, device calls,
frames, audio, or input rights and cannot advance any nested session.
