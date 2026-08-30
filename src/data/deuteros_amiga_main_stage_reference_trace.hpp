#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace eon {

// This is an observation-only contract for the clean Deuteros main stage.
// In particular, its PC is intentionally distinct from the later title stage
// even though the original loads both stages into overlapping RAM ranges.
inline constexpr std::string_view deuteros_amiga_main_stage_reference_trace_adapter =
    "project-eon-reference-trace-v3/deuteros-amiga-en-main-copy-loop-v3";

struct DeuterosAmigaMainStageReferenceTraceDiagnostics {
    std::size_t event_count = 0;
    std::size_t main_copy_loop_pc_count = 0;
};

[[nodiscard]] bool validate_deuteros_amiga_main_stage_reference_events(
    std::string_view events, DeuterosAmigaMainStageReferenceTraceDiagnostics& diagnostics,
    std::string& error);

} // namespace eon
