# Bounded memory-transfer observations

`BoundedMemoryTransferSession` is a platform-neutral observation owner for
large, statically bounded copy loops. Its immutable contract binds one exact
instruction address, source and destination bases, independent strides,
element count and width, maximum chunk size, and address-space limit. Contract
construction rejects empty, overlapping, unsupported, or overflowing ranges.

Each chunk must provide a nonzero, strictly increasing sequence; the exact
next element index; the contract instruction; the corresponding source and
destination addresses; and one or more explicitly observed typed values.
Empty, duplicate, gapped, reordered, oversized, wrongly addressed, or
out-of-width chunks fail without advancing the session. Completion means
exactly the declared element count has been observed; the utility never reads
memory or fills a missing value.

The checkpoint is a value copy containing the immutable contract, next index,
last sequence, completeness flag, and accepted effects. It contains no media
borrow, executable span, callback, or device capability. Runtime owners remain
responsible for hiding or destroying it during release revocation.

The initial integration target is Deuteros Amiga's proven `$38a28` longword
loop: `$a20` elements, four-byte source/destination strides, destination
`$1c482`, and chunks of at most 256 observations. Its selector-dependent
source base is supplied by the Deuteros boundary owner; this generic utility
does not choose a selector, source, value, or caller continuation.
