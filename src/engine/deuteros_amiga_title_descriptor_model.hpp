#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace eon {

// Static, model-derived writes for the caller at $1fbe6 invoking $3fbf8
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
    std::uint32_t selected_row_address = 0x3f904;
    std::array<std::uint8_t, 14> selected_row{};
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
};

[[nodiscard]] const DeuterosAmigaTitleDescriptorProfile&
deuteros_amiga_title_descriptor_profile();

// Returns no plan for any mismatch or for a nonzero runtime gate. The caller
// must supply observed runtime preimages; static ADF bytes are not accepted as
// a substitute. The result contains ordered, sparse big-endian writes.
[[nodiscard]] std::optional<std::vector<DeuterosAmigaTitleDescriptorWrite>>
evaluate_deuteros_amiga_title_descriptor(
    const DeuterosAmigaTitleDescriptorInputs& inputs);

} // namespace eon
