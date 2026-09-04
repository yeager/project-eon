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

The Deuteros title-stage route now forwards every typed observation from the
fifth Exec-service return through the controller seed, graphics calls, tail
wrappers, source table, load service and selector to the `$38a28` copy loop.
The coordinator mirrors the local loop in a bounded transfer owner. Each
chunk must first satisfy its exact sequence/index/source/destination contract;
the completed big-endian batch and runtime memory are then prepared on copies
before the title-stage observation and both owned states are committed. Its
batch ID contains the coordinator's load-copy generation, so the generation
can apply once only. Reset destroys the transfer, generation, and RAM owner;
host revocation suppresses both observations and checkpoints.

There is deliberately no API accepting a detached Deuteros checkpoint. The
only route into runtime RAM begins with the already admitted release-owned
title session and traverses every preceding state-machine boundary.

The same owned route continues through the post-copy dispatch-table reads and
the typed command interpreter. The dispatch byte and the proven opcode 7,
10, 11, and 8 write plans become big-endian runtime-memory batches only after
their required operand/pointer/mode/scale observations succeed. Each accepted
opcode starts a monotonically numbered command generation; at most one write
batch can use that generation. Commands that only advance control flow create
no fabricated memory effect. Exact call returns are forwarded to the local
interpreter but likewise produce no write unless its typed plan contains one.
Malformed, duplicate, out-of-order, post-halt, and revocation observations
fail closed without changing the published memory checkpoint.

The direct English Defjam ADF also owns the recovered Millennium bootstrap
relocator. Admission is restricted to the release whose bootstrap range has
its own complete-disassembly image and runtime address basis; the nested
carrier remains excluded. The coordinator applies the 974 authenticated
`$70032..$703ff` bytes to `$66032..$663ff` as generation 1 during the same
transaction that admits the release. The original DBRA over-read at `$70400`
is never inferred: an explicit `$70036` observation supplies its byte and
atomically applies the 975th destination byte at `$66400`. A later-sequence
`$7003c->$6629e` observation alone completes transfer. The custom-chip write
is retained in the value-only relocator checkpoint but is not misclassified
as RAM. Reset destroys the relocator generation and memory, while source
revocation hides checkpoints and rejects both observations.
