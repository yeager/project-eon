#pragma once

#include "platform/game_data.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace eon {

// The one declarative inventory of versioned trace adapters. It is a
// preservation map, never a guest-code dispatch table.
enum class ReferenceTraceRuntimePolicy { diagnostics_only, transient_call_free_gx_startup };

[[nodiscard]] std::string_view reference_trace_runtime_policy_label(
    ReferenceTraceRuntimePolicy policy);

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
