#include "engine/deuteros_amiga_title_stage_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {

DeuterosAmigaTitleStageSession::DeuterosAmigaTitleStageSession(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::uint16_t incoming_profile)
    : disk_(&disk), stage_(plan.title_stage), profile_(parse_deuteros_amiga_title_stage(disk, plan)) {
    if (stage_.length == 0 || stage_.disk_offset > AmigaAdf::standard_size
        || stage_.length > AmigaAdf::standard_size - stage_.disk_offset
        || stage_.entry_address < stage_.destination
        || stage_.entry_address - stage_.destination >= stage_.length) {
        throw std::runtime_error("Invalid Deuteros Amiga title-stage provenance");
    }
    original_sha256_ = to_hex(sha256(original_bytes()));
    constexpr std::string_view clean_title_stage_sha256 =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    if (original_sha256_ != clean_title_stage_sha256) {
        throw std::runtime_error("Unexpected Deuteros Amiga title-stage media");
    }
    // Keep the caller-proven bootstrap profile joined to the exact loaded
    // stage. The helper models only local writes and stops before Exec.
    entry_prefix_ = execute_deuteros_amiga_title_entry_prefix(disk, plan, incoming_profile);
    entry_prefix_state_ = materialize_deuteros_amiga_title_entry_prefix_state(
        disk, plan, incoming_profile);
    exec_prelude_ = execute_deuteros_amiga_title_exec_prelude(disk, plan, incoming_profile);
    if (entry_prefix_state_.incoming_profile != entry_prefix_.incoming_profile
        || entry_prefix_state_.stop_before_exec_address != entry_prefix_.stop_before_exec_address
        || entry_prefix_state_.writes[0].address != entry_prefix_.mode_word_address
        || entry_prefix_state_.writes[0].width_bytes != 2
        || entry_prefix_state_.writes[0].value != entry_prefix_.mode_word_value
        || entry_prefix_state_.writes[1].address != entry_prefix_.normal_mode_byte_address
        || entry_prefix_state_.writes[1].width_bytes != 1
        || entry_prefix_state_.writes[1].value != entry_prefix_.normal_mode_byte_value) {
        throw std::runtime_error("Deuteros title prefix state detached from original entry evidence");
    }
    if (exec_prelude_.incoming_profile != entry_prefix_.incoming_profile
        || exec_prelude_.entry_address != entry_prefix_.stop_before_exec_address
        || exec_prelude_.stack_pointer_value != 0x40b62
        || exec_prelude_.stop_before_exec_base_read_address != 0x40456) {
        throw std::runtime_error("Deuteros title Exec prelude detached from original entry evidence");
    }
}

std::span<const std::uint8_t> DeuterosAmigaTitleStageSession::original_bytes() const {
    if (!disk_) throw std::runtime_error("Missing Deuteros Amiga title-stage source disk");
    return disk_->bytes(stage_.disk_offset, stage_.length);
}

std::array<RgbColor, 16> DeuterosAmigaTitleStageSession::transition_palette_evidence() const {
    constexpr std::size_t word_count = 16;
    constexpr std::size_t byte_count = word_count * 2U;
    const auto address = profile_.transition_source_palette_address;
    if (address < stage_.destination || address - stage_.destination > stage_.length
        || byte_count > stage_.length - (address - stage_.destination)) {
        throw std::runtime_error("Deuteros title palette evidence lies outside original stage");
    }
    const auto bytes = original_bytes().subspan(address - stage_.destination, byte_count);
    std::array<RgbColor, word_count> colors{};
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const auto rgb4 = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[index * 2U]) << 8U) | bytes[index * 2U + 1U]);
        if ((rgb4 & 0xf000U) != 0) {
            throw std::runtime_error("Invalid Deuteros title RGB4 palette evidence");
        }
        colors[index] = {static_cast<std::uint8_t>(((rgb4 >> 8U) & 0xfU) * 17U),
            static_cast<std::uint8_t>(((rgb4 >> 4U) & 0xfU) * 17U),
            static_cast<std::uint8_t>((rgb4 & 0xfU) * 17U)};
    }
    return colors;
}

} // namespace eon
