#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace eon {

// This adapter name identifies a capture contract, not a runtime execution
// path.  The parser below accepts only raw, ordered observations at the
// recovered title-bridge boundary; it neither supplies observed values to the
// game nor invokes an Amiga ABI.
inline constexpr std::string_view deuteros_amiga_title_bridge_reference_trace_adapter =
    "project-eon-reference-trace-v3/deuteros-amiga-en-title-bridge-v3";

struct DeuterosAmigaTitleBridgeReferenceTraceDiagnostics {
    std::size_t event_count = 0;
    std::size_t exec_return_count = 0;
    std::size_t open_library_return_count = 0;
    std::size_t graphics_call_count = 0;
    std::size_t graphics_return_count = 0;
    std::size_t custom_register_call_count = 0;
    std::size_t custom_register_return_count = 0;
    std::size_t callback_registration_return_count = 0;
    std::size_t queue_snapshot_count = 0;
    std::size_t callback_entry_count = 0;
    std::size_t selector_entry_count = 0;
    std::size_t local_call_count = 0;
    std::size_t local_return_count = 0;
    std::size_t dispatch_snapshot_count = 0;
};

[[nodiscard]] bool validate_deuteros_amiga_title_bridge_reference_events(
    std::string_view events, DeuterosAmigaTitleBridgeReferenceTraceDiagnostics& diagnostics,
    std::string& error);

} // namespace eon
