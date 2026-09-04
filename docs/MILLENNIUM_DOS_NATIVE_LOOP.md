# Millennium DOS post-overlay native loop

This document preserves Project Eon's manually recompiled continuation after
the second post-GX private `INT 91h` boundary. It applies only to the English
`2200AD.EXE`, 54,391 bytes, SHA-256
`427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57`.
It is a static recovery entry and is not evidence that `TITLES.EXE`, the sound
driver, DOS EXEC, the GX loader, or the preceding private call reached it.

`MillenniumDosPostOverlayLoopSession` first requires an explicit return at the
existing `$0129` interrupt site. The previously observed `$da05` selector is
constructor input. Selector `$01` admits the hash-verified `$044e` local
follow-up, whose only deterministic effect is `$da05 := $01`, and then reaches
the caller-connected `$d39d` block. Every other selector stops at the first
palette BIOS boundary, `INT 10h` at `$0476`; the session does not synthesize
sixteen BIOS returns or a palette-device effect.

The `$d39d..$d3e1` block contains fifteen direct calls. Each return must name
its exact call and following instruction address. After call ordinal six, the
session requires the observed AL tested at `$d3ba`. A zero AL additionally
requires the original byte read at `$d3be` from `$07f9`; only then is the
encoded `$07f9 := observed XOR $01` effect retained in private state. No call
target is executed by Eon.

Call ordinal fourteen is the original `$0f05` action poll. Its returned AL is
an explicit register observation, not an SDL key or inferred control. Zero and
rejected dispatch paths repeat only the four encoded calls at ordinals 11–14.
A nonzero action follows the exact `$d3e2..$d412` dispatch bytes:

- `$0b` stops at call `$d40e -> $11a4`.
- Other values first require the native `$da3a` byte read at `$d3e8`.
- A nonzero guard repeats the four-call poll tail.
- `$0c` with a zero guard stops at `$d3f4 -> $d570`.
- `$3b..$44` with a zero guard stop at `$d40a -> $76f1`, retaining only the
  zero-based table index.
- Every other value repeats the poll tail.

The terminal calls remain opaque boundaries. The implementation assigns no
function-key names, gameplay meaning, input mapping, handler return, frame,
audio, save mutation, or parity claim. It holds a non-owning read-only source
span, so a future runtime owner must retain authenticated backing bytes and
must revoke this session before releasing them.
