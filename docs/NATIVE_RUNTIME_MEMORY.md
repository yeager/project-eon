# Native runtime memory effects

`NativeRuntimeMemory` is the canonical destination for recovered native write
effects. It is a sparse runtime-owned byte map, completely separate from the
immutable original-media sessions. An address names either a linear runtime
space or an explicit DOS segment and offset. Every write also declares its
width, byte order, and batch order.

The applier accepts only a named batch marked fully admitted. It validates the
complete batch before changing state: orders must start at one and be
contiguous, locations must match their address-space form and limits, values
must fit their width, and byte ranges in one batch must not overlap. Reusing a
batch ID is rejected. A failed batch is atomic and leaves both bytes and the
deterministic checksum unchanged.

Unobserved addresses do not exist in this model: `read_byte` returns no value
instead of fabricating zero-filled RAM. The copy-safe checkpoint contains only
explicitly initialized runtime bytes. The diagnostics form publishes only the
initialized-byte count, applied-batch count, and deterministic state checksum;
it contains no original-media path, hash, span, or byte.

The first adapters cover two already typed paths:

- a complete `BoundedMemoryTransferCheckpoint`, emitted as big-endian linear
  writes for the Deuteros Amiga `$38a28` longword loop; and
- a Millennium DOS BDF mode-two checkpoint, emitted as little-endian explicit
  segment/offset word or byte writes only after the external transfer has an
  admitted terminal return.

Neither adapter reads source memory. Partial transfer checkpoints, unfinished
BDF sessions, and BDF transfers without an explicit admitted return produce
no effect batch.

## Runtime ownership

`ReleaseRuntimeCoordinator` creates a fresh runtime-memory owner only after a
release has passed normal media admission. Reset destroys that owner, and the
controller/host accessors return no checkpoint while the session is in the
menu or its source is being revoked. Checkpoints are copies; they cannot be
used to mutate or re-submit effects.

The active Millennium BDF mode-two route applies its word/byte effects in the
same transaction that admits the exact terminal return. The transfer
admission and runtime-memory state are both copied first and committed only
after the sequence-bound batch validates and applies. A rejected return,
duplicate batch identifier, malformed effect, or overlap therefore changes
neither lifecycle state nor RAM. The batch identifier includes the admitted
external-transfer entry sequence, so a completed invocation can be applied at
most once. Other BDF modes remain unapplied until their address-space meaning
(including VGA plane selection) is represented explicitly.

The Deuteros `$38a28` adapter remains intentionally disconnected from the
coordinator. Its bounded-copy checkpoint currently exists only inside the
local title-stage service-batch session, while the admitted coordinator chain
does not yet reach or expose that session. Accepting a detached checkpoint
would bypass release ownership and reachability, so there is no public apply
API for it. Integration must wait for the preceding title-stage state machine
to own that boundary and return its copy-safe completed checkpoint.
