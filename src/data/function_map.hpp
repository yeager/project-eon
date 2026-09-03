#pragma once

#include "data/static_control_flow.hpp"
#include "platform/game_data.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace eon {

// A named function-level preservation record.  Unlike a recovery-map parser
// boundary, this names one byte-verified routine or dispatch site and retains
// both its immutable source identity and its observed/load-time address.
// It is not executable metadata: no entry contains replacement bytes, a host
// callback, a hook address, or a result to emulate.
struct FunctionMapEntry {
    std::string_view id;
    std::string_view release_sha256;
    std::string_view parser_profile_id;
    Game game;
    Platform platform;
    std::string_view language;
    std::string_view cpu;
    // The owning original object and the externally declared control-flow
    // range are distinct identities. They may be equal for a complete leaf.
    std::string_view source_asset_sha256;
    std::string_view source_offset;
    std::string_view runtime_address;
    std::string_view evidence_level;
    std::string_view uncertainty;
    std::string_view runtime_status;
    std::string_view documentation_anchor;
    std::string_view source_span_sha256;
    std::string_view address_space = "runtime";
};

// A strict source-provenance cross-check between the compiled function map
// and an already admitted external static-control-flow sidecar. A binding
// means only that the exact release, CPU, address space, leaf hash and byte
// coordinate occur in a declared range. It does not establish that the bytes
// are code, reachable, callable, loaded, or executed.
struct FunctionMapSidecarCoverage {
    std::size_t function_entry_count = 0;
    std::size_t direct_range_binding_count = 0;
    std::size_t not_declared_by_sidecar_count = 0;
};

[[nodiscard]] std::span<const FunctionMapEntry> function_map();
// Function-map declarations are documentation-order records, not an index.
// Return an explicit filtered snapshot so adding a function for an existing
// release later in the declaration cannot silently hide it behind another
// platform's row. This API is diagnostics-only and is not called in any
// frame-critical execution path.
[[nodiscard]] std::vector<FunctionMapEntry> function_map_for_release(
    std::string_view release_sha256);
// Structural validation for declarative diagnostics. This accepts only the
// two explicitly documented address spaces and deliberately does not compare
// a function asset to a parser-profile leaf: some verified functions reside
// in a derived PRG or staged image rather than the outer disk span.
[[nodiscard]] bool function_map_entry_is_well_formed(const FunctionMapEntry& entry);
// Every function row must remain an unambiguous diagnostics-only declaration
// for one manifest release and one parser profile. This validates no media and
// does not turn the map into an execution table.
[[nodiscard]] bool function_map_manifest_is_valid();
[[nodiscard]] bool release_has_function_map_entry(
    std::string_view release_sha256, std::string_view entry_id);
[[nodiscard]] FunctionMapSidecarCoverage function_map_sidecar_coverage(
    const StaticControlFlowSummary& sidecar);

} // namespace eon
