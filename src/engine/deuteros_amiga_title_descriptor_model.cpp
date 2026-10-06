#include "engine/deuteros_amiga_title_descriptor_model.hpp"

namespace eon {
namespace {
constexpr std::uint32_t kRowAddress = 0x3f904;
constexpr std::uint32_t kFlagsAddress = 0x3f7be;
constexpr std::array<std::uint32_t, 2> kDestinations{{0x3f7de, 0x3f7ec}};
}

const DeuterosAmigaTitleDescriptorProfile&
deuteros_amiga_title_descriptor_profile() {
    static constexpr DeuterosAmigaTitleDescriptorProfile profile{
        "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
        0x3fbf8, 0x00b8, 0x1fbe6, 0x3f7fa, 14, kFlagsAddress, kDestinations,
        "a2301bf08c6c1c5615368687c620f17293d3f148e19934092854762ca91507b2",
        "static model only; requires runtime preimages; no action/frame semantics"};
    return profile;
}

std::optional<std::vector<DeuterosAmigaTitleDescriptorWrite>>
evaluate_deuteros_amiga_title_descriptor(
    const DeuterosAmigaTitleDescriptorInputs& in) {
    if (in.gate_byte != 0 || in.caller_d0 != 0x13 || in.caller_d1 != 0x0c
        || in.selected_row_address != kRowAddress)
        return std::nullopt;

    // The original tests the row's first long as signed positive before the
    // D1=$0c selection path. The runtime-provided bytes are preserved verbatim.
    const std::uint32_t first_long =
        (std::uint32_t{in.selected_row[0]} << 24)
        | (std::uint32_t{in.selected_row[1]} << 16)
        | (std::uint32_t{in.selected_row[2]} << 8)
        | std::uint32_t{in.selected_row[3]};
    if ((first_long & 0x80000000U) != 0)
        return std::nullopt;

    const std::uint16_t new_flags = static_cast<std::uint16_t>(in.flags_preimage | 0x000c);
    std::vector<DeuterosAmigaTitleDescriptorWrite> writes;
    writes.reserve(3);
    writes.push_back({kFlagsAddress, {
        static_cast<std::uint8_t>(new_flags >> 8),
        static_cast<std::uint8_t>(new_flags & 0xff)}});
    for (const auto address : kDestinations)
        writes.push_back({address, std::vector<std::uint8_t>(
            in.selected_row.begin(), in.selected_row.end())});
    return writes;
}

} // namespace eon
