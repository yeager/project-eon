#pragma once

#include "platform/game_data.hpp"

#include <span>
#include <string_view>

namespace eon {

// The one declarative inventory of versioned trace adapters. It is a
// preservation map, never a guest-code dispatch table.
enum class ReferenceTraceRuntimePolicy { diagnostics_only, transient_call_free_gx_startup };

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
};

[[nodiscard]] std::span<const ReferenceTraceAdapterDescriptor> reference_trace_adapter_registry();
[[nodiscard]] const ReferenceTraceAdapterDescriptor* reference_trace_adapter_descriptor(
    std::string_view wire_id);

} // namespace eon
