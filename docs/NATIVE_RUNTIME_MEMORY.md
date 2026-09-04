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
