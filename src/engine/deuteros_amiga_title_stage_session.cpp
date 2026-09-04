#include "engine/deuteros_amiga_title_stage_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {

DeuterosAmigaTitleStageSession::DeuterosAmigaTitleStageSession(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::uint16_t incoming_profile)
    : disk_(&disk), stage_(plan.title_stage), profile_(parse_deuteros_amiga_title_stage(disk, plan)),
      entry_prefix_(execute_deuteros_amiga_title_entry_prefix(disk, plan, incoming_profile)),
      entry_prefix_state_(materialize_deuteros_amiga_title_entry_prefix_state(
          disk, plan, incoming_profile)),
      exec_prelude_(execute_deuteros_amiga_title_exec_prelude(disk, plan, incoming_profile)),
      graphics_setup_(parse_deuteros_amiga_title_graphics_setup_profile(disk, plan)),
      display_clear_(parse_deuteros_amiga_title_display_clear_profile(disk, plan)),
      exec_boundary_session_(disk, plan, exec_prelude_, profile_) {
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
    // Validate the two immediately following caller-connected local helpers
    // as part of admitting this live title-stage boundary.  They describe
    // original bytes and operands only: neither helper is executed and no
    // display memory is allocated from their externally supplied pointer.
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
    if (graphics_setup_.palette_source_address != profile_.transition_source_palette_address
        || graphics_setup_.palette_words.size() != 20
        || graphics_setup_.palette_destination_address != 0x12ecc
        || display_clear_.destination_pointer_address
            != graphics_setup_.external_display_base_destinations[0]
        || display_clear_.iteration_count != 0x1f40
        || display_clear_.write_width_bytes != 4) {
        throw std::runtime_error("Deuteros title graphics setup detached from original stage evidence");
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

std::array<RgbColor, 20> DeuterosAmigaTitleStageSession::graphics_setup_palette_evidence() const {
    constexpr std::size_t word_count = 20;
    constexpr std::size_t byte_count = word_count * 2U;
    const auto address = graphics_setup_.palette_source_address;
    if (address < stage_.destination || address - stage_.destination > stage_.length
        || byte_count > stage_.length - (address - stage_.destination)) {
        throw std::runtime_error("Deuteros title graphics-setup palette lies outside original stage");
    }
    const auto bytes = original_bytes().subspan(address - stage_.destination, byte_count);
    std::array<RgbColor, word_count> colors{};
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const auto rgb4 = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[index * 2U]) << 8U) | bytes[index * 2U + 1U]);
        if ((rgb4 & 0xf000U) != 0) {
            throw std::runtime_error("Invalid Deuteros title graphics-setup RGB4 palette");
        }
        colors[index] = {static_cast<std::uint8_t>(((rgb4 >> 8U) & 0xfU) * 17U),
            static_cast<std::uint8_t>(((rgb4 >> 4U) & 0xfU) * 17U),
            static_cast<std::uint8_t>((rgb4 & 0xfU) * 17U)};
        if (rgb4 != graphics_setup_.palette_words[index]) {
            throw std::runtime_error("Deuteros title palette evidence detached from graphics setup");
        }
    }
    return colors;
}

std::optional<DeuterosAmigaTitleStageSession::LocalPrefixAdvance>
DeuterosAmigaTitleStageSession::execute_local_prefix() {
    if (local_prefix_executed_) return std::nullopt;
    local_prefix_executed_ = true;
    const auto exec_boundary = exec_boundary_session_.enter_after_local_prefix(
        exec_prelude_.stack_pointer_value);
    if (!exec_boundary
        || exec_boundary->state
            != DeuterosAmigaTitleExecBoundaryState::awaiting_exec_base_read) {
        throw std::runtime_error("Deuteros title Exec boundary did not advance");
    }
    return LocalPrefixAdvance{entry_prefix_state_.writes, exec_prelude_.stack_pointer_value,
        exec_prelude_.stop_before_exec_base_read_address};
}

std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint>
DeuterosAmigaTitleStageSession::observe_exec_return(
    const DeuterosAmigaObservedExecReturn& observation) {
    if (!local_prefix_executed_) return std::nullopt;
    const auto advanced = exec_boundary_session_.observe_exec_return(observation);
    if (advanced && advanced->state
            == DeuterosAmigaTitleExecBoundaryState::before_open_library_boundary) {
        open_library_boundary_session_.emplace(
            *advanced, graphics_setup_, profile_, display_clear_);
    }
    return advanced;
}

} // namespace eon
