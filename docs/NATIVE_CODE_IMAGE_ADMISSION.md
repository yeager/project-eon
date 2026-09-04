# Native code-image admission

`NativeCodeImageAdmission` is the read-only bridge between the complete
disassembly ledger and native recovery components. A caller names an exact
image and range ID while supplying an already verified release-media session.
Admission requires the release SHA-256, source-leaf SHA-256, parser-profile
offset and length, address basis, and load status to match the compiled
manifest contract.

An accepted result is a const span backed by `VerifiedReleaseMedia`. It does
not unpack to disk, install, mutate, or create another owned media buffer.
The view cannot outlive that media session. Runtime components should retain
the media owner before retaining a view.

The API fails closed when:

- the image or range belongs to another release;
- the parser profile, hash, offset, or length differs;
- the verified media cannot supply that exact leaf and interval;
- the image is disk-relative or has an unproven load status; or
- a scanner candidate is only a container with mapped members.

Byte-complete boot listings remain valuable preservation evidence, but they
cannot become executable runtime spans until a separate load map and entry
are proved. Image-relative Atari files remain in the preservation manifest;
only ranges with a corresponding exact mapped parser profile enter the native
admission registry. No candidate status is upgraded by this API.

The release coordinator now uses this boundary for the Millennium DOS
`2200AD.EXE` native-process startup instead of repeating its executable hash.
The Deuteros Amiga factory requires both mapped main/title ranges, and the
Deuteros Atari factory requires both mapped Replicants stages, before it
constructs the existing adapter. These changes do not alter presentation or
execution. Adapters that require a complete mutable parsing context retain
their prior private disk ownership; the admitted views are temporary
provenance gates and never outlive `VerifiedReleaseMedia`.

`native-code-image-exclusions.json` closes the registry audit: every complete
manifest image must occur either in the compiled descriptor registry or in
that explicit exclusion ledger, never neither or both. Current exclusions
are extracted Atari and Spanish DOS members whose parser candidate remains a
whole-disk container. Their listings remain preservation evidence, but no
exact same-leaf parser range exists for runtime admission.

## Runtime diagnostics

The CLI runtime-diagnostics document and the F10 developer panel expose a
copy-only registry summary. It contains the number of mapped descriptors and
explicit exclusions. When one active native session maps unambiguously to one
descriptor, it also contains only that descriptor's image ID, range ID,
address basis, and load status. The binding is exact-release and typed-session
specific; ambiguous sessions report no active image instead of choosing one.

This surface intentionally omits paths, release/source hashes, archive-member
names, original bytes, and byte spans. `RuntimeHost` also drops the active
binding throughout source revocation, while retaining the static registry
counts. The summary is diagnostic only and cannot admit a code image, advance
a session, or grant input, rendering, or audio capabilities.
