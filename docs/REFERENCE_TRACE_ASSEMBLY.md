# External reference-trace assembly

`tools/record_reference_trace.py` assembles already recorded external evidence
into a new capture directory. It is not an emulator, debugger, recorder,
replay tool, trace normalizer, or runtime input. It never starts game code or
creates a `.eontrace` event from inferred behaviour.

The output remains user-owned preservation evidence. Keep it outside this
repository: no trace, event log, emulator configuration, screenshot, audio,
ROM, disk image, or original game data may be committed or packaged. A receipt
with `status: assembled-not-admitted` is deliberately not a compatibility or
emulator-correctness claim. Admission remains the separate, bounded
`project-eon --data ... --game ... --platform ... --reference-trace ...`
validation described in [REFERENCE_TRACE_FORMAT.md](REFERENCE_TRACE_FORMAT.md).

## Recorder metadata template

Before recording, use the registered adapter identifier to print an
instructional (intentionally invalid) metadata skeleton. This prevents a
recorder setup from guessing its release/platform/stage identity:

```sh
python3 tools/record_reference_trace.py \
  --metadata-template deuteros-amiga-en-title-display-v4
```

The `<actual-…>` placeholders must be replaced with hashes and UTC timestamps
from the real external recorder and its retained configuration, command tail,
and input timeline. The command emits neither events nor media bytes; its
output is rejected unchanged by the assembler. It accepts no assembly paths in
template mode, and it never starts an emulator.

## Inputs and output

All four paths must be absolute. `--source-release`, `--events`, and
`--metadata` must be non-symlink regular files. The output directory must not
already exist. It is safe to use a new sibling directory beneath the same
user-owned preservation collection, because inputs are regular files and the
assembler never writes their paths.

```sh
python3 tools/record_reference_trace.py \
  --source-release /absolute/path/to/user-owned-release.zip \
  --events /absolute/path/to/external-recorder-events \
  --metadata /absolute/path/to/capture-metadata.tsv \
  --output /absolute/path/to/new-capture
```

The assembler opens the original release read-only without following a
symbolic link, identity-checks every opened metadata and event input, records
the original release SHA-256 and byte length before copying the external event
stream, then rehashes the same open file descriptor. It refuses to issue an
output directory if the recognised source identity changed. This protects
the assembler's path and detects concurrent changes; it is not a claim that it
can prevent a privileged third party from modifying the user's file.

Only the external event stream is copied, to `events.eontrace`. The assembler
never extracts or copies the original release. It writes `manifest.eontrace`
and `receipt.json` atomically into a new mode-0700 capture directory. The
receipt binds the source before/after identities, event and manifest hashes,
and tool hash, but contains no original bytes.

## Metadata

Metadata is an LF-only UTF-8 file using `key<TAB>value` records. It supplies
the human/recorder provenance that the assembler cannot observe:

```text
format	project-eon-reference-trace-v1
game	millennium
platform	dos
language	en
capture_start_utc	2026-08-29T00:00:00Z
capture_end_utc	2026-08-29T00:01:00Z
emulator_name	Recorder name
emulator_version	Recorder version
emulator_sha256	<lower-case sha256>
config_sha256	<lower-case sha256>
command_tail_sha256	<lower-case sha256>
input_timeline_sha256	<lower-case sha256>
```

The assembler owns and derives `event_file`, `event_size`, `event_sha256`,
`source_release_sha256`, and `source_release_size`; metadata that includes any
of those fields is rejected. It looks up the derived source hash and size in
`docs/release-manifest.json`, then rejects game, platform, or language values
that do not match that one recognised release.

For v2/v3, metadata must additionally name exactly one registered `adapter`. The
tool accepts only the registered adapters in the public trace format and enforces
each adapter's full outer-release hash/size plus game/platform/language
identity. The two physical-media adapters also
require their exact `source_media_sha256` and `source_stage_sha256` metadata.
The assembler checks manifest provenance and file bounds; the Project Eon CLI
remains the authority for full event-schema validation and trace admission.

If any validation or source rehash fails, the staging directory is removed and
no capture receipt is issued. Do not repair a rejected capture by editing
event order, results, timestamps, or source identity.
