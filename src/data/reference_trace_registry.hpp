#pragma once

#include "platform/game_data.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace eon {

// The one declarative inventory of versioned trace adapters. It is a
// preservation map, never a guest-code dispatch table.
enum class ReferenceTraceRuntimePolicy {
    diagnostics_only,
    transient_call_free_gx_startup,
    immutable_deuteros_title_display_checkpoint,
};

// An aggregate renderer-neutral trace report shape. It does not select an
// event parser, execute source bytes, or admit runtime behavior.
enum class ReferenceTraceDiagnosticReport {
    generic,
    millennium_dos_gx_startup,
    millennium_dos_title_init,
    deuteros_atari_boot,
    millennium_amiga_bootstrap,
    deuteros_amiga_title_stage,
    deuteros_amiga_title_bridge,
    deuteros_amiga_title_display,
};

[[nodiscard]] std::string_view reference_trace_runtime_policy_label(
    ReferenceTraceRuntimePolicy policy);
[[nodiscard]] ReferenceTraceDiagnosticReport reference_trace_diagnostic_report(
    std::string_view wire_id);

struct ReferenceTraceAdapterDescriptor {
    std::string_view wire_id;
    std::string_view format;
    Game game;
    Platform platform;
    std::string_view language;
    std::string_view release_sha256;
    // Empty only when the versioned adapter has no independently pinned
    // source-media/stage evidence.
    std::string_view source_media_sha256;
    std::string_view source_stage_sha256;
    ReferenceTraceRuntimePolicy runtime_policy;
    // These are recovery-map cross references only. They are never guest
    // dispatch targets or a permission to execute source bytes.
    std::array<std::string_view, 3> recovery_entry_ids;
    std::size_t recovery_entry_count;
};

[[nodiscard]] std::span<const ReferenceTraceAdapterDescriptor> reference_trace_adapter_registry();
[[nodiscard]] const ReferenceTraceAdapterDescriptor* reference_trace_adapter_descriptor(
    std::string_view wire_id);

} // namespace eon
