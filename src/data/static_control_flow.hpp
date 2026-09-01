#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace eon {

// A bounded, metadata-only view of tools/extract_static_control_flow.py's v1
// output.  It deliberately retains neither original-media bytes nor decoded
// instructions, and it never supplies a dispatch target to the runtime.
struct StaticControlFlowSummary {
    std::size_t document_count = 0;
    std::size_t range_count = 0;
    std::size_t edge_count = 0;
    std::uint64_t declared_byte_count = 0;
    // Hash identities are provenance metadata, not original media bytes. They
    // let diagnostics bind every parsed document to a rehashed release.
    std::map<std::string, std::size_t, std::less<>> archive_document_counts;
    // Embedded-release documents identify their scanner release through their
    // carrier archive; direct documents use archive_sha256 itself.
    std::map<std::string, std::size_t, std::less<>> release_document_counts;
    std::map<std::string, std::size_t, std::less<>> cpu_counts;
    std::map<std::string, std::size_t, std::less<>> edge_kind_counts;
    std::map<std::string, std::size_t, std::less<>> target_scope_counts;
};

// Parse only the strict project-eon.static-control-flow-set/v1 envelope and
// its project-eon.static-control-flow/v1 documents.  Invalid/unknown schema
// fields, malformed identities, arithmetic overflow, or any classification
// other than static-candidate-unclassified are rejected with std::invalid_argument.
// Input is capped at 32 MiB and parser nesting/node counts are capped too.
[[nodiscard]] StaticControlFlowSummary parse_static_control_flow_sidecar(std::string_view json);

} // namespace eon
