# Experimental recorder candidate manifest

This manifest records the independently reviewed Millennium DOS v21 observer
candidate. It is provenance for a future pin decision, not an approval to run
a capture, read original media, update a required digest, or claim a recovered
runtime path.

## Candidate identity

| Field | Value |
| --- | --- |
| State | `INDEPENDENT_REVIEW` |
| Protocol | `v21-int93-installation` |
| DOSBox-X source revision | `234797680781567e18c374c9e62da24de5423db0` |
| Observer patch SHA-256 | `eb9f21bd22b6d7105137b1c0495d87b02a894d4a5a2d8533d1dce81ba6aa793c` |
| Candidate binary SHA-256 | `26acf29a06ef53abb876b04d155540e38370daf5beb85fc8c51ffcd08bb98fce` |
| Candidate binary bytes | `128754232` |
| Required pinned v21 SHA-256 | `18ec0ead7d08deeca694fbbe8155d5f5e6a99562adaea22fe914a691961fe1f1` |

The source tree, patch and binary remain external under
`/home/yeager/.cache/project-eon-tools/`. Neither the candidate nor its source
is distributed, packaged or committed here.

## Rebuild record

The candidate was built on 2026-09-02 from the stated source revision with
`g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0`. `TMPDIR`, `TEMP` and `TMP` were scoped
under `/home/yeager/.cache/project-eon-tools/recorder-recovery/build-tmp`.
The static-SDL link used the configured DOSBox-X libraries plus a trailing
`-lGL`. A no-media `dosbox-x -version` smoke test with the observer output
variable set produced no sidecar.

## Pin decision prerequisites

The candidate remains rejected by the locator. A `PINNED_RECORDER` decision
requires all of the following, separately recorded and reviewed:

1. A reproducible build manifest with command, host toolchain and resulting
   binary identity, retained outside media and the repository checkout.
2. A protocol test that demonstrates the candidate's own bounded output on an
   approved non-media fixture or independently reviewed equivalent; synthetic
   sidecar-parser tests alone are insufficient.
3. An explicit release-identity and pin record that explains why this binary,
   rather than the currently required v21 executable, becomes the reviewed
   recorder for the finite protocol.
4. A final independent review of those records before any required digest,
   locator result or capture helper admission is changed.
