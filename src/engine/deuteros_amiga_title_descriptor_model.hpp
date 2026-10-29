#pragma once

#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace eon {

// Static, disassembly-derived writes for the caller at $1fbe6 invoking $3fbf8
// with D0=$13/D1=$0c. This is not a capture adapter or a gameplay action.
struct DeuterosAmigaTitleDescriptorWrite {
    std::uint32_t address = 0;
    std::vector<std::uint8_t> bytes;
    bool operator==(const DeuterosAmigaTitleDescriptorWrite&) const = default;
};

struct DeuterosAmigaTitleDescriptorInputs {
    std::uint8_t gate_byte = 0;
    std::uint16_t caller_d0 = 0x13;
    std::uint16_t caller_d1 = 0x0c;
    std::uint32_t descriptor_address = 0x3f904;
    // Caller-reported source offsets for the descriptor marker and optional
    // data pointer. The model checks these offsets and value consistency, but
    // does not establish observation chronology or runtime provenance.
    std::uint32_t descriptor_marker_source_address = 0x3f904;
    std::uint32_t descriptor_marker = 0;
    std::uint32_t descriptor_data_pointer_source_address = 0x3f908;
    std::uint32_t descriptor_data_pointer = 0;
    std::uint32_t source_row_address = 0x3f904;
    std::array<std::uint8_t, 14> source_row{};
    std::uint16_t flags_preimage = 0;
};

struct DeuterosAmigaTitleDescriptorProfile {
    std::string_view release_sha256;
    std::uint32_t routine_address;
    std::uint16_t routine_size;
    std::uint32_t caller_address;
    std::uint32_t source_table_address;
    std::uint16_t row_stride;
    std::uint32_t flags_address;
    std::array<std::uint32_t, 2> destination_addresses;
    std::string_view routine_sha256;
    std::string_view scope;
    std::uint32_t source_read_disk_offset;
    std::uint32_t source_read_runtime_address;
    std::uint32_t source_disk_offset;
    std::string_view caller_sha256;
};

enum class DeuterosAmigaTitleDescriptorCallOrigin : std::uint8_t {
    selector_dispatch,
    response_loop,
};

struct DeuterosAmigaTitleDescriptorCallBoundary {
    std::uint64_t trace_sequence = 0;
    std::string_view release_sha256;
    DeuterosAmigaTitleDescriptorCallOrigin origin =
        DeuterosAmigaTitleDescriptorCallOrigin::selector_dispatch;
    std::uint32_t program_counter = 0;
    std::uint32_t stack_pointer = 0;
    std::uint32_t return_address = 0;
    std::uint16_t d0 = 0;
    std::uint16_t d1 = 0;
};

struct DeuterosAmigaTitleDescriptorNativeAdvance {
    std::uint64_t trace_sequence = 0;
    std::uint32_t next_program_counter = 0;
    std::uint8_t gate_byte = 0;
    std::vector<NativeRuntimeEffectBatch> ordered_batches;
};

[[nodiscard]] const DeuterosAmigaTitleDescriptorProfile&
deuteros_amiga_title_descriptor_profile();

// Models the verified instruction sequence only. The caller must supply
// runtime observations for the gate, table marker/pointer, source row and flag
// preimage; static ADF bytes are not accepted as runtime substitutes. The
// result contains ordered, sparse big-endian writes.
[[nodiscard]] std::optional<std::vector<DeuterosAmigaTitleDescriptorWrite>>
evaluate_deuteros_amiga_title_descriptor(
    const DeuterosAmigaTitleDescriptorInputs& inputs);

// Translate one dynamically observed `$3fbf8` call using only already-owned
// runtime memory. Supported origins are the direct `$1fc08` selector call and
// the `$40786 -> $3f7a8 -> $3fbf8` tail-call path. The scheduler does not invoke
// this automatically; the caller receipt, live code, stack return, and local
// preimages must match the admitted release profile.
[[nodiscard]] std::optional<DeuterosAmigaTitleDescriptorNativeAdvance>
advance_deuteros_amiga_title_descriptor_native(
    const NativeRuntimeMemory& memory,
    const DeuterosAmigaTitleDescriptorCallBoundary& boundary);

} // namespace eon
