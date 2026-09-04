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
