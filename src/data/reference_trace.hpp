#pragma once

#include "platform/game_data.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace eon {

// A trace adapter is anchored to a deliberately small subset of the
// declarative recovery map.  This is a diagnostic cross-reference, not a
// guest-to-host dispatch table: the entries identify documented original-byte
// boundaries that a capture observed, but never cause them to execute.
struct ReferenceTraceBoundary {
    std::string id;
    std::string source_address;
    std::string documentation_anchor;
};

// A capture artifact is provenance-only. It is never decoded, replayed, or
// made available to a recovered game session; the admission path records only
// its fixed role, size, and independently rehashed identity.
struct ReferenceTraceArtifact {
    std::string role;
    std::filesystem::path path;
    std::uint64_t size = 0;
    std::string sha256;
    std::string format;
};

// An external reference trace is preservation evidence, not a save state or
// an instruction stream for the runtime.  Version 1 deliberately retains
// only identity and event ordering: Project Eon validates it and reports its
// provenance, but never replays it or derives game behaviour from it.
struct ReferenceTrace {
    std::filesystem::path manifest_path;
    std::filesystem::path events_path;
    ReleaseArchive source_release;
    std::string capture_start_utc;
    std::string capture_end_utc;
    std::string emulator_name;
    std::string emulator_version;
    std::string emulator_sha256;
    // These are opaque capture-side fingerprints. They are printed so an
    // independent preservation reviewer can compare the recorded emulator
    // configuration, invocation and input evidence without Eon opening,
    // replaying, or retaining any of those private capture files.
    std::string config_sha256;
    std::string command_tail_sha256;
    std::string input_timeline_sha256;
    std::string format;
    std::string adapter;
    std::size_t event_count = 0;
    std::uint64_t event_size = 0;
    std::string event_sha256;
    // The two disk-stage hashes are required by the adapters that name a
    // subrange of a physical image. They remain empty for adapters whose
    // outer archive identity is their complete source boundary.
    std::string source_media_sha256;
    std::string source_stage_sha256;
    std::vector<ReferenceTraceBoundary> recovery_boundaries;
    std::size_t adapter_interrupt_count = 0;
    std::size_t adapter_file_count = 0;
    std::size_t adapter_exec_count = 0;
    std::size_t adapter_private_return_count = 0;
    std::size_t adapter_trap_count = 0;
    std::size_t adapter_callback_count = 0;
    std::size_t adapter_frame_count = 0;
    std::size_t adapter_state_count = 0;
    std::size_t adapter_table_count = 0;
    std::size_t adapter_raw_reader_count = 0;
    std::size_t adapter_cpu_count = 0;
    std::size_t adapter_open_library_count = 0;
    std::size_t adapter_graphics_count = 0;
    std::size_t adapter_custom_register_count = 0;
    // Version 3 title-bridge captures retain call and return observations
    // separately. These counters are diagnostics only; they do not provide
    // values to the recovered title-stage runtime.
    std::size_t adapter_exec_return_count = 0;
    std::size_t adapter_open_library_return_count = 0;
    std::size_t adapter_graphics_call_count = 0;
    std::size_t adapter_graphics_return_count = 0;
    std::size_t adapter_custom_register_call_count = 0;
    std::size_t adapter_custom_register_return_count = 0;
    std::size_t adapter_callback_registration_return_count = 0;
    std::size_t adapter_queue_snapshot_count = 0;
    std::size_t adapter_callback_entry_count = 0;
    std::size_t adapter_selector_entry_count = 0;
    std::size_t adapter_local_call_count = 0;
    std::size_t adapter_local_return_count = 0;
    std::size_t adapter_dispatch_snapshot_count = 0;
    // GX startup v2 has two source-byte reads and one overlay return between
    // its raw private return and six caller-local returns. Keep these as
    // separate provenance counts rather than overloading similarly named
    // Amiga title-bridge fields.
    std::size_t adapter_mode_read_count = 0;
    std::size_t adapter_overlay_return_count = 0;
    // v4 Deuteros title-display records are immutable external checkpoints.
    // Preserve their categories independently so diagnostics do not make a
    // complete visual/audio evidence chain look like an empty generic trace.
    std::size_t adapter_display_layout_count = 0;
    std::size_t adapter_bitplane_layout_count = 0;
    std::size_t adapter_palette_checkpoint_count = 0;
    std::size_t adapter_input_checkpoint_count = 0;
    std::size_t adapter_frame_checkpoint_count = 0;
    std::size_t adapter_audio_checkpoint_count = 0;
    std::vector<ReferenceTraceArtifact> artifacts;
};

struct ReferenceTraceValidation {
    std::optional<ReferenceTrace> trace;
    std::string error;
};

// Aggregate provenance wording for the CLI. It exposes only already
// validated counters and never exposes event payloads or runtime state.
struct ReferenceTraceDiagnosticSummary {
    std::string observations;
    std::string disposition;
};

[[nodiscard]] ReferenceTraceDiagnosticSummary reference_trace_diagnostic_summary(
    const ReferenceTrace& trace);

// Read and validate one user-supplied v1 or narrowly declared v2 manifest. The manifest names its
// sibling events file by basename only, pins it by size/SHA-256, and pins its
// source release to one of the scanner's recognised ReleaseArchive objects.
// Both files are bounded, ASCII/LF-only, and are left untouched.
[[nodiscard]] ReferenceTraceValidation validate_reference_trace(
    const std::filesystem::path& manifest_path,
    const std::vector<ReleaseArchive>& releases,
    Game requested_game,
    Platform requested_platform);

} // namespace eon
